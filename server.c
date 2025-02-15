#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <poll.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <math.h>

#include "common.h"
#include "utils.h"

Client *clients;
struct pollfd *fds;
Client **refs;
int num_clients = 3;
int total_clients = 0;
int max_clients = 5;


// Add a new client, or reconnect an old client if he already connected once

void add_new_client(int sockfd, struct sockaddr_in addr, char* client_id, int index) {

    for (int i = 0; i < total_clients; i++) {
        if (strcmp(clients[i].client_id, client_id) == 0) {
            refs[index] = &clients[i];
            return;
        }
    }

    if (total_clients == max_clients) {
        max_clients = 2 * max_clients;
        fds = (struct pollfd *)realloc(fds, max_clients * sizeof(struct pollfd));
        DIE(fds == NULL, "Fds realloc issue");
        refs = (Client **)realloc(refs, max_clients * sizeof(Client *));
        DIE(refs == NULL, "Refs realloc issue");
        clients = (Client *)realloc(clients, max_clients * sizeof(Client));
        DIE(clients == NULL, "Clients realloc issue");
        DIE((poll(fds, max_clients, -1) < 0), "Poll error");
    }

    clients[total_clients].sockfd = sockfd;
    clients[total_clients].address = addr;
    strcpy(clients[total_clients].client_id, client_id);
    for(int i = 0; i < MAX_TOPICS; i++) {
        memset(clients[total_clients].subscribed_topics[i], 0, MAX_TOPIC_SIZE + 1);
    }
    clients[total_clients].num_topics = 0;
    refs[index] = &clients[total_clients];

    total_clients++;
}

// Close connection to a client

void close_client(int index) {
    if (index < 0 || index > num_clients) return;

    close(refs[index]->sockfd);

    printf("Client %s disconnected.\n", refs[index]->client_id);

    for (int i = index; i < num_clients - 1; i++) {
        fds[i] = fds[i + 1];
        refs[i] = refs[i + 1];
    }

    num_clients--;
}

// Add the topic to client's subscriptions

void add_subscription(Client *client, char* topic) {
    
    topic[50] = '\0';

    for (int i = 0; i < client->num_topics - 1; i++) {
        if (!strcmp(client->subscribed_topics[i], topic)) {
            printf("Client %s already subscribed to %s\n", client->client_id, topic);
            break;
        }
    }
    strcpy(client->subscribed_topics[client->num_topics], topic);
    if (client->subscribed_topics[client->num_topics][49] != 0) {
        client->subscribed_topics[client->num_topics][50] = '\0';
    }
    client->num_topics++;
}

// Remove the topic from client's subscriptions

void remove_subscription(Client *client, const char* topic) {
    for (int i = 0; i < client->num_topics; i++) {
        if(strcmp(client->subscribed_topics[i], topic) == 0) {
            for (int j = i; j < client->num_topics - 1; j++) {
                strcpy(client->subscribed_topics[j], client->subscribed_topics[j + 1]);
            }
            client->num_topics--;
            return;
        }
    }
}

// Process command of client

void process_client_command(Client *client, TCPMessage *msg, int index) {

    switch (msg->command) {
        case SUBSCRIBE:
            add_subscription(client, msg->topic);
            break;
        case UNSUBSCRIBE:
            remove_subscription(client, msg->topic);
            break;
        case EXIT:
            close_client(index);
            break;
        default:
            printf("Invalid command\n");
    }
}


// Split a topic into its parts (between /'s). Returns number of parts of topic
// e.g. split_topic(a/b/c/*/d, parts) == 5

int split_topic(const char* topic, char parts[][50]) {
    int count = 0;
    const char* start = topic;
    while (*start) {
        char* end = strchr(start, '/');
        if (end == NULL) {
            strcpy(parts[count++], start);
            break;
        }
        memcpy(parts[count], start, end - start);
        parts[count][end - start] = '\0';
        count++;
        start = end + 1;
    }
    return count;
}

// Compares two topics, considering wildcards in the subscribed topic

