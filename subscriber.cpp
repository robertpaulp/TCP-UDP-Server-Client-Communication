#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "libsubscriber.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 4) {
        fprintf(stderr, "Wrong number of arguments (./subscriber <id_client> <ip_server> <port_server>)\n");
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error: socket creation");
        return -1;
    }

    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(struct sockaddr_in);

    memset(&server_addr, 0, server_addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[3]));
    server_addr.sin_addr.s_addr = inet_addr(argv[2]);

    int conn_rc = connect(sockfd, (struct sockaddr *)&server_addr, server_addr_len);
    if (conn_rc < 0) {
        perror("Error: connect");
        return -1;
    }

    /* Disable Nagle Algorithm */
    const int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
        perror("Error: setsockopt");
        return -1;
    }

    /* Get client port */
    struct sockaddr_in my_addr;
    socklen_t my_addr_len = sizeof(struct sockaddr_in);
    getsockname(sockfd, (struct sockaddr *)&my_addr, &my_addr_len);

    /* Send client id */
    string client_id = "ID: " + string(argv[1]) + " " + string(inet_ntoa(my_addr.sin_addr)) + " " + to_string(ntohs(my_addr.sin_port));
    int send_rc = send(sockfd, client_id.c_str(), client_id.length(), 0);
    if (send_rc < 0) {
        perror("Error: failed to send client_id");
        return -1;
    }

    char buff[MAX_BUFF_SIZE];
    memset(buff, 0, MAX_BUFF_SIZE);

    struct data_packet sent_packet;
    char received_packet[MAX_BUFF_SIZE];
    memset(received_packet, 0, MAX_BUFF_SIZE);

    struct pollfd fds[2];
    int nfds = 2;

    /* Add socket and stdin to poll */
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;


    while (1) {
        int rc_pool = poll(fds, nfds, -1);
        if (rc_pool < 0) {
            perror("Error: pool");
            return -1;
        }

        if (fds[0].revents & POLLIN) { // Received data from server
            memset(received_packet, 0, MAX_UDP_PACKET_SIZE);
            int recv_rc = recv(sockfd, received_packet, MAX_UDP_PACKET_SIZE, 0);
            if (recv_rc < 0) {
                perror("Error: failed to receive data");
                return -1;
            }

            if (recv_rc == 0) {
                break;
            }

            printf("%s", received_packet);
        } else if (fds[1].revents & POLLIN) { // Received data from stdin
            memset(buff, 0, MAX_BUFF_SIZE);
            fgets(buff, MAX_BUFF_SIZE, stdin);

            if (strcmp(buff, "exit") == 0 || strcmp(buff, "exit\n") == 0) {
                break;
            }

            sent_packet.len = strlen(buff);
            strcpy(sent_packet.buff, buff);
            int send_rc = send(sockfd, sent_packet.buff, sent_packet.len, 0);
            if (send_rc < 0) {
                perror("Error: send_packet");
            }

            if (sent_packet.buff[sent_packet.len - 1] != '\n') {
                sent_packet.buff[sent_packet.len] = '\n';
                sent_packet.len++;
            }
            char *token = strstr(sent_packet.buff, " ");
            if (strstr(sent_packet.buff, "unsubscribe") != NULL) {
                fprintf(stdout, "Unsubscribed from topic%s", token);
            } else if (strstr(sent_packet.buff, "subscribe") != NULL) {
                fprintf(stdout, "Subscribed to topic%s", token);
            } else {
                fprintf(stderr, "Commands available: subscribe <topic>, unsubscribe <topic>, exit\n");
            }
        }
    }

    close(sockfd);

    return 0;
}