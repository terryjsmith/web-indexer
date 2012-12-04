
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <pthread.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>

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
                printf("Unable to connect to redis.\n");
                pthread_exit(0);
        }
	printf("Thread #%d connected to redis.\n", m_threadid);

        // Connect to MySQL
        m_conn = mysql_init(NULL);
        if(!mysql_real_connect(m_conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
                printf("Unable to connect to MySQL: %s\n", mysql_error(m_conn));
                pthread_exit(0);
        }
	printf("Thread #%d connected to MySQL.\n", m_threadid);

	mysql_set_character_set(m_conn, "utf8");

        CURLM* multi = curl_multi_init();

	printf("Thread #%d initialized cURL.\n", m_threadid);

	// Set up to start doing transfers
        unsigned int max_tries = 100;

        redisReply* reply = (redisReply*)redisCommand(m_context, "LLEN url_queue");
        max_tries = min(max_tries, reply->integer);
        freeReplyObject(reply);

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
                HttpRequest* request = new HttpRequest(url);

                unsigned int dir_length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]);
                char* dir = (char*)malloc(dir_length + 1);
                sprintf(dir, "%s%s", BASE_PATH, url->parts[URL_DOMAIN]);
                dir[dir_length] = '\0';

                mkdir(dir, 0644);
                free(dir);

                unsigned int length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
                char* filename = (char*)malloc(length + 1);
                sprintf(filename, "%s%s/%s.html", BASE_PATH, url->parts[URL_DOMAIN], url->hash);
                filename[length] = '\0';

                if(!(request->Open(filename))) {
                        printf("Unable to open file %s.\n", filename);
                        return;
                }

                // Clean up
                free(filename);
                delete url;

                // Add it to the multi stack
                active_connections++;
                curl_multi_add_handle(multi, request->GetHandle());
        }

        while(true) {
               // Main loop, start any transfers that need to be started
                int running = 0;
                CURLMcode code = curl_multi_perform(multi, &running);
                while(code)
                        code = curl_multi_perform(multi, &running);

                int msgs = 0;
                CURLMsg* msg = curl_multi_info_read(multi, &msgs);
                while(msg != NULL) {
                        active_connections--;

                        /// Get the HttpRequest this was associated with
                        HttpRequest* request = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

                        if(!request) {
                                msg = curl_multi_info_read(multi, &msgs);
                                continue;
                        }

                        // This means that something finished; either with an error or with success
                        if(msg->data.result != CURLE_OK) {
                                // Get the error and URL to be logged
                                char* curl_error = (char*)curl_easy_strerror(msg->data.result);
                                char* url =  request->GetURL()->url;

                                printf("%s: %s\n", url, curl_error);

				reply = (redisReply*)redisCommand(m_context, "RPUSH url_queue \"%s\"", url);
	                        freeReplyObject(reply);

                                msg = curl_multi_info_read(multi, &msgs);
                                continue;
                        }

                        // Get the URL from the request
                        URL* url = request->GetURL();

                        // Find the file this page was written to
                        unsigned int length = strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
                        char* filename = (char*)malloc(length + 1);
                        sprintf(filename, "%s/%s.html", url->parts[URL_DOMAIN], url->hash);
                        filename[length] = '\0';

                        // Add it to the parse_queue in redis
                        redisReply* reply = (redisReply*)redisCommand(m_context, "RPUSH parse_queue \"%s\"", filename);
                        freeReplyObject(reply);
                        free(filename);

                        // If the URL changed (ie. a redirect, save the redirected to URL as well)
                        char* redirect = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, redirect);

                        // Also save the HTTP code to the database
                        long code = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);

                        // Only need to do this if there was a new URL
                        if(redirect) {
                                if(strcmp(redirect, url->url)) {
                                        // There was a change in the URL, handle it
                                        URL* new_url = new URL(redirect);
                                        new_url->Parse(NULL);

                                        // Load the site info from the database
                                        Site* site = new Site();
                                        site->Load(new_url->parts[URL_DOMAIN], m_conn);
                                        new_url->domain_id = site->domain_id;

                                        // Find the new domain and path info
                                        new_url->Load(m_conn);

                                        // Make sure we mark that we actually checked this domain as well
                                        site->SetLastAccess(time(NULL));
                                        delete site;

                                        // Insert a record into the URL table
                                        char* query = (char*)malloc(1000);
                                        length = sprintf(query, "INSERT INTO redirect VALUES(%ld, %ld)", url->url_id, new_url->url_id);
                                        query[length] = '\0';
                                        mysql_query(m_conn, query);

                                        free(query);
                                        // Make sure we update the first URL to reflect the status of the last
                                        query = (char*)malloc(1000);
                                        time_t now = time(NULL);
                                        length = sprintf(query, "UPDATE url SET last_code = %ld, last_update = %ld WHERE url_id = %ld", code, now, url->url_id);
                                        query[length] = '\0';
                                        mysql_query(m_conn, query);

                                        free(query);

                                        // Finally, delete the old URL and use the new one
                                        delete url;
                                        url = new_url;
                                }
                        }

                        char* query = (char*)malloc(1000);
                        time_t now = time(NULL);
                        length = sprintf(query, "UPDATE url SET last_code = %ld, last_update = %ld  WHERE url_id = %ld", code, now, url->url_id);
                        query[length] = '\0';
                        mysql_query(m_conn, query);

                        free(query);

                        // Remove it from the stack
                        curl_multi_remove_handle(multi, msg->easy_handle);

                        // Clean up
                        delete request;

                        msg = curl_multi_info_read(multi, &msgs);
                }

                // Set up a maximum number of tries to get a new URL
                unsigned int counter = 0;
                unsigned int max_tries = 100;

                reply = (redisReply*)redisCommand(m_context, "LLEN url_queue");
                max_tries = min(max_tries, reply->integer);
                freeReplyObject(reply);

                while(active_connections < CONNECTIONS_PER_THREAD) {
                        if(counter >= max_tries) break;
                        counter++;

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
                                delete site;
                                delete url;
                                continue;
                        }

                        // Set the last access time to now
                        site->SetLastAccess(now);

                        delete site;
                        delete robots;

                        printf("Fetching URL %s...\n", url->url);

                        HttpRequest* new_request = new HttpRequest(url);

                        unsigned int dir_length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]);
                        char* dir = (char*)malloc(dir_length + 1);
                        sprintf(dir, "%s%s", BASE_PATH, url->parts[URL_DOMAIN]);
                        dir[dir_length] = '\0';

                        mkdir(dir, 0644);
                        free(dir);

                        unsigned int length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
                        char* filename = (char*)malloc(length + 1);
                        sprintf(filename, "%s%s/%s.html", BASE_PATH, url->parts[URL_DOMAIN], url->hash);
                        filename[length] = '\0';

                        if(!(new_request->Open(filename))) {
                                printf("Unable to open file %s.\n", filename);
                                return;
                        }

                        // Clean up
                        free(filename);
                        delete url;

                        // Add it to the multi stack
                        active_connections++;
                        curl_multi_add_handle(multi, new_request->GetHandle());
                }

                sleep(1);
        }

        // Clean up
        mysql_close(m_conn);
        curl_multi_cleanup(multi);
        redisFree(m_context);

        return;
}
