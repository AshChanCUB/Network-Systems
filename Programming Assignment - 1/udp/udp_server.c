#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>

#define BUFSIZE 1024
#define MAXFILENAME 256

void error(char *msg) {
    perror(msg);
    exit(1);
}

void sendFile(int sockfd, struct sockaddr_in clientaddr, socklen_t clientlen, char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        char error_msg[] = "File not found.";
        sendto(sockfd, error_msg, strlen(error_msg), 0, (struct sockaddr *)&clientaddr, clientlen);
        return;
    }

    char buffer[BUFSIZE];
    ssize_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFSIZE, file)) > 0) {
        ssize_t bytes_sent = sendto(sockfd, buffer, bytes_read, 0, (struct sockaddr *)&clientaddr, clientlen);
        if (bytes_sent < 0) {
            perror("Error sending file data");
            break;
        }
    }

    fclose(file);

    // Send a success message to the console
    printf("Sent file: %s\n", filename);

    // Send the "END" marker to indicate the end of file transfer
    char end_marker[] = "END";
    sendto(sockfd, end_marker, strlen(end_marker), 0, (struct sockaddr *)&clientaddr, clientlen);
}

void listFiles(int sockfd, struct sockaddr_in clientaddr, socklen_t clientlen) {
    DIR *dir;
    struct dirent *ent;
    char file_list[BUFSIZE] = "";

    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                strcat(file_list, ent->d_name);
                strcat(file_list, "\n");
            }
        }
        closedir(dir);

        ssize_t n = sendto(sockfd, file_list, strlen(file_list), 0, (struct sockaddr *)&clientaddr, clientlen);
        if (n < 0) {
            error("ERROR in sendto");
        }
    } else {
        char response[] = "Error opening directory.\n";
        sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientaddr, clientlen);
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    int portno;
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    socklen_t clientlen;
    char buffer[BUFSIZE];
    int optval = 1;
    int n;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    clientlen = sizeof(clientaddr);

    while (1) {
        bzero(buffer, BUFSIZE);
        n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        // Remove trailing newline character if present
        if (buffer[n - 1] == '\n') {
            buffer[n - 1] = '\0';
            n--;
        }

        if (strncmp(buffer, "get ", 4) == 0) {
            char filename[MAXFILENAME];
            sscanf(buffer, "get %s", filename);

            if (access(filename, F_OK) != -1) {
                sendFile(sockfd, clientaddr, clientlen, filename);
            } else {
                char response[BUFSIZE];
                snprintf(response, BUFSIZE, "File not found: %s", filename);
                sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientaddr, clientlen);
            }
        } else if (strncmp(buffer, "put ", 4) == 0) {
            char filename[MAXFILENAME];
            sscanf(buffer, "put %s", filename);

            FILE *file = fopen(filename, "wb");
            if (file == NULL) {
                perror("Error opening file");
            } else {
                while (1) {
                    bzero(buffer, BUFSIZE);
                    n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen);
                    if (n <= 0) {
                        break;
                    }
                    fwrite(buffer, 1, n, file);
                }
                fclose(file);

                // Send a success message to the console
                printf("Received file: %s\n", filename);

                // Send the "END" marker to indicate the end of file transfer
                char end_marker[] = "END";
                sendto(sockfd, end_marker, strlen(end_marker), 0, (struct sockaddr *)&clientaddr, clientlen);
            }
        } else if (strncmp(buffer, "delete ", 7) == 0) {
            char filename[BUFSIZE];
            sscanf(buffer, "delete %s", filename);
            if (remove(filename) == 0) {
                // Send a success message to the console
                printf("Deleted file: %s\n", filename);
                char response[] = "File deleted successfully.\n";
                sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientaddr, clientlen);
            } else {
                perror("Error deleting file");
                char response[] = "Error deleting file.\n";
                sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientaddr, clientlen);
            }
        } else if (strcmp(buffer, "ls") == 0) {
            listFiles(sockfd, clientaddr, clientlen);
        } else if (strcmp(buffer, "exit") == 0) {
            printf("Server is exiting gracefully.\n");
            close(sockfd);
            exit(0);
        } else {
            char response[BUFSIZE];
            snprintf(response, BUFSIZE, "Unknown command: %s\n", buffer);
            sendto(sockfd, response, strlen(response), 0, (struct sockaddr *)&clientaddr, clientlen);
        }
    }

    close(sockfd);
    return 0;
}
