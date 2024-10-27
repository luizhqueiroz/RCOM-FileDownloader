#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define MAX_SIZE 512

#define SERVER_FTP_PORT 21

#define SERVER_DATA_CONNECTION_OPENED   125
#define SERVER_DATA_CONNECTION          150
#define SERVER_WELCOME                  220
#define SERVER_QUIT                     221
#define SERVER_TRANSFER_COMPLETE        226
#define SERVER_PASSIVE_MODE             227
#define SERVER_LOGIN_SUCCESSFUL         230
#define SERVER_PASSWORD                 331

enum Socket_type {
    controlo,
    dados,
};

enum State {
    START,
    RESPONSE,
    RESPONSES,
    END,
};

struct URL {
    char user[MAX_SIZE]; 
    char password[MAX_SIZE];
    char host[MAX_SIZE]; 
    char resource[MAX_SIZE];
    char filename[MAX_SIZE];      
    char ip[MAX_SIZE];        
};


int parse(char *parsing, struct URL *url);

int getSocket();

int closeSocket(int sockfd);

int closeConnectionToServer(int sockfd);

void handlingServerAddr(struct sockaddr_in *server_addr, char *server_address, int server_port);

int connectToServer(int sockfd, enum Socket_type type, char *server_address, int server_port);

int authToServer(int sockfd, char *user, char *password);

int sendToServer(int sockfd, char *buf);

int passiveMode(int sockfd, char *serverIP, int *port);

int responseCode(int sockfd, char *response);

int parsePasvResponse(char *response, char *serverIP, int *port);

int getHostEntry(char *host, struct hostent **h);

int getIP(char *host, char *ip);

int requestFileTransfer(int sockfd, char* resource);

int getFileTransfer(int sockfd1, int sockfd2, char *filename);
