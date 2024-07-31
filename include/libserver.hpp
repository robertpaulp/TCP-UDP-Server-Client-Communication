#ifndef __LIB_SERVER_H__
#define __LIB_SERVER_H__

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <math.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <map>
#include <vector>
#include <string>
#include <string.h>

#define MAX_BUFF_SIZE 1024
#define MAX_UDP_TOPIC_NAME_SIZE 50
#define MAX_UDP_DATA_SIZE 1501
#define MAX_UDP_PACKET_SIZE 1552
#define MAX_CONNECTIONS INT16_MAX
#define CLIENT_ID_SIZE 10

struct client_id_table {
    char id[10];
    int socket_fd;
};

struct data_packet {
    uint16_t len;
    char buff[MAX_BUFF_SIZE + 1];
};

struct udp_topic {
    char topic_nm[MAX_UDP_TOPIC_NAME_SIZE];
    uint8_t type;
    char data[MAX_UDP_DATA_SIZE];
};

bool search_client_id_table(struct client_id_table *table, int len, char *id);
void add_client_id_table_entry(struct client_id_table *table, int *len, char *id, int socketfd);
char *get_client_id_table_entry(struct client_id_table *table, int len, int socketfd);
void remove_client_id_table_entry(struct client_id_table *table, int *len, int socketfd);

bool wildcard_pattern_match(char *text, char *pattern);

#endif