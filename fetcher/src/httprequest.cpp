
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

HttpRequest::HttpRequest(Url* url) {
	m_socket = 0;
	m_state = 0;
	m_url = url;
	m_content = 0;
	m_fp = 0;
	m_filename = 0;
	m_error = 0;
	m_channel = 0;
	m_size = 0;
	m_code = 0;
	m_effective = 0;
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

	if(m_effective) {
		free(m_effective);
		m_effective = 0;
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
	// we use if statements here so that we can execute multiple code blocks in a row where necessary
	if(m_state == HTTPREQUESTSTATE_DNS) {
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
	}

	if(m_state == HTTPREQUESTSTATE_CONNECT) {
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
	}

	if(m_state == HTTPREQUESTSTATE_RECV) {
		while(true) {
               		char* buffer = (char*)malloc(SOCKET_BUFFER_SIZE);
               		memset(buffer, 0, SOCKET_BUFFER_SIZE);

	                int count = read(m_socket, buffer, SOCKET_BUFFER_SIZE);
               		if((count < 0) && (errno == EAGAIN)) {
	                        free(buffer);
               		        break;
	                }

               		if(count == 0) {
               		        free(buffer);
				m_state = HTTPREQUESTSTATE_WRITE;
	                        break;
               		}

	                if(count > 0) {
               		        unsigned int length = m_size + count;
	                        char* new_content = (char*)malloc(length + 1);

               		        if(m_content)
                               		strncpy(new_content, m_content, m_size);
	                        strncpy(new_content + m_size, buffer, count);
               		        new_content[length] = '\0';

	                        if(m_content)
               		                free(m_content);
	                        m_content = new_content;

               		        m_size += count;
	                }

               		if(count < SOCKET_BUFFER_SIZE) {
               		        free(buffer);
				m_state = HTTPREQUESTSTATE_WRITE;
	                        break;
               		}

	                free(buffer);
	        }
	}

	if(m_state == HTTPREQUESTSTATE_WRITE) {
		// First process the HTTP response headers
		unsigned int pos = 0;

       		// Make sure we have something to process
	        char* line = strtok(m_content, "\n");
	        pos += strlen(line) + 1;
	        if(line == NULL)
               		return(false); // TODO: maybe handle this better

	        // Parse the first line as the HTTP code
	        if(strlen(line) >= 13) {
               		char* code = (char*)malloc(4);
	                strncpy(code, line + 9, 3);
               		code[3] = '\0';
	                m_code = atoi(code);
               		free(code);
	        }

		// Process the rest of the header
       		while(line != NULL) {
	                if(strlen(line) <= 2) {
               		        pos += strlen(line) + 1;
	                        break;
                	}

			// If we get a location header, record it
	                if((strstr(line, "Location: ")) == line) {
               		        unsigned int length = strlen(line) - 11;
	                        m_effective = (char*)malloc(length + 1);
                		strncpy(m_effective, line + 10, length);
		                m_effective[length] = '\0';
               		}

	                pos += strlen(line) + 1;
               		line = strtok(NULL, "\n");
       		}

		// If we get here, it means we need to (optionally) dump it out to a file if there's on set, otherwise, complete
                if(!m_filename) {
                        m_state = HTTPREQUESTSTATE_COMPLETE;
                        return(true);
                }

		// Finally, if we've got a 200 status code, write it out
		if(m_code == 200) {
	                // Save the rest to a file
	                unsigned int dir_length = strlen(BASE_PATH) + strlen(m_url->get_host());
               		char* dir = (char*)malloc(dir_length + 1);
	                sprintf(dir, "%s%s", BASE_PATH, m_url->get_host());
               		dir[dir_length] = '\0';

	                mkdir(dir, 0644);
               		free(dir);

	                FILE* fp = fopen(m_filename, "w");
               		if(!fp) {
				m_error = (char*)malloc(100 + strlen(m_filename));
	                        sprintf(m_error, "Unable to open file %s.", m_filename);

				m_state = HTTPREQUESTSTATE_ERROR;
	                        return(false);
               		}

	                fwrite(m_content + pos, m_size - pos, 1, fp);
               		fclose(fp);
	        }
	}

	if(m_state == HTTPREQUESTSTATE_ERROR) {
                return(false);
        }

	return(true);
}
