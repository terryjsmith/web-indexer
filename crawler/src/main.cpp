
/* INCLUDES */

#include <vector>
using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <regex.h>
#include <pthread.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>

#include <url.h>
#include <httprequest.h>

/* DEFINITIONS */

#define NUM_CONNECTIONS	10

/* GLOBALS */

redisContext *context = NULL;
FILE* log_file = NULL;

/* FUNCTIONS */

void log(char* line) {
	char* newline = "\n";
	time_t timestamp = time(NULL);
	fprintf(log_file, "[%lld]: ", timestamp);

	fwrite(line, strlen(line), 1, log_file);
	fwrite(newline, 1, 1, logfile);
}

int main(int argc, char** argv) {
	// Open a connection to our log file
	if(!log_file = fopen("/var/log/crawler.log", "w+")) {
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
		requests[i] = new HttpRequest(reply->str);
		freeReplyObject(reply);

		// Add it to the multi stack
		curl_multi_add_handle(multi, requests[i]->GetHandle());
	}

	bool new_transfers = true;
	while(true) {
		// Main loop, start any transfers that need to be started
		unsigned int running = 0;
		CURLMcode code = curl_multi_perform(multi, &running);
		while(code = curl_multi_perform(multi, &running))
			code = curl_multi_perform(multi, &running);

		unsigned int msgs = 0;
		CURLmsg msg = curl_multi_info_read(multi, &msgs);
		while(msg != NULL) {
			// This means that something finished; either with an error or with success
			if(msg->result != CURLE_OK) {
				// Get the HttpRequest this was associated with
				HttpRequest* request = 0;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

				// Get the error and URL to be logged
				char* curl_error = curl_easy_strerror(msg->result);
				char* url =  request->GetUrl();

				unsigned int length = strlen(url) + 2 + strlen(curl_error);
				char* final = (char*)malloc(length + 1);
				sprintf(final, "%s: %s", url, curl_error);
				final[length] = '\0';

				log(final);

				delete request;
				free(curl_error);
				free(final);
			}

			// Handle a fetched page

			// Remove it from the stack
			curl_multi_remove_handle(multi, msg->easy_handle);

			// Grab a new one to add on the stack
                	redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");
	                request = new HttpRequest(reply->str);
        	        freeReplyObject(reply);

                	// Add it to the multi stack
	                curl_multi_add_handle(multi, request->GetHandle());
		}

		sleep(1);
	}

	// Clean up
	curl_multi_cleanup(multi);
	redisFree(context);

	return(0);
}
