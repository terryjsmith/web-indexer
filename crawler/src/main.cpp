
/* INCLUDES */

#include <vector>
using namespace std;

#include <regex.h>
#include <pthread.h>
#include <curl/curl.h>
#include <hiredis/hiredis.h>

#include <url.h>
#include <page.h>
#include <parser.h>

/* DEFINITIONS */

#define NUM_THREADS	1

/* GLOBALS */

redisContext *c = NULL;

int main(int argc, char** argv) {
	// Get a connection to redis to grab URLs
	c = redisConnect("localhost", 6379);
	if(c->err) {
		printf("Unable to connect to redis: %s", c->errstr);
		return(0);
	}

	

	// Set up to start doing transfers

	return(0);
}
