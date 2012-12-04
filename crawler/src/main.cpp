
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

#include <defines.h>
#include <url.h>
#include <httprequest.h>
#include <robotstxt.h>
#include <site.h>
#include <worker.h>

/* FUNCTIONS */

int main(int argc, char** argv) {
	// Global CURL init
        curl_global_init(CURL_GLOBAL_ALL);

	// Initialize a pool of workers
        Worker** workers = (Worker**)malloc(sizeof(Worker*) * NUM_THREADS);

        // Initialize them
        for(unsigned int i = 0; i < NUM_THREADS; i++) {
                workers[i] = new Worker;
                workers[i]->Start();
        }

        // Enter the main loop
        while(true) {
                usleep(1);
        }

	return(0);
}
