/*
CSCI4430 Assignment 1
Client Code
Group: 15
Terry LAI & David Lau
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CONNECTION 10
#define MAX_NAME_LENGTH 255
#define MAX_MESSAGE 255
#define MAX_MESSAGE_SIZE 255
#define MAX_INBOX 20
#define MESSAGE_BUFFER_SIZE 500
#define DEBUG_MODE 0

// online client list structure
typedef struct clientList{
	char name[MAX_NAME_LENGTH];
	unsigned int ipAddr;
	unsigned short port;
	int valid;
}clientList;

// message structure
typedef struct messageList{
	char text[MAX_MESSAGE_SIZE];
	char sender[MAX_NAME_LENGTH];
	int unRead;
}messageList;

// client connected list
typedef struct clientConnect{
	char name[MAX_NAME_LENGTH];
	int clientSocket;
	int connected;
	pthread_t clientThread;
}clientConnect;

// Global Variables
unsigned short listeningPort;
int listenSocket;
int connectServerSocket;
unsigned char buffer[MESSAGE_BUFFER_SIZE];
char loginName[MAX_NAME_LENGTH];
clientList clients[MAX_CONNECTION];
messageList messages[MAX_MESSAGE];
clientConnect clientConnected[MAX_CONNECTION];
char receiver[MAX_NAME_LENGTH];
char sendMessage[MAX_MESSAGE_SIZE];
int currentClientSlot;
int clientSlotForB;
int foundInList;
int typeAfail;
int typeBfail;

// Thread Related
pthread_t listenThread;
int isListening = 0;
pthread_mutex_t messageMutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connectedMutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t listMutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t listenValueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t listenReadyCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t typeAMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t typeAReadyCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t typeBMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t typeBReadyCond = PTHREAD_COND_INITIALIZER;

void sendGETLISTmessage(){
	int sentByte;

	memset(buffer,0,MESSAGE_BUFFER_SIZE);
	buffer[0] = 0x03;
	if((sentByte = send(connectServerSocket,buffer,1,0))<0){
		printf("Error: Error in sending GETLIST message to server\n");
		printf("Errno: %s, %s\n",strerror(errno),errno);
	}
}

void receiveGETLISTOKmessage(int show){
	int receiveByte;
	int alreadyReceiveByte;
	int argLength;
	int nameLength;
	int i,j;
	struct in_addr tempAddr;

	memset(buffer,0,MESSAGE_BUFFER_SIZE);
	alreadyReceiveByte = 0;
	while(1){
		if((receiveByte = recv(connectServerSocket,buffer+alreadyReceiveByte,MESSAGE_BUFFER_SIZE - alreadyReceiveByte,0))<0){
			printf("Error: Error in receiving GET LIST OK feedback from server\n");
 			printf("Errno: %s, %s\n",strerror(errno),errno);
 			exit(1);
		}

		alreadyReceiveByte += receiveByte;
		if(receiveByte >= 5){
			memcpy(&argLength,buffer+1,sizeof(int));
			argLength = ntohl(argLength);
		}else{
			continue;
		}

		// clear exisiting client list
		for(i=0;i<(MAX_CONNECTION-1);i++){
			clients[i].valid = 0;
		}

		if(alreadyReceiveByte == (argLength + 1 +sizeof(int))){
			j = 1 + sizeof(int);
			if(show == 1){
				printf("\n+--------Online Clients List---------+\n");
			}
			for(i=0;i<MAX_CONNECTION;i++){
				memcpy(&nameLength,buffer+j,sizeof(int));
				nameLength = ntohl(nameLength);
				j+=sizeof(int);
				if(DEBUG_MODE){ printf("check name length\n"); }
				memset(clients[i].name,0,MAX_NAME_LENGTH);
				memcpy(clients[i].name,buffer+j,nameLength);
				j+=nameLength;
				if(DEBUG_MODE){ printf("check ip address\n"); }
				memcpy(&clients[i].ipAddr,buffer+j,sizeof(int));
				j+=sizeof(int);
				memcpy(&clients[i].port,buffer+j,sizeof(unsigned short));
				j+=sizeof(unsigned short);
				tempAddr.s_addr = clients[i].ipAddr;
				if(strcmp(clients[i].name,loginName)==0){
					clients[i].valid = 0;
					i--;
				}else{
					clients[i].valid = 1;
					if(show == 1){
						printf("Name: %s | IP Address: %s | Port: %d\n",clients[i].name,inet_ntoa(tempAddr),ntohs(clients[i].port));
					}
				}
				if(j == (argLength + 1 + sizeof(int))){
					if(show == 1){
						printf("+--------Online Clients List---------+\n\n");
						fflush(stdout);
					}
					return;
				}
			}
		}
	}

}

void *clientThreadTypeB(void *args){
	char tempBuffer[MAX_MESSAGE_SIZE];
	int sentByte;
	int alreadySentByte;
	int receiveByte;
	int alreadyReceiveByte;
	int i;
	int messageLength;
	char senderName[MAX_NAME_LENGTH];
	int senderNameLength;
	int foundName;
	int temp = 1;
	int localSlot = clientSlotForB;

	pthread_mutex_lock(&typeBMutex);

	// receive hello message
	if(DEBUG_MODE){ printf("create Thread Type B\n"); }
	alreadyReceiveByte = 0;
	memset(tempBuffer,0,MAX_MESSAGE_SIZE);
	while(1){
		if((receiveByte = recv(clientConnected[localSlot].clientSocket,tempBuffer+alreadyReceiveByte,MAX_MESSAGE_SIZE-alreadyReceiveByte,0))<0){
			printf("Error: Couldn't receive HELLO.\n");
			typeBfail = 1;
			pthread_cond_signal(&typeBReadyCond);
			pthread_mutex_unlock(&typeBMutex);
			pthread_exit(NULL);
		}
		alreadyReceiveByte+=receiveByte;

		if(tempBuffer[0]==0x10){
			if(alreadyReceiveByte>=5){
				memcpy(&senderNameLength,tempBuffer+1,sizeof(int));
				senderNameLength=ntohl(senderNameLength);
			}else{
				continue;
			}
			if(alreadyReceiveByte==(senderNameLength+1+sizeof(int))){
			if(DEBUG_MODE){	printf("copy the sender name\n"); }
				memcpy(senderName,tempBuffer+1+sizeof(int),senderNameLength);
				break;
			}else{
				continue;
			}
		}else{
			printf("Error: Received undefined message from other client.\n");
			typeBfail = 1;
			pthread_cond_signal(&typeBReadyCond);
			pthread_mutex_unlock(&typeBMutex);
			pthread_exit(NULL);
		}
	}
	if(DEBUG_MODE){
	printf("Sender Name Length: %d\n",senderNameLength);
	printf("Sender Name: %s\n",senderName);
	}

	foundName = 0;
	for(i=0;i<MAX_CONNECTION;i++){
		if(strcmp(clients[i].name,senderName)==0){
			foundName = 1;
			break;
		}
	}

	if(foundName == 0){
	sendGETLISTmessage();
	receiveGETLISTOKmessage(0);

		for(i=0;i<MAX_CONNECTION;i++){
			if(strcmp(clients[i].name,senderName)==0){
			foundName = 1;
			break;
			}
		}
	}
	memset(tempBuffer,0,MAX_MESSAGE_SIZE);
	if(foundName == 0){
		tempBuffer[0]=0xff;
		memcpy(tempBuffer+1,&temp,sizeof(int));
		tempBuffer[1+sizeof(int)]=0x04;

		alreadySentByte=0;
		while(1){
			if((sentByte=send(clientConnected[localSlot].clientSocket,tempBuffer+alreadySentByte,6-alreadySentByte,0))<0){
			printf("Error: Couldn't send error message to '%s'\n",senderName);
			typeBfail = 1;
			pthread_cond_signal(&typeBReadyCond);
			pthread_mutex_unlock(&typeBMutex);
			pthread_exit(NULL);
			}
			alreadySentByte+=sentByte;
			if(alreadySentByte>=6){
				break;
			}
		}
		pthread_exit(NULL);
	}else{
		if(DEBUG_MODE){ printf("ready to send HELLO OK\n"); }
		tempBuffer[0]=0x20;

		alreadySentByte=0;
		while(1){
			if((sentByte=send(clientConnected[localSlot].clientSocket,tempBuffer+alreadySentByte,1-alreadySentByte,0))<0){
			printf("Error: Couldn't send HELLO OK message to '%s'\n",senderName);
			typeBfail = 1;
			pthread_cond_signal(&typeBReadyCond);
			pthread_mutex_unlock(&typeBMutex);
			pthread_exit(NULL);
			}
			alreadySentByte+=sentByte;
			if(alreadySentByte>=1){
				break;
			}
		}
		clientConnected[localSlot].connected = 1;
		memcpy(clientConnected[localSlot].name,senderName,senderNameLength);
		pthread_cond_signal(&typeBReadyCond);
		pthread_mutex_unlock(&typeBMutex);

		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		alreadyReceiveByte = 0;
		messageLength=0;
		while(1){
			if((receiveByte=recv(clientConnected[localSlot].clientSocket,tempBuffer+alreadyReceiveByte,MAX_MESSAGE_SIZE - alreadyReceiveByte,0))<0){
				clientConnected[localSlot].connected=0;
				pthread_exit(NULL);
			}
			if(receiveByte == 0){ 
				printf("\nConnection from '%s' dropped.\n",senderName);
				clientConnected[localSlot].connected=0;
				pthread_exit(NULL);
			}
		    if(DEBUG_MODE){	printf("Byte received: %d\n",receiveByte); }
			alreadyReceiveByte+=receiveByte;

			if(tempBuffer[0]==0x30){
				if(alreadyReceiveByte>=5){
					memcpy(&messageLength,tempBuffer+1,sizeof(int));
					messageLength=ntohl(messageLength);
				   if(DEBUG_MODE){ printf("Got message length: %d\n",messageLength); }
				}else{
					continue;
				}
				if(alreadyReceiveByte==messageLength+1+sizeof(int)){
					pthread_mutex_lock(&messageMutex);
						for(i=0;i<MAX_MESSAGE;i++){
							if(messages[i].unRead == 0){
								memset(messages[i].sender,0,MAX_NAME_LENGTH);
								memset(messages[i].text,0,MAX_MESSAGE_SIZE);
								memcpy(messages[i].sender,senderName,senderNameLength);
								memcpy(messages[i].text,tempBuffer+1+sizeof(int),messageLength);
							if(DEBUG_MODE){	printf("received message: %s\n",tempBuffer+1+sizeof(int)); }
								messages[i].unRead=1;
								break;
							}
						}
					pthread_mutex_unlock(&messageMutex);
					alreadyReceiveByte=0;
					messageLength=0;
				}else{
					continue;
				}
			}else{
				printf("Error: Undefine message from '%s'.\n",senderName);
				exit(1);
			}
		}
	}
}

void* listening(void* args){
	int clientAddrLength;
	int inClientSocket;
	int i;
	int returnTypeBThread;

	pthread_mutex_lock(&listenValueMutex);
	listenSocket = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in sockname;
	struct sockaddr_in clientAddr;
	socklen_t nameLength = sizeof(sockname);
	// listen a random port through sockname

	if(listen(listenSocket,MAX_CONNECTION)<0){
		printf("Error: Couldn't establish listen socket\n");
		printf("Errno: %s, %s\n",strerror(errno),errno);
		exit(1);
	}
	if(getsockname(listenSocket,(struct sockaddr *)&sockname,&nameLength)<0){
		printf("Error: Couldn't 'getsockname'\n");
		perror("getsockname()");
		exit(1);
	}
	listeningPort = sockname.sin_port;
	isListening = 1;

	pthread_cond_signal(&listenReadyCond);
	pthread_mutex_unlock(&listenValueMutex);

	//start listening..
	while(1){
		clientAddrLength = sizeof(clientAddr);
		if((inClientSocket = accept(listenSocket,(struct sockaddr *) &clientAddr,&clientAddrLength))<0){
			printf("Error: Couldn't accept incoming connection\n");
			printf("Errno: %s, %s\n",strerror(errno),errno);
			exit(1);
		}

		if(DEBUG_MODE){ printf("Some one connecting me\n"); }

		// handle incoming client connection
		clientSlotForB = -1;
		pthread_mutex_lock(&connectedMutex);
		pthread_mutex_lock(&listMutex);
		for(i=0;i<MAX_CONNECTION;i++){
			if(clientConnected[i].connected==0){
				clientSlotForB = i;
				break;
			}
		}
		typeBfail=0;
		if(clientSlotForB == -1){
			printf("Error: Maximum Connection reached. Rejected.\n");
			close(inClientSocket);
			pthread_mutex_unlock(&listMutex);
			pthread_mutex_unlock(&connectedMutex);
			continue;
		}else{
		
			clientConnected[clientSlotForB].clientSocket=inClientSocket;
 			if((returnTypeBThread = pthread_create(&(clientConnected[clientSlotForB].clientThread),NULL,clientThreadTypeB,NULL))!=0){
 			printf("Error: Couldn't create client thread type B. Program terminate.\n");
 			exit(1);
 			}
 			// check if the Thread Type B connected and sent out message
 			pthread_mutex_lock(&typeBMutex);
 			if(clientConnected[clientSlotForB].connected == 1 || typeBfail == 1){
 				;
 			}else{
 				pthread_cond_wait(&typeBReadyCond,&typeBMutex);
 			}
 			pthread_mutex_unlock(&typeBMutex);
		}
		pthread_mutex_unlock(&listMutex);
		pthread_mutex_unlock(&connectedMutex);
	}
}

void sendLoginMessage(){
	int loginNameLength;
    int listeningPortLength;
    int sentByte;
    int alreadySentByte;
    int messageLength;
	memset(buffer,0,MESSAGE_BUFFER_SIZE);
 		buffer[0] = 0x01;
 		loginNameLength = strlen(loginName);
 		loginNameLength = htonl(loginNameLength);
 		listeningPortLength = sizeof(unsigned short);
 		listeningPortLength = htonl(listeningPortLength);

 		memcpy(buffer+1,&loginNameLength,sizeof(int));
 		memcpy(buffer+1+sizeof(int),loginName,ntohl(loginNameLength));
 		memcpy(buffer+1+sizeof(int)+ntohl(loginNameLength),&listeningPortLength,sizeof(int));
 		memcpy(buffer+1+sizeof(int)+ntohl(loginNameLength)+sizeof(int),&listeningPort,sizeof(unsigned short));

 		// send out the LOGIN message
 		messageLength = 1+sizeof(int)+ntohl(loginNameLength)+sizeof(int)+sizeof(unsigned short);
 		sentByte = 0;
 		alreadySentByte = 0;
 		while(1){
 			if((sentByte = send(connectServerSocket,buffer+alreadySentByte,(messageLength - alreadySentByte),0))<0){
 				printf("Error: Error in sending LOGIN message\n");
 				printf("Errno: %s, %s\n",strerror(errno),errno);
 				exit(1);
 			}
 			alreadySentByte+=sentByte;
 			if(alreadySentByte >= messageLength){
 				return;
 			}
 		}
}

int receiveLoginMessage(){
	int receiveByte;
    int alreadyReceiveByte;
	
	receiveByte = 0;
 	alreadyReceiveByte = 0;
 	memset(buffer,0,MESSAGE_BUFFER_SIZE);
 	
 	while(1){
 		if((receiveByte = recv(connectServerSocket,buffer+alreadyReceiveByte,MESSAGE_BUFFER_SIZE,0))<0){
 			printf("Error: Error in receiving LOGIN feedback from server\n");
 			printf("Errno: %s, %s\n",strerror(errno),errno);
 		}
 		alreadyReceiveByte += receiveByte;

 			//check the receive message
 		if(buffer[0] == 0x02){
 				// LOGIN OK
 				printf(" \"Welcome! %s\" ]\n",loginName);
 				return 1;
 		}else if(buffer[0] == 0xff){
 			int argPosition = 1 + sizeof(int);

 			if(buffer[argPosition]==0x01){
 				printf(" \"Sorry, '%s' is already taken. Please try again.\" ]\n",loginName);
 				return 2;
 			}else if(buffer[argPosition]==0x02){
 				printf(" \"Sorry, there is another online client is using the same (IP address, listening port number) pair.\" ]\n");
 				return 0;
 			}else if(buffer[argPosition]==0x03){
 				printf(" \"Sorry, maximum number of onine clients is reached. Please try again.\" ]\n");
 				return 0;
 			}else{
 				printf(" \"Sorry, unknown error message is received from server. Please try again.\" ]\n");
 				return 0;
 			}
 		}
 	}
}

void closingClient(){
	int i;

	for(i=0;i<MAX_CONNECTION;i++){
		if(clientConnected[i].connected == 1){
			close(clientConnected[i].clientSocket);
		}
	}
	close(listenSocket);
 	close(connectServerSocket);
 	printf("Good Bye! See you again.\n");
 	exit(0);
}

void mainMenuLoop(){
	int menuChoice;
	int loginCheck;
    
	while(1){
		if(loginCheck != 2){
		printf("\n");
 		printf("+---------- Main Menu --------+\n");
 		printf("| (1) Login to the server     |\n");
 		printf("| (2) Exit                    |\n");
 		printf("+-----------------------------+\n");
 		printf("What is your choice: ");
 		scanf("%d",&menuChoice);
 		}
 			
 		if(menuChoice == 1){
 			printf("Screen name [Type '.' to return to menu]: ");
 			memset(loginName,0,MAX_NAME_LENGTH);
 			scanf("%s",loginName);
 			if(strcmp(loginName,".")==0){
 				continue;
 			}
 		}else if(menuChoice == 2){
 			closingClient();
 		}else{
 			printf("Invalid Selection. Please Try Again!\n");
 			continue;
 		}

 		printf("[ Logging in ... ");
 		// create LOGIN protocol message
 		sendLoginMessage();
 		// receive LOGIN OK message
 		loginCheck = receiveLoginMessage();
 		if(loginCheck == 1){
 			return;
 		}
 	}
}

void readUnreadMessage(){
	int i;
	int noMessage = 0;

	for(i=0;i<MAX_MESSAGE;i++){
		if(messages[i].unRead == 1){
			noMessage++;
			printf("\n+----------------------------------------------+\n");
			printf(" Message from %s\n",messages[i].sender);
			printf("+----------------------------------------------+\n");
			printf(" %s \n",messages[i].text);
			printf("+----------------------------------------------+\n");
			messages[i].unRead = 0;
		}
	}
	printf("\n");
	if(noMessage == 0){
		printf("[ No unread message. ]\n\n");
	}
}

void *clientThreadTypeA(void *args){
		int sentByte;
		int alreadySentByte;
		int receiveByte;
		int alreadyReceiveByte;
		int messageLength;
		int receiverLength;
		int totalLength;
		char tempBuffer[MAX_MESSAGE_SIZE];
		char localReceiver[MAX_NAME_LENGTH];
		int i;
		int slotNum = currentClientSlot;
		int localList = foundInList;
		struct in_addr container;

		pthread_mutex_lock(&typeAMutex);

		memcpy(localReceiver,receiver,strlen(receiver));
		//establish connection
		if((clientConnected[slotNum].clientSocket = socket(AF_INET,SOCK_STREAM,0))<0){
			printf("Error: Couldn't create socket in Thread Type A\n");
			typeAfail = 1;
			pthread_cond_signal(&typeAReadyCond);
			pthread_mutex_unlock(&typeAMutex);
			exit(1);
		}
		struct sockaddr_in newClientAddr;
		memset(&newClientAddr,0,sizeof(newClientAddr));
		newClientAddr.sin_family = AF_INET;
		container.s_addr=clients[localList].ipAddr;
		newClientAddr.sin_addr.s_addr = inet_addr(inet_ntoa(container));
		//newClientAddr.sin_addr.s_addr = inet_addr("137.189.90.156");
		newClientAddr.sin_port = clients[localList].port;
		if(DEBUG_MODE){
		printf("IP Addr: %s\n",inet_ntoa(container));
		printf("Port: %d\n",clients[localList].port);
		}

		long val = 1;
		if(setsockopt(clientConnected[slotNum].clientSocket,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(long))==-1){
			perror("setsockopt");
			typeAfail = 1;
			pthread_cond_signal(&typeAReadyCond);
			pthread_mutex_unlock(&typeAMutex);
			exit(1);
		}

		if(DEBUG_MODE){ printf("now ready to connect\n"); }
		if(connect(clientConnected[slotNum].clientSocket,(struct sockaddr *)&newClientAddr,sizeof(newClientAddr))<0){
			printf("Error: Couldn't send message to '%s'.\n",localReceiver);
			perror("connect()");
			typeAfail = 1;
			pthread_cond_signal(&typeAReadyCond);
			pthread_mutex_unlock(&typeAMutex);
			pthread_exit(NULL);
		}
	
		if(DEBUG_MODE){ printf("ready to send out hello\n");}
		// create HELLO message
		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		tempBuffer[0]=0x10;
		receiverLength=strlen(loginName);
		receiverLength=htonl(receiverLength);
		memcpy(tempBuffer+1,&receiverLength,sizeof(int));
		memcpy(tempBuffer+1+sizeof(int),loginName,ntohl(receiverLength));

		alreadySentByte = 0;
		messageLength = 1 + sizeof(int) + ntohl(receiverLength);
		while(1){
			if((sentByte = send(clientConnected[slotNum].clientSocket,tempBuffer+alreadySentByte,messageLength - alreadySentByte,0))<0){
				printf("Error: Couldn't send message to '%s'\n",localReceiver);
				typeAfail = 1;
				pthread_cond_signal(&typeAReadyCond);
				pthread_mutex_unlock(&typeAMutex);
				pthread_exit(NULL);
			}
			alreadySentByte+=sentByte;
			if(alreadySentByte>=messageLength){
				break;
			}
		}

		if(DEBUG_MODE){ printf("ready to receive HELLO OK \n"); }
		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		alreadyReceiveByte = 0;

		while(1){
			if((receiveByte = recv(clientConnected[slotNum].clientSocket,tempBuffer+alreadyReceiveByte,MAX_MESSAGE_SIZE,0))<0){
				printf("Error: Couldn't send message to '%s'\n",localReceiver);
				typeAfail = 1;
				pthread_cond_signal(&typeAReadyCond);
				pthread_mutex_unlock(&typeAMutex);
				pthread_exit(NULL);
			}
			alreadyReceiveByte+=receiveByte;

			if(tempBuffer[0]==0x20){
				break;
			}else if(tempBuffer[0]==0xff){
				if(alreadyReceiveByte<6){
					continue;
				}
				if(tempBuffer[1+sizeof(int)]==0x04){
					printf("Error: '%s' couldn't find you in the online client list\n",localReceiver);
					typeAfail = 1;
					pthread_cond_signal(&typeAReadyCond);
					pthread_mutex_unlock(&typeAMutex);
					pthread_exit(NULL);
				}else{
					printf("Error: Undefine message from '%s'. Message not sent.\n",localReceiver);
					typeAfail = 1;
					pthread_cond_signal(&typeAReadyCond);
					pthread_mutex_unlock(&typeAMutex);
					exit(1);
				}
			}else{
				printf("Error: Undefine message from '%s'. Message not sent.\n",localReceiver);
				typeAfail = 1;
				pthread_cond_signal(&typeAReadyCond);
				pthread_mutex_unlock(&typeAMutex);
				exit(1);
			}
		}

		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		tempBuffer[0]=0x30;
		messageLength = strlen(sendMessage);
		messageLength=htonl(messageLength);
		memcpy(tempBuffer+1,&messageLength,sizeof(int));
		memcpy(tempBuffer+1+sizeof(int),sendMessage,ntohl(messageLength));

		alreadySentByte = 0;
		totalLength = 1 + sizeof(int) + ntohl(messageLength);
		while(1){
			if((sentByte = send(clientConnected[slotNum].clientSocket,tempBuffer+alreadySentByte,totalLength - alreadySentByte,0))<0){
				printf("Error: Couldn't send message to '%s'.\n",localReceiver);
				typeAfail = 1;
				pthread_cond_signal(&typeAReadyCond);
				pthread_mutex_unlock(&typeAMutex);
				return;
			}
			alreadySentByte+=sentByte;
			if(alreadySentByte>=totalLength){
				break;
			}
		}
		printf("[ Message (%d bytes) sent to '%s'. ]\n\n",strlen(sendMessage),localReceiver);
		fflush(stdout);

		memcpy(clientConnected[slotNum].name,receiver,strlen(receiver));
		clientConnected[slotNum].connected = 1;
		pthread_cond_signal(&typeAReadyCond);
		pthread_mutex_unlock(&typeAMutex);

		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		alreadyReceiveByte = 0;
		messageLength=0;
		while(1){
			if((receiveByte=recv(clientConnected[slotNum].clientSocket,tempBuffer+alreadyReceiveByte,MAX_MESSAGE_SIZE - alreadyReceiveByte,0))<0){
				clientConnected[slotNum].connected=0;
				pthread_exit(NULL);
			}
			if(receiveByte == 0){ 
				printf("\nconnection drop from '%s'\n",localReceiver); 
				clientConnected[slotNum].connected=0;
				pthread_exit(NULL);
			}
			alreadyReceiveByte+=receiveByte;

			if(tempBuffer[0]==0x30){
				if(alreadyReceiveByte>=5){
					memcpy(&messageLength,tempBuffer+1,sizeof(int));
					messageLength=ntohl(messageLength);
				}else{
					continue;
				}
				if(alreadyReceiveByte==messageLength+1+sizeof(int)){
					pthread_mutex_lock(&messageMutex);
						for(i=0;i<MAX_MESSAGE;i++){
							if(messages[i].unRead == 0){
								memset(messages[i].sender,0,MAX_NAME_LENGTH);
								memset(messages[i].text,0,MAX_MESSAGE_SIZE);
								memcpy(messages[i].sender,localReceiver,strlen(localReceiver));
								memcpy(messages[i].text,tempBuffer+1+sizeof(int),messageLength);
								messages[i].unRead=1;
								break;
							}
						}
					pthread_mutex_unlock(&messageMutex);
					alreadyReceiveByte=0;
					messageLength=0;
				}else{
					continue;
				}
			}
			/* else{
				printf("Error: Undefine message from '%s'.\n",localReceiver);
				exit(1);
			} */
		}
}

