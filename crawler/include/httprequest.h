
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

class HttpRequest {
public:
	HttpRequest(URL* url);
	~HttpRequest();

	// Initialize this request (create the socket)
	int Initialize();

	// Run this request
	bool Start();

	// Read data from socket (returns number of bytes read)
	int Read();

	// Process what we have
	bool Process();

	// Getters
	int GetFD() { return m_socket; }
	URL* GetURL() { return m_url; }
	char* GetFilename() { return m_filename; }
	long int GetCode() { return m_code; }

protected:
	// The URL we're working on
	URL* m_url;

	// Get the filename
	char* m_filename;

	// The socket for communication
	int m_socket;

	// The respone we've gotten back
	char* m_content;
	unsigned int m_size;

	// The returned HTTP code
	long int m_code;

	// Is the transfer complete?
	bool m_complete;
};

#endif
