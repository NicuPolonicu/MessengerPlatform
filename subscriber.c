#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <math.h>
#include <errno.h>

#include "common.h"
#include "utils.h"

void subscribe(int sockfd, char *topic) {

    TCPMessage message;
    message.command = SUBSCRIBE;
    strcpy(message.topic, topic);

    send_all(sockfd, &message, sizeof(TCPMessage));

    printf("Subscribed to topic %s\n", topic);
}

void unsubscribe(int sockfd, char *topic) {

    TCPMessage message;
    message.command = UNSUBSCRIBE;
    strcpy(message.topic, topic);

    send_all(sockfd, &message, sizeof(TCPMessage));

    printf("Unsubscribed from topic %s\n", topic);
}

void handle_input(int sockfd) {

    char buffer[BUF_SIZE];

    if (fgets(buffer, BUF_SIZE, stdin)) {

        // Remove newline character from buffer with strcspn
        buffer[strcspn(buffer, "\n")] = 0;

        if (strncmp(buffer, "subscribe ", 10) == 0) {
            subscribe(sockfd, buffer + 10);
        } else if (strncmp(buffer, "unsubscribe ", 12) == 0) {
            unsubscribe(sockfd, buffer + 12);
        } else if (strcmp(buffer, "exit") == 0) {
            TCPMessage message;
            message.command = EXIT;
            send_all(sockfd, &message, sizeof(TCPMessage));
            exit(0);
        } else {
            printf("Invalid command.\n");
        }
    }
}

void display_message(TCPSubMessage *msg) {

    printf("%s:%d - %s - ", msg->ip, msg->port, msg->topic);

    switch (msg->data_type) {
        case INT: {

            uint8_t sign = msg->content[0];
            int value = 0;

            memcpy(&value, msg->content+1, 4);

            value = ntohl(value);

            if(value == 0) {
                printf("INT - 0\n");
                break;
            }

            printf("INT - %s%d\n", sign ? "-" : "", value);
            break;
        }
        case SHORT_REAL: {

            float value = ntohs(*((u_int16_t *)(msg->content))) / 100.0;

            printf("SHORT_REAL - %.2f\n", value);

            break;
        }
        case FLOAT: {
            
            unsigned int sign = msg->content[0];
            u_int32_t number = 0;

            memcpy(&number, msg->content+1, 4);

            char exponent = msg->content[5];

            u_int32_t mantissa = ntohl(number);
            float real_value = mantissa * pow(10, -exponent);

            printf("FLOAT - %s%.4f\n", sign ? "-" : "", real_value);
            break;
        }
        case STRING: {
            printf("STRING - %s\n", msg->content);
            break;
        }
        default:
            printf("Unknown data type\n");
    }
}


int main(int argc, char *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
        exit(1);
    }
    
    setvbuf(stdout, NULL, _IONBF, 0);

    int sockfd;
    struct sockaddr_in serv_addr;
    char *ip_server = argv[2];
    int port_server = atoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_server);
    if (inet_pton(AF_INET, ip_server, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }

    int flag = 1;
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
    if (result < 0) {
      perror("setsockopt(TCP_NODELAY) failed");
      exit(EXIT_FAILURE); 
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    
    char id[MAX_ID_SIZE];
    strcpy(id, argv[1]);

    send_all(sockfd, id, MAX_ID_SIZE);

    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("Poll error");
            continue;
        }

        if (fds[0].revents & POLLIN) {
            handle_input(sockfd);
        }

        if (fds[1].revents & POLLIN) {

            uint16_t len;

            int rc = recv_all(sockfd, &len, sizeof(uint16_t));
            if (rc <= 0) {
                break;
            }

            TCPSubMessage msg;
            memset(&msg, 0, sizeof(TCPSubMessage));

            DIE((recv_all(sockfd, &msg, len) <= 0), "recv error / conn closed");

            display_message(&msg);
        }
    }

    close(sockfd);
    return 0;
}
