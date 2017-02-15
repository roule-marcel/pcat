/*
 * server.c
 *
 *  Created on: 15 févr. 2017
 *      Author: jfellus
 */




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>

#define BUFSIZE 512

extern char SERVER_PORT[];
char MY_IP[64];

static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

extern void client(const char* _hostname, const char* _port);
extern int bootstraped;

typedef struct {
	int fd;
	pthread_t th;
	char* addr;
	char* port;
	int bDuplicate;
	int out;
} t_connection;
t_connection connections[512];
int nb_connections = 0;

static void rtrim(char* s) {
	int i;
	for(i=strlen(s)-1; i>0; i--) {
		if(!isspace(s[i]) && s[i]!='\n') break;
		s[i] = 0;
	}
}

static int str_starts_with(const char* s, const char* prefix) {
	return strncmp(s, prefix, strlen(prefix))==0;
}


void dump_connections() {
	fprintf(stderr, "--------------------\n");
	int i;
	for(i=0; i<nb_connections; i++) {
		if(connections[i].fd == -1) continue;
		fprintf(stderr, "%u   %s:%s  (%s) %s\n", connections[i].fd, connections[i].addr, connections[i].port, (connections[i].out ? "out" : "in" ), (connections[i].bDuplicate ? "(duplicate)" : ""));
	}
	fprintf(stderr, "--------------------\n\n");
}

void on_close_connection(t_connection* c) {
	if(!c->bDuplicate) {
		int i;
		for(i=0; i<nb_connections; i++) {
			if(&connections[i] != c && connections[i].fd!=-1 && !strcmp(c->addr, connections[i].addr) && !strcmp(c->port, connections[i].port)) break;
		}
		if(i<nb_connections) c->bDuplicate = 0;
	}

	c->fd = -1;
	free(c->addr); c->addr = 0;
	free(c->port); c->port = 0;

//	dump_connections();
}

void handle_duplicate_connections(t_connection* c) {
	int i;
	for(i=0; i<nb_connections; i++) {
		if(&connections[i] != c && connections[i].fd!=-1 && connections[i].port && connections[i].addr && !strcmp(c->addr, connections[i].addr) && !strcmp(c->port, connections[i].port)) break;
	}
	if(i<nb_connections) {
		c->bDuplicate = 1;
	}
}


void forward(const char* buf, size_t len, int allbutfd) {
	//	pthread_mutex_lock(&mut);
		int i;
		for(i=0; i<nb_connections; i++) {
			if(connections[i].fd != -1 && connections[i].fd != allbutfd && !connections[i].bDuplicate)
				send(connections[i].fd, buf, strlen(buf), 0);
		}
	//	pthread_mutex_lock(&mut);
}



void broadcast(const char* buf, size_t len) {
//	pthread_mutex_lock(&mut);
	int i;
	for(i=0; i<nb_connections; i++) {
		if(connections[i].fd != -1 && !connections[i].bDuplicate)
		send(connections[i].fd, buf, strlen(buf), 0);
	}
//	pthread_mutex_lock(&mut);
}

void vbroadcast(const char* fmt, ...) {
	char s[512];
	va_list args;
	va_start (args, fmt);
	vsprintf (s, fmt, args);
	va_end (args);
	broadcast(s, strlen(s));
}

int has_connection(const char* addr, const char* port) {
	int i;
	for(i=0; i<nb_connections; i++) {
		if(connections[i].fd != -1 && !strcmp(addr, connections[i].addr) && connections[i].port && !strcmp(port, connections[i].port))
			return 1;
	}
	return 0;
}

