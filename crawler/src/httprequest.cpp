
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

	return(m_socket);
}

bool HttpRequest::Start() {
	struct sockaddr_in server;
        struct hostent* host;

	// Get the IP address for the server (TODO: make this async as well, possibly using libevent)
        if((host = gethostbyname(m_url->parts[URL_DOMAIN])) == NULL) {
                printf("Unable to resolve host: %s.\n", m_url->parts[URL_DOMAIN]);
                return(false);
        }
	printf("Resolved host.\n");

        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        memcpy((char*)&server.sin_addr.s_addr, (char*)&host->h_addr, host->h_length);
        server.sin_port = htons(80);

	// Connect!
	if(connect(m_socket,(struct sockaddr *) &server, sizeof(server)) < 0) {
		printf("Unable to connect to %s.\n", m_url->url);
		return(false);
	}
	printf("Connected.\n");

	// Make our socket non-blocking
	int flags;
  	if((flags = fcntl(m_socket, F_GETFL, 0)) < 0) {
		printf("Unable to read socket for non-blocking I/O on %s.\n", m_url->url);
                return(false);
	}
	printf("Got flags.\n");

  	flags |= O_NONBLOCK;
  	if((fcntl(m_socket, F_SETFL, flags)) < 0) {
		printf("Unable to set up socket for non-blocking I/O on %s.\n", m_url->url);
                return(false);
    	}
	printf("Set flags.\n");

	// Construct our HTTP request
	char* request = (char*)malloc(strlen(m_url->parts[URL_PATH]) + strlen(m_url->parts[URL_QUERY]) + strlen(m_url->parts[URL_DOMAIN]) + 27);
	unsigned int length = 0;
	if(strlen(m_url->parts[URL_QUERY]))
		length = sprintf(request, "GET %s?%s HTTP/1.1\r\nHost: %s\r\n\r\n", m_url->parts[URL_PATH], m_url->parts[URL_QUERY], m_url->parts[URL_DOMAIN]);
	else
		length = sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", m_url->parts[URL_PATH], m_url->parts[URL_DOMAIN]);

	// Send the request
	unsigned int sent = 0;
	while(sent != length) {
		int status = send(m_socket, request, length, 0);
		printf("%d\n", status);
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
	int retval = 0;
	while(true) {
		char* buffer = (char*)malloc(SOCKET_BUFFER_SIZE);
		memset(buffer, 0, SOCKET_BUFFER_SIZE);

		int count = read(m_socket, buffer, SOCKET_BUFFER_SIZE);
		if(count < 0) {
			if(errno == EAGAIN) break;
		}

		if(count == 0) return(0);

		if(count > 0) {
			char* new_content = (char*)malloc(m_size + count + 1);

			if(m_content)
				strncpy(new_content, m_content, m_size);
			strncpy(new_content + m_size, buffer, count);
			new_content[m_size + count] = '\0';

			if(m_content)
				free(m_content);
			m_content = new_content;

			m_size += count;
			retval += count;
		}
	}

	return(retval);
}

bool HttpRequest::Process() {
	// Make sure we have something to process
	char* line = strtok(m_content, "\n");
	if(line == NULL)
		return(false);

	// Parse the first line as the HTTP code
	if(strlen(line) > 13) {
		char* code = (char*)malloc(4);
		strncpy(code, line + 10, 3);
		code[3] = '\0';
		m_code = atol(code);
	}

	// Process the header first
	while(strlen(line) > 1) {
		strtok(NULL, "\r");
	}

	// Save the rest to a file
	unsigned int dir_length = strlen(BASE_PATH) + strlen(m_url->parts[URL_DOMAIN]);
        char* dir = (char*)malloc(dir_length + 1);
        sprintf(dir, "%s%s", BASE_PATH, m_url->parts[URL_DOMAIN]);
        dir[dir_length] = '\0';

        mkdir(dir, 0644);
        free(dir);

        unsigned int length = strlen(BASE_PATH) + strlen(m_url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
        char* filename = (char*)malloc(length + 1);
        sprintf(filename, "%s%s/%s.html", BASE_PATH, m_url->parts[URL_DOMAIN], m_url->hash);
        filename[length] = '\0';

	length = strlen(m_url->parts[URL_DOMAIN]) + 1 + (MD5_DIGEST_LENGTH * 2) + 5;
	char* m_filename = (char*)malloc(length + 1);
	sprintf(m_filename, "%s/%s.html", m_url->parts[URL_DOMAIN], m_url->hash);
        m_filename[length] = '\0';

	FILE* fp = fopen(filename, "w");
	if(!fp) {
                printf("Unable to open file %s.\n", filename);
		free(filename);
                return(false);
        }

	fwrite(m_content, 1, m_size, fp);
	fclose(fp);

	free(filename);
	return(true);
}
