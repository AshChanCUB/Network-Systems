#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>

#define MAXBUFSIZE 100

typedef struct{
	int sock;
	struct sockaddr_in sin;
	struct sockaddr_in remote;
	socklen_t remote_length;
	char incoming[MAXBUFSIZE];
	char outgoing[MAXBUFSIZE];
}sock_data;

//convert string to lowercase
void strToLower(char *str){
	int i=0;
	while(str[i] != '\n' && str[i] != EOF && i < MAXBUFSIZE){
		str[i] = tolower(str[i]);
		i++;
	}
}

//extract first n characters from incoming message and check if client made a request (eg. GET PUT LS)
void getCmd(char *str, char *cmd, int n){
	bzero(cmd,MAXBUFSIZE);
	strncpy(cmd, str, n);
	cmd[n] = '\0';
	strToLower(cmd);
}

//gets the request from client
int receive(sock_data *S){
	return recvfrom(S->sock, &S->incoming, sizeof(S->incoming), 0, (struct sockaddr *)&S->remote, &S->remote_length);
}

int sendToServer(sock_data *S){
	int nbytes = sendto(S->sock, &S->outgoing, sizeof(S->outgoing), 0, (struct sockaddr *)&S->remote, S->remote_length);
	bzero(S->outgoing,MAXBUFSIZE);
	return nbytes;
}

int sendHandshake(sock_data *S, char *type, long long int size, char *filename){
	strcat(S->outgoing,";");
	strcat(S->outgoing,type);
	strcat(S->outgoing,";");
	char numbuf[MAXBUFSIZE];
	sprintf(numbuf, "%lli", size);
	strcat(S->outgoing,numbuf);
	strcat(S->outgoing,";");
	strcat(S->outgoing,filename);

	sendToServer(S);
	receive(S);

	char cmd[5];
	getCmd(S->incoming,cmd,5);

	if(strcmp(cmd,";cts;") == 0){
		return 0;
	}
	return -1;
}

long long int fileSize(char *msg){
	char str_size[MAXBUFSIZE];
	int i=5; // we can ignore first 5 characters
	while(msg[i] != '\n' && msg[i] != EOF && i < MAXBUFSIZE && msg[i] != ';'){
		str_size[i-5]=msg[i];
		i++;
	}
	return atoi(str_size);
}

//extracts file name from client text request
//req has the format eg 'get filename' or 'put filename'
char *parseFilenameFromReq(char *req){
	char *fn = malloc(MAXBUFSIZE);
	int i=4; // we can ignore first 4 characters since they will be 'get ' or 'put ' - note space
	while(req[i] != '\n' && req[i] != EOF && i < MAXBUFSIZE && req[i] != ' '){
		fn[i-4]=req[i];
		i++;
	}
	return fn;
}

char *parseFilename(char *msg){
	char *fn = malloc(MAXBUFSIZE);
	int i=5; // we can ignore first 5
	int j=0;
	int semi = 0;
	while(msg[i] != '\n' && msg[i] != EOF && i < MAXBUFSIZE && msg[i] != ' '){
		if(semi == 1){
			fn[j++]=msg[i];
		}
		if(msg[i] == ';'){
			semi = 1;
		}
		i++;
	}
	return fn;
}

//checks to see if requested file exists
bool fileExists(char *fn){
	if(access( fn, F_OK ) != -1 ) {
    	// file exists
		return true;
	} else {
    	// file doesn't exist
		return false;
	}
}

//gets the number of bytes in a file
long long int numBytes(FILE *f){
	fseek(f, 0L, SEEK_END);
	int len = ftell(f);
	rewind(f);
	return len;
}

void receiveLs(sock_data *S){
	long long int size = fileSize(S->incoming);
	strcat(S->outgoing,";CTS;");
	sendToServer(S);
	while(size > 0){
		size -= receive(S);
		printf("%s",S->incoming);
	}
	printf("\n");
}

