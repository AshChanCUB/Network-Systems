#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFSIZE 1024
#define MAXFILENAME 256

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

// Function to send a file to the client
void sendFile(int sockfd, struct sockaddr_in clientaddr, socklen_t clientlen, char* filename) {
    // Open the file for reading
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        // File not found, send an error message
        char error_msg[] = "File not found.";
        sendto(sockfd, error_msg, strlen(error_msg), 0, (struct sockaddr*)&clientaddr, clientlen);
        return;
    }

    char buffer[BUFSIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, BUFSIZE)) > 0) {
        // Send the file data to the client
        sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr*)&clientaddr, clientlen);
    }

    // Close the file
    close(fd);
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    socklen_t clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    char buf[BUFSIZE]; /* message buf */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    /* 
     * check command line arguments 
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
             (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
           sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then process the command
     */
    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
             (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        // Determine the command and process it
        else if (strncmp(buf, "get ", 4) == 0) {
            // Handle the "get [file_name]" command
            char filename[BUFSIZE];
            sscanf(buf, "get %s", filename);
            // Check if the file exists
            if (access(filename, F_OK) != -1) {
                // Open the file for reading
                FILE *file = fopen(filename, "rb");
                if (file == NULL) {
                    perror("Error opening file");
                } else {
                    // Read and send the file to the client
                    char filebuf[BUFSIZE];
                    while (!feof(file)) {
                        size_t bytesRead = fread(filebuf, 1, BUFSIZE, file);
                        sendto(sockfd, filebuf, bytesRead, 0,
                               (struct sockaddr*)&clientaddr, clientlen);
                    }
                    fclose(file);
                }
            } else {
                // File not found, send an error message
                char response[BUFSIZE];
                snprintf(response, BUFSIZE, "File not found: %s", filename);
                sendto(sockfd, response, strlen(response), 0,
                       (struct sockaddr*)&clientaddr, clientlen);
            }
        } else if (strncmp(buf, "put ", 4) == 0) {
            // Handle the "put [file_name]" command
            char filename[BUFSIZE];
            sscanf(buf, "put %s", filename);
            // Open the file for writing
            FILE *file = fopen(filename, "wb");
            if (file == NULL) {
                perror("Error opening file");
            } else {
                // Receive and write the file data from the client
                while (1) {
                    bzero(buf, BUFSIZE);
                    n = recvfrom(sockfd, buf, BUFSIZE, 0,
                                 (struct sockaddr*)&clientaddr, &clientlen);
                    if (n <= 0) {
                        break;
                    }
                    fwrite(buf, 1, n, file);
                }
                fclose(file);
                printf("Received file: %s\n", filename);
            }
        } else if (strncmp(buf, "delete ", 7) == 0) {
            // Handle the "delete [file_name]" command
            char filename[BUFSIZE];
            sscanf(buf, "delete %s", filename);
            if (remove(filename) == 0) {
                printf("Deleted file: %s\n", filename);
            } else {
                perror("Error deleting file");
            }
        } else if (strcmp(buf, "ls") == 0) {
            // Handle the "ls" command
            // List files in the local directory and send the list to the client

            // Open the current directory
            DIR *dir;
            struct dirent *ent;
            char file_list[BUFSIZE] = "";

            if ((dir = opendir(".")) != NULL) {
                // Iterate through the directory entries and concatenate filenames
                while ((ent = readdir(dir)) != NULL) {
                    if (ent->d_type == DT_REG) {  // Check if it's a regular file
                        strcat(file_list, ent->d_name);
                        strcat(file_list, "\n");
                    }
                }
                closedir(dir);

                // Send the list of files to the client
                n = sendto(sockfd, file_list, strlen(file_list), 0, (struct sockaddr*)&clientaddr, clientlen);
                if (n < 0)
                    error("ERROR in sendto");
            } else {
                // Error opening the directory
                char response[] = "Error opening directory.";
                sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientaddr, clientlen);
            }  
        } else if (strcmp(buf, "exit") == 0) {
            // Handle the "exit" command
            // Gracefully exit the server
            printf("Server is exiting gracefully.\n");
            close(sockfd);
            exit(0);
        } else {
            // For any other commands, echo back to the client with a message
            char response[BUFSIZE];
            snprintf(response, BUFSIZE, "Unknown command: %s", buf);
            sendto(sockfd, response, strlen(response), 0,
                   (struct sockaddr*)&clientaddr, clientlen);
        }
    
    // Close the socket and exit (this part will not be reached)
    close(sockfd);
    return 0;
}
