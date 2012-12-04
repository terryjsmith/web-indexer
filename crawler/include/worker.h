
#ifndef __WORKER_H__
#define __WORKER_H__

class Worker {
public:
	Worker();
	~Worker();

	// Start the thread
	void Start(int pos);

	// Run
	void Run();

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
};

#endif
