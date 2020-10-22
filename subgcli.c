/*
 *  ======== subgcli.c ========
 *  Creator: Adam
 *  Purpose: This is a CLI program for user's interaction against CC1310 host_collector.
 *  Date: 2020/7/14
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

typedef enum {
    Cmd_Help = 1,
    Cmd_Bye = 2,
    Cmd_ResetPan = 3,
    Cmd_AllowJoin = 4,
    Cmd_NetStat = 5,
    Cmd_ListDev = 6,
    Cmd_RmvDev = 7,
    Cmd_Version = 8, // mc_chen
    Cmd_OadFile = 9, // mc_chen
    Cmd_OadUpdate = 10, // mc_chen
    Cmd_OadStatus = 11, // mc_chen
} CmdId_t;

int sockfd = 0;

unsigned long int inet_addr(const char *);
int validate_params(int, char **, int);
int send_allow_join(int);
int send_allow_join2(unsigned int); // mc_chen
int send_get_version_info(void); // mc_chen
int recv_version_info(void); //mc_chen
int send_oad_file(char *); //mc_chen
int recv_oad_file(void); //mc_chen
int send_get_network_info(void);
int send_get_device_list(void);
int send_remove_device(int);
int recv_network_info(void);
int recv_device_list(void);
unsigned short get_uint16(unsigned char *, int);
unsigned int get_uint32(unsigned char *, int);
void get_mac(unsigned char *, int, unsigned char *);
int create_socket(void);

int main(int argc , char *argv[])
{
    char input_cmd[100];
    char *token;
    char *tokens[50];
    char linestr[500];
    char c_chan_mask_str[200];
    char c_pan_id_str[100];
    char pan_id_str[10];
    char token_str[10];
    char answ[10];
    char inpc;
    int tok_cnt;
    int i, j, flag;
    unsigned int pan_id, interval;
    char version[] = "v1.2";

    time_t t;
    FILE *fp, *fp_temp;

    printf("SubgCLI version %s\n", version);
    while(1) {
        printf("subgcli> ");
        input_cmd[0] = '\0';
        scanf("%[^\n]", input_cmd);
        getchar(); //to clean the ending '\n'
        //printf("%s\n", input_cmd);
        if(strlen(input_cmd)==0) {
            continue;
        }
        token = strtok(input_cmd, " ");
        tok_cnt = 0;
        while( token != NULL ) {
              printf( " %s\n", token ); //printing each token
              tokens[tok_cnt++] = token;
              token = strtok(NULL, " ");
        }
        printf( "tok_cnt=%d\n", tok_cnt); //printing tok_cnt

        if(strcmp(tokens[0], "resetpan")==0) {
            if(validate_params(Cmd_ResetPan, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'resetpan 89', 'resetpan 89 90 100 .... 114'\n");
                continue;
            }

            printf("Warning: The PAN configuration, including sensors data is going to be erased!\n");
            while(1) {
                printf("Press 'y' to continue, 'n' to abort.\n");
                answ[0] = '\0';
                scanf("%[^\n]", answ);
                getchar(); //to clean the ending '\n'
                if(!strcmp(answ, "y")) {
                    printf("Reboot the gateway for taking effect after command done.\n");
                    break;
                } else if(!strcmp(answ, "n")) {
                    printf("'resetpan' is aborted.\n");
                    break;
                }
            }

            if(!strcmp(answ, "n"))
                continue;

            c_chan_mask_str[0] = 9; //tab character
            strcpy(c_chan_mask_str+1, "config-channel-mask =");
            for(i=1;i<tok_cnt;i++) {
                //printf(" %s", tokens[i]);
                if(i==tok_cnt-1)
                    sprintf(token_str, " %s\n", tokens[i]);
                else
                    sprintf(token_str, " %s", tokens[i]);
                strcat(c_chan_mask_str, token_str);
            }


            c_pan_id_str[0] = 9; //tab character
            strcpy(c_pan_id_str+1, "config-pan-id =");
            //to get a random number for pan id
            srand((unsigned) time(&t));
            while(1) {
                pan_id = 0;
                for(i=0;i<4;i++) {
                    j = rand() % 0x10;
                    pan_id = pan_id + (j<<(i*4));
                }
                if(pan_id!=0 && pan_id!=0xffff) {
                    break;
                }
            }
            sprintf(pan_id_str, " 0x%x\n", pan_id);
            strcat(c_pan_id_str, pan_id_str);

            fp = fopen("collector.cfg", "r");
            if(fp==NULL) {
                printf("Cannot find 'collector.cfg' in current directory.\n");
                continue;
            }
            fp_temp = fopen("temp.cfg", "w+");
            if(fp_temp==NULL) {
                printf("Cannot create 'temp.cfg' in current directory.\n");
                continue;
            }
            while(fgets(linestr, 500, fp)!=NULL ) {
                //puts(linestr);
                flag = 1;
                if(strstr(linestr, "config-channel-mask")) {
                    if(!((int)linestr[0]==9 && (int)linestr[1]==0x3b)) { //This is not a comment line.
                        flag = 0;
                        //Replace with the user's input (channel mask)
                        fputs(c_chan_mask_str, fp_temp);
                    }
                } else if(strstr(linestr, "config-pan-id")) {

                    if(!((int)linestr[0]==9 && (int)linestr[1]==0x3b)) { //This is not a comment line.
                        flag = 0;
                        //Replace with new random pan id
                        fputs(c_pan_id_str, fp_temp);
                        printf("New PAN Id: 0x%04x\n", pan_id);
                    }
                }
                if(flag)
                    fputs(linestr, fp_temp);
            }
            fclose(fp);
            fclose(fp_temp);

            remove("collector.cfg");
            rename("temp.cfg", "collector.cfg"); //rename the temp file
            remove("nv-simulation.bin"); //delete the file for simulating non-volatile section

        } else if(strcmp(tokens[0], "version")==0) {
            if(validate_params(Cmd_Version, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'version'\n");
                continue;
            }

            if(create_socket()) {
                continue;
            }

            if(send_get_version_info()) {
                printf("Command failure\n");
            } else {
                if(recv_version_info())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "netstat")==0) {
            if(validate_params(Cmd_NetStat, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'netstat'\n");
                continue;
            }

            if(create_socket()) {
                continue;
            }

            if(send_get_network_info()) {
                printf("Command failure\n");
            } else {
                if(recv_network_info())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "listdev")==0) {
            if(validate_params(Cmd_ListDev, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'listdev'\n");
                continue;
            }
            if(create_socket())
                continue;
            if(send_get_device_list()) {
                printf("Command failure\n");
            } else {
                if(recv_device_list())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "allowjoin")==0) {
            if(validate_params(Cmd_AllowJoin, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'allowjoin on', 'allowjoin off'\n");
                continue;
            }
            if(create_socket())
                continue;
	    /**************************************************/
	    if (!strcmp(tokens[1], "on")) { // read 3rd if on...
		interval = strtoul(tokens[2], NULL, 16);
	    }
	    else {
		interval = 0;
	    }
            printf("Permit join interval: 0x%x\n", interval);
            if(send_allow_join2(interval)) {
                printf("Command failure\n");
            }
	    /**************************************************/
            //if(send_allow_join((!strcmp(tokens[1], "on"))?1:0)) {
            //    printf("Command failure\n");
            //}
	    sleep(1);
            close(sockfd);

        } else if(strcmp(tokens[0], "rmdev")==0) {
            if(validate_params(Cmd_RmvDev, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("'rmdev <short addr>' where <short addr> is expressed in hex.\n");
                printf("e.g. 'rmdev 0xa', 'rmdev a'\n");
                continue;
            }

            if(create_socket())
                continue;
            if(send_remove_device(strtoul(tokens[1], NULL, 16))) {
                printf("Command failure\n");
            }
	    sleep(1);
            close(sockfd);

        } else if(strcmp(tokens[0], "oadfile")==0) {
            if(validate_params(Cmd_OadFile, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'oadfile /mnt/data/HEM/db/sensor_oad.bin'\n");
                continue;
            }
            if(create_socket())
                continue;
            if(send_oad_file(tokens[1])) {
                printf("Command failure\n");
            } else {
                printf("Call recv_oad_file\n");
                if(recv_oad_file())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "oadupdate")==0) {
            if(validate_params(Cmd_OadUpdate, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'oadupdate 0x01'\n");
                continue;
            }
            if(create_socket())
                continue;
            if(send_oad_update(strtoul(tokens[1], NULL, 16))) {
                printf("Command failure\n");
            } else {
                if(recv_oad_update())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "oadstatus")==0) {
            if(validate_params(Cmd_OadStatus, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'oadstatus'\n");
                continue;
            }
            if(create_socket())
                continue;
            if(send_oad_status()) {
                printf("Command failure\n");
            } else {
                if(recv_oad_status())
                    printf("Command failure\n");
            }
            close(sockfd);

        } else if(strcmp(tokens[0], "help")==0) {
            printf("Supported commands: resetpan, allowjoin, netstat, listdev, rmdev, bye, help\n");
            printf("resetpan: To reset the PAN configuration.\n");
            printf("allowjoin: To open or close joining for new devices.\n");
            printf("netstat: To get the network information.\n");
            printf("listdev: To get the devices list.\n");
            printf("rmdev: To remove a device.\n");
            printf("bye: To exit the CLI program.\n");
        } else if(strcmp(tokens[0], "bye")==0) {
            printf("Good bye!\n");
            exit(0);
        } else {
            printf("Unsupported command\n");
        }
    }
}

int send_allow_join(int on_off)
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x4; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0xb; //permit join command

    if(on_off==1) {
        buff[4] = 0xff; //4 bytes parameters (open)
        buff[5] = 0xff;
        buff[6] = 0xff;
        buff[7] = 0xff;
    } else {
        buff[4] = 0x0; //4 bytes parameters (close)
        buff[5] = 0x0;
        buff[6] = 0x0;
        buff[7] = 0x0;
    }

    sent = send(sockfd, buff, 8, 0);
    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}

int send_allow_join2(unsigned int val)
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x4; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0xb; //permit join command

    buff[4] = val & 0x000000FF; //4 bytes parameters (open)
    buff[5] = (val>>8) & 0x000000FF;
    buff[6] = (val>>16) & 0x000000FF;
    buff[7] = (val>>24) & 0x000000FF;
    printf("interval: 0x%x, 0x%x, 0x%x, 0x%x\n", buff[4], buff[5], buff[6], buff[7]);

    sent = send(sockfd, buff, 8, 0);
    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}

