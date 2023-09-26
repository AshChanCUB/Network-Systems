#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h> // Include for boolean data type

#define BUFSIZE 1024
#define MAXFILENAME 256

void error(char *msg) {
    perror(msg);
    exit(1);
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

            // Receive the server's response (the list of files)
            while (1) {
                bzero(buffer, BUFSIZE);
                n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                if (n <= 0) {
                    break;
                }
                if (strcmp(buffer, "END\n") == 0) {
                    break; // End of response
                }
                strcat(response, buffer);
            }

            // Print the list of files received from the server
            printf("List of files on the server:\n%s", response);
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

            // Receive and save the file from the server
            FILE *received_file = fopen(filename, "wb");
            if (received_file == NULL) {
                perror("Error opening file for writing");
            } else {
                // Receive and write the file data from the server
                while (1) {
                    bzero(buffer, BUFSIZE);
                    n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                    if (n <= 0) {
                        break;
                    }
                    fwrite(buffer, 1, n, received_file);
                }
                fclose(received_file);
                
                // Check for a success message from the server
                bzero(buffer, BUFSIZE);
                n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &serverlen);
                if (n <= 0) {
                    error("ERROR receiving success message from server");
                } else {
                    printf("Received file: %s\n", filename);
                    printf("%s", buffer); // Print the success message
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
