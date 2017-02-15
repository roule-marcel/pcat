#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char SERVER_PORT[64];

extern int server(const char* port);
extern void client(const char* hostname, const char* port);
extern int bootstraped;

int main(int argc, char** argv) {
	const char* bootstrap;
	if (argc < 2) {
		fprintf(stderr,"USAGE: %s <port> [bootstrap_host]\n", argv[0]);
		exit(1);
	}

	strcpy(SERVER_PORT, argv[1]);

	if(argc>=3) {
		bootstrap = argv[2];

		char* bootstrap_port = strchr(bootstrap, ':');
		if(!bootstrap_port) bootstrap_port = (char*)SERVER_PORT;
		else {
			*bootstrap_port = 0;
			bootstrap_port++;
		}

		client(bootstrap, bootstrap_port);
	} else bootstraped = 1;

	server(SERVER_PORT);
	return 0;
}
