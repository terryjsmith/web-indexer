
#ifndef __WORKER_H__
#define __WORKER_H__

class Worker {
public:
	Worker();
	~Worker();

	// Start the thread
	void start(int pos);

	// Run
	void run();

	// Fill our list of URLs with ones we can use and go fetch (run all checks and stuff)
	void fill_list();

	// Get the domain ID for a string domain
	domain* load_domain_info(char* domain);

	// Check if a URL already exists in our database
	bool url_exists(Url* url, domain* info);

	// Check the robots rules in the database
	bool check_robots_rules(Url* url);

protected:
	// Our internal thread processing function
	static void* _thread_function(void* ptr);

protected:
	// Our connections to our databases
	MYSQL* m_conn;
	redisContext* m_context;

	// Out thread
	int m_threadid;
	pthread_t m_thread;

	// Our list of requests we are currently processing
	HttpRequest** m_requests;

	// Our instance of epoll
	int m_epoll;
};

#endif
