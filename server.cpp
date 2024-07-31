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

#include "libserver.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 2) {
        fprintf(stderr, "Wrong number of arguments (./server <port_number>)\n");
        return -1;
    }

    /* Create TCP and UDP sockets */
    int tcp_listensfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listensfd < 0) {
        perror("Error: tcp_socket creation");
        return -1;
    }

    int udp_listensfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_listensfd < 0) {
        perror("Error: udp_socket creation");
        return -1;
    }

    uint16_t port = atoi(argv[1]);
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    int rc;
    struct client_id_table id_table[MAX_CONNECTIONS];
    int id_table_len = 0;
    map<char *, vector<string>> topic_map;

    /* Set SO_REUSEADDR option for TCP sockets */
    const int enable = 1;
    if (setsockopt(tcp_listensfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("Error: setsockopt(SO_REUSEADDR) failed");
        return -1;
    }

    memset(&server_addr, 0, server_addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    rc = bind(tcp_listensfd, (const struct sockaddr *)&server_addr, server_addr_len);
    if (rc < 0) {
        perror("Error: tcp_socket bind");
        return -1;
    }

    rc = bind(udp_listensfd, (const struct sockaddr *)&server_addr, server_addr_len);
    if (rc < 0) {
        perror("Error: udp_socket bind");
        return -1;
    }

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_sockets = 3;

    rc = listen(tcp_listensfd, MAX_CONNECTIONS);
    if (rc < 0) {
        perror("Error: listen");
        return -1;
    }

    /* Add the sockets to the poll_fds array */
    poll_fds[0].fd = tcp_listensfd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = udp_listensfd;
    poll_fds[1].events = POLLIN;

    poll_fds[2].fd = STDIN_FILENO;
    poll_fds[2].events = POLLIN;

    while (1) {
        rc = poll(poll_fds, num_sockets, -1);
        if (rc < 0) {
            perror("Error: poll");
            return -1;
        }

        for (int i = 0; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == tcp_listensfd) { // New TCP connection

                    struct sockaddr_in client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_sfd = accept(tcp_listensfd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (client_sfd < 0) {
                        perror("Error: accept");
                        return -1;
                    }

                    const int enable = 1;
                    if (setsockopt(client_sfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0) {
                        perror("Error: setsockopt");
                        return -1;
                    }

                    poll_fds[num_sockets].fd = client_sfd;
                    poll_fds[num_sockets].events = POLLIN;
                    num_sockets++;
                } else if (poll_fds[i].fd == STDIN_FILENO) {

                    char buff[1024];
                    memset(buff, 0, sizeof(buff));
                    fgets(buff, sizeof(buff), stdin);

                    if (strcmp(buff, "exit") == 0 || strcmp(buff, "exit\n") == 0) {
                        for (int j = 3; j < num_sockets; j++) {
                            close(poll_fds[j].fd);
                        }

                        close(tcp_listensfd);
                        close(udp_listensfd);
                        return 0;
                    } else {
                        fprintf(stderr, "Only accepting exit as command\n");
                    }

                } else if (poll_fds[i].fd == udp_listensfd) { // UDP connection
                    char udp_buff[MAX_UDP_PACKET_SIZE];
                    memset(udp_buff, 0, sizeof(udp_buff));
                    int udp_rc = recvfrom(udp_listensfd, udp_buff, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr *)&server_addr, &server_addr_len);
                    if (udp_rc < 0) {
                        perror("Error: recvfrom");
                        return -1;
                    }

                    uint8_t sign, module;
                    long long data;
                    double data_double;
                    struct udp_topic *udp_topic = (struct udp_topic *)udp_buff;
                    string send_buffer;
                    send_buffer.clear();
                    send_buffer += string(inet_ntoa(server_addr.sin_addr)) + ":" + to_string(ntohs(server_addr.sin_port)) + " - " + string(udp_topic->topic_nm) + " - ";

                    switch (udp_topic->type) {
                        case 0: // INT
                            sign = udp_topic->data[0];
                            data = ntohl(*(uint32_t *)(udp_topic->data + sizeof(uint8_t)));

                            if(sign == 1)
                                data *= -1;

                            send_buffer += "INT - " + to_string(static_cast<int>(data)) + "\n";
                            for (int j = 3; j < num_sockets; j++) {
                                if (poll_fds[j].fd != tcp_listensfd && poll_fds[j].fd != udp_listensfd) {
                                    char *client_id = get_client_id_table_entry(id_table, id_table_len, poll_fds[j].fd);
                                    vector<string> &topics = topic_map[client_id];
                                    for (size_t k = 0; k < topics.size(); k++) {
                                        if (wildcard_pattern_match((char *)udp_topic->topic_nm, (char *)topics[k].c_str())) {
                                            send(poll_fds[j].fd, send_buffer.c_str(), send_buffer.size(), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                            send_buffer.clear();
                            break;
                        case 1: // DOUBLE WITH 2 DECIMALS
                            data_double = ntohs(*(uint16_t *)udp_topic->data);
                            data_double /= 100;

                            sprintf(udp_topic->data, "%.2f", data_double);

                            send_buffer += "SHORT_REAL - " + string(udp_topic->data) + "\n";

                            for (int j = 3; j < num_sockets; j++) {
                                if (poll_fds[j].fd != tcp_listensfd && poll_fds[j].fd != udp_listensfd) {
                                    char *client_id = get_client_id_table_entry(id_table, id_table_len, poll_fds[j].fd);
                                    vector<string> &topics = topic_map[client_id];
                                    for (size_t k = 0; k < topics.size(); k++) {
                                        if (wildcard_pattern_match((char *)udp_topic->topic_nm, (char *)topics[k].c_str())) {
                                            send(poll_fds[j].fd, send_buffer.c_str(), send_buffer.size(), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                            send_buffer.clear();
                            break;
                        case 2: // DOUBLE
                            sign = udp_topic->data[0];
                            data_double = ntohl(*(uint32_t *)(udp_topic->data + sizeof(uint8_t)));
                            module = (*(uint8_t *)(udp_topic->data + sizeof(uint8_t) + sizeof(uint32_t)));
                            if (module > 0)
                                data_double /= pow(10, module);
                            if (sign == 1)
                                data_double *= -1;

                            send_buffer += "FLOAT - " + to_string(static_cast<double>(data_double)) + "\n";

                            for (int j = 3; j < num_sockets; j++) {
                                if (poll_fds[j].fd != tcp_listensfd && poll_fds[j].fd != udp_listensfd) {
                                    char *client_id = get_client_id_table_entry(id_table, id_table_len, poll_fds[j].fd);
                                    vector<string> &topics = topic_map[client_id];
                                    for (size_t k = 0; k < topics.size(); k++) {
                                        if (wildcard_pattern_match((char *)udp_topic->topic_nm, (char *)topics[k].c_str())) {
                                            send(poll_fds[j].fd, send_buffer.c_str(), send_buffer.size(), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                            send_buffer.clear();
                            break;
                        case 3: // STRING
                            send_buffer += "STRING - " + string(udp_topic->data) + "\n";

                            for (int j = 3; j < num_sockets; j++) {
                                if (poll_fds[j].fd != tcp_listensfd && poll_fds[j].fd != udp_listensfd) {
                                    char *client_id = get_client_id_table_entry(id_table, id_table_len, poll_fds[j].fd);
                                    vector<string> &topics = topic_map[client_id];
                                    for (size_t k = 0; k < topics.size(); k++) {
                                        if (wildcard_pattern_match((char *)udp_topic->topic_nm, (char *)topics[k].c_str())) {
                                            send(poll_fds[j].fd, send_buffer.c_str(), send_buffer.size(), 0);
                                            break;
                                        }
                                    }
                                }
                            }
                            send_buffer.clear();
                            break;
                        default:
                            fprintf(stderr, "Unknown type: %d\n", udp_topic->type);
                            break;
                    }
                
                } else { // Existing connection
                    char existing_conn_buff[MAX_UDP_PACKET_SIZE];
                    memset(existing_conn_buff, 0, sizeof(existing_conn_buff));

                    int exist_conn_rc = recv(poll_fds[i].fd, existing_conn_buff, sizeof(existing_conn_buff), 0);
                    if (exist_conn_rc < 0) {
                        perror("Error: recv_packet");
                        return -1;
                    }

                    if (exist_conn_rc == 0) {
                        printf("Client %s disconnected.\n", get_client_id_table_entry(id_table, id_table_len, poll_fds[i].fd));
                        remove_client_id_table_entry(id_table, &id_table_len, poll_fds[i].fd);

                        close(poll_fds[i].fd);

                        for (int j = i; j < num_sockets - 1; j++) {
                            poll_fds[j] = poll_fds[j + 1];
                        }
                        num_sockets--;
                    } else {
						
						if (strstr(existing_conn_buff, "ID:") != NULL) {
							char *client_id = strtok(existing_conn_buff, " ");
							client_id = strtok(NULL, " ");
							char *client_ip = strtok(NULL, " ");
							char *client_port = strtok(NULL, " ");

							if (client_id == NULL || client_ip == NULL || client_port == NULL) {
								fprintf(stderr, "Invalid ID format\n");
								close(poll_fds[i].fd);

								for (int j = i; j < num_sockets - 1; j++) {
									poll_fds[j] = poll_fds[j + 1];
								}
								
								num_sockets--;
							}

							if (search_client_id_table(id_table, id_table_len, client_id)) {
                        		printf("Client %s already connected.\n", client_id);
								close(poll_fds[i].fd);

								for (int j = i; j < num_sockets - 1; j++) {
									poll_fds[j] = poll_fds[j + 1];
								}
                        		
								num_sockets--;
							} else {
								add_client_id_table_entry(id_table, &id_table_len, client_id, poll_fds[i].fd);
								printf("New client %s connected from %s:%s.\n", client_id, client_ip, client_port);
							}
						}


                        char *client_id = get_client_id_table_entry(id_table, id_table_len, poll_fds[i].fd);
                        char *token = strstr(existing_conn_buff, " ");
                        if (token != NULL) {
                        token[strlen(token) - 1] = '\0';
                        }

                        if (strstr(existing_conn_buff, "unsubscribe") != NULL) {
                            string topic = string(token + 1);
        
                            for (size_t j = 0; j < topic_map[client_id].size(); j++) {
                                if (topic_map[client_id][j] == topic) {
                                    topic_map[client_id].erase(topic_map[client_id].begin() + j);
                                    break;
                                }
                            }

                        } else if (strstr(existing_conn_buff, "subscribe") != NULL) {
                            string topic = string(token + 1);
                            topic_map[client_id].push_back(topic);
                        } else {
                            fprintf(stderr, "Command received %s. ", existing_conn_buff);
                            fprintf(stderr, "Only accepting <_> from client : subscribe <topic> or unsubscribe <topic>\n");
                        }
                    }
                }
            }
        }
    }

    close(tcp_listensfd);
    close(udp_listensfd);

    return 0;
}