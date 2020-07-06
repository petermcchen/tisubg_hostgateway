#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


int main(int argc , char *argv[])
{

    int sockfd = 0;
    int sent;
    unsigned char buff[512];

    if (argc <2) {
	printf("Pass your command option.");
	return;
    }
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd == -1){
        printf("Fail to create a socket.");
	return;
    }

    struct sockaddr_in info;
    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;

    //localhost test
    info.sin_addr.s_addr = inet_addr("127.0.0.1");
    info.sin_port = htons(5000);

    int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connection error");
    }
	buff[0] = 0x4; //two bytes len
	buff[1] = 0x0;
	buff[2] = 0xa; //subsystem id
	buff[3] = 0xb; //permit join command
	if (!strcmp(argv[1], "1")) { // 1: open
        	printf("Command: open\n");
		buff[4] = 0xff; //4 bytes parameters (open)
		buff[5] = 0xff;
		buff[6] = 0xff;
		buff[7] = 0xff;
	}
	else if (!strcmp(argv[1], "2")) { // 2: close
        	printf("Command: close\n");
		buff[4] = 0x0; //4 bytes parameters (close)
		buff[5] = 0x0;
		buff[6] = 0x0;
		buff[7] = 0x0;
	}
	sent = send(sockfd, buff, 8, 0);
	if(sent>=0)
		printf("Sent %d bytes\n", sent);
	else
		printf("Sent failed\n");

    printf("close Socket\n");
    close(sockfd);
    return 0;
}
