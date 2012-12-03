
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

int main(int argc, char** argv) {
	URL* url = new URL("http://www.icedteapowered.com/");
	url->Parse(NULL);

	RobotsTxt* robots = new RobotsTxt();
	robots->Load(url);
}