int send_get_version_info() // mc_chen
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x0; // no data
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x12; //get version info command APPSRV_GET_SW_VER_REQ, refer to appsrv.h file

    sent = send(sockfd, buff, 4, 0);

    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}

int send_oad_file(char *fname) // mc_chen
{
    unsigned char buff[64];
    int len;
    int sent;

    printf("File Path=%s\n", fname);
    if (fname == NULL)
	len = 0;
    else
	len = strlen(fname);
    printf("File Path %d bytes\n", len);
    buff[0] = len+2; //two bytes len + file path
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x14; //set oad fw file path, APPSRV_OAD_FW_FILE_REQ

    buff[4] = len & 0xff;
    buff[5] = len >> 8;
    strncpy(&buff[6], fname, len);

    sent = send(sockfd, buff, 6+len, 0);
    if(sent>=0) {
        printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        printf("Sent failed\n");
        return(-1);
    }
}

int send_oad_update(int short_addr) // mc_chen
{
    unsigned char buff[64];
    int sent;

    printf("Short Address=%d\n", short_addr);
    buff[0] = 0x2; //two bytes len for short address
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x16; //do oad fw update, APPSRV_OAD_FW_UPDATE_REQ

    buff[4] = short_addr & 0xff;
    buff[5] = short_addr >> 8;

    sent = send(sockfd, buff, 6, 0);
    if(sent>=0) {
        printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        printf("Sent failed\n");
        return(-1);
    }
}