void receiveFile(sock_data *S){
	char *fn = parseFilename(S->incoming);
	long long int size = fileSize(S->incoming);
	//printf("num bytes = %lli\n",size);
	char *newFn = fn; strcat(newFn,".received");
	FILE *fp = fopen(newFn,"wb");
	strcat(S->outgoing,";CTS;");
	sendToServer(S);
	while(size > 0){
		receive(S);
		int writeSize = MAXBUFSIZE; 				//default bytes to write to new file
		if(size < MAXBUFSIZE){writeSize = size;}    //write only the number of bits left if less than MAXBUFSIZE
		int written = fwrite(&S->incoming, 1, writeSize, fp);
		//printf("bytes written = %d\n",written);
		size-=written;
		//printf("bytes remaining: %lli\n",size);
	}
	fclose(fp);
	printf("%s successfully recieved!\n", fn);
}

//sends file after user makes a GET request
void sendFile(sock_data *S){
	char *fn = parseFilenameFromReq(S->outgoing);
	printf("**************\n");
	printf("Req type: PUT\nFilename: %s\n",fn);
	if(fileExists(fn)){
		sendToServer(S);					//send put req
		printf("Exists: true\n");
		FILE *fp = fopen(fn,"rb");
		long long int size = numBytes(fp); 	//get size of file
		printf("Bytes: %lli\n",size);
		printf("**************\n");
		if(sendHandshake(S,"RTS",size,fn) == 0){
			while(!feof(fp)){
				fread(&S->outgoing, 1, sizeof(S->outgoing), fp);
				sendToServer(S);
			}
			printf("%s sent!\n\n",fn);
		}else{
			printf("Error on hs with client\n\n");
		}
		fclose(fp);
	}else{
		printf("Exists: false!\n");
		printf("**************\n\n");
		printf("Error: '%s' does not exist\n",fn);
	}
}

//This code populates the sockaddr_in struct with the information about our socket
int populateSockAddr(sock_data *S, char *addr, char *port){
	bzero(&S->remote,sizeof(S->remote));						//zero remote
	S->remote.sin_family = AF_INET;                   				//address family (IPv4)
	S->remote.sin_port = htons(atoi(port));        					//htons() sets the port # to network byte order (byte order - section 3.2 in Beej Guide)
	S->remote.sin_addr.s_addr = inet_addr(addr);         				//sets IP address

	S->remote_length = sizeof(S->remote);						//set remote length
	bzero(S->incoming,MAXBUFSIZE);							//zero the incoming message

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((S->sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
		return -1;								//if the socket gen fails return -1 flag
	}
	return 0;									//return 0 on success
}

//gets user input from terminal and stores in ClientSock.outgoing
void getUserInput(sock_data *S){
	printf("> ");
	int i = 0;
	char c = getchar();
	while (c != '\n' && c != EOF && i != MAXBUFSIZE){
		S->outgoing[i++] = c;
		c = getchar();
	}
}

int main (int argc, char * argv[]){
	if (argc < 3){
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}
	char *addr = argv[1];
	char *port = argv[2];

	sock_data ClientSock;
	if(populateSockAddr(&ClientSock,addr,port)>0){
		printf("Error: Unable to create socket");
		exit(1);
	}
	printf("Connected to %s:%s...\n",argv[1],argv[2]);

	for(;;){
		bzero(ClientSock.outgoing,MAXBUFSIZE);					//zero
		bzero(ClientSock.incoming,MAXBUFSIZE);

		getUserInput(&ClientSock);
		char cmd[MAXBUFSIZE];
		getCmd(ClientSock.outgoing,cmd,4);
		if(strcmp(cmd,"put ")==0){
			sendFile(&ClientSock);
		}else if(strcmp(cmd,"exit")==0){
			sendToServer(&ClientSock);
			close(ClientSock.sock);
			return 0;
		}else{
			sendToServer(&ClientSock);

			receive(&ClientSock);
			getCmd(ClientSock.incoming,cmd,4);

			//if server requests to send ls to client
			if(strcmp(cmd,";rls")==0){
				receiveLs(&ClientSock);
			//if server requests to send file
			}else if(strcmp(cmd,";rts")==0){
				receiveFile(&ClientSock);
			}else if(strcmp(cmd,";err;")==0){
				char *fn = parseFilename(ClientSock.incoming);
				printf("Error: '%s' does not exist\n",fn);
			}else{
				printf("%s\n",ClientSock.incoming);
			}
		}
	}

	close(ClientSock.sock);
	return 0;

}
