#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <resolv.h>
#include <poll.h>
#include <inttypes.h>
#include "server.h"
#include "connection_packet.h"
#include "control_packet.h"
#include "acknowledge_packet.h"
#include "audio_packet.h"

#define MAX_MSG 1024

/* functions */
//void *(*callbacks[255])(char * data); 
typedef void *(*packet_function)(char *data, unsigned int len, struct sockaddr_in * cli_addr, unsigned int cli_len);

packet_function f0_callbacks[255];
int socket_desc;

/* context of the server */
struct server * ts_server;

static void test_init_server()
{
	/* Test server */
	ts_server = new_server();
	/* Add channels */
	add_channel(ts_server, new_predef_channel());
	add_channel(ts_server, new_predef_channel());
	add_channel(ts_server, new_predef_channel());
	destroy_channel_by_id(ts_server, 1);
	add_channel(ts_server, new_predef_channel());
	add_channel(ts_server, new_predef_channel());
	add_channel(ts_server, new_predef_channel());
	add_channel(ts_server, new_channel("Name", "Topic", "Desc", CHANNEL_FLAG_DEFAULT, CODEC_SPEEX_16_3, 0, 16));

	/* Add players */
	/*
	new_default_player();
	add_player(ts_server, new_default_player());
	*/
	print_server(ts_server);
}

static void init_callbacks()
{
	bzero(f0_callbacks, sizeof(void *(char*, unsigned int, struct sockaddr_in *, unsigned int)));
	f0_callbacks[0x2c] = &c_req_leave;	/* client wants to leave */
	//f4be0300
	/* callbacks[0] = myfunc1; ... */
}

void handle_connection_type_packet(char * data, int len, struct sockaddr_in * cli_addr, unsigned int cli_len)
{

	printf("(II) Packet : Connection.\n");
	switch( ((uint16_t *)data)[1] ) {
		/* Client requesting a connection */
		case 3:
			handle_player_connect(data, len, cli_addr, cli_len);
			break;
		case 1:
			handle_player_keepalive(data, len, cli_addr, cli_len);
			break;
		default:
			printf("(WW) Unknown connection packet : 0xf4be%x.\n", ((uint16_t *)data)[1]);

	}
}

packet_function get_f0_function(unsigned char * code)
{
	/* Function packets */
	if(code[3] == 0) { /* 0 = server packet, 1 = client packet*/
		switch(code[2]) {
			case 0x05:
				return &c_req_chans;
			case 0xc9:
			case 0xd1:
			default:
				return NULL;
		}
	} else {
		if(f0_callbacks[code[2]])
			return f0_callbacks[code[2]];
		else
			return NULL;
	}
}


#define ERROR_IF(cond) \
	if(cond) { \
		printf("(EE) %s", strerror(errno)); \
		exit(1); \
	}

#define WARNING_IF(cond) \
	if(cond) { \
		printf("(WW) %s", strerror(errno)); \
	}

void handle_control_type_packet(char * data, int len, struct sockaddr_in * cli_addr, unsigned int cli_len)
{
	packet_function func;
	uint8_t code[4] = {0,0,0,0};

	printf("(II) Packet : Control.\n");
	/* Valid code (no overflow) */
	memcpy(code, data, MIN(4, len));
	func = get_f0_function(code);
	if(func != NULL) {
		/* Execute */
		(*func)(data, len, cli_addr, cli_len);
	} else {
		printf("(WW) Function with code : 0x%"PRIx32" is invalid or is not implemented yet.\n", *(uint32_t *)code);
	}
}

void handle_ack_type_packet(char * data, int len, struct sockaddr_in * cli_addr, unsigned int cli_len)
{
	printf("(II) Packet : ACK.\n");
}

void handle_data_type_packet(char * data, int len, struct sockaddr_in * cli_addr, unsigned int cli_len)
{
	int res;
	printf("(II) Packet : Audio data.\n");
	res = audio_received(data, len);
	printf("(II) Return value : %i.\n", res);
}


/* Manage an incoming packet */
void handle_packet(char * data, int len, struct sockaddr_in * cli_addr, unsigned int cli_len)
{
	/* first a few tests */
	switch( ((uint16_t *)data)[0] ) {
		case 0xbef0:		/* commands */
			handle_control_type_packet(data, len, cli_addr, cli_len);
			break;
		case 0xbef1:		/* acknowledge */
			handle_ack_type_packet(data, len, cli_addr, cli_len);
			break;
		case 0xbef2: 		/* audio data */
			handle_data_type_packet(data, len, cli_addr, cli_len);
			break;
		case 0xbef4:		/* connection and keepalives */
			handle_connection_type_packet(data, len, cli_addr, cli_len);
			break;
		default:
			printf("(WW) Unvalid packet type field : 0x%x.\n", ((uint16_t *)data)[0]);
	}
}


int main()
{
	int rc, n;
	unsigned int cli_len;
	struct sockaddr_in serv_addr, cli_addr;
	char data[MAX_MSG];
	struct pollfd socket_poll;
	int pollres;

	/* socket creation */
	socket_desc = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_IF(socket_desc < 0);

	/* bind local server port */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(8767);
	rc = bind(socket_desc, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	ERROR_IF(rc < 0);
	/* initialize for polling */
	socket_poll.fd = socket_desc;
	socket_poll.events = POLLIN;
	socket_poll.revents = 0;

	/* do some initialization of the finite state machine */
	init_callbacks();
	test_init_server();

	/* main loop */
	while(1) {
		/* infinite timeout, wait for data to be available */
		pollres = poll(&socket_poll, 1, -1);
		switch(pollres) {
			case 0:
				printf("(EE) Time limit expired\n");
				break;
			case -1:
				printf("(EE) Error occured while polling : %s\n", strerror(errno));
				break;
			default:
				cli_len = sizeof(cli_addr);
				n = recvfrom(socket_desc, data, MAX_MSG, 0,
						(struct sockaddr *) &cli_addr, &cli_len);
				if(n == -1) {
					fprintf(stderr, "(EE) %s\n", strerror(errno));
				} else {
					printf("(II) %i bytes received.\n", n);
					handle_packet(data, n, &cli_addr, cli_len);
				}
		}

	}
	/* exit */
}