int send_oad_status() // mc_chen
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x0; // no data
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x18; // query collector state, APPSRV_OAD_GET_STATE_REQ

    sent = send(sockfd, buff, 4, 0);
    if(sent>=0) {
        printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        printf("Sent failed\n");
        return(-1);
    }
}

int send_get_network_info()
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x0; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x3; //get network info command

    sent = send(sockfd, buff, 4, 0);

    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}


int send_get_device_list()
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x0; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0x6; //get devices array

    sent = send(sockfd, buff, 4, 0);
    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}

int send_remove_device(int short_addr)
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x2; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = 0xf; //remove device

    buff[4] = short_addr & 0xff;
    buff[5] = short_addr >> 8;

    sent = send(sockfd, buff, 6, 0);
    if(sent>=0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    } else {
        //printf("Sent failed\n");
        return(-1);
    }
}


int recv_oad_status() // mc_chen
{
    unsigned char rxbuff[32];
    int count, i, index, len, status;
    unsigned char subsystem_id, cmd;

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);

    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    if(len != 4) // int
        return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=0x19) // APPSRV_OAD_GET_STATE_CNF 0x19
        return(-1);

    status = get_uint32(rxbuff, index);

    printf("APPSRV_OAD_GET_STATE_CNF: %d\n", status);

    return(0);
}

