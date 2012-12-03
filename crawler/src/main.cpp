
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

bool fill_domain_url_id(URL* url) {
	// Our return value; false = URL existed, true = new URL
	bool retval = false;

	// See if we have an existing domain
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT domain_id FROM domain WHERE domain = '%s'", url->parts[URL_DOMAIN]);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                url->domain_id = atol(row[0]);
        }

        free(query);
        mysql_free_result(result);

	// If we don't have a domain, create one
	time_t last_access = time(NULL);
	if(!url->domain_id) {
        	query = (char*)malloc(1000);
                unsigned int zero = 0;
                length = sprintf(query, "INSERT INTO domain VALUES(NULL, '%s', %ld, %d)", url->parts[URL_DOMAIN], last_access, zero);
        	query[length] = '\0';

	        mysql_query(conn, query);
        	free(query);

		url->domain_id = (unsigned long int)mysql_insert_id(conn);
	}

	// Now that we have a domain ID, get the URL ID, if it exists
	query = (char*)malloc(1000);
        length = sprintf(query, "SELECT url_id FROM url WHERE domain_id = %ld AND path_hash = '%s'", url->domain_id, url->hash);
        query[length] = '\0';

        mysql_query(conn, query);
        free(query);

        result = mysql_store_result(conn);
        if(mysql_num_rows(result)) {
		MYSQL_ROW row = mysql_fetch_row(result);
		url->url_id = atol(row[0]);
        }

	mysql_free_result(result);

	// If we didn't have a URL, create one
	if(!url->url_id) {
        	query = (char*)malloc(5000);
	        length = sprintf(query, "INSERT INTO url VALUES(NULL, %ld, '%s', '%s', '', 0, 0)", url->domain_id, url->url, url->hash);
        	mysql_query(conn, query);
	        free(query);

        	url->url_id = (unsigned long int)mysql_insert_id(conn);
		retval = true;
	}

	return(retval);
}

void update_last_access(URL* url) {
	time_t timestamp = time(NULL);

	char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "UPDATE domain SET last_access = %ld WHERE domain_id = %ld", timestamp, url->domain_id);
        query[length] = '\0';

        mysql_query(conn, query);
        free(query);
}

bool check_last_access(URL* url) {
	// Our return value; false = invalid, true = valid
	bool retval = true;

	// Check if we already have a URL on this domain (note: the domain has already been converted to lowercase as part of the split)
        for(unsigned int i = 0; i < NUM_CONNECTIONS; i++) {
		if(!requests[i]) continue;

                if(strcmp(requests[i]->GetURL()->parts[URL_DOMAIN], url->parts[URL_DOMAIN]))
	                return(1);
        }

	// Do a quick check in MySQL and make sure we haven't accessed this domain recently
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT last_access FROM domain WHERE domain = '%s'", url->parts[URL_DOMAIN]);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                long int last_access = atol(row[0]);
                if(abs(time(NULL) - last_access) < (60*10)) {
                        retval = false;
                }
        }

        free(query);
        mysql_free_result(result);

	return(retval);
}

