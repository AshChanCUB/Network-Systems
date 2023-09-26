#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <fcntl.h> // Added for file operations

#define BUFSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char filename[BUFSIZE]; // Added for file upload
    FILE *file; // Added for file upload

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
       exit(1);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(1);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    while (1) {
        /* Prompt the user to enter a command or file upload */
        bzero(buf, BUFSIZE);
        printf("Enter command: ");
        fgets(buf, BUFSIZE, stdin);
        buf[strcspn(buf, "\n")] = '\0';

        if (strncmp(buf, "put ", 4) == 0) {
            // Handle file upload (put) command
            sscanf(buf, "put %s", filename);

            // Open the file for reading
            file = fopen(filename, "rb");
            if (file == NULL) {
                perror("Error opening file");
                continue; // Return to command prompt
            }

            // Send the "put" command to the server
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) 
                error("ERROR in sendto");

            // Read and send the file content to the server
            while ((n = fread(buf, 1, BUFSIZE, file)) > 0) {
                sendto(sockfd, buf, n, 0, (struct sockaddr*)&serveraddr, serverlen);
            }

            // Close the file
            fclose(file);

            // Notify the user
            printf("File '%s' uploaded successfully.\n", filename);
        } else if (strcmp(buf, "exit") == 0) {
            // Handle the "exit" command
            printf("Exiting...\n");
            close(sockfd);
            exit(0);
        } else {
            // Send other commands to the server
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr*)&serveraddr, serverlen);
            if (n < 0) 
                error("ERROR in sendto");

            /* Receive and print the server's response */
            bzero(buf, BUFSIZE);
            n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr*)&serveraddr, &serverlen);
            if (n < 0) 
                error("ERROR in recvfrom");

            printf("Server response:\n%s\n", buf);
        }
    }

    close(sockfd);
    return 0;
}
