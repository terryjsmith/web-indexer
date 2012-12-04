
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
	// Multiple connections to MySQL requires this first
	mysql_library_init(0, 0, 0);

	// Initialize a pool of workers
        Worker** workers = (Worker**)malloc(sizeof(Worker*) * NUM_THREADS);

        // Initialize them
        for(unsigned int i = 0; i < NUM_THREADS; i++) {
                workers[i] = new Worker;
                workers[i]->Start(i);
        }

        // Enter the main loop
        while(true) {
                usleep(1);
        }

	return(0);
}
