
/* INCLUDES */

#include <vector>
using namespace std;

#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <curl/curl.h>
#include <openssl/md5.h>

#include <url.h>
#include <httprequest.h>

/* DEFINITIONS */

#define NUM_CONNECTIONS	10
#define BASE_PATH	"/var/indexer/"

/* GLOBALS */

FILE* log_file = NULL;

/* FUNCTIONS */

void log(char* line) {
	char* newline = "\n";
	time_t timestamp = time(NULL);
	fprintf(log_file, "[%ld]: ", timestamp);

	fwrite(line, strlen(line), 1, log_file);
	fwrite(newline, 1, 1, log_file);

	printf("%s\n", line);

	fflush(log_file);
}

int main(int argc, char** argv) {
	// Open a connection to our log file
	if(!(log_file = fopen("/var/log/crawler.log", "w+"))) {
		printf("Unable to open log file.");
		return(0);
	}

	log("Log file opened.");

	// Hard code our URL to fetch
	char* fetch = "http://www.icedteapowered.com/";

	URL* url = new URL(fetch);
	url->Parse(NULL);

	log("URL parsed.");

	// Do global cURL initialization
	curl_global_init(CURL_GLOBAL_DEFAULT);

	log("cURL initialized.");

	HttpRequest* request = new HttpRequest(url);

	CURLM* multi = curl_multi_init();

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

	unsigned int dir_length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]);
	char* dir = (char*)malloc(dir_length + 1);
	sprintf(dir, "%s%s", BASE_PATH, url->parts[URL_DOMAIN]);
	dir[dir_length] = '\0';

	mkdir(dir, 0644);

	unsigned int length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
	char* filename = (char*)malloc(length + 1);
	sprintf(filename, "%s%s/%s.html", BASE_PATH, url->parts[URL_DOMAIN], path_hash);
	filename[length] = '\0';

	request->Open(filename);

	// Clean up
	delete url;

	// Add it to the multi stack
	curl_multi_add_handle(multi, request->GetHandle());

	while(true) {
		// Main loop, start any transfers that need to be started
		int running = 0;
		CURLMcode code = curl_multi_perform(multi, &running);
		while((code = curl_multi_perform(multi, &running)))
			code = curl_multi_perform(multi, &running);

		int msgs = 0;
		bool done = false;
		CURLMsg* msg = curl_multi_info_read(multi, &msgs);
		while(msg != NULL) {
			// Get the HttpRequest this was associated with
                        HttpRequest* request = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &request);

			curl_multi_remove_handle(multi, msg->easy_handle);

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

			CURLMsg* msg = curl_multi_info_read(multi, &msgs);

			// Clean up
			if(request)
	                        delete request;
			done = true;
		}

		if(done) break;

		sleep(1);
	}

	// Clean up
	curl_multi_cleanup(multi);

	return(0);
}
