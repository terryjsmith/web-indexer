
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

class HttpRequest {
public:
	HttpRequest(char* url);
	~HttpRequest();

	// Get our handle
	CURL* GetHandle() { return m_curl; }

private:
	// The URL we'll be fetching from
	char* m_url;

	// The response
	char* m_response;

	// cURL stuff: internal handle and return HTTP code
	CURL* m_curl;
	CURLcode m_code;
};

#endif
