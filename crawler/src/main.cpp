
/* INCLUDES */

#include <vector>
using namespace std;

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>

#include <url.h>
#include <httprequest.h>
#include <robotstxt.h>
#include <site.h>

/* DEFINITIONS */

#define NUM_CONNECTIONS 20
#define BASE_PATH	"/mnt/indexer/"
#define MIN_ACCESS_TIME 10

/* GLOBALS */

FILE* log_file = NULL;
MYSQL* conn = NULL;

/* FUNCTIONS */

void log(char* line) {
	char* newline = "\n";
	time_t timestamp = time(NULL);
	fprintf(log_file, "[%ld]: ", timestamp);

	fwrite(line, strlen(line), 1, log_file);
	fwrite(newline, 1, 1, log_file);

	fflush(log_file);
	printf("%s\n", line);
}

int main(int argc, char** argv) {
	redisContext *context = NULL;

	// Open a connection to our log file
	if(!(log_file = fopen("/var/log/crawler.log", "w+"))) {
		printf("Unable to open log file.");
		return(0);
	}

	// Get a connection to redis to grab URLs
	context = redisConnect("localhost", 6379);
	if(context->err) {
		log("Unable to connect to redis.");
		return(0);
	}
	log("Connected to redis.");

	// Connect to MySQL
	conn = mysql_init(NULL);
	if(mysql_real_connect(conn, "localhost", "crawler", "SpasWehabEp4", NULL, 0, NULL, 0) == NULL) {
		log("Unable to connect to MySQL.\n");
		return(0);
	}
	log("Connected to MySQL.");

	mysql_select_db(conn, "crawler");

	// Do global cURL initialization
	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURLM* multi = curl_multi_init();

	// Set up to start doing transfers
	unsigned int max_tries = 100;

	redisReply* reply = (redisReply*)redisCommand(context, "LLEN url_queue");
	max_tries = min(max_tries, reply->integer);
	freeReplyObject(reply);

	unsigned int counter = 0;
	for(unsigned int i = 0; i < NUM_CONNECTIONS; i++) {
		if(counter >= max_tries) break;
		counter++;

		// Fetch a URL from redis
		redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

		printf("Trying URL %s...\n", reply->str);

		// Split the URL info it's parts; no base URL
		URL* url = new URL(reply->str);
		url->Parse(0);

		freeReplyObject(reply);

		// Load the site info from the database
		Site* site = new Site();
		site->Load(url->parts[URL_DOMAIN], conn);
		url->domain_id = site->domain_id;

		// Check whether this domain is on a timeout
		time_t now = time(NULL);
		if((now - site->GetLastAccess()) < MIN_ACCESS_TIME) {
                        reply = (redisReply*)redisCommand(context, "RPUSH url_queue \"%s\"", url->url);
			freeReplyObject(reply);

			delete site;
                        delete url;
                        i--;
                        continue;
		}

		// Next check if we've already parsed this URL
		if(url->Load(conn)) {
			delete site;
			delete url;
			i--;
			continue;
		}

		// Finally, make sure the URL isn't disallowed by robots.txt
		RobotsTxt* robots = new RobotsTxt();
		robots->Load(url, conn);
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
			return(0);
		}

		// Clean up
		free(filename);
		delete url;

		// Add it to the multi stack
		curl_multi_add_handle(multi, request->GetHandle());
	}

	while(true) {
		// Main loop, start any transfers that need to be started
		int running = 0;
		CURLMcode code = curl_multi_perform(multi, &running);
		while((code = curl_multi_perform(multi, &running)))
			code = curl_multi_perform(multi, &running);

		int msgs = 0;
		CURLMsg* msg = curl_multi_info_read(multi, &msgs);
		while(msg != NULL) {
			/// Get the HttpRequest this was associated with
                        HttpRequest* request = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

			if(!request) continue;

			// This means that something finished; either with an error or with success
			if(msg->data.result != CURLE_OK) {
				// Get the error and URL to be logged
				char* curl_error = (char*)curl_easy_strerror(msg->data.result);
				char* url =  request->GetURL()->url;

				unsigned int length = strlen(url) + 2 + strlen(curl_error);
				char* final = (char*)malloc(length + 1);
				sprintf(final, "%s: %s", url, curl_error);
				final[length] = '\0';

				log(final);
				free(final);

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
			redisReply* reply = (redisReply*)redisCommand(context, "RPUSH parse_queue \"%s\"", filename);
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
                		        site->Load(new_url->parts[URL_DOMAIN], conn);
                                	new_url->domain_id = site->domain_id;

					// Find the new domain and path info
					new_url->Load(conn);

					// Make sure we mark that we actually checked this domain as well
					site->SetLastAccess(time(NULL));
					delete site;

					// Insert a record into the URL table
					char* query = (char*)malloc(1000);
		                        length = sprintf(query, "INSERT INTO redirect VALUES(%ld, %ld)", url->url_id, new_url->url_id);
                		        query[length] = '\0';
		                        mysql_query(conn, query);

                		        free(query);

					// Make sure we update the first URL to reflect the status of the last
					query = (char*)malloc(1000);
					time_t now = time(NULL);
		                        length = sprintf(query, "UPDATE url SET last_code = %ld, last_update = %ld WHERE url_id = %ld", code, now, url->url_id);
                		        query[length] = '\0';
		                        mysql_query(conn, query);

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
                        mysql_query(conn, query);

                        free(query);

			// Remove it from the stack
                        curl_multi_remove_handle(multi, msg->easy_handle);

			// Clean up
                        delete request;

			// Set up a maximum number of tries to get a new URL
			unsigned int counter = 0;
		        unsigned int max_tries = 100;

		        reply = (redisReply*)redisCommand(context, "LLEN url_queue");
		        max_tries = min(max_tries, reply->integer);
		        freeReplyObject(reply);

			// Fetch a new URL from redis
			while(true) {
				if(counter >= max_tries) break;
				counter++;

				// Fetch a URL from redis
        	        	redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

				printf("Trying URL %s...\n", reply->str);

	        	        // Split the URL info it's parts; no base URL
        	        	url = new URL(reply->str);
	                	url->Parse(NULL);

				freeReplyObject(reply);

				// Load the site info from the database
		                Site* site = new Site();
                		site->Load(url->parts[URL_DOMAIN], conn);
		                url->domain_id = site->domain_id;

                		// Check whether this domain is on a timeout
		                time_t now = time(NULL);
                		if((now - site->GetLastAccess()) < MIN_ACCESS_TIME) {
		                        reply = (redisReply*)redisCommand(context, "RPUSH url_queue \"%s\"", url->url);
					freeReplyObject(reply);

					delete site;
                		        delete url;
		                        continue;
                		}

		                // Next check if we've already parsed this URL
                		if(url->Load(conn)) {
					delete site;
		                        delete url;
		                        continue;
                		}

		              	// Finally, make sure the URL isn't disallowed by robots.txt
                		RobotsTxt* robots = new RobotsTxt();
		                robots->Load(url, conn);
                		if(robots->Check(url) == true) {
					delete site;
		                        delete url;
                        		continue;
		                }

                		// Set the last access time to now
		                site->SetLastAccess(now);

                		delete site;
		                delete robots;

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
		                	return(0);
                		}

				// Clean up
				free(filename);
				delete url;

        	        	// Add it to the multi stack
	        	        curl_multi_add_handle(multi, new_request->GetHandle());
				break;
			}

			msg = curl_multi_info_read(multi, &msgs);
		}

		sleep(1);
	}

	// Clean up
	mysql_close(conn);
	curl_multi_cleanup(multi);
	redisFree(context);

	return(0);
}