int recv_oad_update() // mc_chen
{
    unsigned char rxbuff[32];
    int count, i, index, len, status;
    unsigned char subsystem_id, cmd;

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    if(len != 4) // int
        return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=0x17) // APPSRV_OAD_FW_UPDATE_CNF 0x17
        return(-1);

    status = get_uint32(rxbuff, index);

    printf("APPSRV_OAD_FW_UPDATE_CNF: %d\n", status);

    return(0);
}

int recv_oad_file() // mc_chen
{
    unsigned char rxbuff[32];
    int count, i, index, len, status;
    unsigned char subsystem_id, cmd;

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);

    //
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    //

    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    if(len != 4) // int
        return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=0x15) // APPSRV_OAD_FW_FILE_CNF 0x15
        return(-1);

    status = get_uint32(rxbuff, index);

    printf("APPSRV_OAD_FW_FILE_CNF: %d\n", status);

    return(0);
}

int recv_version_info() // mc_chen
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd, transport, product, major, minor, maint, version;
    unsigned char sdk1, sdk2, sdk3, sdk4;
    char *transport_str[] = {"", "", "Standard RPC frame", "Extended RPC frame"};
    char *product_str[] = {"Z-Stack", "TI-15.4-Stack"};

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    if(len < 6) // transport, product, major, minor, maint, version
        return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=0x13) // APPSRV_GET_SW_VER_CNF 0x13
        return(-1);

    transport = rxbuff[index++];
    product = rxbuff[index++];
    major = rxbuff[index++];
    minor = rxbuff[index++];
    maint = rxbuff[index++];
    sdk1 = rxbuff[index++];
    sdk2 = rxbuff[index++];
    sdk3 = rxbuff[index++];
    sdk4 = rxbuff[index++];
    version = rxbuff[index++];

    printf("MT Transport: %s\n", transport_str[transport]);
    printf("MT Product: %s\n", product_str[product]);
    printf("MT Major: %d\n", major);
    printf("MT Minor: %d\n", minor);
    printf("MT Maint: %d\n", maint);
    printf("Linux SDK Version: %d,%d,%d,%d\n", sdk1, sdk2, sdk3, sdk4);
    printf("SW Version: %d\n", version);

    return(0);
}