void sendHelloMessage(){
	int i;
	int clientSocket;
	int foundExisting;
	int receiverLength;
	int sentByte;
	int alreadySentByte;
	int messageLength;
	int totalLength;
	int returnTypeAThread;
	char tempBuffer[MAX_MESSAGE_SIZE];

	foundInList = -1;
	// check if on the list
	for(i=0;i<MAX_CONNECTION;i++){
		if(clients[i].valid == 1 && (strcmp(clients[i].name,receiver)==0)){
			foundInList = i;
			break;
		}
	}

	if(foundInList == -1){
		printf("'%s' is not on the online client list. Please try again.\n",receiver);
		return;
	}

	// check if got exisiting socket
	currentClientSlot = -1;
	foundExisting = -1;
	for(i=0;i<MAX_CONNECTION;i++){
		if(clientConnected[i].connected == 1 && (strcmp(clientConnected[i].name,receiver)==0)){
			foundExisting = i;
			break;
		}
		if(clientConnected[i].connected == 0 && currentClientSlot == -1){
			currentClientSlot = i;
		}
	}

	if(foundExisting == -1){
		if(currentClientSlot == -1){
			printf("Error: Already meet the maximum client-to-client connection (%d).\n",MAX_CONNECTION);
			return;
		}else{
			typeAfail = 0;
			// create client Thread Type A
 			if((returnTypeAThread = pthread_create(&(clientConnected[currentClientSlot].clientThread),NULL,clientThreadTypeA,NULL))!=0){
 			printf("Error: Couldn't create client thread type A. Program terminate.\n");
 			exit(1);
 			}
 			// check if the Thread Type A connected and sent out message
 			pthread_mutex_lock(&typeAMutex);
 			if(clientConnected[currentClientSlot].connected == 1 || typeAfail == 1){
 				;
 			}else{
 				pthread_cond_wait(&typeAReadyCond,&typeAMutex);
 			}
 			pthread_mutex_unlock(&typeAMutex);
 		}
	}else{
		memset(tempBuffer,0,MAX_MESSAGE_SIZE);
		tempBuffer[0]=0x30;
		messageLength = strlen(sendMessage);
		messageLength=htonl(messageLength);
		memcpy(tempBuffer+1,&messageLength,sizeof(int));
		memcpy(tempBuffer+1+sizeof(int),sendMessage,ntohl(messageLength));

		alreadySentByte = 0;
		totalLength = 1 + sizeof(int) + ntohl(messageLength);
		while(1){
			if((sentByte = send(clientConnected[foundExisting].clientSocket,tempBuffer+alreadySentByte,totalLength - alreadySentByte,0))<0){
				printf("Error: Couldn't send message to '%s'.\n",receiver);
				return;
			}
			alreadySentByte+=sentByte;
			if(alreadySentByte>=totalLength){
				break;
			}
		}
		printf("[ Message (%d bytes) sent to '%s'. ]\n\n",strlen(sendMessage),receiver);
	}
}

