#include "download.h"

int parse(char *parsing, struct URL *url) {
    if (sscanf(parsing, "ftp://%[^:]:%[^@]@%[^/]/%s", 
               url->user, 
               url->password, 
               url->host,
               url->resource
               ) == 4) {
    } else if (sscanf(parsing, "ftp://%[^/]/%s", 
                      url->host, 
                      url->resource 
                      ) == 2) {
                        strcpy(url->user, "anonymous");
                        strcpy(url->password, "anonymous");
    } else {
        return -1;
    }

    char *last = strrchr(url->resource, '/');
    if (last != NULL) {
        strcpy(url->filename, last + 1);
    } else {
        strcpy(url->filename, url->resource);
    }

    return 0;
}

int getSocket() {
    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    return sockfd;
}

int closeSocket(int sockfd) {
    if (close(sockfd)<0) {
        perror("close()");
        return -1;
    }   

    return 0;
}

int closeConnectionToServer(int sockfd) {
    if (sendToServer(sockfd, "quit\r\n")) return -1;

    char response[BUFFER_SIZE];
    if (responseCode(sockfd, response) != SERVER_QUIT) return -1;

    return 0;
}

void handlingServerAddr(struct sockaddr_in *server_addr, char *server_address, int server_port)  {
    bzero((char *) server_addr, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = inet_addr(server_address);  
    server_addr->sin_port = htons(server_port);                  
}

int connectToServer(int sockfd, enum Socket_type type, char *server_address, int server_port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    
    handlingServerAddr(&server_addr, server_address, server_port);
    
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        return -1;
    }

    if (type == controlo) {
        char response[BUFFER_SIZE];
        if (responseCode(sockfd, response) != SERVER_WELCOME) return -1;
    }
   
    return 0;
}

int authToServer(int sockfd, char *user, char *password) {
    char response[BUFFER_SIZE];
    char userCommand[5+strlen(user)+2]; 
    sprintf(userCommand, "USER %s\r\n", user);
 
    if (sendToServer(sockfd, userCommand)) return -1;

    if (responseCode(sockfd, response) != SERVER_PASSWORD) return -1;

    char passwordCommand[5+strlen(password)+2]; 
    sprintf(passwordCommand, "PASS %s\r\n", password);

    if (sendToServer(sockfd, passwordCommand)) return -1;
    if (responseCode(sockfd, response) != SERVER_LOGIN_SUCCESSFUL) return -1;

    return 0;
}

int sendToServer(int sockfd, char *buf) {
    size_t bytes;
    bytes = write(sockfd, buf, strlen(buf));

    if (bytes <= 0) {
        perror("write()");
        return -1;
    }

    return 0;
}

int passiveMode(int sockfd, char *serverIP, int *port) {
    char response[BUFFER_SIZE];
    if (sendToServer(sockfd, "pasv\r\n")) return -1;
    if (responseCode(sockfd, response) != SERVER_PASSIVE_MODE) return -1;
    if (parsePasvResponse(response, serverIP, port)) return -1;

    return 0;
}

int responseCode(int sockfd, char *response) {
        char serverResponse[BUFFER_SIZE], responseByte;
        ssize_t responseBytes = 0, byte;
        enum State state = START;

        while (state != END) {
            byte = recv(sockfd, &responseByte, 1, 0);
            if (byte < 0) {
                perror("Error receiving response");
                return -1;
            }
            serverResponse[responseBytes++] = responseByte;
    
            switch (state) {
                case START:
                    if (responseByte == ' ') {
                        state = RESPONSE;
                    } else if (responseByte == '-') {
                        state = RESPONSES;
                    } else if (responseByte == '\n') {
                        state = END;
                    }

                    break;
                case RESPONSE:
                    if (responseByte == '\n') {
                        state = END;
                    }

                    break;
                case RESPONSES:
                    if (responseByte == '\n') {
                        state = START;
                        memset(serverResponse, 0, sizeof(serverResponse));
                        responseBytes = 0;
                    }

                    break;
                case END:
                    break;

                default:
                    return -1;
            }
        }
        
        serverResponse[responseBytes] = '\0';
        strcpy(response, serverResponse);
        printf("%s", response);
    

    int responseCode;
    sscanf(response, "%3d", &responseCode);

    return responseCode;
}