int recv_network_info()
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd, status, coord_mac[8];
    unsigned char channel, fh, security_en, network_mode, state;
    unsigned short pan_id, coord_short_addr;
    char *status_str[] = {"Start a new network", "Start a network from NV data"};
    char *nwkmode_str[] = {"", "Beacon enabled", "Non beacon", "Frequency hopping"};
    char *state_str[] = {"Initialized waiting for user to start",
                         "Starting coordinator",
                         "Restoring coordinator from NV",
                         "Started",
                         "Restored",
                         "Joining allowed for new devices",
                         "Joining not allowed for new devices"};

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    if(len < 18)
        return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=5) //APPSRV_GET_NWK_INFO_CNF
        return(-1);

    status = rxbuff[index++];
    pan_id = get_uint16(rxbuff, index);
    index += 2;
    coord_short_addr = get_uint16(rxbuff, index);
    index += 2;
    get_mac(rxbuff, index, coord_mac);
    index += 8;
    channel = rxbuff[index++];
    fh = rxbuff[index++];
    security_en = rxbuff[index++];
    network_mode = rxbuff[index++];
    state = rxbuff[index++];
    /*
    printf("%2x %4x %4x [%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x] %d %2x %2x %2x %2x\n", status, pan_id, coord_short_addr,
           coord_mac[0], coord_mac[1], coord_mac[2], coord_mac[3], coord_mac[4], coord_mac[5], coord_mac[6], coord_mac[7],
           channel, fh, security_en, network_mode, state);
    */

    printf("Status: %s\n", status_str[status]);
    printf("PAN Id: 0x%04x\n", pan_id);
    printf("Coordinator short addr: 0x%04x\n", coord_short_addr);
    printf("Coordinator extension addr: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
           coord_mac[0], coord_mac[1], coord_mac[2], coord_mac[3], coord_mac[4], coord_mac[5], coord_mac[6], coord_mac[7]);
    printf("Channel: %d\n", channel);
    printf("Frequency hopping: %s\n", fh==1?"Enabled":"Disabled");
    printf("Security: %s\n", security_en==1?"Enabled":"Disabled");
    printf("Network mode: %s\n", nwkmode_str[network_mode]);
    printf("State: %s\n", state_str[state]);

    return(0);
}