bool match_topic(const char* sub_topic, const char* msg_topic) {
    char sub_parts[10][50], msg_parts[10][50];
    int sub_parts_count = split_topic(sub_topic, sub_parts);
    int msg_parts_count = split_topic(msg_topic, msg_parts);

    if (sub_parts_count == 0 || msg_parts_count == 0) return false;

    int i = 0, j = 0;
    while (i < sub_parts_count && j < msg_parts_count) {
        if (strcmp(sub_parts[i], "+") == 0) {
            i++; j++;
        } else if (strcmp(sub_parts[i], "*") == 0) {
            if (i == sub_parts_count - 1) return true;
            i++;
            while (j < msg_parts_count && strcmp(sub_parts[i], msg_parts[j]) != 0) j++;
        } else if (strcmp(sub_parts[i], msg_parts[j]) != 0) {
            return false;
        } else {
            i++; j++;
        }
    }
    return i == sub_parts_count && j == msg_parts_count;
}

// Process a UDP message (stored in buffer) into a TCP packet (to store in msg)

void process_udp_packet(const char *buffer, size_t msg_len, const struct sockaddr_in *client_addr, TCPSubMessage *msg, uint16_t *len) {
    
    memset(msg, 0, sizeof(TCPSubMessage));

    // Find the length of the topic
    int topic_length = 0;
    while (topic_length < MAX_TOPIC_SIZE && buffer[topic_length] != '\0') {
        topic_length++;
    }

    // Copy the topic
    strncpy(msg->topic, buffer, MAX_TOPIC_SIZE);

    msg->topic[topic_length] = '\0';

    msg->data_type = buffer[MAX_TOPIC_SIZE];

    // Get the content
    switch (msg->data_type) {
        case INT:
            memcpy(msg->content, buffer + MAX_TOPIC_SIZE + 1, 5);
            *len = NON_PAYLOAD_DATA + 5;
            break;
        case SHORT_REAL:
            memcpy(msg->content, buffer + MAX_TOPIC_SIZE + 1, 2);
            *len = NON_PAYLOAD_DATA + 2;
            break;
        case FLOAT:
            memcpy(msg->content, buffer + MAX_TOPIC_SIZE + 1, 6);
            *len = NON_PAYLOAD_DATA + 6;
            break;
        case STRING:
            strncpy(msg->content, buffer + MAX_TOPIC_SIZE + 1, MAX_CONTENT_SIZE);
            msg->content[MAX_CONTENT_SIZE] = '\0';
            *len = NON_PAYLOAD_DATA + strlen(msg->content) + 1;
            break;
        default:
            printf("Invalid data type\n");
            return;
    }

    // Set IP and port from client address
    snprintf(msg->ip, sizeof(msg->ip), "%s", inet_ntoa(client_addr->sin_addr));
    msg->port = ntohs(client_addr->sin_port);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int tcp_sockfd, udp_sockfd;
    struct sockaddr_in server_addr;

    // Clients database (all clients, including disconnected ones)
    clients = (Client *)malloc(max_clients * sizeof(Client));
    if (clients == NULL) {
        perror("client alloc issue");
        exit(0);
    }
    
    // Client references for open connections
    refs = (Client **)malloc((max_clients + 2) * sizeof(Client *));
    if (refs == NULL) {
        perror("refs alloc issue");
        exit(0);
    }

    // Connections pollfd structs
    fds = (struct pollfd *)malloc((max_clients + 2) * sizeof(struct pollfd));
    if (fds == NULL) {
        perror("fds alloc issue");
        exit(0);
    }

    // Setup TCP socket
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setup UDP socket
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    const int enable = 1;
    if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

    if (setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");



    // General server address settings
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(atoi(argv[1]));

    // Bind TCP socket
    if (bind(tcp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Bind UDP socket
    if (bind(udp_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_sockfd, 10) < 0) {
        perror("Socket listen failed");
        exit(EXIT_FAILURE);
    }

    fds[0].fd = tcp_sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = udp_sockfd;
    fds[1].events = POLLIN;
    fds[2].fd = STDIN_FILENO;
    fds[2].events = POLLIN;

    setvbuf(stdout, NULL, _IONBF, 0);

    while (1) {

        int ret = poll(fds, max_clients + 2, -1);
        if (ret < 0) {
            perror("Poll error");
            continue;
        }

        

        // New TCP connection
        if (fds[0].revents & POLLIN) {

            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = accept(tcp_sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_fd < 0) {
                perror("Accept failed");
                continue;
            }

            int flag = 1;
            int result = setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
            if (result < 0) {
                perror("setsockopt(TCP_NODELAY) failed");
                exit(EXIT_FAILURE);
            }

            // Handle new TCP client
            // Receive client ID size and then the ID

            char client_id[MAX_ID_SIZE];
            memset(client_id, 0, MAX_ID_SIZE);
            
            // read client_id
            int bytes_read = recv_all(client_fd, client_id, MAX_ID_SIZE);
            if (bytes_read <= 0) {
                close(client_fd);
                continue;
            }

            int client_exists = 0;
            for (int i = 3; i < num_clients; i++) {
                if (strcmp(refs[i]->client_id, client_id) == 0) {
                    client_exists = 1;
                    break;
                }
            }

            if (client_exists) {
                printf("Client %s already connected.\n", client_id);
                close(client_fd);
                continue;
            }

            // Realloc arrays if full
            if (num_clients == max_clients) {
                max_clients = 2 * max_clients;
                fds = (struct pollfd *)realloc(fds, max_clients * sizeof(struct pollfd));
                DIE(fds == NULL, "Fds realloc issue");
                refs = (Client **)realloc(refs, max_clients * sizeof(Client *));
                DIE(refs == NULL, "Refs realloc issue");
                clients = (Client *)realloc(clients, max_clients * sizeof(Client));
                DIE(clients == NULL, "Clients realloc issue");
                DIE((poll(fds, max_clients, -1) < 0), "Poll error");
            }
                
            add_new_client(client_fd, client_addr, client_id, num_clients);
            fds[num_clients].fd = client_fd;
            fds[num_clients].events = POLLIN;
            num_clients++;

            printf("New client %s connected from %s:%d\n", client_id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }

        // New UDP packet
        if (fds[1].revents & POLLIN) {

            char buffer[BUF_SIZE];
            memset(buffer, 0, BUF_SIZE);

            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            
            ssize_t msg_len = recvfrom(udp_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);

            if (msg_len < 0) {
                perror("recvfrom failed");
                continue;
            }

            // Process datagram into a TCP packet

            TCPSubMessage sub_msg;
            memset(&sub_msg, 0, sizeof(TCPSubMessage));
            uint16_t len;
            process_udp_packet(buffer, msg_len, &client_addr, &sub_msg, &len);

            // Send the processed data to all subscribed TCP clients
            for (int i = 3; i < num_clients; i++) {
                for (int j = 0; j < refs[i]->num_topics; j++) {
                    if (match_topic(refs[i]->subscribed_topics[j], buffer)) {
                        DIE((send_all(refs[i]->sockfd, &len, sizeof(uint16_t)) < 0), "client len send error");
                        DIE((send_all(refs[i]->sockfd, &sub_msg, len) < 0), "client packet send error");
                        break;
                    }
                }
            }

        }

        // TCP client message
        for (int i = 3; i < num_clients; i++) {

            if (fds[i].revents & POLLIN) {

                TCPMessage message;

                int bytes_read = recv_all(fds[i].fd, &message, sizeof(TCPMessage));
                if (bytes_read <= 0) {
                    close_client(i);
                    continue;
                }

                process_client_command(refs[i], &message, i);
            }
        }

        // Process stdin for server (in case of exit command)
        if (fds[2].revents & POLLIN) {
            char command[BUF_SIZE];
            fgets(command, BUF_SIZE, stdin);
            if (strncmp(command, "exit", 4) == 0) {
                for (int i = 3; i < num_clients; i++) {
                    close_client(i);
                }
                break;
            } else {
                printf("Invalid command, only exit is accepted.\n");
            }
        }
    }
    
    free(refs);
    free(fds);
    free(clients);
    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}
