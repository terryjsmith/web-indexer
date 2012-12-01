
#ifndef __PARSER_H__
#define __PARSER_H__

class Parser {
public:
	Parser();
	~Parser();

	// Start the parsing thread
	void Start();

	// Add a page to the list
	void AddPage(Page* page);

protected:
	// The list of pages to be parsed
	std::list<Page*> m_pages;

	// The thread and mutex for accessing pages
	pthread_t m_thread;
	pthread_mutex_t m_mutex;
};

#endif
