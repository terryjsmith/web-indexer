
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <regex.h>
#include <openssl/md5.h>
#include <hiredis/hiredis.h>
#include <my_global.h>
#include <mysql.h>

#include <defines.h>
#include <url.h>
#include <htmlcxx/html/ParserDom.h>

using namespace std;
using namespace htmlcxx;
 
int main(int argc, char** argv) {
	// Get a connection to redis to grab URLs
        redisContext* context = redisConnect(REDIS_HOST, REDIS_PORT);
        if(context->err) {
		printf("Unable to connect to redis.\n");
		return(0);
        }

	// Also connect to MySQL
        MYSQL* conn = mysql_init(NULL);
        if(!mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, NULL, 0)) {
                printf("Unable to connect to MySQL: %s\n", mysql_error(conn));
                return(0);
        }

        mysql_set_character_set(conn, "utf8");

	while(true) {
		// Fetch a document to parse from redis
                redisReply* reply = (redisReply*)redisCommand(context, "LPOP parse_queue");
                if(reply->type == REDIS_REPLY_ERROR) {
                        printf("REDIS ERROR: %s\n", reply->str);
                        return(0);
                }

                // Make a copy of the filename, removing surrounding quotes where necessary
                char* filename = 0;
                if(reply->str[0] == '"') {
                        int length = strlen(reply->str) - 2;
                        filename = (char*)malloc(length + 1);
                        strncpy(filename, reply->str + 1, length);
                        filename[length] = '\0';
                }
                else {
                        int length = strlen(reply->str);
                        filename = (char*)malloc(length + 1);
                        strcpy(filename, reply->str);
                }
                freeReplyObject(reply);

		printf("Fetching URLs from %s.\n", filename);

		// Open the file and get it's contents
		FILE* fp = fopen(filename, "r");
		if(!fp) {
			printf("Unable to open file: %s\n", filename);
			free(filename);
			continue;
		}

		// Find out how big the file is and then read it in
		fseek(fp, 0, SEEK_END);
		long int filesize = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		if(filesize <= 0) {
			printf("Invalid file size %d: %s\n", filesize, filename);
			fclose(fp);
			free(filename);
			continue;
		}

		// Read in the entire file
		char* content = (char*)malloc(filesize + 1);
		unsigned int read = fread(content, 1, filesize, fp);
		if(read != filesize) {
			printf("Unable to read filesize %d from file %s, got %d instead.\n", filesize, filename, read);
			fclose(fp);
			free(content);
			free(filename);
                        continue;
		}
		content[filesize] = '\0';

		// Close the file
		fclose(fp);

		// Cut out the header
		char* header_end = strstr(content, "\r\n\r\n");
		int header_length = (header_end - content) + 4;

		printf("Got a header length of %d\n", header_length);

		// Do we even have any HTML after this?
		if((filesize - header_length) <= 0) {
			printf("Nothing to read after header in file %s\n", filename);
			free(content);
                        free(filename);
                        continue;
		}

		// Now we can crop out the header
		char* new_content = (char*)malloc(filesize - header_length + 1);
		strcpy(new_content, content + header_length);

		free(content);
		content = new_content;

		// Finally, before we start, parse the domain and URL hash out of the file
		char* domain_start = strrchr(filename, '/') + 1;
		char* domain_end = strrchr(filename, '_');
		char* hash_start = domain_end + 1;
		char* hash_end = strrchr(filename, '.');

		char* domain = (char*)malloc((domain_end - domain_start) + 1);
		strncpy(domain, domain_start, domain_end - domain_start);
		domain[domain_end - domain_start] = '\0';

		char* hash = (char*)malloc((hash_end - hash_start) + 1);
		strncpy(hash, hash_start, hash_end - hash_start);
		hash[hash_end - hash_start] = '\0';

		// Look them up in the database, domain first
	        char* query = (char*)malloc(100 + strlen(domain));
        	sprintf(query, "SELECT domain_id FROM domain WHERE domain = '%s'", domain);

		mysql_query(conn, query);
	        MYSQL_RES* result = mysql_store_result(conn);
		MYSQL_ROW row = mysql_fetch_row(result);

	        free(query);

		// Save the domain ID
		unsigned int domain_id = atol(row[0]);

		printf("Got domain ID %d for domain %s.\n", domain_id, domain);

		mysql_free_result(result);

		// Then look up the URL
		query = (char*)malloc(100 + strlen(hash));
                sprintf(query, "SELECT url_id, url FROM url WHERE domain_id = %d AND url_hash = '%s'", domain_id, hash);

                mysql_query(conn, query);
                result = mysql_store_result(conn);
		row = mysql_fetch_row(result);

                free(query);

                // Save the URL ID
                unsigned int url_id = atol(row[0]);
		Url* url = new Url(row[1]);
		url->parse(NULL);

		printf("Got URL ID %d and full URL %s.\n", url_id, url->get_url());

                mysql_free_result(result);

		// Is the URL valid?
		if(!url->get_path()) {
			printf("Invalid URL format %s\n", url->get_url());
			delete url;
                        free(content);
                        free(filename);
                        continue;
		}

		// Is the URL valid?
                if(!strlen(url->get_path())) {
                        printf("Invalid URL format %s\n", url->get_url());
                        delete url;
                        free(content);
                        free(filename);
                        continue;
                }

		//Parse some html code
  		HTML::ParserDom parser;
	  	tree<HTML::Node> dom = parser.parseTree(content);

		printf("Got %d nodes.\n", dom.size());
  
  		//Dump all links in the tree
	  	tree<HTML::Node>::iterator it = dom.begin();
  		tree<HTML::Node>::iterator end = dom.end();
	  	for (; it != end; ++it) {
			if(it->tagName().size() < 1) continue;

			// Get a copy of the tag we can work with
			char* tag = (char*)malloc(it->tagName().size() + 1);
			strcpy(tag, it->tagName().c_str());

			// Make it lowercase
			int length = strlen(tag);
			for(int i = 0; i < length; i++) {
				tag[i] = tolower(tag[i]);
			}

  			if(strcmp(tag, "a") == 0) {
				free(tag);

				// Get the URL out
  				it->parseAttributes();
				char* found_url = (char*)it->attribute("href").second.c_str();

				// Parse it to get the full URL
				Url* new_url = new Url(found_url);
				new_url->parse(url);

				printf("Found URL %s parsed to %s\n", found_url, new_url->get_url());

				// See if we can find this URL in the database
				if(!new_url->get_scheme()) {
					printf("invalid scheme (0)\n");
                                        delete new_url;
                                        continue;
				}

				if(strlen(new_url->get_scheme()) < 4) {
					printf("invalid scheme (1)\n");
					delete new_url;
					continue;
				}

				// Check if we have a valid scheme
				if(strcmp(new_url->get_scheme(), "http") != 0) {
					printf("invalid scheme (2)\n");
					delete new_url;
					continue;
				}

				// Do we also have all of the other valid parts?
				if(!strlen(new_url->get_host()) || !strlen(new_url->get_path())) {
					printf("invalid domain or path.\n");
					delete new_url;
					continue;
				}

				// Try to find the domain in the database
				long int domain_id = 0;
				query = (char*)malloc(100 + strlen(hash));
		                sprintf(query, "SELECT domain_id FROM domain WHERE domain = '%s'", new_url->get_host());

		                mysql_query(conn, query);
				free(query);

	        	        result = mysql_store_result(conn);

				if(mysql_num_rows(result)) {
					MYSQL_ROW row = mysql_fetch_row(result);
					domain_id = atol(row[0]);
				}

		                mysql_free_result(result);

				long int url_id = 0;
				if(domain_id) {
					// See if the URL hash already exists
					query = (char*)malloc(100 + strlen(hash));
	                                sprintf(query, "SELECT url_id FROM url WHERE domain_id = %d AND url_hash = '%s'", domain_id, new_url->get_path_hash());

					mysql_query(conn, query);
	                                result = mysql_store_result(conn);
					free(query);

					if(mysql_num_rows(result)) {
						MYSQL_ROW row = mysql_fetch_row(result);
						url_id = atol(row[0]);
					}

					mysql_free_result(result);
				}

				if(url_id) {
					printf("exists in database\n");
                                        delete new_url;
                                        continue;
				}

				printf("Inserting URL %s into database.\n", new_url->get_url());

				reply = (redisReply*)redisCommand(context, "RPUSH url_queue \"%s\"", new_url->get_url());
	                        if(reply->type == REDIS_REPLY_ERROR) {
        	                        printf("REDIS ERROR: %s\n", reply->str);
                	                return(0);
                        	}
	                        freeReplyObject(reply);

				delete new_url;
	  		}
			else {
				free(tag);
			}

			usleep(1);
  		}

		delete url;

		free(hash);
		free(domain);
		free(content);
		free(filename);
		usleep(10000);
  	}
}
