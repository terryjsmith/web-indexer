
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

enum {
	HTTPREQUESTSTATE_NEW = 0,
	HTTPREQUESTSTATE_DNS,
	HTTPREQUESTSTATE_CONNECT,
	HTTPREQUESTSTATE_CONNECTING,
	HTTPREQUESTSTATE_SEND,
	HTTPREQUESTSTATE_RECV,
	HTTPREQUESTSTATE_WRITE,
	HTTPREQUESTSTATE_COMPLETE,
	HTTPREQUESTSTATE_ROBOTS,
	HTTPREQUESTSTATE_ERROR
};

enum {
	HTTPTIMEOUT_CONNECT = 15,
	HTTPTIMEOUT_RECV = 60,
	HTTPTIMEOUT_ANY = 90,
};

class HttpRequest {
public:
	HttpRequest();
	~HttpRequest();

	// Initialize our request and kick things off
	int initialize(Url* url);

	// Start the connection once we have DNS back
	void connect(struct hostent* host);

	// The generic handler function to be called from the main loop; returns false on error
	bool process(void* arg);

	// Force out request to error out
	void error(char* error);

	 // Set the output file for this (if applicable)
	void set_output_filename(char* filename);

	// Set a flag to fetch the robots.txt file first and return the content
	void fetch_robots(Url* url);

	// Resend a request, possibly once robots.txt are done
	int resend();

	// Getter functions
	int    get_socket() { return m_socket; }
	char*  get_content() { return m_content; }
	char*  get_filename() { return m_filename; }
	char*  get_error() { return m_error; }
	int    get_code() { return (int)m_code; }
	char*  get_effective_url() { return m_effective; }
	int    get_state() { return m_state; }
	Url*   get_url() { return m_url; }
	void*  get_sockaddr() { return &m_sockaddr; }
	time_t get_last_time() { return m_lasttime; }
	time_t get_last_check() { return m_lastcheck; }

	// Our static DNS lookup functions
	static void _dns_lookup(void *arg, int status, int timeouts, hostent* host);

protected:
	// A pointer to our internal URL
	Url* m_url;

	// An effective final URL if there was supposed to be a redirect
	char* m_effective;

	// The actual HTML content we've received so far
	char* m_content;

	// The content of the HTTP header
	char* m_header;
	bool m_inheader;

	// The size of the HTML content
	int m_size;
	int m_contentlength;

	// The string filename we're outputting to
	char* m_filename;

	// An open file handle if applicable
	FILE* m_fp;

	// The state of this request from the enum at the top of this file
	short m_state;

	// Our internal socket
	int m_socket;

	// A string description of an error that occured
	char* m_error;

	// The DNS channel we use to do look ups through c-ares
	ares_channel m_channel;

	// Our HTTP code
	long int m_code;

	// Our robots.txt URL if we need to fetch it
	Url* m_robots;

	// A saved cop of the server info so we can make multiple requests
	sockaddr_in m_sockaddr;

	// Whether keep-alive connections have been enabled for multiple requests
	bool m_keepalive;

	// The last timestamp we did something
	time_t m_lasttime;

	// The last timestamp we checked for data
	time_t m_lastcheck;
};

#endif