void command(t_connection* connection, const char* cmd) {
	//printf("-> %s", cmd);
	if(str_starts_with(cmd, "MYPORT ")) {
		if(connection->port) free(connection->port);
		char* port = strchr(cmd, ' ')+1;
		rtrim(port);
		connection->port = malloc(strlen(port)+1);
		strcpy(connection->port, port);
//		printf("Connection from : %s:%s\n", connection->addr, connection->port);
		handle_duplicate_connections(connection);
//		dump_connections();
	} else if(str_starts_with(cmd, "YOUARE ")) {
		rtrim((char*)cmd);
		strcpy(MY_IP, strchr(cmd, ' ')+1);
		if(!strcmp(MY_IP, "127.0.0.1")) strcpy(MY_IP, "localhost");
		//printf("I AM %s:%s\n", MY_IP, SERVER_PORT);
	} else if(str_starts_with(cmd, "NEW ")) {
		rtrim((char*)cmd);
		char* addr = strchr(cmd, ' ')+1;
		char* port = strchr(cmd, ':'); *port = 0; port++;
		if(!has_connection(addr,port) && !has_client(addr,port) && bootstraped && (strcmp(addr, MY_IP) || strcmp(port, SERVER_PORT)) ) {
			client(addr, port);
		}
	}
}

void* connection_thread(void* _c) {
	char buf[BUFSIZE];
	t_connection* connection = (t_connection*)_c;
	int n;
	char line[BUFSIZE*2];
	int i = 0;
	int j = 0;
	while((n = recv(connection->fd, buf, BUFSIZE-1, 0)) > 0) {
		pthread_mutex_lock(&mut);
		for(i=0; i<n; i++) {
			line[j++] = buf[i];
			if(buf[i]=='\n') {
				line[j] = 0;
				if(line[0]=='$') command(connection, line+1);
				j = 0;
			}
			if(line[0]!='$') putchar(buf[i]);
		}
		pthread_mutex_unlock(&mut);
    }
	on_close_connection(connection);
	return 0;
}




void vsend(int sockfd, const char* fmt, ...) {
	char s[512];
	va_list args;
	va_start (args, fmt);
	vsprintf (s, fmt, args);
	va_end (args);
	send(sockfd, s, strlen(s), 0);
}

pthread_t th_server;
void* server_thread(void* x) {
	char buf[BUFSIZE];
	while(fgets(buf, BUFSIZE, stdin)) {
		if(buf[0]=='+') { // Connect to a peer when input is '+ip:port'
			rtrim(buf);
			char* addr = &buf[1];
			char* port = strchr(addr, ':'); *port = 0; port++;
			client(addr, port);
		}
		else broadcast(buf, strlen(buf));
	}
	return 0;
}

void send_connections(int fd) {
	int i;
	for(i=0; i<nb_connections; i++) {
		if(connections[i].fd != -1 && connections[i].fd != fd && !connections[i].bDuplicate && connections[i].addr && connections[i].port)
			vsend(fd, "$NEW %s:%s\n", connections[i].addr, connections[i].port);
	}
}

void on_new_connection(int fd, const char* addr, const char* port, int bWait) {

	t_connection* connection = connections;
	int i;
	for(i=0; i<512; i++) {
		if(connections[i].fd == -1) {
			connection = &connections[i];
			if(i>=nb_connections) nb_connections = i+1;
			break;
		}
	}
	connection->fd = fd;
	connection->bDuplicate = 0;
	connection->out = 0;

	if(addr) {
		connection->addr = malloc(strlen(addr)+1); strcpy(connection->addr, addr);
		if(!strcmp(connection->addr, "127.0.0.1")) strcpy(connection->addr, "localhost");
	}
	if(port) {connection->port = malloc(strlen(port)+1); strcpy(connection->port, port); }
	pthread_create(&connection->th, NULL, connection_thread, connection);

	vsend(connection->fd, "$YOUARE %s\n", addr);
	if(bWait) {
		connection->out = 1;

//		printf("Connected to : %s:%s\n", connection->addr, connection->port);
		handle_duplicate_connections(connection);
//		dump_connections();

		vsend(connection->fd, "$MYPORT %s\n", SERVER_PORT);
		bootstraped = 1;

		pthread_join(connection->th, NULL);
	} else {
		send_connections(connection->fd);
	}
}




static void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

int server(const char* port) {
	int i;
	for(i=0; i<512; i++) {
		connections[i].fd = -1;
		connections[i].addr = 0;
		connections[i].port = 0;
		connections[i].bDuplicate = 0;
		connections[i].out = 0;
	}
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, 100) == -1) {
        perror("listen");
        exit(1);
    }

    pthread_create(&th_server, NULL, server_thread, NULL);

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);

        on_new_connection(new_fd, s, 0, 0);
    }

    return 0;
}
