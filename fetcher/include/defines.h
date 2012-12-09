
#ifndef __DEFINES_H__
#define __DEFINES_H__

#define NUM_THREADS			1
#define CONNECTIONS_PER_THREAD		5
#define SOCKET_BUFFER_SIZE		2048

#define BASE_PATH       		"/mnt/indexer/"

#define MIN_ACCESS_TIME 		10
#define ROBOTS_MIN_ACCESS_TIME		604800 			// One week

#define	MYSQL_HOST			"localhost"
#define MYSQL_USER			"crawler"
#define MYSQL_PASS			"SpasWehabEp4"
#define MYSQL_DB			"crawler"

#define REDIS_HOST			"localhost"
#define REDIS_PORT			6379

struct domain {
	int domain_id;
	char* domain;
	unsigned long int last_access;
	unsigned long int robots_last_access;
};

#endif
