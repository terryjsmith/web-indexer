
/* INCLUDES */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

#define SOCKET_BUFFER_SIZE	1024

int main(int argc, char** argv) {
	// The socket for communication
        int m_socket = 0;

        // The respone we've gotten back
        char* m_content = 0;
        unsigned int m_size = 0;

	// Set up our libevent notification base
        int epoll = epoll_create(1);
        if(epoll < 0) {
                printf("Unable to create epoll interface.\n");
                return(0);
        }

        printf("Initialized epoll.\n");

	if((m_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
                printf("Unable to create socket.\n");
                return(0);
        }

        struct sockaddr_in server;
        struct hostent* host;

	char* hoststr = "www.icedteapowered.com";
	char* path = "/";

        // Get the IP address for the server
        if((host = gethostbyname(hoststr)) == NULL) {
                printf("Unable to resolve host: %s.\n", hoststr);
                return(false);
        }
        printf("Resolved host.\n");

        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        bcopy((char*)host->h_addr, (char*)&server.sin_addr.s_addr, host->h_length);
        server.sin_port = htons(80);

        // Connect!
        if(connect(m_socket,(struct sockaddr *) &server, sizeof(server)) < 0) {
                printf("Unable to connect: %s\n", strerror(errno));
                return(false);
        }
        printf("Connected.\n");

        // Make our socket non-blocking
        int flags;
        if((flags = fcntl(m_socket, F_GETFL, 0)) < 0) {
                printf("Unable to read socket for non-blocking I/O.\n");
                return(false);
        }
        printf("Got flags.\n");

        flags |= O_NONBLOCK;
        if((fcntl(m_socket, F_SETFL, flags)) < 0) {
                printf("Unable to set up socket for non-blocking I/O.\n");
                return(false);
        }
        printf("Set flags.\n");

	// Add the socket to epoll
        struct epoll_event event;
	memset(&event, 0, sizeof(epoll_event));

        event.data.fd = m_socket;
        event.events = EPOLLIN | EPOLLET;
        if((epoll_ctl(epoll, EPOLL_CTL_ADD, m_socket, &event)) < 0) {
                printf("Unable to setup epoll.\n");
                return(0);
        }

        // Construct our HTTP request
        char* request = (char*)malloc(strlen(path) + strlen(hoststr) + 46);
        unsigned int length = 0;
        length = sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nAccept-Encoding:\r\n\r\n", path, hoststr);

        // Send the request
        unsigned int sent = 0;
        while(sent != length) {
                int status = send(m_socket, request, length, 0);
                printf("%d\n", status);
                if(status > 0)
                        sent += status;
                else {
                        if(errno != EAGAIN) {
                                printf("Error sending HTTP request.\n");
                                free(request);
                                return(false);
                        }
                }
        }

	FILE* fp = fopen("index.html", "w");
	bool complete = false;
        while(true) {
		epoll_event* events = (epoll_event*)malloc(sizeof(epoll_event));
                memset(events, 0, sizeof(epoll_event));

		int msgs = epoll_wait(epoll, events, 1, -1);
                for(unsigned int i = 0; i < msgs; i++) {
			while(true) {
	        	        char* buffer = (char*)malloc(SOCKET_BUFFER_SIZE);
		                memset(buffer, 0, SOCKET_BUFFER_SIZE);

				int count = recv(m_socket, buffer, SOCKET_BUFFER_SIZE, 0);
				if((count == -1) && (errno == EAGAIN)) {
					free(buffer);
					break;
				}

                		/*int count = read(m_socket, buffer, SOCKET_BUFFER_SIZE);
				if((count < 0) && (errno == EAGAIN)) {
					complete = true;
                                        free(buffer);
					break;
				}*/

	                	if(count == 0) {
					complete = true;
					free(buffer);
					break;
				}

        	        	if(count > 0) {
					unsigned int new_length = m_size + count;
	                	        char* new_content = (char*)malloc(new_length + 1);

        	                	if(m_content)
                	                	strncpy(new_content, m_content, m_size);
	                	        strncpy(new_content + m_size, buffer, count);
        	                	new_content[new_length] = '\0';

        	        	        if(m_content)
                	        	        free(m_content);
	                	        m_content = new_content;

        	                	m_size += count;
				}

				free(buffer);

				if(count < SOCKET_BUFFER_SIZE) {
					complete = true;
					break;
				}

				usleep(1);
			}
                }

		free(events);

		if(complete) break;

		usleep(1);
        }

	fwrite(m_content, m_size, 1, fp);
	fclose(fp);

	printf("Complete\n");
}
