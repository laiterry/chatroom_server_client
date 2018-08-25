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


#define PORT 12315
unsigned short PROT_USED;
#define client_max 500
#define false 0
#define true 1

#include <time.h>

void gettime()
{
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime); /* get the current time */
    timeinfo = localtime(&rawtime);
    printf("%02d:%02d:%02d ",
            timeinfo->tm_hour, /* hour */
            timeinfo->tm_min, /* minute */
            timeinfo->tm_sec); /* second */

   
}



int client_count = 0;
pthread_t thread[client_max];
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;	



typedef struct Client {
	//q1:为什么不用sockaddr_in
	unsigned int IP_addr;
	unsigned short Port_num;
	unsigned short Listening_Port_num;
	char name[255];
	int client_sd;
	int used;
	int logined;
}Client;

Client client_inf[client_max];
Client client_overflow;


int init_server_socket(){

	int sd=socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in server_addr;

	long val=1;
	if(setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(long))==-1){
		perror("setsockopt");
		exit(1);
	}
	memset(&server_addr,0,sizeof(server_addr));

	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	server_addr.sin_port=htons(PROT_USED);
	
	if(bind(sd,(struct sockaddr *) &server_addr,sizeof(server_addr))<0){
		printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	
	if(listen(sd,client_max)<0){
		printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	
	printf("*********************************************************************\n");
	gettime();
	//printf("**Server Standby** IP:%s  PORT:%d\n", inet_ntoa(server_addr.sin_addr),PROT_USED);
	return sd;

}




void *pthread_prog(void *args) {

	int client_index = *(int*)args;
    unsigned char buf[500];
	int length;
	
	struct in_addr temp_inf;
	int sd=client_inf[client_index].client_sd;

	while(1){

		/***********************************reveive screen name + port number***********************************/
		printf("receive!!!!!!!!!!!\n");
		int recv_len=0;
		length=recv(sd,buf+recv_len,sizeof(buf),0);
		
		printf("here?\n");
		if(length==0)
		{
			temp_inf.s_addr=client_inf[client_index].IP_addr;

			printf("*********************************************************************\n");
			gettime();
			printf("Offline User Disconnect:IP %s Port %d \n",(char *)inet_ntoa(temp_inf),ntohs(client_inf[client_index].Port_num));
			pthread_mutex_lock(&mutex);
			
			client_inf[client_index].used=false;
			client_inf[client_index].logined=false;
			pthread_mutex_unlock(&mutex);
			pthread_exit(NULL);
		}
		
		while(length<=5);

		printf("here2?\n");

		int screen_name_len;
		recv_len = recv_len + length;
		
		

		if(buf[0]!=0x01)
		{
			gettime();
			printf("not login protocal\n");
			continue;
		}


		memcpy(&screen_name_len,buf+1,sizeof(int));
		screen_name_len=ntohl(screen_name_len);
		
		while(recv_len!=screen_name_len+11);
		memcpy(client_inf[client_index].name,buf+sizeof(char)+sizeof(int),screen_name_len);
		memcpy(&client_inf[client_index].Listening_Port_num,buf+sizeof(char)+sizeof(int)+screen_name_len+sizeof(int),sizeof(unsigned short));
		client_inf[client_index].name[screen_name_len]='\0';

		

		

		// printf("here3?\n");
		/***********************************check error***********************************/
		int i=0;
		int error1=false,error2=false,error3=false;

		pthread_mutex_lock(&mutex);
		if(client_count>=10)
		{
			error3=true;
		}
		pthread_mutex_unlock(&mutex);
		
		for(i=0;i<client_max;i++)
		{
			if((i!=client_index)&&(client_inf[i].logined==true))
			{
			if(strcmp(client_inf[i].name,client_inf[client_index].name)==0)
				error1=true;

			if((client_inf[i].IP_addr==client_inf[client_index].IP_addr)&&(client_inf[i].Listening_Port_num==client_inf[client_index].Listening_Port_num))
				error2=true;
			}
			// printf("zzz\n");
		}

		/***********************************error handleing***********************************/
		if(error3)
		{
			printf("*********************************************************************\n");
			gettime();
			printf("ERROR: maximum number of online clients\n");
			char error_msg[7];
					memset(error_msg,0,7);
					int temp=1;
					temp=htonl(temp);
					error_msg[0]=0xff;
					memcpy(error_msg+1,&temp,sizeof(int));
					error_msg[1+sizeof(int)]=0x03;
					error_msg[2+sizeof(int)]=0x00;
					int msg_len=7;
					int already_sent=0;
					while(1){
						int len=send(sd,error_msg+already_sent,msg_len-already_sent,0);
						if(len<=0){
							perror("send()");
						}else{
							already_sent+=len;
						}
						if(already_sent>=msg_len){
							break;
						}
					}
		
		}
		else if(error2)
		{


					char error_msg[7];
					memset(error_msg,0,7);
					int temp=1;
					temp=htonl(temp);
					error_msg[0]=0xff;
					memcpy(error_msg+1,&temp,sizeof(int));
					error_msg[1+sizeof(int)]=0x02;
					error_msg[2+sizeof(int)]=0x00;
					int msg_len=6;
					int already_sent=0;
					while(1){
						int len=send(sd,error_msg+already_sent,msg_len-already_sent,0);
						if(len<=0){
							perror("send()");
						}else{
							already_sent+=len;
						}
						if(already_sent>=msg_len){
							break;
						}
					}						
					printf("*********************************************************************\n");

					gettime();
					printf("Error: Same IP address:%s and same listening port  %d\n",(char *)inet_ntoa(client_inf[client_index].IP_addr),ntohs(client_inf[client_index].Listening_Port_num));
			}
		
		else if(error1)
		{
					char error_msg[7];
					memset(error_msg,0,7);
					int temp=1;
					temp=htonl(temp);
					error_msg[0]=0xff;
					memcpy(error_msg+1,&temp,sizeof(int));
					error_msg[1+sizeof(int)]=0x01;
					error_msg[2+sizeof(int)]=0x00;
					int msg_len=6;
					int already_sent=0;
					while(1){
						int len=send(sd,error_msg+already_sent,msg_len-already_sent,0);
						if(len<=0){
							perror("send()");
						}else{
							already_sent+=len;
						}
						if(already_sent>=msg_len){
							break;
						}
					}
					printf("*********************************************************************\n");
					gettime();
					printf("Error: Same screen name\n");
					
					
		}

		

		

		if((error2==false)&&(error1)==false&&(error3)==false)
		{
			pthread_mutex_lock(&mutex);
			client_inf[client_index].logined=true;
			client_count++;
			pthread_mutex_unlock(&mutex);
			// printf("777\n");
		temp_inf.s_addr=client_inf[client_index].IP_addr;
		printf("*********************************************************************\n");
		gettime();
		printf("          *******Login successfully*******\n");
		printf("                          Screename:%s\n",client_inf[client_index].name);
	    printf("             IP:%s",(char *)inet_ntoa(temp_inf));
		printf("       Listening Port:%d\n",ntohs(client_inf[client_index].Listening_Port_num));
		char reg_success[2];
		reg_success[0]=0x02;
		reg_success[1]=0x00;
		length=send(sd,reg_success,strlen(reg_success),0);
		if(length<=0){
			perror("send()");
		}

		break;
		}

}
	//q 上个while中定义的这里还能用吗？
	
	while(1)
	{

		length=recv(sd,buf,sizeof(buf),0);
		if(length==0)
		{	
			// printf("length==0\n");
			// temp_inf.s_addr=client_inf[client_index].IP_addr;
			// printf("Online user left: name %s IP %s Listening Port %d\n",client_inf[client_index].name,inet_ntoa(temp_inf),ntohs(client_inf[client_index].Listening_Port_num));
			printf("*********************************************************************\n");
			gettime();
			printf("Online user Disconnect\n");
			temp_inf.s_addr=client_inf[client_index].IP_addr;
			printf("       Screename:%s\n",client_inf[client_index].name);
			printf("       IP address:%s",(char *)inet_ntoa(temp_inf));
			printf("       Listening Port:%d\n",ntohs(client_inf[client_index].Listening_Port_num));


			close(sd);
			pthread_mutex_lock(&mutex);
			client_count--;
			client_inf[client_index].used=false;
			client_inf[client_index].logined=false;
			pthread_mutex_unlock(&mutex);
			pthread_exit(NULL);
		}
		else
		{
			/********************getting list********************/
			if(buf[0]!=0x03)
			{
				printf("*********************************************************************\n");
				gettime();
				printf("protocol error: not 0x03 for getting list\n");

				continue;
			}

		printf("*********************************************************************\n");
		gettime();
		printf("Send client list to   (current online clients:%d) \n",client_count); 
		printf("Screename:%s (IP address:%s Listening Port:%d )\n",client_inf[client_index].name,(char *)inet_ntoa(temp_inf),ntohs(client_inf[client_index].Listening_Port_num));



			// printf("else\n");
			char list_buff[1000];

			int current_len=0;
			unsigned char code=0x04;
			memcpy(list_buff,&code,1);
		
			current_len=5;
			pthread_mutex_lock(&mutex);
			int i;
			for(i=0;i<client_max;i++){
				if(client_inf[i].logined==1){
					int namelen=strlen(client_inf[i].name);
					int bnamelen=htonl(namelen);
					memcpy(list_buff+current_len,&bnamelen,sizeof(int));
					current_len+=sizeof(int);
					memcpy(list_buff+current_len,client_inf[i].name,namelen);
					current_len+=namelen;
					memcpy(list_buff+current_len,&client_inf[i].IP_addr,sizeof(int));
					current_len+=sizeof(int);
					memcpy(list_buff+current_len,&client_inf[i].Listening_Port_num,sizeof(unsigned short));
					current_len+=sizeof(unsigned short);
				}
			
}			current_len-=5;
			current_len=htonl(current_len);
			memcpy(list_buff+1,&current_len,sizeof(int));
			pthread_mutex_unlock(&mutex);
			int msg_len=ntohl(current_len)+5;
			int already_sent=0;
			while(1){
				int len=send(sd,list_buff+already_sent,msg_len-already_sent,0);
				if(len<=0){
					perror("send()");
				}else{
					already_sent+=len;
				}
				if(already_sent>=msg_len){
					break;
				}
			}
		}

		
	}



}	


int main(int argc, char** argv){
	
	 //use usr provided prot or not
		
		if(argc==2){
			PROT_USED=atoi(argv[1]);
		}else{
			PROT_USED=PORT;
		}

		
	int server_socket = init_server_socket();
	pthread_mutex_init(&mutex,NULL);



	int i;
	for(i=0;i<client_max;i++)
	{
		client_inf[i].used=false;
		client_inf[i].logined=false;
		}

	int val=1;
	if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(long))==-1){
		perror("setsockopt");
		exit(1);
	}

				


	while(1){

		int client_sd;
		struct sockaddr_in client_addr;
		int addr_len=sizeof(client_addr);

	
		if((client_sd=accept(server_socket,(struct sockaddr *) &client_addr,&addr_len))<0)
		{
			printf("accept erro: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		// printf("here222\n");
		//inet_ntoa:将一个IP转换成一个互联网标准点分格式的字符串。
		//ntohs:将一个无符号短整形数从网络字节顺序转换为主机字节顺序。
						
				printf("*********************************************************************\n");
				gettime();
		printf("Login attempt IP: %s Port: %d\n",(char *)inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));


		// printf("Login attempt IP: %s Port: %d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
			
			/*pthread_create
			第一个参数为指向线程标识符的指针。
			第二个参数用来设置线程属性。
			第三个参数是线程运行函数的起始地址。
			最后一个参数是运行函数的参数。*/


					pthread_mutex_lock(&mutex);
					int pass_index;
					int i;
					for(i=0;i<client_max;i++)
					{
						// printf("zheli2\n");
						if(client_inf[i].used==false)
							{
							 client_inf[i].IP_addr=client_addr.sin_addr.s_addr;
				   			 client_inf[i].Port_num=client_addr.sin_port;
				  			 client_inf[i].client_sd=client_sd;
				  			 client_inf[i].used=true;
				  			 pass_index=i;
				  			 break;
							}
					}
				
					// printf("here\n");
					
						int ret_val_err= pthread_create(&thread[pass_index], NULL, pthread_prog, &pass_index);
						if (ret_val_err != 0)
				 		{
				 			printf("can't create thread: %s\n", strerror(ret_val_err));
				       		exit(0);
			      	 	}

			      	 pthread_mutex_unlock(&mutex);
      	 	}
	close(server_socket);
	return 0;
}

