typedef struct player_info *PLAYER;
struct player_info {
	int player_num; //Identity of the player
    int m_socket; //Server socket of the master
	int nplayers;
	int nhops;
};

void init_player_info(PLAYER p) {
    p->player_num = -1;
    p->m_socket = -1;
    p->nplayers = -1;
	p->nhops = -1;
}

