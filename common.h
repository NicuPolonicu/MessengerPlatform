#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024

struct chat_packet {
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};


#define MAX_TOPIC_SIZE 50
#define MAX_CONTENT_SIZE 1500
#define CLIENT_ID_SIZE 100
#define MAX_CLIENTS 50
#define BUF_SIZE 2048
#define NON_PAYLOAD_DATA 70
#define MAX_TOPICS 100
#define MAX_ID_SIZE 20

#define SUBSCRIBE 1
#define UNSUBSCRIBE 2
#define EXIT 3

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

typedef struct {
    char topic[MAX_TOPIC_SIZE];
    uint8_t data_type;
    // IP of the UDP client
    char ip[16];
    // Port of the UDP client
    uint16_t port;
    char content[MAX_CONTENT_SIZE + 1];
} TCPSubMessage;

typedef struct {
    uint8_t command;
    char topic[MAX_TOPIC_SIZE];
} TCPMessage;

typedef struct {
    int sockfd;
    struct sockaddr_in address;
    char client_id[MAX_ID_SIZE];
    char subscribed_topics[MAX_TOPICS][MAX_TOPIC_SIZE + 1];
    //int max_topics;
    int num_topics;
    //int max_topics;
} Client;

#endif
