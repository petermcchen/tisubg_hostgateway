#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int ConvertTwosComplementByteToInteger(char rawValue)
{
    // If a positive value, return it
    if ((rawValue & 0x80) == 0)
    {
        printf("return as positive.");
        return rawValue;
    }

    // Otherwise perform the 2's complement math on the value
    return (char)(~(rawValue - 0x01)) * -1;
}

int main(int argc , char *argv[])
{

    int sockfd = 0;
    int count;
    int i, offset;
    unsigned short payloadlen, framectrl;
    unsigned short num;
    unsigned int inum;
    short ambience_t, object_t, saddr;
    int rssi;
    int endianness; /* 0: little, 1: big */
    char *c;

    i = 1;
    c = (char *) &i;
    if (*c)
        endianness=0;
    else
        endianness=1;

retry:
    sockfd = socket(AF_INET , SOCK_STREAM , 0);

    if (sockfd == -1){
        printf("Fail to create a socket.");
	sleep (1);
	goto retry;
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

    //Send a message to server
    //char message[] = {"Hi there"};
    char receiveMessage[512] = {};
    //send(sockfd,message,sizeof(message),0);
    while(1)
    {
	offset=0;
    	count = recv(sockfd,receiveMessage,sizeof(receiveMessage),0);
     	for(i = 0;i<count;i++)
     	{
     		//printf("%02x ", (unsigned char)receiveMessage[i]);
     	}
     	printf("\n");
    	/* check data begin */
     	printf("Message process begin...\n");
    	num=0;
	num = *(unsigned short *)&receiveMessage[0];
    	//printf("payloadlen: %d (before swap)\n", num);
	if (endianness)
		payloadlen = (num>>8) | (num<<8); // swap 2 bytes
	else
		payloadlen = num;
    	//printf("payloadlen: %d\n", payloadlen);
	i=0;
	if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x00)) { // APPSRV_SYS_ID_RPC, DEVICE_JOINED_IND
    		printf("CMD1 DEVICE_JOINED_IND not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x02)) { // APPSRV_SYS_ID_RPC, NWK_INFO_IND
    		printf("CMD1 NWK_INFO_IND not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x05)) { // APPSRV_SYS_ID_RPC, GET_NWK_INFO_CNF
    		printf("CMD1 GET_NWK_INFO_CNF not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x07)) { // APPSRV_SYS_ID_RPC, GET_DEVICE_ARRAY_CNF
    		printf("CMD1 GET_DEVICE_ARRAY_CNF not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x08)) { // APPSRV_SYS_ID_RPC, DEVICE_NOTACTIVE_UPDATE_IND
    		printf("CMD1 DEVICE_NOTACTIVE_UPDATE_IND not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x09)) { // APPSRV_SYS_ID_RPC, DEVICE_DATA_RX_IND
    		//printf("APPSRV_SYS_ID_RPC, DEVICE_DATA_RX_IND.\n");
		if (receiveMessage[i+4] == 0x02) { // ADDTYPE_SHORT
    			num=0;
			num = *(unsigned short *)&receiveMessage[i+5];
			if (endianness)
				saddr = (num>>8) | (num<<8); // swap 2 bytes
			else
				saddr = num;
    			printf("short addr: %d\n", saddr);
			offset += 2;
		}
		else if (receiveMessage[i+4] == 0x03) { // ADDTYPE_EXT
			offset += 8;
		}
		char tmp = receiveMessage[i+offset+5];
		//rssi = tmp;		
		rssi = ConvertTwosComplementByteToInteger(tmp);		
    		printf("rssi: %d\n", rssi);

		/* Sub-commands processing */
		printf("command id: %d\n", receiveMessage[i+offset+6]);
		if (receiveMessage[i+offset+6] == 0x02) { // CONFIG_RSP
    			printf("Receive CONFIG_RSP command (not implement yet).\n");
		}
		else if (receiveMessage[i+offset+6] == 0x05) { // SENSOR_DATA
			int k=i+offset+7;
    			printf("Receive SENSOR_DATA command.\n");
			printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n", receiveMessage[k+7]&0xFF, receiveMessage[k+6]&0xFF, receiveMessage[k+5]&0xFF, receiveMessage[k+4]&0xFF, receiveMessage[k+3]&0xFF, receiveMessage[k+2]&0xFF, receiveMessage[k+1]&0xFF, receiveMessage[k]&0xFF);
			
    			num=0;
			num = *(unsigned short *)&receiveMessage[i+offset+15];
    			//printf("framecontrol: %x (before swap).\n", num);
			if (endianness)
				framectrl = (num>>8) | (num<<8); // swap 2 bytes
			else
				framectrl = num;
    			printf("framecontrol: %x.\n", framectrl);
			k=i+offset+17;
			if (framectrl & 0x01) { // tempSensor
    				printf("Found tempSensor data.\n");
				num=0;
				num = *(unsigned short *)&receiveMessage[k];
				if (endianness)
					ambience_t = (num>>8) | (num<<8); // swap 2 bytes
				else
					ambience_t = num;
				k = k+2;
				num = *(unsigned short *)&receiveMessage[k];
				if (endianness)
					object_t = (num>>8) | (num<<8); // swap 2 bytes
				else
					object_t = num;
    				printf("- ambience temperature: %d, object temperature: %d.\n", ambience_t, object_t);
				k = k+2;
			}
			if (framectrl & 0x02) { // lightSensor
    				printf("Found lightSensor data.\n");
				k = k+2;
			}
			if (framectrl & 0x04) { // humiditySensor
    				printf("Found humiditySensor data.\n");
				k = k+4;
			}
			if (framectrl & 0x08) { // msgStats
    				printf("Found msgStats data.\n");
				num=0;
				num = *(unsigned short *)&receiveMessage[k+4];
				short msgcounts;
				if (endianness)
					msgcounts = (num>>8) | (num<<8); // swap 2 bytes
				else
					msgcounts = num;
    				printf("- msgsAttempted: %d.\n", msgcounts);
				num=0;
				num = *(unsigned short *)&receiveMessage[k+6];
				short msgsent;
				if (endianness)
					msgsent = (num>>8) | (num<<8); // swap 2 bytes
				else
					msgsent = num;
    				printf("- msgsSent: %d.\n", msgsent);
				printf("Device (%d) has packet loss rate: %f\n", saddr, (float)(msgcounts-msgsent)/(float)msgcounts);
				num=0;
				num = *(unsigned short *)&receiveMessage[k+44];
				short e2edelay;
				if (endianness)
					e2edelay = (num>>8) | (num<<8); // swap 2 bytes
				else
					e2edelay = num;
    				printf("- avgE2EDelay: %d.\n", e2edelay);
				num=0;
				num = *(unsigned short *)&receiveMessage[k+46];
				short maxe2edelay;
				if (endianness)
					maxe2edelay = (num>>8) | (num<<8); // swap 2 bytes
				else
					maxe2edelay = num;
    				printf("- worstCaseE2EDelay: %d.\n", maxe2edelay);
				k = k+48; // 2*24, ATTENTION!!!
			}
			if (framectrl & 0x10) { // configSettings
    				printf("Found configSettings data.\n");
				inum=0;
				inum = *(unsigned int *)&receiveMessage[k];
				unsigned int reportinterval;
				if (endianness)
					reportinterval = ((inum>>24)&0xff) | // move byte 3 to byte 0
			                    ((inum<<8)&0xff0000) | // move byte 1 to byte 2
			                    ((inum>>8)&0xff00) | // move byte 2 to byte 1
			                    ((inum<<24)&0xff000000); // byte 0 to byte 3
				else
					reportinterval = inum;
    				printf("- reporting interval: %d.\n", reportinterval);
				inum = *(unsigned int *)&receiveMessage[k+4];
				unsigned int pollinterval;
				if (endianness)
					pollinterval = ((inum>>24)&0xff) | // move byte 3 to byte 0
			                    ((inum<<8)&0xff0000) | // move byte 1 to byte 2
			                    ((inum>>8)&0xff00) | // move byte 2 to byte 1
			                    ((inum<<24)&0xff000000); // byte 0 to byte 3
				else
					pollinterval = inum;
    				printf("- polling interval: %d.\n", pollinterval);
				k = k+8;
			}
			if (framectrl & 0x8000) { // batteryVoltage (ACCTON)
    				printf("(c) Found batteryVoltage data.\n");
			}
		}
		else if (receiveMessage[i+offset+6] == 0x12) { // DATE_CODE (ACCTON)
    			printf("(c) Receive DATE_CODE command (not implement yet).\n");
		}
		else if (receiveMessage[i+offset+6] == 0x13) { // ZONE_STATUS (ACCTON)
    			printf("(c) Receive ZONE_STATUS command (not implement yet).\n");
		}
		else {
    			printf("Unknown command.\n");
		}
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x0a)) { // APPSRV_SYS_ID_RPC, COLLECTOR_STATE_CNG_IND
    		printf("CMD1 COLLECTOR_STATE_CNG_IND not implemented yet.\n");
	}
	else if ((receiveMessage[i+2] == 0x4a) && (receiveMessage[i+3] == 0x0c)) { // APPSRV_SYS_ID_RPC, SET_JOIN_PERMIT_CNF
    		printf("CMD1 SET_JOIN_PERMIT_CNF not implemented yet.\n");
	}
	else {
    		printf("Unknown message.\n");
		break;
	}
     	printf("Message process end...\n");
    	/* check data end */
    }
    printf("close Socket\n");
    close(sockfd);
    return 0;
}
//swapped = ((num>>24)&0xff) | // move byte 3 to byte 0
//                    ((num<<8)&0xff0000) | // move byte 1 to byte 2
//                    ((num>>8)&0xff00) | // move byte 2 to byte 1
//                    ((num<<24)&0xff000000); // byte 0 to byte 3
