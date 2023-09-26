#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h> // Include the pthread library

#define BUFSIZE 1024
#define MAXFILENAME 256

void error(char *msg) {
    perror(msg);
    exit(1);
}

// Function to receive a file from the server
void receiveFile(int sockfd, struct sockaddr_in serveraddr, socklen_t serverlen, char *filename) {
    FILE *received_file = fopen(filename, "wb");
    if (received_file == NULL) {
        perror("Error opening file for writing");
        return;
    }

    char buffer[BUFSIZE];
    int n;

    while (1) {
        bzero(buffer, BUFSIZE);
        n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
        if (n <= 0) {
            break;
        }
        if (strcmp(buffer, "END\n") == 0) {
            break; // End of file transfer
        }
        fwrite(buffer, 1, n, received_file);
    }

    fclose(received_file);
    printf("Received file: %s\n", filename);
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

        if (strcmp(buffer, "exit\n") == 0) {
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

            // Create a thread to receive the file while the main loop continues
            pthread_t thread;
            pthread_create(&thread, NULL, (void *(*)(void *))receiveFile, (void *) (intptr_t) sockfd);
            pthread_detach(thread); // Detach the thread to avoid resource leak

            // Note: The main loop will continue to execute and accept user input
        } else {
            // Send other commands to the server
            n = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&serveraddr, serverlen);
            if (n < 0)
                error("ERROR sending command to server");

            // Handle responses from the server
            while (1) {
                bzero(buffer, BUFSIZE);
                n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                if (n <= 0) {
                    break;
                }
                if (strcmp(buffer, "END\n") == 0) {
                    break; // End of response
                }
                printf("%s", buffer);
            }
        }
    }

    close(sockfd);
    return 0;
}
