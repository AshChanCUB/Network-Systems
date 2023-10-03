#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define MAXFILENAME 256

void error(char *msg) {
    perror(msg);
    exit(1);
}

void receiveFile(int sockfd, char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    char buffer[BUFSIZE];
    ssize_t bytes_received;

    while (1) {
        bzero(buffer, BUFSIZE);
        bytes_received = recv(sockfd, buffer, BUFSIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("Received file: %s\n", filename);
}

int main(int argc, char *argv[]) {
    int sockfd;
    int portno;
    struct sockaddr_in serveraddr;
    char buffer[BUFSIZE];
    int n;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(1);
    }

    char *hostname = argv[1];
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(1);
    }

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR connecting");

    while (1) {
        printf("Enter command: ");
        bzero(buffer, BUFSIZE);
        fgets(buffer, BUFSIZE, stdin);

        n = write(sockfd, buffer, strlen(buffer));
        if (n < 0)
            error("ERROR writing to socket");

        if (strcmp(buffer, "exit\n") == 0) {
            printf("Client is exiting.\n");
            break;
        } else if (strncmp(buffer, "get ", 4) == 0) {
            char filename[MAXFILENAME];
            sscanf(buffer, "get %s", filename);
            receiveFile(sockfd, filename);
        } else {
            bzero(buffer, BUFSIZE);
            n = read(sockfd, buffer, BUFSIZE);
            if (n < 0)
                error("ERROR reading from socket");
            printf("%s", buffer);
        }
    }

    close(sockfd);
    return 0;
}
