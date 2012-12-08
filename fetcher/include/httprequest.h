
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

enum {
	HTTPREQUESTSTATE_NEW = 0,
	HTTPREQUESTSTATE_DNS,
	HTTPREQUESTSTATE_CONNECT,
	HTTPREQUESTSTATE_SEND,
	HTTPREQUESTSTATE_RECV,
	HTTPREQUESTSTATE_WRITE,
	HTTPREQUESTSTATE_COMPLETE,
	HTTPREQUESTSTATE_ERROR
};

class HttpRequest {
public:
	HttpRequest(URL* url);
	~HttpRequest();

	// Initialize our request and kick things off
	bool initialize();

	// The generic handler function to be called from the main loop; returns false on error
	bool process(void* arg);

	// Force out request to error out
	void error(char* error);

	 // Set the output file for this (if applicable)
	void set_output_filename(char* filename);

	// Getter functions
	int   get_socket() { return m_socket; }
	char* get_content() { return m_content; }
	char* get_filename() { return m_filename; }
	char* get_error() { return m_error; }

	// Our static DNS lookup functions
	static void _dns_lookup(void *arg, int status, int timeouts, struct hostent *hostent);

protected:
	// A pointer to our internal URL
	URL* m_url;

	// The actual HTML content we've received so far
	char* m_content;

	// The file handle we are writing out to (if applicable)
	FILE* m_fp;

	// The string filename we're outputting to
	char* m_filename;

	// The state of this request from the enum at the top of this file
	short m_state;

	// Our internal socket
	int m_socket;

	// A string description of an error that occured
	char* m_error;

	// The DNS channel we use to do look ups through c-ares
	ares_channel m_channel;
};

#endif
