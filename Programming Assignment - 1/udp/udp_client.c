#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h> // Added for errno
#include <stdbool.h> // Added for boolean type

#define BUFSIZE 1024
#define MAXFILENAME 256
#define TIMEOUT_SECONDS 10 // Adjust the timeout as needed

void error(char *msg) {
    perror(msg);
    exit(1);
}

// Function to handle receiving data with a timeout
bool receiveWithTimeout(int sockfd, struct sockaddr_in *serveraddr, socklen_t *serverlen, char *buffer, int bufsize, int timeoutSec) {
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    timeout.tv_sec = timeoutSec;
    timeout.tv_usec = 0;

    int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
        perror("Error in select");
        return false;
    } else if (ready == 0) {
        printf("Timeout: No response received from the server.\n");
        return false;
    } else {
        // Data is available to be received
        int n = recvfrom(sockfd, buffer, bufsize, 0, (struct sockaddr *)serveraddr, serverlen);
        if (n <= 0) {
            perror("Error receiving data from server");
            return false;
        }
        return true;
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    int portno;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    socklen_t serverlen;
    char *hostname;
    char buffer[BUFSIZE];
    int n;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(1);
    }

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);

    while (1) {
        // Get user command
        printf("Enter command: ");
        bzero(buffer, BUFSIZE);
        fgets(buffer, BUFSIZE, stdin);

        if (strcmp(buffer, "ls\n") == 0) {
            // Handle the "ls" command
            char response[BUFSIZE];
            bzero(response, BUFSIZE);

            // Send the "ls" command to the server
            n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR sending command to server");

            // Receive the server's response (the list of files) with a timeout
            if (receiveWithTimeout(sockfd, &serveraddr, &serverlen, response, BUFSIZE, TIMEOUT_SECONDS)) {
                if (strcmp(response, "END\n") == 0) {
                    printf("List of files on the server:\n");
                } else {
                    printf("List of files on the server:\n%s", response);
                }
            }
        } else if (strcmp(buffer, "exit\n") == 0) {
            // Handle the "exit" command
            n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR sending command to server");
            close(sockfd);
            printf("Client is exiting.\n");
            exit(0);
        } else if (strncmp(buffer, "get ", 4) == 0) {
            // Handle the "get" command
            char filename[MAXFILENAME];
            sscanf(buffer, "get %s", filename);

            // Send the "get" command to the server
            n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR sending command to server");

            // Receive and save the file from the server with a timeout
            FILE *received_file = fopen(filename, "wb");
            if (received_file == NULL) {
                perror("Error opening file for writing");
            } else {
                bool receiveSuccess = false;
                while (1) {
                    bzero(buffer, BUFSIZE);
                    receiveSuccess = receiveWithTimeout(sockfd, &serveraddr, &serverlen, buffer, BUFSIZE, TIMEOUT_SECONDS);
                    if (!receiveSuccess) {
                        break;
                    }
                    fwrite(buffer, 1, n, received_file);
                }
                if (receiveSuccess) {
                    fclose(received_file);
                    printf("Received file: %s\n", filename);
                } else {
                    printf("Failed to receive the file: %s\n", filename);
                }
            }
        } else {
            // Send other commands to the server
            n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR sending command to server");
        }
    }

    close(sockfd);
    return 0;
}
