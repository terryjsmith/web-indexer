
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <my_global.h>
#include <mysql.h>
#include <url.h>
#include <httprequest.h>
#include <defines.h>
#include <netdb.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <sys/types.h>

HttpRequest::HttpRequest(URL* url) {
	// Save a copy of the URL
	m_url = url->Clone();
	m_socket = 0;
	m_filename = 0;
	m_content = 0;
	m_size = 0;
	m_code = 0;
	m_complete = false;
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

	if(m_filename) {
		free(m_filename);
		m_filename = 0;
	}

	if(m_content) {
		free(m_content);
		m_content = 0;
	}
}

int HttpRequest::Initialize() {
	if((m_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		printf("Unable to create socket.\n");
		return(0);
	}

	// Make our socket non-blocking
        int flags;
        if((flags = fcntl(m_socket, F_GETFL, 0)) < 0) {
                printf("Unable to read socket for non-blocking I/O on %s.\n", m_url->url);
                return(false);
        }

        flags |= O_NONBLOCK;
        if((fcntl(m_socket, F_SETFL, flags)) < 0) {
                printf("Unable to set up socket for non-blocking I/O on %s.\n", m_url->url);
                return(false);
        }

	return(m_socket);
}

bool HttpRequest::Connect() {
	struct sockaddr_in server;
        struct hostent* host;

	// Get the IP address for the server (TODO: make this async as well, possibly using libevent)
        if((host = gethostbyname(m_url->parts[URL_DOMAIN])) == NULL) {
                printf("Unable to resolve host: %s.\n", m_url->parts[URL_DOMAIN]);
                return(false);
        }

        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        bcopy((char*)host->h_addr, (char*)&server.sin_addr.s_addr, host->h_length);
        server.sin_port = htons(80);

	// Connect!
	connect(m_socket,(struct sockaddr *) &server, sizeof(server));
	return(true);
}

bool HttpRequest::Send() {
	// Construct our HTTP request
        char* request = (char*)malloc(strlen(m_url->parts[URL_PATH]) + strlen(m_url->parts[URL_QUERY]) + strlen(m_url->parts[URL_DOMAIN]) + 200);
        unsigned int length = 0;
        if(strlen(m_url->parts[URL_QUERY]))
                length = sprintf(request, "GET %s?%s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding:\r\nAccept: text/html,application/xhtml+xml,application/xml\r\nUser-Agent: Open Web Indexer (+http://www.icedteapowered.com/openweb/)\r\n\r\n", m_url->parts[URL_PATH], m_url->parts[URL_QUERY], m_url->parts[URL_DOMAIN]);
        else
                length = sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding:\r\nAccept: text/html,application/xhtml+xml,application/xml\r\nUser-Agent: Open Web Indexer (+http://www.icedteapowered.com/openweb/)\r\n\r\n", m_url->parts[URL_PATH], m_url->parts[URL_DOMAIN]);

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
	return(true);
}

int HttpRequest::Read() {
	while(true) {
		char* buffer = (char*)malloc(SOCKET_BUFFER_SIZE);
		memset(buffer, 0, SOCKET_BUFFER_SIZE);

		int count = read(m_socket, buffer, SOCKET_BUFFER_SIZE);
		if((count < 0) && (errno == EAGAIN)) {
			free(buffer);
			break;
		}

		if(count == 0) {
			m_complete = true;
			free(buffer);
			return(0);
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
			m_complete = true;
			free(buffer);
			return(0);
		}

		free(buffer);
	}

	return(1);
}

bool HttpRequest::Process() {
	unsigned int pos = 0;

	// Make sure we have something to process
	char* line = strtok(m_content, "\n");
	pos += strlen(line) + 1;
	if(line == NULL)
		return(false);

	// Parse the first line as the HTTP code
	if(strlen(line) >= 13) {
		char* code = (char*)malloc(4);
		strncpy(code, line + 9, 3);
		code[3] = '\0';
		m_code = atoi(code);
		free(code);
	}

	// Process the header first
	while(line != NULL) {
		if(strlen(line) <= 2) {
			pos += strlen(line) + 1;
			break;
		}

		if((strstr(line, "Location: ")) == line) {
			unsigned int length = strlen(line) - 11;
			char* location = (char*)malloc(length + 1);
			strncpy(location, line + 10, length);
			location[length] = '\0';

			printf("Got Location redirect %s for URL %s\n", location, m_url->url);

			URL* new_url = new URL(location);
			new_url->Parse(m_url);

			free(location);
			delete m_url;
			m_url = new_url->Clone();
			delete new_url;
		}

		pos += strlen(line) + 1;
		line = strtok(NULL, "\n");
	}

	if(m_code == 200) {
		// Save the rest to a file
		unsigned int dir_length = strlen(BASE_PATH) + strlen(m_url->parts[URL_DOMAIN]);
	        char* dir = (char*)malloc(dir_length + 1);
        	sprintf(dir, "%s%s", BASE_PATH, m_url->parts[URL_DOMAIN]);
	        dir[dir_length] = '\0';

        	mkdir(dir, 0644);
	        free(dir);

        	unsigned int length = strlen(BASE_PATH) + strlen(m_url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
	        m_filename = (char*)malloc(length + 1);
        	sprintf(m_filename, "%s%s/%s.html", BASE_PATH, m_url->parts[URL_DOMAIN], m_url->hash);
	        m_filename[length] = '\0';

		FILE* fp = fopen(m_filename, "w");
		if(!fp) {
        	        printf("Unable to open file %s.\n", m_filename);
			free(m_filename);
	                return(false);
        	}

		fwrite(m_content + pos, m_size - pos, 1, fp);
		fclose(fp);
	}

	return(true);
}