int recv_device_list()
{
    unsigned char rxbuff[907]; //consider 50 devices at max
    int count, i, index, len, num_dev;
    unsigned char subsystem_id, cmd, status, mac[8];
    unsigned char pan_coord, ffd, mains_power, rx_onidle, security, alloc_addr;
    unsigned short pan_id, short_addr;
    char *status_str[] = {"Success", "Failure"};

    count = recv(sockfd, rxbuff, sizeof(rxbuff) ,0);
    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */
    if(count<4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    //if(len < 18)
    //    return(-1);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if(cmd!=7) //APPSRV_GET_DEVICE_ARRAY_CNF
        return(-1);

    status = rxbuff[index++];
    num_dev = get_uint16(rxbuff, index);
    index += 2;
    printf("Status: %s\n", status_str[status]);
    printf("Number of devices: %d\n", num_dev);
    for(i=0;i<num_dev;i++) {
        pan_id = get_uint16(rxbuff, index);
        index += 2;
        short_addr = get_uint16(rxbuff, index);
        index += 2;
        get_mac(rxbuff, index, mac);
        index += 8;
        pan_coord = rxbuff[index++];
        ffd = rxbuff[index++];
        mains_power = rxbuff[index++];
        rx_onidle = rxbuff[index++];
        security = rxbuff[index++];
        alloc_addr = rxbuff[index++];

        printf("------------------\n");
        printf("PAN Id: 0x%04x\n", pan_id);
        printf("Short addr: 0x%04x\n", short_addr);
        printf("Extension addr: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6], mac[7]);
        printf("Pan coordinator: %s\n", pan_coord==1?"true":"false");
        printf("Full func device: %s\n", ffd==1?"true":"false");
        printf("Mains powered: %s\n", mains_power==1?"true":"false");
        printf("Rx On when idle: %s\n", rx_onidle==1?"true":"false");
        printf("Security support: %s\n", security==1?"true":"false");
        printf("Allocation of short addr is needed: %s\n", alloc_addr==1?"true":"false");
    }
    /*
    printf("%2x %4x %4x [%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x] %d %2x %2x %2x %2x\n", status, pan_id, coord_short_addr,
           coord_mac[0], coord_mac[1], coord_mac[2], coord_mac[3], coord_mac[4], coord_mac[5], coord_mac[6], coord_mac[7],
           channel, fh, security_en, network_mode, state);
    */
    return(0);
}



unsigned short get_uint16(unsigned char *buff, int index)
{
    return(buff[index+1]<<8|buff[index]);
}

unsigned int get_uint32(unsigned char *buff, int index)
{
    return(buff[index+3]<<24|buff[index+2]<<16|buff[index+1]<<8|buff[index]);
}

void get_mac(unsigned char *buff, int index, unsigned char *macp)
{
    int i;
    for(i=0;i<8;i++)
        macp[i] = buff[index+(7-i)];
    return;
}

int validate_params(int command, char **tokens, int tok_cnt)
{
    int result = 0;
    int i;
    int flag, value;
    //Caution: tok_cnt includes the command byte. tokens[0] is the command byte.
    switch(command) {
        case(Cmd_ResetPan):
            if(tok_cnt>1 && tok_cnt<28) { //channel mask as the parameters. 1 channel at least, and 26 channels at maximum.
                flag = 0;
                for(i=1;i<tok_cnt;i++) {
                    if(atoi((const char *)tokens[i])<89 || atoi((const char *)tokens[i])>114) {
                        flag = 1;
                        break;
                    }
                }
                if(flag)
                    result = -1;
            } else {
                result = -1;
            }

        break;
        case(Cmd_AllowJoin):
	    //if(tok_cnt!=2||(strcmp((const char *)tokens[1], "on") && strcmp((const char *)tokens[1], "off")))
		// result = -1;
/*****************************************************/
	    printf("tok_cnt=%d\n", tok_cnt);
            if((tok_cnt!=2) && (!strcmp((const char *)tokens[1], "off")))
                result = -1;
            if((tok_cnt!=3) && (!strcmp((const char *)tokens[1], "on")))
                result = -1;
/*****************************************************/
        break;
        case(Cmd_NetStat):
            if(tok_cnt!=1)
                result = -1;
        break;
        case(Cmd_ListDev):
            if(tok_cnt!=1)
                result = -1;
        break;
        case(Cmd_RmvDev):
            if(tok_cnt!=2)
                result = -1;
            else {
                value = strtoul(tokens[1], NULL, 16); //hex string to integer
                if(!(value>0 && value<0x10000))
                    result = -1;
            }
        break;
        case(Cmd_Version): // mc_chen
            if(tok_cnt!=1)
                result = -1;
        break;
        case(Cmd_OadFile): // mc_chen
            if((tok_cnt!=2)&&(tok_cnt!=1))
                result = -1;
        break;
        case(Cmd_OadUpdate): // mc_chen
            if(tok_cnt!=2)
                result = -1;
            else {
                value = strtoul(tokens[1], NULL, 16); //hex string to integer
                if(!(value>0 && value<0x10000))
                    result = -1;
            }
        break;
        case(Cmd_OadStatus): // mc_chen
            if(tok_cnt!=1)
                result = -1;
        break;
        default:
        break;
    }
    return(result);
}


int create_socket()
{
    int err;
    struct sockaddr_in info;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd == -1){
        printf("Fail to create a socket.\n");
        return(-1);
    }

    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;

    info.sin_addr.s_addr = inet_addr("127.0.0.1");
    info.sin_port = htons(5000);

    err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        printf("Connect collector socket error.\n");
        close(sockfd);
        return(-1);
    }
    return(0);
}
