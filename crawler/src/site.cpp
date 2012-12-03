
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <my_global.h>
#include <mysql.h>
#include <site.h>

void Site::Load(char* domain, MYSQL* conn) {
	// Save our MySQL connection
	m_conn = conn;

        // See if we have an existing domain
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT domain_id FROM domain WHERE domain = '%s'", domain);
        query[length] = '\0';

        mysql_query(conn, query);
        MYSQL_RES* result = mysql_store_result(conn);

        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                domain_id = atol(row[0]);
        }

        free(query);
        mysql_free_result(result);

        // If we don't have a domain, create one
        if(!domain_id) {
                query = (char*)malloc(1000);
                length = sprintf(query, "INSERT INTO domain VALUES(NULL, '%s', 0, 0)", domain);
                query[length] = '\0';

                mysql_query(conn, query);
                free(query);

                domain_id = (unsigned long int)mysql_insert_id(conn);
        }
}

unsigned long int Site::GetLastAccess() {
	unsigned long int retval = 0;

	// See if we have an existing domain
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "SELECT last_access FROM domain WHERE domain_id = %ld", domain_id);
        query[length] = '\0';

        mysql_query(m_conn, query);
        MYSQL_RES* result = mysql_store_result(m_conn);

        if(mysql_num_rows(result)) {
                MYSQL_ROW row = mysql_fetch_row(result);
                retval = atol(row[0]);
        }

        free(query);
        mysql_free_result(result);

	return(retval);
}

void Site::SetLastAccess(unsigned long int timestamp) {
	// See if we have an existing domain
        char* query = (char*)malloc(1000);
        unsigned int length = sprintf(query, "UPDATE domain SET last_access = %ld WHERE domain_id = %ld", timestamp, domain_id);
        query[length] = '\0';

        mysql_query(m_conn, query);
        free(query);
}
