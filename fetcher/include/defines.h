
#ifndef __DEFINES_H__
#define __DEFINES_H__

#define NUM_THREADS			2
#define CONNECTIONS_PER_THREAD		20
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

#endif
