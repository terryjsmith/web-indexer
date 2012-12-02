
/* INCLUDES */

#include <vector>
using namespace std;

#include <regex.h>
#include <pthread.h>
#include <curl/curl.h>

#include <url.h>
#include <page.h>
#include <parser.h>

/* DEFINITIONS */

#define NUM_THREADS	1

/* GLOBALS */

redisContext *c = NULL;

int main(int argc, char** argv) {
	// Do global cURL initialization
	curl_global_init(CURL_GLOBAL_DEFAULT);

	// Set up to start doing transfers
	CURL* curl = curl_init();
	curl_easy_setopt(curl, CURLOPT_URL, "http://www.icedteapowered.com/");
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Open Web Index Crawler (+http://www.icedteapowerd.com/openweb/)");
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60);
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv2);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_function);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_PRIVATE, this);
        curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);

	return(0);
}
