
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

class HttpRequest {
public:
	HttpRequest(URL* url);
	~HttpRequest();

	// Set the output file
	FILE* Open(char* filename);

	// Getters
	CURL* GetHandle() { return m_curl; }
	URL* GetURL() { return m_url; }

protected:
	// The file we're going to write out to
	FILE* m_fp;

	// The URL we're working on
	URL* m_url;

	// cURL stuff: internal handle and return HTTP code
	CURL* m_curl;
};

#endif
