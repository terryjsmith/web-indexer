
/* INCLUDES */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <regex.h>
#include <openssl/md5.h>
#include <my_global.h>
#include <mysql.h>
#include <ares.h>

#include <url.h>
#include <httprequest.h>

int main(int argc, char** argv) {
	// Connect to MySQL
        MYSQL* conn = mysql_init(NULL);
        if(mysql_real_connect(conn, "localhost", "crawler", "SpasWehabEp4", "crawler", 0, NULL, 0) == NULL) {
                printf("Unable to connect to MySQL.\n");
                return(0);
        }
        printf("Connected to MySQL.\n");

	Url* url = new Url("http://www.icedteapowered.com/");
	url->parse(NULL);

	printf("Scheme: %s\nHost: %s\nPath: %s\nQuery: %s\n", url->get_scheme(), url->get_host(), url->get_path(), url->get_query());

	/*Site* site = new Site();
	site->Load(url->parts[URL_DOMAIN], conn);
	url->domain_id = site->domain_id;

	site->SetLastAccess(time(NULL));

	printf("Loading robots.txt...\n");

	RobotsTxt* robots = new RobotsTxt();
	robots->Load(url, conn);

	printf("done.\n");

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

	delete robots;
	delete site;*/
	delete url;

	mysql_close(conn);

	return(0);
}
