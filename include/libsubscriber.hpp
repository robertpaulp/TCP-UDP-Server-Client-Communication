#ifndef __LIB_SUBSCRIBER_H__
#define __LIB_SUBSCRIBER_H__

#include <stddef.h>
#include <stdint.h>

#define MAX_BUFF_SIZE 1600
#define MAX_UDP_PACKET_SIZE 1552

struct data_packet {
    uint16_t len;
    char buff[MAX_BUFF_SIZE + 1];
};

#endif