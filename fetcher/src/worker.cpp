
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <pthread.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>
#include <sys/epoll.h>

#include <defines.h>
#include <url.h>
#include <httprequest.h>
#include <worker.h>

Worker::Worker() {
	m_threadid = 0;
	m_conn = 0;
	m_context = 0;
}

Worker::~Worker() {
}

void Worker::Start(int pos) {
	m_threadid = pos;

	int rc = pthread_create(&m_thread, NULL, Worker::_thread_function, this);
	if(rc) {
		printf("Unable to create thread %d.\n", pos);
		return;
	}
}

void* Worker::_thread_function(void* ptr) {
	// Start up, connect to databases
	Worker* worker = (Worker*)ptr;
	worker->run();
	return(0);
}

domain* Worker::load_domain_info(char* domain) {
	// Our return domain
	domain* ret = (domain*)malloc(sizeof(domain));
	memset(domain, 0, sizeof(domain));

	// Copy the domain in
	ret->domain = (char*)malloc(strlen(domain) + 1);
        strcpy(ret->domain, domain);

        // See if we have an existing domain
        char* query = (char*)malloc(100 + strlen(domain));
        unsigned int length = sprintf(query, "SELECT domain_id, domain, last_access, last_robots_access FROM domain WHERE domain = '%s'", domain);

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

	free(query);

        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);

		// Save the data
                ret->domain_id = atol(row[0]);
		ret->last_access = atol(row[2]);
		ret->last_robots_access = atol(row[3]);
        }
	else {
		// Insert a new row
		query = (char*)malloc(1000);
                length = sprintf(query, "INSERT INTO domain VALUES(NULL, '%s', 0, 0)", domain);
                query[length] = '\0';

                mysql_query(conn, query);
                free(query);

                ret->domain_id = (unsigned long int)mysql_insert_id(conn);
		ret->last_access = 0;
		ret->last_robots_access = 0;
	}

	mysql_free_result(result);

	return(ret);
}

bool Worker::url_exists(Url* url, domain* info) {
	// See if we have an existing domain
        char* query = (char*)malloc(100 + strlen(url->get_hash()));
        unsigned int length = sprintf(query, "SELECT url_id FROM url WHERE domain_id = %ld AND path_hash = '%s'", info->domain_id, url->get_hash());

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

	bool exists = false;
	if(mysql_num_rows(result)) {
		exists = true;
	}

	mysql_free_result(result);
	return(exists);
}

void Worker::run() {
	// Get a connection to redis to grab URLs
        m_context = redisConnect(REDIS_HOST, REDIS_PORT);
        if(m_context->err) {
                printf("Thread #%d unable to connect to redis.\n", m_threadid);
                pthread_exit(0);
        }

        // Connect to MySQL
        m_conn = mysql_init(NULL);
        if(!mysql_real_connect(m_conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
                printf("Thread #%d unable to connect to MySQL: %s\n", m_threadid, mysql_error(m_conn));
                pthread_exit(0);
        }

	mysql_set_character_set(m_conn, "utf8");

	// Set up our libevent notification base
	int epoll = epoll_create(CONNECTIONS_PER_THREAD);
	if(epoll < 0) {
		printf("Thread #%d unable to create epoll interface.\n", m_threadid);
                pthread_exit(0);
	}

	// Set up to start doing transfers
        unsigned int max_tries = 100;

        redisReply* reply = (redisReply*)redisCommand(m_context, "LLEN url_queue");
        max_tries = min(max_tries, reply->integer);
        freeReplyObject(reply);

	// Initialize our stack of HttpRequests
	m_requests = (HttpRequest**)malloc(CONNECTIONS_PER_THREAD * sizeof(HttpRequest*));
	memset(m_requests, 0, CONNECTIONS_PER_THREAD * sizeof(HttpRequest*));

	fill_list();

        while(true) {
		epoll_event* events = (epoll_event*)malloc(CONNECTIONS_PER_THREAD * sizeof(epoll_event));
		memset(events, 0, CONNECTIONS_PER_THREAD * sizeof(epoll_event));

		int msgs = epoll_wait(epoll, events, CONNECTIONS_PER_THREAD, -1);
		for(unsigned int i = 0; i < msgs; i++) {
			// Find the applicable HttpRequest object
                        int position = 0;
                        for(unsigned int j = 0; j < CONNECTIONS_PER_THREAD; j++) {
                                if(!m_requests[j]) continue;

                                if(m_requests[j]->GetFD() == events[i].data.fd) {
                                        position = j;
                                        break;
                                }
                        }

                        if(!m_requests[position]) {
                                printf("Thread #%d unable to look up request for socket.\n", m_threadid);
                                pthread_exit(0);
                        }

			if(!m_requests[i]->process(events[i])) {
				// TODO: something went wrong, figure that out
			}

			if(m_requests[i]->get_state() == HTTPREQUESTSTATE_COMPLETE) {
				// Mark as done and all that
				char* query = (char*)malloc(1000);
                                time_t now = time(NULL);
                                int length = sprintf(query, "UPDATE url SET last_code = %d, last_update = %ld  WHERE url_id = %ld", code, now, url->url_id);
                                query[length] = '\0';
                                mysql_query(m_conn, query);
                                free(query);

                                if(code == 200) {
                                        char* filename = requests[position]->GetFilename();

                                        // Add it to the parse_queue in redis
                                        redisReply* reply = (redisReply*)redisCommand(m_context, "RPUSH parse_queue \"%s\"", filename);
                                        freeReplyObject(reply);
                                }

                                if((code == 302) || (code == 301)) {
                                }

				delete m_requests[position];
				m_requests[position] = 0;
			}
		}

		free(events);
		fill_list();

                sleep(1);
        }

        // Clean up
        mysql_close(m_conn);
        redisFree(m_context);

        return;
}