int parsePasvResponse(char *response, char *serverIP, int *port) {
    unsigned int ip1, ip2, ip3, ip4, port1, port2;
    
    sscanf(response, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).",
           &ip1, &ip2, &ip3, &ip4, &port1, &port2);

    sprintf(serverIP, "%u.%u.%u.%u", ip1, ip2, ip3, ip4);
    *port = port1*256 + port2;
  
    return 0;
}

int getHostEntry(char *host, struct hostent **h) {
    if ((*h = gethostbyname(host)) == NULL) {
        herror("gethostbyname()");
        return -1;
    }

    return 0;
}

int getIP(char *host, char *ip) {
    struct hostent *h = NULL;
 
    if (getHostEntry(host, &h)) return -1;

    strcpy(ip, inet_ntoa(*(struct in_addr *) h->h_addr));

    return 0;
}

int requestFileTransfer(int sockfd, char* resource) {
    char response[BUFFER_SIZE];
    char FileTransferCommand[5+strlen(resource)+2];
    sprintf(FileTransferCommand, "retr %s\r\n", resource);

    if (sendToServer(sockfd, FileTransferCommand)) return -1;

    int code = responseCode(sockfd, response);
    if (code != SERVER_DATA_CONNECTION && code != SERVER_DATA_CONNECTION_OPENED) return -1;

    return 0;
}

int getFileTransfer(int sockfd1, int sockfd2, char *filename) {
    char response[BUFFER_SIZE], data[BUFFER_SIZE];
    ssize_t bytes;

    FILE *fd = fopen(filename, "wb");
    if (fd == NULL) return -1;

    while ((bytes = recv(sockfd2, data, sizeof(data), 0)) > 0) {
        if (fwrite(data, 1, bytes, fd) != bytes) return -1;
    }

    if (bytes < 0) return -1;

    fclose(fd);
    
    if (responseCode(sockfd1, response) != SERVER_TRANSFER_COMPLETE) return -1;

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    int sockA, sockB;
    struct URL url;
    memset(&url, 0, sizeof(url));

    if (parse(argv[1], &url)) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    if (getIP(url.host, url.ip)) {
        fprintf(stderr, "Erro ao obter o IP");
        exit(-1);
    }

    if ((sockA = getSocket()) == -1) {
        fprintf(stderr, "Erro ao obter o socket A");
        exit(-1);
    }
    
    if ((sockB = getSocket()) == -1) {
        fprintf(stderr, "Erro ao obter o socket B");
        exit(-1);
    }
    
    if (connectToServer(sockA, controlo, url.ip, SERVER_FTP_PORT)) {
        fprintf(stderr, "Erro ao conectar com o servidor A");
        exit(-1);
    }

    if (authToServer(sockA, url.user, url.password)) {
        fprintf(stderr, "Erro ao autenticar no servidor");
        exit(-1);
    }

    char serverIP[MAX_SIZE];
    int port;
    if (passiveMode(sockA, serverIP, &port)) {
        fprintf(stderr, "Erro ao entrar em modo passivo");
        exit(-1);
    }

    if (connectToServer(sockB, dados, serverIP, port)) { 
        fprintf(stderr, "Erro ao conectar no servidor B");
        exit(-1);
    }
 
    if (requestFileTransfer(sockA, url.resource)) {
        fprintf(stderr, "Erro ao pedir a transferencia do ficheiro");
        exit(-1);
    }

    if (getFileTransfer(sockA, sockB, url.filename)) {
        fprintf(stderr, "Erro ao transferir o ficheiro");
        exit(-1);
    }

    if (closeConnectionToServer(sockA)) {
        fprintf(stderr, "Erro ao encerrar a conexão com o servidor");
        exit(-1);
    }

    if (closeSocket(sockA)) {
        fprintf(stderr, "Erro ao encerrar o socket A");
        exit(-1);
    }

    if (closeSocket(sockB)) {
        fprintf(stderr, "Erro ao encerrar o socket B");
        exit(-1);
    }
    
    return 0;
}
