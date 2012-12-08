
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ares.h>

#include <defines.h>
#include <url.h>

#include <httprequest.h>

HttpRequest::HttpRequest() {
	m_socket = 0;
	m_state = 0;
	m_url = 0;
	m_content = 0;
	m_fp = 0;
	m_filename = 0;
	m_error = 0;
	m_channel = 0;
}

HttpRequest::~HttpRequest() {
	if(m_socket) {
		close(m_socket);
		m_socket = 0;
	}

	if(m_url) {
		delete m_url;
		m_url = 0;
	}

	if(m_content) {
		free(m_content);
		m_content = 0;
	}

	if(m_fp) {
		fclose(m_fp);
		m_fp = 0;
	}

	if(m_filename) {
		free(m_filename);
		m_filename = 0;
	}

	if(m_error) {
		free(m_error);
		m_error = 0;
	}

	if(m_channel) {
		ares_destroy(m_channel);
	}
}

void HttpRequest::set_output_filename(char* filename) {
	unsigned int length = strlen(filename);
	m_filename = (char*)malloc(length + 1);
	strcpy(m_filename, filename);
}

bool HttpRequest::initialize() {
	if((m_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		m_error = (char*)malloc(1000);
		sprintf(m_error, "Unable to create socket.");
                return(false);
        }

        // Make our socket non-blocking
        int flags;
        if((flags = fcntl(m_socket, F_GETFL, 0)) < 0) {
                m_error = (char*)malloc(1000);
		sprintf(m_error, "Unable to read socket for non-blocking I/O on %s.", m_url->get_url());
                return(false);
        }

        flags |= O_NONBLOCK;
        if((fcntl(m_socket, F_SETFL, flags)) < 0) {
                sprintf(m_error, "Unable to set up socket for non-blocking I/O on %s.\n", m_url->get_url());
                return(false);
        }

	// Start our DNS request
	ares_init(&m_channel);
	ares_gethostbyname(m_channel, m_url->get_host(), AF_INET, _dns_lookup, this);
	m_state = HTTPREQUESTSTATE_DNS;

        return(true);
}

void HttpRequest::_dns_lookup(void *arg, int status, int timeouts, struct hostent *hostent) {
	HttpRequest* req = (HttpRequest*)arg;
	if(status == ARES_SUCCESS) {
		req->process(hostent);
	}
	else {
		req->error("Unable to do DNS lookup");
	}
}

void HttpRequest::error(char* error) {
	m_error = (char*)malloc(strlen(error) + 2 + strlen(m_url->get_url()) + 1);
	sprintf(m_error, "%s: %s", error, m_url->get_url());
	m_state = HTTPREQUESTSTATE_ERROR;
}

bool HttpRequest::process(void* arg) {
	switch(m_state) {
	case HTTPREQUESTSTATE_ERROR:
		{
			return(false);
		} break;
	case HTTPREQUESTSTATE_DNS:
		{
			if(arg) {
				// Process a lookup being passed along by the c-ares callback function
				struct sockaddr_in server;
			        struct hostent* host = (hostent*)arg;

        			memset(&server, 0, sizeof(server));
			        server.sin_family = AF_INET;
			        bcopy((char*)host->h_addr, (char*)&server.sin_addr.s_addr, host->h_length);
			        server.sin_port = htons(80);

				connect(m_socket,(struct sockaddr *) &server, sizeof(server));
				m_state = HTTPREQUESTSTATE_CONNECT;
			}
			else {
				// Otherwise, poll to see if the data we need is available yet
				int nfds, count;
				fd_set readers, writers;
				struct timeval tv, *tvp;

				FD_ZERO(&read);
     				FD_ZERO(&write);
				nfds = ares_fds(m_channel, &read, &write);

				if(nfds == 0) break;

				tvp = ares_timeout(m_channel, NULL, &tv);
				count = select(nfds, &read, &write, NULL, tvp);

				// This will automatically fire off our callback if it finds something so nothing to do after this
				ares_process(m_channel, &read, &write);
			}
		} break;
	case HTTPREQUESTSTATE_CONNECT: 
		{
			// Find out if we've connected to errored out
			epoll_event* event = (epoll_event*)arg;
			if(event.events & EPOLLERR) {
				m_error = (char*)malloc(1000 + strlen(m_url->get_url()));
				sprintf(m_error, "Unable to connect to %s: %s", m_url->get_url(), strerror(errno));
				m_state = HTTPREQUESTSTATE_ERROR;
				return(false);
			}
			else {
				// Connect should have been successful, send data
				char* request = (char*)malloc(strlen(m_url->get_path()) + strlen(m_url->get_query()) + strlen(m_url->get_host()) + 200);
				char* always = "Accept-Encoding:\r\nAccept: text/html,application/xhtml+xml,application/xml\r\nUser-Agent: Open Web Indexer (+http://www.icedteapowered.com/openweb/)\r\n\r\n";
				unsigned int length = 0;

				if(strlen(m_url->get_query())) {
					length = sprintf("GET %s?%s HTTP/1.1\r\nHost: %s\r\n%s", m_url->get_path(), m_url->get_query(), m_url->get_host(), always);
				}
				else {
					length = sprintf("GET %s HTTP/1.1\r\nHost: %s\r\n%s", m_url->get_path(), m_url->get_host(), always);
				}

        			// Send the request
			        unsigned int sent = 0;
			        while(sent != length) {
			                int status = send(m_socket, request, length, 0);
			                if(status > 0)
			                        sent += status;
			                else {
			                        if(errno != EAGAIN) {
                        			        printf("Error sending HTTP request on %s.\n", m_url->url);
			                                free(request);
                        			        return(false);
			                        }
                			}
        			}

			        free(request);
				m_state = HTTPREQUESTSTATE_RECV;
			}
		} break;
	case HTTPREQUESTSTATE_RECV:
		{
		} break;
	default: break;
	};

	return(true);
}
