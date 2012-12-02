
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <httprequest.h>

HttpRequest::HttpRequest(char* url) {
	// Initialize no content
	m_content = 0;

	// Save a copy of the URL
	m_url = (char*)malloc(strlen(url) + 1);
	strcpy(m_url, url);

	// Set up to start doing transfers
        m_curl = curl_easy_init();
        curl_easy_setopt(m_curl, CURLOPT_URL, m_url);
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 5);
        curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "Open Web Index Crawler (+http://www.icedteapowerd.com/openweb/)");
        curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10);
        curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 60);
        curl_easy_setopt(m_curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP);
        curl_easy_setopt(m_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv2);
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &_write_function);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(m_curl, CURLOPT_PRIVATE, this);
        curl_easy_setopt(m_curl, CURLOPT_FORBID_REUSE, 1);
}

HttpRequest::~HttpRequest() {
	if(m_curl) {
		curl_easy_cleanup(m_curl);
		m_curl = 0;
	}

	if(m_content) {
		free(m_content);
		m_content = 0;
	}

	if(m_url) {
		free(m_url);
		m_url = 0;
	}
}

size_t HttpRequest::_write_function(char *ptr, size_t size, size_t nmemb, void *userdata) {
	return(size * nmemb);
}