bool check_robots_txt(URL* url) {
	// Our return value; false = not found in robots.txt, true = found and should discard
	bool retval = false;

	// First check whether we have a robots.txt record
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT robots_last_access FROM domain WHERE domain_id = %ld", url->domain_id);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

	bool robots_valid = true;
	if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                long int last_access = atol(row[0]);
                if(abs(time(NULL) - last_access) < (60*60*24*7)) {
                        robots_valid = false;
                }
        }

        free(query);
        mysql_free_result(result);

	if(!robots_valid) {
		// We need to fetch the robots.txt file and parse it, get the path to the robots.txt file
		char* robots_path = "/robots.txt";
		URL* robots_url = new URL(robots_path);
		robots_url->Parse(url);

		HttpRequest* req = new HttpRequest(robots_url);
		CURL* curl = req->GetHandle();

		// Set the filename to output to
		char* tmpname = tmpnam(robots_url->hash);
		unsigned int length = strlen(tmpname);

		char* filename = (char*)malloc(length + 5 + 1);
		sprintf(filename, "/tmp/%s", tmpname);
		filename[length] = '\0';

		FILE* fp = req->Open(filename);

		if(curl_easy_perform(curl)) {
			// Do a complete refresh of the rules we have
			query = (char*)malloc(1000);
                	length = sprintf(query, "DELETE FROM robotstxt WHERE domain_id = %ld",url->domain_id);
	                query[length] = '\0';
			mysql_query(conn, query);
			free(query);

			// We successfully got a robots.txt file back, parse it
			fseek(fp, 0, SEEK_SET);

			// Read the file in one line at a time
			bool applicable = false;
			while(!feof(fp)) {
				char* line = (char*)malloc(1000);
				fgets(line, 1000, fp);

				// Check to see if this is a user agent line, start by making it all lowercase
				for(unsigned int i = 0; i < strlen(line); i++) {
                                	line[i] = tolower(line[i]);
                                }

				// If it is a user-agent line, make sure it's aimed at us
				if(strstr(line, "user-agent") != NULL) {
					if(strstr(line, "user-agent: *")) 
						applicable = true;
					else
						applicable = false;
				}

				if(applicable) {
					// Record the rule in the database
					char* part = NULL;
					if((part = strstr(line, "disallow: ")) != NULL) {
						unsigned int copy_length = strlen(line) - 10;

						// If copy length is 1, it's just a newline, "Disallow: " means you can index everything
						if(copy_length == 1) continue;

						char* disallowed = (char*)malloc(copy_length);
						strncpy(disallowed, line + 10, copy_length);
						disallowed[copy_length] = '\0';

						query = (char*)malloc(1000);
			                        length = sprintf(query, "INSERT INTO robotstxt(%ld, '%s');", url->domain_id, disallowed);
                        			query[length] = '\0';
			                        mysql_query(conn, query);

						free(disallowed);
                        			free(query);
					}
				}

				free(line);
			}
		}

		delete robots_url;
		delete req;

		// Update that we checked it
		query = (char*)malloc(1000);
		time_t last_access = time(NULL);
	        length = sprintf(query, "UPDATE domain SET robots_last_access = %ld WHERE domain_id = %ld", last_access, url->domain_id);
        	query[length] = '\0';

	        mysql_query(conn, query);
	}

	// Okay, once we're here, we can load the rules and compare the URL
	query = (char*)malloc(1000);
        length = sprintf(query, "SELECT rule FROM robotstxt WHERE domain_id = %ld", url->domain_id);
        query[length] = '\0';

        mysql_query(conn, query);
        result = mysql_store_result(conn);

	// Make a lowercase copy of the URL to use for comparison
	char* lowercase = (char*)malloc(strlen(url->parts[URL_PATH]) + 1);
	strcpy(lowercase, url->parts[URL_PATH]);

	for(unsigned int i = 0; i < strlen(url->parts[URL_PATH]); i++)
		lowercase[i] = tolower(lowercase[i]);

	for(unsigned int i = 0; i < mysql_num_rows(result); i++) {
                MYSQL_ROW row = mysql_fetch_row(result);
		if(strncmp(lowercase, row[0], strlen(row[0])) == 0) {
			retval = true;
		}
        }

	free(lowercase);
        free(query);
        mysql_free_result(result);

	return(retval);
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

		freeReplyObject(reply);

		// Check whether this domain is on a timeout
		if(check_last_access(url) == false) {
                        redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

                        delete url;
                        i--;
                        continue;
		}

		// Next check if we've already parsed this URL
		if(fill_domain_url_id(url) == false) {
			delete url;
			i--;
			continue;
		}

		// Finally, make sure the URL isn't disallowed by robots.txt
		if(check_robots_txt(url) == true) {
			delete url;
                        i--;
                        continue;
		}

		// Otherwise, we're good
		requests[i] = new HttpRequest(url);

		// Update the last access time
		update_last_access(url);

		unsigned int dir_length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]);
        	char* dir = (char*)malloc(dir_length + 1);
	        sprintf(dir, "%s%s", BASE_PATH, url->parts[URL_DOMAIN]);
        	dir[dir_length] = '\0';

	        mkdir(dir, 0644);

		unsigned int length = strlen(BASE_PATH) + strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2);
		char* filename = (char*)malloc(length + 1);
		sprintf(filename, "%s%s/%s.html", BASE_PATH, url->parts[URL_DOMAIN], url->hash);

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

			// Get the URL from the request
			URL* url = request->GetURL();

			// Find the file this page was written to
			unsigned int length = strlen(url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2);
	                char* filename = (char*)malloc(length + 1);
        	        sprintf(filename, "%s/%s.html", url->parts[URL_DOMAIN], url->hash);

			// Add it to the parse_queue in redis
			redisReply* reply = (redisReply*)redisCommand(context, "RPUSH url_queue \"%s\"", filename);
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

					// Find the new domain and path info
					fill_domain_url_id(new_url);

					// Make sure we mark that we actually checked this domain as well
					update_last_access(url);

					// Insert a record into the URL table
					char* query = (char*)malloc(1000);
		                        length = sprintf(query, "INSERT INTO redirect VALUES(%ld, %ld)", url->url_id, new_url->url_id);
                		        query[length] = '\0';
		                        mysql_query(conn, query);

                		        free(query);

					// Make sure we update the first URL to reflect the status of the last
					query = (char*)malloc(1000);
		                        length = sprintf(query, "UPDATE url SET last_code = %ld WHERE url_id = %ld", code, url->url_id);
                		        query[length] = '\0';
		                        mysql_query(conn, query);

                		        free(query);

					// Finally, delete the old URL and use the new one
					delete url;
					url = new_url;
				}
			}

                        char* query = (char*)malloc(1000);
                        length = sprintf(query, "UPDATE url SET last_code = %ld WHERE url_id = %ld", code, url->url_id);
                        query[length] = '\0';
                        mysql_query(conn, query);

                        free(query);

			// Clean up
                        delete request;

			// Fetch a new URL from redis
			while(true) {
				// Fetch a URL from redis
        	        	redisReply* reply = (redisReply*)redisCommand(context, "LPOP url_queue");

	        	        // Split the URL info it's parts; no base URL
        	        	url = new URL(reply->str);
	                	url->Parse(NULL);

				freeReplyObject(reply);

				// Check whether this domain is on a timeout
		                if(check_last_access(url) == false) {
                		        redisCommand(context, "RPUSH url_queue \"%s\"", url->url);

		                        delete url;
                		        continue;
		                }

                		// Next check if we've already parsed this URL
		                if(fill_domain_url_id(url) == false) {
                		        delete url;
		                        continue;
                		}

				// Finally, make sure the URL isn't disallowed by robots.txt
		                if(check_robots_txt(url) == true) {
                		        delete url;
                		        continue;
		                }

	                	HttpRequest* new_request = new HttpRequest(url);

				// Mark that we're accessing this
				update_last_access(url);

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
