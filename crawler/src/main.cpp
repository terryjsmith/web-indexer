
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
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>

#include <url.h>
#include <httprequest.h>

/* DEFINITIONS */

#define NUM_CONNECTIONS	10
#define BASE_PATH	"/mnt/indexer/"

/* GLOBALS */

FILE* log_file = NULL;
MYSQL* conn = NULL;
HttpRequest** requests = NULL;

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

int check_url(URL* url, char* path_hash) {
	// Check if we already have a URL on this domain (note: the domain has already been converted to lowercase as part of the split)
        for(unsigned int i = 0; i < NUM_CONNECTIONS; i++) {
		if(!requests[i]) continue;

                if(strcmp(requests[i]->GetURL()->parts[URL_DOMAIN], url->parts[URL_DOMAIN]))
	                return(1);
        }

	// Do a quick check in MySQL and make sure we haven't accessed this domain recently
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT domain_id, last_access FROM domain WHERE domain = '%s'", url->parts[URL_DOMAIN]);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

        bool row_exists = false;
	long int domain_id = 0;
        if(mysql_num_rows(result)) {
                row_exists = true;
                MYSQL_ROW row = mysql_fetch_row(result);
		domain_id = atol(row[0]);
                long int last_access = atol(row[1]);
                if(abs(time(NULL) - last_access) < (60*10)) {
                        return(1);
                }
        }

        free(query);
        mysql_free_result(result);

        // We know this is a valid URL, update the last access time
        long int last_access = time(NULL);
        query = (char*)malloc(1000);
        if(row_exists)
                length = sprintf(query, "UPDATE domain SET last_access = %ld WHERE domain = '%s'", last_access, url->parts[URL_DOMAIN]);
        else {
                unsigned int zero = 0;
                length = sprintf(query, "INSERT INTO domain VALUES(NULL, '%s', %ld, %d)", url->parts[URL_DOMAIN], last_access, zero);
        }
        query[length] = '\0';

        mysql_query(conn, query);
        free(query);

	// Have we already checked this page?
	if(row_exists) {
		query = (char*)malloc(1000);
		length = sprintf(query, "SELECT url_id FROM url WHERE domain_id = %ld AND path_hash = '%s'", domain_id, path_hash);
		query[length] = '\0';

		mysql_query(conn, query);
		free(query);

		MYSQL_RES* result = mysql_store_result(conn);
		if(mysql_num_rows(result)) {
			mysql_free_result(result);
			return(2);
		}

		mysql_free_result(result);
	}

	// We're good, insert it
	query = (char*)malloc(3000);
	length = sprintf(query, "INSERT INTO url VALUES(NULL, %ld, '%s', '%s', '', 0, 0)", domain_id, url->url, path_hash);
	mysql_query(conn, query);
	free(query);

	return(0);
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
	if(mysql_real_connect(conn, "localhost", "crawler", "8ruFrUthuj@Duch", NULL, 0, NULL, 0) == NULL) {
		log("Unable to connect to MySQL.\n");
		return(0);
	}
	log("Connected to MySQL.");

	// Do global cURL initialization
	curl_global_init(CURL_GLOBAL_DEFAULT);

	CURLM* multi = curl_multi_init();

	// Set up to start doing transfers
	requests = (HttpRequest**)malloc(NUM_CONNECTIONS * sizeof(HttpRequest*));
	memset(requests, 0, NUM_CONNECTIONS * sizeof(HttpRequest*));

	for(unsigned int i = 0; i < NUM_CONNECTIONS; i++) {
		// Fetch a URL from redis
		redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

		// Split the URL info it's parts; no base URL
		URL* url = new URL(reply->str);
		url->Parse(NULL);

		// Get the MD5 hash of the path
                unsigned char hash[MD5_DIGEST_LENGTH];
                memset(hash, 0, MD5_DIGEST_LENGTH);
                MD5((const unsigned char*)url->parts[URL_PATH], strlen(url->parts[URL_PATH]), hash);

                // Convert it to hex
                char* path_hash = (char*)malloc((MD5_DIGEST_LENGTH * 2) + 1);
                for(unsigned int k = 0; k < MD5_DIGEST_LENGTH; k++) {
                        sprintf(path_hash + (k * 2), "%02x", hash[k]);
                }
                path_hash[MD5_DIGEST_LENGTH * 2] = '\0';

		// If this does exist, clean up a few things and re-iterate
		int result = 0;
		if((result = check_url(url, path_hash))) {
			if(result == 1)
				redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

			delete url;
			freeReplyObject(reply);
			i--;
			continue;
		}

		// Otherwise, we're good
		requests[i] = new HttpRequest(url);

		unsigned int dir_length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]);
        	char* dir = (char*)malloc(dir_length + 1);
	        sprintf(dir, "%s%s", BASE_PATH, url->parts[URL_DOMAIN]);
        	dir[dir_length] = '\0';

	        mkdir(dir, 0644);

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
                        delete request;

			// Fetch a new URL from redis
			while(true) {
				// Fetch a URL from redis
        	        	redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

	        	        // Split the URL info it's parts; no base URL
        	        	url = new URL(reply->str);
	                	url->Parse(NULL);

				// Get the MD5 hash of the path
		                unsigned char hash[MD5_DIGEST_LENGTH];
                		memset(hash, 0, MD5_DIGEST_LENGTH);
		                MD5((const unsigned char*)url->parts[URL_PATH], strlen(url->parts[URL_PATH]), hash);

		                // Convert it to hex
                		char* path_hash = (char*)malloc((MD5_DIGEST_LENGTH * 2) + 1);
		                for(unsigned int k = 0; k < MD5_DIGEST_LENGTH; k++) {
                		        sprintf(path_hash + (k * 2), "%02x", hash[k]);
                		}
		                path_hash[MD5_DIGEST_LENGTH * 2] = '\0';

        		        // If this does exist, clean up a few things and re-iterate
				int result = 0;
                		if((result = check_url(url, path_hash))) {
					if(result == 1)
	                        		redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

	                        	delete url;
					freeReplyObject(reply);
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
