#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "info.h"

char buf[512];
char host[64];
int rc, len, port;

int s; //socket
struct hostent *master, *player;
struct sockaddr_in master_in, player_in;

static int mport = 0;
static int nplayers = 0;
static int nhops = 0;

struct sockaddr_in *player_pin = NULL;

int perr = 0;

void close_conn(char *err) {
	if(!perr)
		perror(err);
	close(s);
	close(mport);
	exit(-1);
}

void setup_neighbors(int sockfd[]) {
	int i;
	//All players registered. Let players know who their right neighbor is
	for(i=0; i<nplayers; i++) {
		//printf("sending neighbor info\n");
		rc = send(sockfd[i], &player_pin[(i+1)%nplayers], sizeof(struct sockaddr_in), 0);
		if(rc < 0) {
			close_conn("send");
		}
	}
}

receive_neighbor_info(int sockfd[]) {
	int i;
	player_pin = (struct sockaddr_in *) malloc(nplayers * sizeof(struct sockaddr_in));	
	for(i=0; i<nplayers; i++) {
		rc = 0;
		while(rc!= sizeof(struct sockaddr_in)) {
			rc = rc + recv(sockfd[i], &player_pin[i] , sizeof(struct sockaddr_in), 0);
		}
		if(rc < 0) {
			close_conn("recv");
		}
	}
}

void initiate_ring(int sockfd[]) {
	/* master will send the initiate signal starting with player 0.
	 * master waits on each player to respond back that its connection 
	 * to neighboring player was successful.
	 * master continues to the next player and repeats the above steps
	 */
	int i;
	for(i=0; i<nplayers; i++) {
		rc = send(sockfd[i], "initiate", 8, 0);
		if(rc < 0) {
			close_conn("send");
		}

		rc = 0;	
		while(rc!= 7) {	
			rc = rc + recv(sockfd[i], buf, 7, 0);
		}
		if(rc < 0) {
			close_conn("recv");
		}
		buf[rc] = '\0';
		if(!strcmp("success", buf)) {
			continue;
		} else {
			printf("Ring formation failed between player %d and player %d\n", i, i+1);
			close(s);
			exit(-1);
		}
	}
}

void accept_connection(int *sockfd, PLAYER p[]) {
	int i = 0;

	while (i < nplayers) {
		len = sizeof(player_in);

		p[i] = (struct player_info *) malloc(sizeof(struct player_info));
		if(p[i] == NULL) {
			perror("malloc failed ");
			exit(-1);
		}
		init_player_info(p[i]);
		
		sockfd[i] = accept(s, (struct sockaddr *)&player_in, &len);
		if (sockfd[i] < 0) {
			perror("accept:");
			close(s);
			exit(rc);
		}
		player = gethostbyaddr((char *)&player_in.sin_addr, sizeof(struct in_addr), AF_INET);

		//Update the structure before sending it to the player
		p[i]->player_num = i;
		p[i]->m_socket = mport;
		p[i]->nplayers = nplayers;
		p[i]->nhops = nhops;
		printf("Player %d on %s\n", p[i]->player_num+1, player->h_name);
	
		//printf("sending player info to respective players\n");	
		rc = send(sockfd[i], p[i], sizeof(struct player_info), 0);
		if(rc < 0) {
			close_conn("send");
		}
		i++;
	}
}

void setup_master() {
	int option = 1;
	/* use address family INET and STREAMing sockets (TCP) */
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket:");
		exit(s);
	}

	/* set up the address and port */
	master_in.sin_family = AF_INET;
	master_in.sin_port = htons(mport);
	memcpy(&master_in.sin_addr, master->h_addr_list[0], master->h_length);

	/* bind socket s to address sin */
	rc = bind(s, (struct sockaddr *)&master_in, sizeof(master_in));
	if (rc < 0) {
			perror("bind:");
			close(s);
			exit(rc);
	}

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
	rc = listen(s, 5);
	if (rc < 0) {
		perror("listen:");
		close(s);
		exit(rc);
  	}
}

int main (int argc, void *argv[])
{
	int i;

	if (argc < 4 || argc > 4) {
		fprintf(stderr, "Usage: ./master <port-number> <number of players> <no of hops> \n");
		exit(1);
	}

	mport = atoi(argv[1]);
	if(mport <= 0) {
		printf("master server port error\n");
		exit(-1);
	}

	nplayers = atoi(argv[2]);
	if(nplayers<0 || nplayers < 2) {
		printf("Incorrect number of players\n");
		exit(-1);
	}

	if((nhops = atoi(argv[3])) < 0) {
		printf("Invalid number of hops\n");
		exit(-1);
	}


	int sockfd[nplayers-1];
	PLAYER p[nplayers-1];

	/* fill in hostent struct for self */
	if((gethostname(host, sizeof(host))) == -1) {
		perror("gethostname ");
		exit(-1);
	}
	master = gethostbyname(host);
	if (master == NULL) {
		fprintf(stderr, "%s: host not found (%s)\n", (char *)argv[0], host);
		exit(-1);
	}

	printf("Potato master on %s\n", host);
	printf("Players = %d\n", nplayers);
	printf("Hops = %d\n", nhops);

	setup_master();
	accept_connection(sockfd, p);

	receive_neighbor_info(sockfd);
	setup_neighbors(sockfd);

	initiate_ring(sockfd);

	if(nhops == 0) {
		/* no hops = no game */
		for(i=0; i<nplayers; i++) {
			rc = send(sockfd[i], "exit", 4, 0);
			if(rc < 0) {
				close_conn("send");
			}
		}
		perr = 1;
		for(i=0; i<nplayers; i++) {
			rc = send(sockfd[i], "close", 5, 0);
			if(rc < 0)
				close_conn("send");
		}
		printf("Trace of potato:\n");
		close_conn("");
		exit(0);
	}

	fd_set read_flags;
	FD_ZERO(&read_flags);

	for(i=0;i<nplayers;i++) {
		FD_SET(sockfd[i], &read_flags);
	}
	
	/* Pass the potato to a player */
	int random_player = rand()%nplayers;
	printf("All players present. Passing potato to player %d\n", random_player+1);
	int *potato = (int *) calloc(nhops, sizeof(int));


	rc = send(sockfd[random_player], "cont", 4, 0);
	if(rc < 0)
		close_conn("send");
	rc = send(sockfd[random_player], potato, (nhops)*sizeof(int), 0);
	if(rc < 0)
		close_conn("send");

	len = select(65535, &read_flags, NULL, NULL, NULL);
	if(len < 0)
		close_conn("select");

	for(i=0; i<nplayers; i++) {
		if(FD_ISSET(sockfd[i], &read_flags)) {
			FD_CLR(sockfd[i], &read_flags);
			break;
		}
	}

	rc = 0;
	while(rc!=(nhops)*sizeof(int)) {
		rc = rc + recv(sockfd[i], potato+(rc/sizeof(int)), (nhops)*sizeof(int), 0);
		if(rc < 0)
			close_conn("recv");
	}	

	for(i=0; i<nplayers; i++) {
		rc = send(sockfd[i], "exit", 4, 0);
		if(rc < 0)
			close_conn("send");
	}

	for(i=0; i<nplayers; i++) {
		rc = send(sockfd[i], "close", 5, 0);
		if(rc < 0)
			close_conn("send");
	}

	printf("Trace of potato:\n");
	for(i=0; i<nhops-1; i++) {
		printf("%d,", potato[i]);
	}
	printf("%d", potato[i]);
	printf("\n");
	close(s);
	close(mport);
	exit(0);
}