void chatRoomLoop(){
	int chatRoomChoice;
	char tempMessage[MAX_MESSAGE_SIZE][MAX_MESSAGE_SIZE];
	int i,j;
	int currentPosition;
	int stringLength;

	while(1){
		printf("+--------- ChatRoom Menu ---------+\n");
		printf("| (1) Show List of Online Clients |\n");
		printf("| (2) Read Unread Message         |\n");
		printf("| (3) Chat With Someone           |\n");
		printf("| (4) Exit                        |\n");
		printf("+---------------------------------+\n");
		printf("What is your choice: ");
		scanf("%d",&chatRoomChoice);

		if(chatRoomChoice == 1){
			pthread_mutex_lock(&listMutex);
			sendGETLISTmessage();
			receiveGETLISTOKmessage(1);
			pthread_mutex_unlock(&listMutex);
		}else if(chatRoomChoice == 2){
			pthread_mutex_lock(&messageMutex);
			readUnreadMessage();
			pthread_mutex_unlock(&messageMutex);
		}else if(chatRoomChoice == 3){
			printf("Chat with? [Type '.' to return to menu] : ");
			memset(receiver,0,MAX_NAME_LENGTH);
			scanf("%s",receiver);
			if(strcmp(receiver,".")==0){
				continue;
			}else{
				printf("---------------------------------------\n");
				printf("Type your message to '%s'\n",receiver);
				printf("---------------------------------------\n");
				printf("(Type '.' to end your message)\n");
				i=0;
				while(1){
					memset(tempMessage[i],0,MAX_MESSAGE_SIZE);
					gets(tempMessage[i]);
					fflush(stdin);
					if(strcmp(tempMessage[i],".")==0){
						break;
					}
					i++;
				}

				// combine message together
				memset(sendMessage,0,MAX_MESSAGE_SIZE);
				currentPosition=0;
				for(j=0;j<i;j++){
					stringLength = strlen(tempMessage[j]);
					if(stringLength != 0){
					memcpy(sendMessage+currentPosition,tempMessage[j],stringLength);
					currentPosition+=stringLength;
					sendMessage[currentPosition]='\n';
					currentPosition++;
					}
				}
				sendMessage[currentPosition]='\0';
				pthread_mutex_lock(&listMutex);
				pthread_mutex_lock(&connectedMutex);
				sendHelloMessage();
				pthread_mutex_unlock(&connectedMutex);
				pthread_mutex_unlock(&listMutex);
			}
		}else if(chatRoomChoice == 4){
			closingClient();
		}else{
			printf("Invalid Selection. Please Try Again!\n");
		}
	}
}



