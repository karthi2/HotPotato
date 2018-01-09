/* CLOSE ALL SOCKETS IN EACH FAILURE CHECK */
/* CLEAN UP AT THE END OF THE GAME */


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include "info.h"

#define LEN	64

static int server_socket;
static int player_socket, rt_socket, lt_socket;

char host[LEN], str[LEN];
char buf[LEN]; 
struct hostent *player, *master;
int rc, mport, sport;
struct sockaddr_in rt_player, lt_player;
struct sockaddr_in player_pin;
struct sockaddr_in master_in, player_in;

static int perr;

PLAYER p = NULL;

void close_conn(char *err) {
	if(!perr)
    	perror(err);
	if(player_socket)
  		close(player_socket);
	if(server_socket)
		close(server_socket);
	if(rt_socket) {
		close(rt_socket);
		close(sport);
	}
	if(lt_socket) {
		close(lt_socket);
		close(sport);
	}
	close(mport);
    exit(1);
}

int max() {
	if(player_socket > rt_socket) {
		if(player_socket > lt_socket) {
			return player_socket;
		} else {
			return lt_socket;
		}
	} else { 
		if(rt_socket > lt_socket) {
			return rt_socket;
		}
		return lt_socket;
	}
}

void recv_neighbors() {
	rc = 0;
	while(rc!= sizeof(struct sockaddr_in)) {
		rc = rc + recv(player_socket, &player_in, sizeof(struct sockaddr_in), 0);
	}
	if(rc < 0) {
		close_conn("recv");
	}
}

int free_port() {
	int init = 10000; //start scanning from the 10000th port
	struct sockaddr_in temp;

	int option = 1;
	
	rc = -1;
	
	temp.sin_family = AF_INET;
	temp.sin_port = htons(init);
	while(rc < 0) {
		rc = bind(server_socket, (struct sockaddr *)&temp, sizeof(temp));
	}

	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,(char*)&option,sizeof(option));
	printf("free port is %d\n", rc);
	return init;
}

void setup_server(PLAYER p) {
	/*fill in hostent struct for myself*/
	gethostname(host, sizeof(host));
	player = gethostbyname(host);
 
  	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	//printf("server_socket = %d\n", server_socket);
  	if (server_socket < 0) {
		close_conn("create server socket for player");
  	}

	sport = 500+p->player_num;
  
	int rc = -1;	
	player_pin.sin_family = AF_INET;

	while(rc < 0) {
  		player_pin.sin_port = htons(sport);
  		memcpy(&player_pin.sin_addr, player->h_addr_list[0], player->h_length);
		rc = bind(server_socket, (struct sockaddr *)&player_pin, sizeof(player_pin));
		++sport;
	}
	if(rc < 0) {
		close_conn("bind");
	}

	rc = send(player_socket, &player_pin, sizeof(player_pin), 0);
	if(rc < 0) {
		close_conn("send");
	}
	rc = listen(server_socket, 5);
	if(rc < 0) {
		close_conn("listen");
	}
}

void initiate_ring() {
	int len = sizeof(player_in);
	
	rc = 0;
	while(rc!=8) {
		rc = rc + recv(player_socket, buf, 8, 0);
	}
	if(rc < 0) {
		close_conn("recv");
	}
	buf[rc] = '\0';
	if(!strcmp("initiate", buf)) {
	/* make connection with the right player */
		rt_socket = socket(AF_INET, SOCK_STREAM, 0);
		if(rt_socket < 0) {
			close_conn("socket");
		}
		rc = connect(rt_socket, (struct sockaddr *)&player_in, sizeof(player_in));
		if(rc < 0) {
			perror("connect with rt player ");
			send(player_socket, "failed", 6, 0);
		}
	}
	rc = send(player_socket, "success", 7, 0);
	/*if(p->player_num == 0) {
		int len;
		len = sizeof(player_in);
		lt_socket = accept(server_socket, (struct sockaddr *)&player_in, &len);
	} else {*/
		lt_socket = accept(server_socket, (struct sockaddr *)&player_in, &len);
	//}
	if(lt_socket < 0) {
		close_conn("accept");
	}
}


void setup_client() {
	player_socket = socket(AF_INET, SOCK_STREAM, 0);
	//printf("player_socket = %d\n", player_socket);
	if(player_socket<0) {
		close_conn("socket");
	}

  	/* set up the address and port */
  	master_in.sin_family = AF_INET;
  	master_in.sin_port = htons(mport);
  	memcpy(&master_in.sin_addr, master->h_addr_list[0], master->h_length);
	

  	/* connect to socket at above addr and port */
  	rc = connect(player_socket, (struct sockaddr *)&master_in, sizeof(master_in));
  	if (rc < 0) {
		close_conn("connect");
  	}
	
	//Get the complete player info from master
	rc = 0;
	while(rc!= sizeof(struct player_info)) {
		rc = rc + recv(player_socket, p, sizeof(struct player_info), 0);
	}
	if(rc < 0) {
		close_conn("recv");
	}
}

