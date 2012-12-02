
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

class HttpRequest {
public:
	HttpRequest(char* url);
	~HttpRequest();

	// Get our handle
	CURL* GetHandle() { return m_curl; }

	// cURL write function
	static size_t _write_function(char *ptr, size_t size, size_t nmemb, void *userdata);

	// Getters
	char* GetUrl() { return m_url; }
	char* GetContent() { return m_content; }

protected:
	// The URL we'll be fetching from
	char* m_url;

	// The response
	char* m_content;

	// cURL stuff: internal handle and return HTTP code
	CURL* m_curl;
	CURLcode m_code;
};

#endif