int main(int argc, char** argv){
	if(argc != 3){
		printf("Error: Invalid Argument\n");
		printf("Usage: %s [Server IP Address] [Server Port Number]\n",argv[0]);
		exit(1);
	}
   
    // Operation Variables 
    int i;
    int returnListenThread;
    int serverAddrLength;
    struct sockaddr_in serverAddr;

    // create listen thread
 	if((returnListenThread = pthread_create(&listenThread,NULL,listening,NULL))!=0){
 		printf("Error: Couldn't create listen thread. Program terminate.\n");
 		exit(1);
 	}
 	// check if the listen thread is started and listening
 	pthread_mutex_lock(&listenValueMutex);
 		if(isListening == 1){
 			;
 		}else{
 			pthread_cond_wait(&listenReadyCond,&listenValueMutex);
 		}
 	pthread_mutex_unlock(&listenValueMutex);

 	printf("[ Opening an arbitrary listening port (%d) ... done ]\n",ntohs(listeningPort));
 	printf("Connecting to server ... ");
 	fflush(stdout);

 	// connect to server 
 	connectServerSocket = socket(AF_INET,SOCK_STREAM,0);
 	memset(&serverAddr,0,sizeof(serverAddr));
 	serverAddr.sin_family = AF_INET;
 	serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
 	serverAddr.sin_port = htons(atoi(argv[2]));
 	serverAddrLength = sizeof(serverAddr);
 	if(DEBUG_MODE){ printf("\nready to connect to server\n"); fflush(stdout);}
 	if(connect(connectServerSocket,(struct sockaddr *) &serverAddr, serverAddrLength)<0){
 		printf("\nError: Couldn't connect to server\n");
 		perror("connect()");
 		fflush(stdout);
 		exit(1);
 	}

 	printf("done\n\n");
 	printf("+------ Welcome to CSCI4430 Chatroom ------+\n");
 	fflush(stdout);

 	// initialization message box
 	for(i=0;i<MAX_MESSAGE;i++){
 		messages[i].unRead=0;
 	}

 	// loop into the main menu
 	mainMenuLoop();
 	// loop into the chatroom
 	chatRoomLoop();

	return 0;
}