int main (int argc, char *argv[])
{
	int len;

	p = (struct player_info *) malloc(sizeof(struct player_info));
	init_player_info(p);

  	/* read host and port number from command line */
  	if (argc != 3) {
    	fprintf(stderr, "Usage: %s <host-name> <port-number>\n", argv[0]);
    	exit(1);
  	}

	/********** client part of the player ****************/
  	/* fill in hostent struct */
  	master = gethostbyname(argv[1]); 
  	if (master == NULL) {
    	fprintf(stderr, "%s: host not found (%s)\n", argv[0], host);
    	exit(1);
  	}
  	mport = atoi(argv[2]);

	setup_client();
	printf("Connected as player %d\n", p->player_num + 1);

	/***** SERVER PART OF THE PLAYER *****/
	setup_server(p);

	recv_neighbors();
	initiate_ring();
	
	int *potato = NULL;
	//int *potato = (int *) calloc(p->nhops+1, sizeof(int));
	/*if(potato == NULL) {
		perror("calloc");
		exit(-1);
	}*/

	//Wait on select if the master sends the potato to him at the beginning
	int i = 0;
	fd_set read_flags;
	while(1) {
		FD_ZERO(&read_flags);
	
		FD_SET(player_socket, &read_flags);
		FD_SET(rt_socket, &read_flags);
		FD_SET(lt_socket, &read_flags);
	
		rc = select(max()+1, &read_flags, NULL, NULL, NULL);
		if(rc < 0) {
			close_conn("select");
		}

		if(FD_ISSET(player_socket, &read_flags)) {
			//printf("from master\n");
			//buf = NULL;
			len = 0;
			while(len!=4) {
				len = len + recv(player_socket, buf, 4, 0);
			}
			buf[len] = '\0';
			if (!strcmp("exit", buf)) {
				//buf = NULL;
				len = 0;
				while(len!=5) {
					len = len + recv(player_socket, buf, 5, 0);
				}
				buf[len] = '\0';
				if (!strcmp("close", buf)) {
					perr = 1;
					close_conn("");
				}
			} else if(!strcmp("cont", buf)) {
				//potato = (int *) calloc(p->nhops+1, sizeof(int));
				potato = (int *) calloc(p->nhops, sizeof(int));
				rc = 0;
				while(rc!=(p->nhops)*sizeof(int)) {
					rc = rc + recv(player_socket, potato+(rc/sizeof(int)), (p->nhops)*sizeof(int), 0);
					bzero(potato, (p->nhops)*sizeof(int));
					//printf("potato received\n");
				}
			}
		} else if(FD_ISSET(rt_socket, &read_flags)) {
			//printf("rt_socket\n");
			//potato = (int *) calloc(p->nhops+1, sizeof(int));
			potato = (int *) calloc(p->nhops, sizeof(int));
			rc = 0;
			while(rc!=(p->nhops)*sizeof(int)) {
				rc = rc + recv(rt_socket, potato+(rc/sizeof(int)), (p->nhops)*sizeof(int), 0);
				//printf("no of bytes received = %d\n", rc);
			}
		} else if(FD_ISSET(lt_socket, &read_flags)) {
			//printf("lt_socket\n");
			potato = (int *) calloc(p->nhops, sizeof(int));
			rc = 0;
			while(rc!=(p->nhops)*sizeof(int)) {
				rc = rc + recv(lt_socket, potato+(rc/sizeof(int)), (p->nhops)*sizeof(int), 0);
				//printf("no of bytes received = %d\n", rc);
			}
		}
		//printf("player 4 shud print this\n");
		if(rc < 0) {
			close_conn("recv");
		}

		//play the game...pass the potato
		for(i=0; i<p->nhops; i++) {
			if(potato[i] == 0) {
				potato[i] = p->player_num+1;
				//printf("%d ", i+1);
				break;
			}
		}

		if(i == (p->nhops-1)) {
			//End of game!	
			printf("I'm it\n");
			rc = send(player_socket, potato, (p->nhops)*sizeof(int), 0);
			if(rc<0) {
				close_conn("send");
			}
			//perr = 1;
			//close_conn("");
			continue;
			//break;
		}

		if(rand()%2 == 0) {
			if(p->player_num == 0) { 
				printf("Sending potato to %d\n", p->nplayers);
			} else {
				printf("Sending potato to %d\n", (p->player_num)%p->nplayers);
			}
			rc = send(lt_socket, potato, (p->nhops)*sizeof(int), 0);
			free(potato);
			//printf("no of bytes sent = %d\n", rc);
		} else {
			if(p->player_num == 0) {
				printf("Sending potato to %d\n", p->player_num+2);
			} else {
				if((p->player_num+1)%p->nplayers == 0) {
					printf("Sending potato to 1\n");
				} else
					printf("Sending potato to %d\n", (p->player_num+2));
			}
			rc = send(rt_socket, potato, (p->nhops)*sizeof(int), 0);
			free(potato);
			//printf("no of bytes sent = %d\n", rc);
		}
		if(rc < 0) {
			close_conn("send");
		}
	}
	//close_conn("");
  	exit(0);
}
