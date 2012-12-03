
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

RobotsTxt::RobotsTxt() {
	m_rules = 0;
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

        bool robots_valid = true;
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
                CURL* curl = req->GetHandle();

                // Set the filename to output to
                char* tmpname = tmpnam(robots_url->hash);
                FILE* fp = req->Open(tmpname);

                if(curl_easy_perform(curl) == 0) {
                        // Do a complete refresh of the rules we have
                        query = (char*)malloc(1000);
                        length = sprintf(query, "DELETE FROM robotstxt WHERE domain_id = %ld", url->domain_id);
                        query[length] = '\0';
                        mysql_query(conn, query);
                        free(query);

                        // We successfully got a robots.txt file back, parse it
                        fseek(fp, 0, SEEK_SET);

                        bool applicable = false;
                        while(!feof(fp)) {
                                char* line = (char*)malloc(1000);
                                char* read = fgets(line, 1000, fp);
				if(read == NULL) break;

                                // Check to see if this is a user agent line, start by making it all lowercase
                                for(unsigned int i = 0; i < strlen(line); i++) {
                                        line[i] = tolower(line[i]);
                                }

                                // If it is a user-agent line, make sure it's aimed at us
                                if(strstr(line, "user-agent") != NULL) {
                                        if(strstr(line, "user-agent: *")) {
						printf("ua\n");
                                                applicable = true;
					}
                                        else
                                                applicable = false;
                                }

                                if(applicable) {
                                        // Record the rule in the database
                                        char* part = strstr(line, "disallow: ");
                                        if(part) {
                                                unsigned int copy_length = (strchr(line, '\n') - line) - 10;

                                                // If copy length is 1, it's just a newline, "Disallow: " means you can index everything
                                                if(copy_length == 1) continue;

                                                char* disallowed = (char*)malloc(copy_length + 1);
                                                strncpy(disallowed, line + 10, copy_length);
                                                disallowed[copy_length] = '\0';

                                                query = (char*)malloc(1000);
                                                length = sprintf(query, "INSERT INTO robotstxt VALUES(%ld, '%s');", url->domain_id, disallowed);
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
                        return(true);
                }
        }

	return(false);
}
