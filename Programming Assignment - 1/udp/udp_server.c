#include <sys/types.h>
#include <sys/socket.h> 		//structures needed for sockets
#include <netinet/in.h>			//constants and structures needed for internet domain addresses
#include <arpa/inet.h>			//definitions for internet operations
#include <netdb.h>				//definitions for network database operations
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
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
	strncpy(cmd,str, n);
	cmd[n] = '\0';
	strToLower(cmd);
}

//gets the request from client
int receive(sock_data *S){
	return recvfrom(S->sock, &S->incoming, sizeof(S->incoming), 0, (struct sockaddr *)&S->remote, &S->remote_length);
}

int sendToClient(sock_data *S){
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

	sendToClient(S);
	receive(S);

	char cmd[5];
	getCmd(S->incoming,cmd,5);

	if(strcmp(cmd,";cts;") == 0){
		return 0;
	}
	return -1;
}

//gets a string representation of ls and returns via *msg
void sendLs(sock_data *S){
	FILE *ls = popen("ls", "r");
	char buf[MAXBUFSIZE];
	char paths[MAXBUFSIZE][MAXBUFSIZE];
	int i=0;
	long long int total = 0;
	while (fgets(buf, sizeof(buf), ls) != 0) {
		char *path = strtok(buf, "\n");
		strcat(path,"   ");
		total += strlen(path);
		strcpy(paths[i++],path);
	}
	pclose(ls);
	if(sendHandshake(S, "RLS", total, "ls") == 0){
		int added = 0;
		for(int j=0; j<i;j++){
			if(total <= 0) break;
			if(added + strlen(paths[j]) > MAXBUFSIZE){
				sendToClient(S);
				added = 0;
			}
			strcat(S->outgoing,paths[j]);
			added+=strlen(paths[j]);
		}
		if(added > 0){
			sendToClient(S);
		}
	}else{
		printf("Error sending ls");
	}
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

//sends file after user makes a GET request
void sendFile(sock_data *S){
	char *fn = parseFilenameFromReq(S->incoming);
	printf("**************\n");
	printf("Req type: GET\nFilename: %s\n",fn);
	if(fileExists(fn)){
		printf("Exists: true\n");
		FILE *fp = fopen(fn,"rb");
		long long int size = numBytes(fp); 	//get size of file
		printf("Bytes: %lli\n",size);
		printf("**************\n");
		if(sendHandshake(S,"RTS",size,fn) == 0){
			while(!feof(fp)){
				fread(&S->outgoing, 1, sizeof(S->outgoing), fp);
				sendToClient(S);
			}
			printf("%s sent!\n\n",fn);
		}else{
			printf("Error on hs with client\n\n");
		}
		fclose(fp);
	}else{
		printf("Exists: false!\n");
		printf("**************\n\n");
		strcat(S->outgoing,";ERR;DNE;");
		strcat(S->outgoing,fn);
		sendToClient(S);
	}
}

void receiveFile(sock_data *S){
	receive(S);
	char cmd[MAXBUFSIZE];
	getCmd(S->incoming,cmd,5);
	if(strcmp(cmd,";rts;")==0){
		char *fn = parseFilename(S->incoming);
		long long int size = fileSize(S->incoming);
		char *newFn = fn; strcat(newFn,".received");
		FILE *fp = fopen(newFn,"wb");
		strcat(S->outgoing,";CTS;");
		sendToClient(S);
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
	}else{
		printf("Error on handshake\n");
	}
}

//This code populates the sockaddr_in struct with the information about our socket
int populateSockAddr(sock_data *S, char *port){
	bzero(&S->sin,sizeof(S->sin));                    		//zero the struct
	bzero(&S->remote,sizeof(S->remote));					//zero remote
	S->sin.sin_family = AF_INET;                   			//address family (IPv4)
	S->sin.sin_port = htons(atoi(port));        			//htons() sets the port # to network byte order (byte order - section 3.2 in Beej Guide)
	S->sin.sin_addr.s_addr = INADDR_ANY;           			//supplies the IP address of the local machine

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((S->sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
		return -1;											//if the socket gen fails return -1 flag
	}
	return 0;												//return 0 on success
}

//bind the socket to the local address and port we've supplied in the sockaddr_in struct
int bindSocket(sock_data *S, char *port){
	if (bind(S->sock, (struct sockaddr *)&S->sin, sizeof(S->sin)) < 0){
		return -1;											//if bind fails return -1
	}
	S->remote_length = sizeof(S->remote);					//set remote length
	bzero(S->incoming,MAXBUFSIZE);							//zero the incoming message
	printf("Listening on port %s...\n", port);				//success, server will now listen for requests
	return 0;
}

int main (int argc, char * argv[] ){
	if (argc != 2){
		printf ("USAGE: <port>\n");
		exit(1);
	}

	sock_data ServerSock;										//initialize ServerSock struct which stores all the relevant sock info
	char *port = argv[1];

	if(populateSockAddr(&ServerSock,port)>0){
		printf("Error: Unable to create socket");
		exit(1);
	}

	if(bindSocket(&ServerSock,port)>0){
		printf("Error: Unable to bind the socket");
	}

	//infinite loop - terminated by ctrl+C by the server admin
	for(;;){
		char cmd[MAXBUFSIZE];								//stores command GET or PUT
		bzero(ServerSock.outgoing,MAXBUFSIZE);					//clear outgoing message from last time

		//receives request from client
		receive(&ServerSock);
		printf("The client says %s\n", ServerSock.incoming);
		getCmd(ServerSock.incoming, cmd, 4);

		//if client requests ls
		if(strcmp(cmd, "ls") == 0){
			sendLs(&ServerSock);

		//if client makes GET request
		}else if(strcmp(cmd,"get ") == 0){
			sendFile(&ServerSock);

		//if client makes a PUT request
		}else if(strcmp(cmd,"put ") == 0){
			receiveFile(&ServerSock);
		}else if(strcmp(cmd,"exit")==0){
			printf("client left\n");
		}else{
			strcat(ServerSock.outgoing,"Unknown command: '");
			strcat(ServerSock.outgoing,ServerSock.incoming);
			strcat(ServerSock.outgoing,"'");
			sendToClient(&ServerSock);
		}
	}

	close(ServerSock.sock);
	return 0;
}
