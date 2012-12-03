
/* INCLUDES */

#include <vector>
using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>

#include <url.h>
#include <httprequest.h>

/* DEFINITIONS */

#define NUM_CONNECTIONS	10
#define BASE_PATH	"/var/indexer/"

/* GLOBALS */

redisContext *context = NULL;
FILE* log_file = NULL;

/* FUNCTIONS */

void log(char* line) {
	char* newline = "\n";
	time_t timestamp = time(NULL);
	fprintf(log_file, "[%ld]: ", timestamp);

	fwrite(line, strlen(line), 1, log_file);
	fwrite(newline, 1, 1, log_file);
}

int main(int argc, char** argv) {
	// Open a connection to our log file
	if(!(log_file = fopen("/var/log/crawler.log", "w+"))) {
		printf("Unable to open log file.");
		return(0);
	}

	// Get a connection to redis to grab URLs
	context = redisConnect("localhost", 6379);
	if(context->err) {
		printf("Unable to connect to redis: %s\n", context->errstr);
		return(0);
	}
	printf("Connected to redis.\n");

	// Do global cURL initialization
	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURLM* multi = curl_multi_init();

	// Set up to start doing transfers
	HttpRequest** requests = (HttpRequest**)malloc(NUM_CONNECTIONS * sizeof(HttpRequest*));
	for(unsigned int i = 0; i < NUM_CONNECTIONS; i++) {
		// Fetch a URL from redis
		redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

		// Split the URL info it's parts; no base URL
		URL* url = new URL(reply->str);
		url->Parse(NULL);

		// Check if we already have a URL on this domain (note: the domain has already been converted to lowercase as part of the split)
		bool exists = false;
		for(unsigned int j = 0; j < i; j++) {
			if(strcmp(requests[i]->GetURL()->parts[URL_DOMAIN], url->parts[URL_DOMAIN]))
				exists = true;
		}

		// If this does exist, clean up a few things and re-iterate
		if(exists) {
			redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

			delete url;
			i--;
			continue;
		}

		// Otherwise, we're good
		requests[i] = new HttpRequest(url);

		// Get the MD5 hash of the path
                unsigned char hash[MD5_DIGEST_LENGTH];
                memset(hash, 0, MD5_DIGEST_LENGTH);
                MD5((const unsigned char*)url->parts[URL_PATH], strlen(url->parts[URL_PATH]), hash);

                // Convert it to hex
                char* path_hash = (char*)malloc((MD5_DIGEST_LENGTH * 2) + 1);
                for(unsigned int i = 0; i < MD5_DIGEST_LENGTH; i++) {
                        sprintf(path_hash + (i * 2), "%02x", hash[i]);
                }
		path_hash[MD5_DIGEST_LENGTH * 2] = '\0';

		unsigned int length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2);
		char* filename = (char*)malloc(length + 1);
		sprintf(filename, "%s%s/%s.html", BASE_PATH, url->parts[URL_DOMAIN], path_hash);

		requests[i]->Open(filename);

		// Clean up
		delete url;
		freeReplyObject(reply);

		// Add it to the multi stack
		curl_multi_add_handle(multi, requests[i]->GetHandle());
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
			// Get the HttpRequest this was associated with
                        HttpRequest* request = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

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

				free(curl_error);
				free(final);
			}

			// Remove it from the stack
                        curl_multi_remove_handle(multi, msg->easy_handle);

			// Get the URL and parse it; no base path since this should be absolute
			URL* url = request->GetURL();

			// Clean up
			delete url;
                        delete request;

			// Fetch a new URL from redis
			while(true) {
				// Fetch a URL from redis
        	        	redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

	        	        // Split the URL info it's parts; no base URL
        	        	url = new URL(reply->str);
	                	url->Parse(NULL);

		                // Check if we already have a URL on this domain (note: the domain has already been converted to lowercase as part of the split)
        		        bool exists = false;
                		for(unsigned int i = 0; i < running; i++) {
                        		if(strcmp(requests[i]->GetURL()->parts[URL_DOMAIN], url->parts[URL_DOMAIN]))
                                		exists = true;
		                }

        		        // If this does exist, clean up a few things and re-iterate
                		if(exists) {
                        		redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

	                        	delete url;
        	        	        continue;
	        	        }

	                	HttpRequest* new_request = new HttpRequest(url);
	        	        freeReplyObject(reply);

				// Clean up
				delete url;

        	        	// Add it to the multi stack
	        	        curl_multi_add_handle(multi, new_request->GetHandle());
				break;
			}
		}

		sleep(1);
	}

	// Clean up
	curl_multi_cleanup(multi);
	redisFree(context);

	return(0);
}
