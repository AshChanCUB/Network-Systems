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

void receiveFile(int sockfd, struct sockaddr_in serveraddr, socklen_t serverlen, char *filename) {
    FILE *received_file = fopen(filename, "wb");
    if (received_file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    char buffer[BUFSIZE];
    ssize_t bytes_received;

    while (1) {
        bzero(buffer, BUFSIZE);
        bytes_received = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (bytes_received <= 0) {
            break;
        }
        if (strcmp(buffer, "END\n") == 0) {
            break; // End of file
        }
        fwrite(buffer, 1, bytes_received, received_file);
    }

    fclose(received_file);
    printf("Received file: %s\n", filename);
}

int main(int argc, char *argv[]) {
    int sockfd;
    int portno;
    struct sockaddr_in serveraddr;
    socklen_t serverlen;
    char buffer[BUFSIZE];
    int n;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(1);
    }

    char *hostname = argv[1];
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(1);
    }

    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serverlen = sizeof(serveraddr);

    while (1) {
        // Get user command
        printf("Enter command: ");
        bzero(buffer, BUFSIZE);
        fgets(buffer, BUFSIZE, stdin);

        // Send the user command to the server
        n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0)
            error("ERROR sending command to server");

        // Handle the server's response
        if (strncmp(buffer, "get ", 4) == 0) {
            // Handle the "get" command
            char filename[MAXFILENAME];
            sscanf(buffer, "get %s", filename);

            // Receive and save the file from the server
            receiveFile(sockfd, serveraddr, serverlen, filename);
        } else if (strcmp(buffer, "exit\n") == 0) {
            // Handle the "exit" command
            printf("Client is exiting.\n");
            break;
        } else {
            // Receive and print the server's response
            bzero(buffer, BUFSIZE);
            n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
            if (n < 0)
                error("ERROR receiving response from server");
            printf("Server response:\n%s", buffer);
        }
    }

    close(sockfd);
    return 0;
}
