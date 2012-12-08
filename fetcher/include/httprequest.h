
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

enum {
	HTTPREQUESTSTATE_NEW = 0,
	HTTPREQUESTSTATE_DNS,
	HTTPREQUESTSTATE_CONNECT,
	HTTPREQUESTSTATE_SEND,
	HTTPREQUESTSTATE_RECV,
	HTTPREQUESTSTATE_WRITE,
	HTTPREQUESTSTATE_COMPLETE
};

class HttpRequest {
public:
	HttpRequest(URL* url);
	~HttpRequest();

	// The generic handler function to be called from the main loop
	void process();

	 // Set the output file for this (if applicable)
	void set_output_filename(char* filename);

	// Getter functions
	int get_socket() { return m_socket; }
	char* get_content() { return m_content; }
	char* get_filename() { return m_filename; }

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
};

#endif
