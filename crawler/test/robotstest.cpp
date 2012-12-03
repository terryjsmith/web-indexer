
/* INCLUDES */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>

#include <url.h>
#include <httprequest.h>
#include <robotstxt.h>
#include <site.h>

int main(int argc, char** argv) {
	// Connect to MySQL
        MYSQL* conn = mysql_init(NULL);
        if(mysql_real_connect(conn, "localhost", "crawler", "SpasWehabEp4", NULL, 0, NULL, 0) == NULL) {
                printf("Unable to connect to MySQL.\n");
                return(0);
        }
        printf("Connected to MySQL.\n");

	mysql_select_db(conn, "crawler");

	URL* url = new URL("http://www.icedteapowered.com/");
	url->Parse(NULL);

	Site* site = new Site();
	site->Load(url->parts[URL_DOMAIN], conn);
	url->domain_id = site->domain_id;

	RobotsTxt* robots = new RobotsTxt();
	robots->Load(url, conn);

	URL* test = new URL("http://www.icedteapowered.com/");
	test->Parse(NULL);

	printf("Testing %s... ", test->url);
	if(robots->Check(test))
		printf("FAIL\n");
	else
		printf("PASS\n");
	delete test;

	test = new URL("http://www.icedteapowered.com/wp-admin/index.php");
	test->Parse(NULL);

        printf("Testing %s... ", test->url);
        if(robots->Check(test))
                printf("FAIL\n");
        else
                printf("PASS\n");
        delete test;

	test = new URL("http://www.icedteapowered.com/wp-admin/");
	test->Parse(NULL);

        printf("Testing %s... ", test->url);
        if(robots->Check(test))
                printf("FAIL\n");
        else
                printf("PASS\n");
        delete test;

	test = new URL("http://www.icedteapowered.com/a-test-post");
	test->Parse(NULL);

        printf("Testing %s... ", test->url);
        if(robots->Check(test))
                printf("FAIL\n");
        else
                printf("PASS\n");
        delete test;

	test = new URL("http://www.icedteapowered.com/WP-ADMIN/admin-ajax.php");
	test->Parse(NULL);

        printf("Testing %s... ", test->url);
        if(robots->Check(test))
                printf("FAIL\n");
        else
                printf("PASS\n");
        delete test;

	return(0);
}
