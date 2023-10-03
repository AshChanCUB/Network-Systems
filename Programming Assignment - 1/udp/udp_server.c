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

#define BUFSIZE 1024
#define MAXFILENAME 256

void error(char *msg) {
    perror(msg);
    exit(1);
}

void sendFile(int sockfd, char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        char error_msg[] = "File not found.";
        send(sockfd, error_msg, strlen(error_msg), 0);
        return;
    }

    char buffer[BUFSIZE];
    ssize_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFSIZE, file)) > 0) {
        ssize_t bytes_sent = send(sockfd, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Error sending file data");
            break;
        }
    }

    fclose(file);
    char end_marker[] = "END\n";
    send(sockfd, end_marker, strlen(end_marker), 0);
}

void receiveFile(int sockfd, char *filename) {
    char buffer[BUFSIZE];
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        char error_msg[] = "Error opening file for writing.";
        send(sockfd, error_msg, strlen(error_msg), 0);
        return;
    }

    while (1) {
        bzero(buffer, BUFSIZE);
        ssize_t bytes_received = recv(sockfd, buffer, BUFSIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        if (strcmp(buffer, "END\n") == 0) {
            break;
        }

        fwrite(buffer, 1, bytes_received, file);
    }

    fclose(file);
    printf("Received file: %s\n", filename);
}

void listFiles(int sockfd) {
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

        send(sockfd, file_list, strlen(file_list), 0);
    } else {
        char response[] = "Error opening directory.\n";
        send(sockfd, response, strlen(response), 0);
    }
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    int portno;
    socklen_t clientlen;
    char buffer[BUFSIZE];
    int optval = 1;
    int n;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    struct sockaddr_in serveraddr, clientaddr;
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);
    clientlen = sizeof(clientaddr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (newsockfd < 0)
            error("ERROR on accept");

        pid_t pid = fork();
        if (pid == 0) {
            close(sockfd);
            while (1) {
                bzero(buffer, BUFSIZE);
                n = read(newsockfd, buffer, BUFSIZE);
                if (n < 0)
                    error("ERROR reading from socket");

                if (strcmp(buffer, "exit\n") == 0) {
                    printf("Client is exiting.\n");
                    break;
                } else if (strncmp(buffer, "get ", 4) == 0) {
                    char filename[MAXFILENAME];
                    sscanf(buffer, "get %s", filename);
                    sendFile(newsockfd, filename);
                } else if (strncmp(buffer, "put ", 4) == 0) {
                    char filename[MAXFILENAME];
                    sscanf(buffer, "put %s", filename);
                    receiveFile(newsockfd, filename);
                } else if (strncmp(buffer, "delete ", 7) == 0) {
                    char filename[MAXFILENAME];
                    sscanf(buffer, "delete %s", filename);
                    if (remove(filename) == 0) {
                        printf("Deleted file: %s\n", filename);
                        char response[] = "File deleted successfully.\n";
                        send(newsockfd, response, strlen(response), 0);
                    } else {
                        perror("Error deleting file");
                        char response[] = "Error deleting file.\n";
                        send(newsockfd, response, strlen(response), 0);
                    }
                } else if (strcmp(buffer, "ls\n") == 0) {
                    listFiles(newsockfd);
                } else {
                    char response[] = "Unknown command or incorrect input.\n";
                    send(newsockfd, response, strlen(response), 0);
                }
            }
            close(newsockfd);
            exit(0);
        } else if (pid < 0) {
            error("ERROR on fork");
        }
        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
