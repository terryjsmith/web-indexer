
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
#include <robotstxt.h>
#include <site.h>
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
	printf("Created thread #%d\n", m_threadid);
}

void* Worker::_thread_function(void* ptr) {
	// Start up, connect to databases
	Worker* worker = (Worker*)ptr;
	worker->Run();
	return(0);
}

void Worker::Run() {
	printf("Started thread #%d\n", m_threadid);

	// Get a connection to redis to grab URLs
        m_context = redisConnect(REDIS_HOST, REDIS_PORT);
        if(m_context->err) {
                printf("Thread #%d unable to connect to redis.\n", m_threadid);
                pthread_exit(0);
        }
	printf("Thread #%d connected to redis.\n", m_threadid);

        // Connect to MySQL
        m_conn = mysql_init(NULL);
        if(!mysql_real_connect(m_conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
                printf("Thread #%d unable to connect to MySQL: %s\n", m_threadid, mysql_error(m_conn));
                pthread_exit(0);
        }
	printf("Thread #%d connected to MySQL.\n", m_threadid);

	mysql_set_character_set(m_conn, "utf8");

	// Set up our libevent notification base
	int epoll = epoll_create(CONNECTIONS_PER_THREAD);
	if(epoll < 0) {
		printf("Thread #%d unable to create epoll interface.\n", m_threadid);
                pthread_exit(0);
	}

	printf("Thread #%d initialized epoll.\n", m_threadid);

	// Set up to start doing transfers
        unsigned int max_tries = 100;

        redisReply* reply = (redisReply*)redisCommand(m_context, "LLEN url_queue");
        max_tries = min(max_tries, reply->integer);
        freeReplyObject(reply);

	// Initialize our stack of HttpRequests
	HttpRequest** requests = (HttpRequest**)malloc(CONNECTIONS_PER_THREAD * sizeof(HttpRequest*));
	memset(requests, 0, CONNECTIONS_PER_THREAD * sizeof(HttpRequest*));

        unsigned int active_connections = 0;

        unsigned int counter = 0;
        for(unsigned int i = 0; i < CONNECTIONS_PER_THREAD; i++) {
		if(counter >= max_tries) break;
                counter++;

                // Fetch a URL from redis
                redisReply* reply = (redisReply*)redisCommand(m_context, "LPOP url_queue");

                // Split the URL info it's parts; no base URL
                URL* url = new URL(reply->str);
                url->Parse(0);

                freeReplyObject(reply);

                // Verify the scheme is one we want (just http for now)
                if(strcmp(url->parts[URL_SCHEME], "http") != 0) {
                        delete url;
                        i--;
                        continue;
                }

                // Load the site info from the database
                Site* site = new Site();
                site->Load(url->parts[URL_DOMAIN], m_conn);
                url->domain_id = site->domain_id;

                // Check whether this domain is on a timeout
                time_t now = time(NULL);
                if((now - site->GetLastAccess()) < MIN_ACCESS_TIME) {
                        reply = (redisReply*)redisCommand(m_context, "RPUSH url_queue \"%s\"", url->url);
                        freeReplyObject(reply);

                        delete site;
                        delete url;
                        i--;
                        continue;
                }

                // Next check if we've already parsed this URL
                if(url->Load(m_conn)) {
                        delete site;
                        delete url;
                        i--;
                        continue;
                }

                // Finally, make sure the URL isn't disallowed by robots.txt
                RobotsTxt* robots = new RobotsTxt();
                robots->Load(url, m_conn);
                if(robots->Check(url) == true) {
			delete robots;
                        delete site;
                        delete url;
                        i--;
                        continue;
                }

                // Set the last access time to now
                site->SetLastAccess(now);

                delete site;
                delete robots;

                printf("Fetching URL %s...\n", url->url);

                // Otherwise, we're good
                requests[i] = new HttpRequest(url);
		int socket = requests[i]->Initialize();
		if(!socket) {
			printf("Thread #%d unable to initialize socket for %s.\n", m_threadid, url->url);
	                pthread_exit(0);
		}

		if(!requests[i]->Start()) {
			delete requests[i];
			requests[i] = 0;
                        delete url;
                        i--;
                        continue;
		}

		// Add the socket to epoll
		struct epoll_event event;
		memset(&event, 0, sizeof(epoll_event));

		event.data.fd = socket;
		event.events = EPOLLIN | EPOLLET;
		if((epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &event)) < 0) {
			printf("Thread #%d unable to setup epoll for %s.\n", m_threadid, url->url);
                        pthread_exit(0);
		}

                // Clean up
                delete url;

                // Add it to the multi stack
                active_connections++;
        }

        while(true) {
		epoll_event* events = (epoll_event*)malloc(CONNECTIONS_PER_THREAD * sizeof(epoll_event));
		memset(events, 0, CONNECTIONS_PER_THREAD * sizeof(epoll_event));

		int msgs = epoll_wait(epoll, events, CONNECTIONS_PER_THREAD, -1);
		for(unsigned int i = 0; i < msgs; i++) {
			// Find the applicable HttpRequest object
			int position = 0;
			for(unsigned int j = 0; j < CONNECTIONS_PER_THREAD; j++) {
				if(!requests[j]) continue;

				if(requests[j]->GetFD() == events[i].data.fd) {
					position = j;
					break;
				}
			}

			if(!requests[position]) {
				printf("Thread #%d unable to look up request for socket.\n", m_threadid);
				pthread_exit(0);
			}

			bool process = false;
			if(requests[position]->Read() == 0) {
				process = true;
			}

			if(process) {
				URL* url = requests[position]->GetURL();
				requests[position]->Process();

				int code = requests[position]->GetCode();

				char* query = (char*)malloc(1000);
                                time_t now = time(NULL);
				int length = sprintf(query, "UPDATE url SET last_code = %d, last_update = %ld  WHERE url_id = %ld", code, now, url->url_id);
				printf("UPDATE url SET last_code = %d, last_update = %ld  WHERE url_id = %ld\n", code, now, url->url_id);
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

				active_connections--;
				delete requests[position];
				requests[position] = 0;
			}
		}

		free(events);

                // Set up a maximum number of tries to get a new URL
                unsigned int counter = 0;
                unsigned int max_tries = 100;

                reply = (redisReply*)redisCommand(m_context, "LLEN url_queue");
                max_tries = min(max_tries, reply->integer);
                freeReplyObject(reply);

                while(active_connections < CONNECTIONS_PER_THREAD) {
                        if(counter >= max_tries) break;
                        counter++;

			// Make sure we have an empty slot
			int slot = -1;
			for(unsigned int i = 0; i < CONNECTIONS_PER_THREAD; i++) {
				if(requests[i] == 0)
					slot = i;
			}

			if(slot < 0) break;

                        // Fetch a URL from redis
                        redisReply* reply = (redisReply*)redisCommand(m_context, "LPOP url_queue");

                        // Split the URL info it's parts; no base URL
                        URL* url = new URL(reply->str);
                        url->Parse(NULL);

                        // Verify the scheme is one we want (just http for now)
                        if(strcmp(url->parts[URL_SCHEME], "http") != 0) {
                                delete url;
                                continue;
                        }

                        freeReplyObject(reply);

                        // Load the site info from the database
                        Site* site = new Site();
                        site->Load(url->parts[URL_DOMAIN], m_conn);
                        url->domain_id = site->domain_id;

                        // Check whether this domain is on a timeout
                        time_t now = time(NULL);
                        if((now - site->GetLastAccess()) < MIN_ACCESS_TIME) {
                                reply = (redisReply*)redisCommand(m_context, "RPUSH url_queue \"%s\"", url->url);
                                freeReplyObject(reply);

                                delete site;
                                delete url;
                                continue;
                        }

                        // Next check if we've already parsed this URL
                        if(url->Load(m_conn)) {
                                delete site;
                                delete url;
                                continue;
                        }

                        // Finally, make sure the URL isn't disallowed by robots.txt
                        RobotsTxt* robots = new RobotsTxt();
                        robots->Load(url, m_conn);
                        if(robots->Check(url) == true) {
				delete robots;
                                delete site;
                                delete url;
                                continue;
                        }

                        // Set the last access time to now
                        site->SetLastAccess(now);

                        delete site;
                        delete robots;

                        printf("Fetching URL %s...\n", url->url);

			// Otherwise, we're good
	                requests[slot] = new HttpRequest(url);
        	        int socket = requests[slot]->Initialize();
	                if(!socket) {
        	                printf("Thread #%d unable to initialize socket for %s.\n", m_threadid, url->url);
                	        pthread_exit(0);
	                }

			if(!requests[slot]->Start()) {
        	                delete requests[slot];
	                        requests[slot] = 0;
                        	delete url;
        	                continue;
	                }

        	        // Add the socket to epoll
                	struct epoll_event event;
			memset(&event, 0, sizeof(epoll_event));

	                event.data.fd = socket;
        	        event.events = EPOLLIN | EPOLLET;
                	if((epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &event)) < 0) {
	                        printf("Thread #%d unable to setup epoll for %s.\n", m_threadid, url->url);
        	                pthread_exit(0);
                	}

                        // Clean up
                        delete url;

                        // Add it to the multi stack
                        active_connections++;
                }

                sleep(1);
        }

        // Clean up
        mysql_close(m_conn);
        redisFree(m_context);

        return;
}
