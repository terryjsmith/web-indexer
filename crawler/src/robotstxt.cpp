
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>

#include <url.h>
#include <httprequest.h>
#include <robotstxt.h>

RobotsTxt::RobotsTxt() {
	m_rules = 0;
	m_count = 0;
}

RobotsTxt::~RobotsTxt() {
	if(m_rules) {
		for(unsigned int i = 0; i < m_count; i++) {
			free(m_rules[i]);
			m_rules[i] = 0;
		}

		free(m_rules);
	}
}

void RobotsTxt::Load(URL* url, MYSQL* conn) {
	// First check whether we have a robots.txt record
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT robots_last_access FROM domain WHERE domain_id = %ld", url->domain_id);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

        bool robots_valid = false;
        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                long int last_access = atol(row[0]);
                if((time(NULL) - last_access) > (60*60*24*7)) {
                        robots_valid = false;
                }
        }
	else {
		robots_valid  = false;
	}

        free(query);
        mysql_free_result(result);

        if(!robots_valid) {
                // We need to fetch the robots.txt file and parse it, get the path to the robots.txt file
                char* robots_path = "/robots.txt";
                URL* robots_url = new URL(robots_path);
                robots_url->Parse(url);

                HttpRequest* req = new HttpRequest(robots_url);
		printf("init\n");
		if(!req->Initialize())
			printf("fail\n");
		printf("start\n");
		req->Start();
		printf("read\n");
		while(req->Read() != 0) ;
		printf("process\n");
		req->Process();

		int code = req->GetCode();

		// Get the output filename
		char* tmpname = (char*)malloc(strlen(req->GetFilename()) + 1);
		strcpy(tmpname, req->GetFilename());

		delete req;

		if(true) {
			if(code == 200)  {
	                        // Do a complete refresh of the rules we have
        	                query = (char*)malloc(1000);
                	        length = sprintf(query, "DELETE FROM robotstxt WHERE domain_id = %ld", url->domain_id);
                        	query[length] = '\0';
	                        mysql_query(conn, query);
        	                free(query);

				FILE* fp = fopen(tmpname, "r");
				if(!fp) {
					printf("Unable to open robots.txt file %s for URL %s.\n", tmpname, url->url);
				}

				if(fseek(fp, 0, SEEK_SET)) {
					printf("Unable to seek in robots.txt file %s for URL %s.\n", tmpname, url->url);
				}

                        	// We successfully got a robots.txt file back, parse it
	                        bool applicable = false;
				char* line = (char*)malloc(1000);
                	        while(fgets(line, 1000, fp) != NULL) {
                        	        // Check to see if this is a user agent line, start by making it all lowercase
                                	for(unsigned int i = 0; i < strlen(line); i++) {
                                        	line[i] = tolower(line[i]);
	                                }

        	                        // If it is a user-agent line, make sure it's aimed at us
                	                if(strstr(line, "user-agent") != NULL) {
                        	                if(strstr(line, "user-agent: *") != NULL) {
                                	                applicable = true;
						}
	                                        else
        	                                        applicable = false;
                	                }

                        	        if(applicable) {
                                	        // Record the rule in the database
                                        	char* part = strstr(line, "disallow: ");
	                                        if(part) {
							char* position = strchr(line, '\n');
        	                                        unsigned int copy_length = (position == NULL) ? strlen(line) : (position - line);
							copy_length -= 10;

                	                                // If copy length is 1, it's just a newline, "Disallow: " means you can index everything
                        	                        if(copy_length <= 1) {
								free(line);
								line = (char*)malloc(1000);
								continue;
							}

	                                                char* disallowed = (char*)malloc(copy_length + 1);
        	                                        strncpy(disallowed, line + 10, copy_length);
                	                                disallowed[copy_length] = '\0';

	                                                query = (char*)malloc(5000);
        	                                        length = sprintf(query, "INSERT INTO robotstxt VALUES(%ld, '%s');", url->domain_id, disallowed);
                	                                query[length] = '\0';
                        	                        mysql_query(conn, query);

                                	                free(disallowed);
                                        	        free(query);
	                                        }
        	                        }

                	                free(line);
					line = (char*)malloc(1000);

                        	}

				free(line);
				fclose(fp);
			}

			// Update that we checked it
                	query = (char*)malloc(1000);
        	        time_t last_access = time(NULL);
	               	length = sprintf(query, "UPDATE domain SET robots_last_access = %ld WHERE domain_id = %ld", last_access, url->domain_id);
	               	query[length] = '\0';

        	        mysql_query(conn, query);
	                free(query);
                }
		
                delete robots_url;
        }

	// Okay, once we're here, we can load the rules and compare the URL
        query = (char*)malloc(1000);
        length = sprintf(query, "SELECT rule FROM robotstxt WHERE domain_id = %ld", url->domain_id);
        query[length] = '\0';

        mysql_query(conn, query);
        result = mysql_store_result(conn);

	if(mysql_num_rows(result)) {
		m_rules = (char**)malloc(mysql_num_rows(result) * sizeof(char*));
		m_count = mysql_num_rows(result);

	        for(unsigned int i = 0; i < mysql_num_rows(result); i++) {
        	        MYSQL_ROW row = mysql_fetch_row(result);
			m_rules[i] = (char*)malloc(strlen(row[0]) + 1);
			strcpy(m_rules[i], row[0]);
                }
        }

        free(query);
        mysql_free_result(result);
}

bool RobotsTxt::Check(URL* url) {
	// Make a lowercase copy of the URL to use for comparison
        char* lowercase = (char*)malloc(strlen(url->parts[URL_PATH]) + 1);
        strcpy(lowercase, url->parts[URL_PATH]);

        for(unsigned int i = 0; i < strlen(url->parts[URL_PATH]); i++)
                lowercase[i] = tolower(lowercase[i]);

        for(unsigned int i = 0; i < m_count; i++) {
                if(strncmp(lowercase, m_rules[i], strlen(m_rules[i])) == 0) {
			free(lowercase);
                        return(true);
                }
        }

	free(lowercase);
	return(false);
}
