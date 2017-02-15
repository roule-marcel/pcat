/*
 * client.c
 *
 *  Created on: 15 févr. 2017
 *      Author: jfellus
 */




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define BUFSIZE 512

static int end = 0;


typedef struct {
	pthread_t th;
	char* hostname;
	char* port;
	int sockfd;
} t_client;
t_client clients[100];
static int nb_clients=0;

int bootstraped = 0;


extern void on_new_connection(int fd, const char* addr, const char* port, int bWait);

static void reconnect(t_client* client) {
	int rv;
	struct addrinfo hints, *servinfo, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(client->hostname, client->port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "OUILLE getaddrinfo: %s %s:%s\n", gai_strerror(rv), client->hostname, client->port);
		exit(1);
	}

	int dt = 100000;
	while(!end) {
		// loop through all the results and connect to the first we can
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((client->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}

			if (connect(client->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(client->sockfd);
				continue;
			}

			break;
		}

		if (p) break;
		usleep(dt);
		dt *= 1.2;
		if(dt >= 5000000) dt = 5000000;
	}
	freeaddrinfo(servinfo);
}



void* client_thread(void* c) {
	t_client* client = (t_client*)c;

	while(!end && client->hostname && clients->port) {
		reconnect(client);
		on_new_connection(client->sockfd, client->hostname, client->port, 1);
	}

	return 0;
}


int has_client(const char* addr, const char* port) {
	int i;
	for(i=0; i<nb_clients; i++) {
		if(clients[i].hostname && !strcmp(addr, clients[i].hostname) && clients[i].port && !strcmp(addr, clients[i].port))
			return 1;
	}
	return 0;
}

void close_any_client(const char* addr, const char* port) {
	int i;
	for(i=0; i<nb_clients; i++) {
		if(clients[i].hostname && !strcmp(addr, clients[i].hostname) && clients[i].port && !strcmp(port, clients[i].port)) {
			clients[i].hostname = 0;
			clients[i].port = 0;
			close(clients[i].sockfd);
		}
	}
}

void client(const char* _hostname, const char* _port) {
	if(has_client(_hostname, _port)) return;
	if(has_connection(_hostname, _port)) return;
	clients[nb_clients].hostname = malloc(strlen(_hostname)+1);
	clients[nb_clients].port = malloc(strlen(_port)+1);
	strcpy(clients[nb_clients].hostname, _hostname);
	strcpy(clients[nb_clients].port, _port);
	pthread_create(&clients[nb_clients].th, NULL, client_thread, &clients[nb_clients]);
	nb_clients++;
}
