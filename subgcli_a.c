 /*
  * FILE NAME: subgcli.c
  *
  * DESCRIPTION: This is a CLI program for user's interaction against 
  *              CC1310 host_collector via socket interface.
  *
  * NOTES:
  *
  * CREATOR: Adam Wu, 2020/7/14
  *
  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>

typedef int bool;
#define false 0  
#define true  1  

//APPSRV service command code
#define APPSRV_DEVICE_JOINED_IND 0
#define APPSRV_DEVICE_LEFT_IND 1
#define APPSRV_NWK_INFO_IND 2
#define APPSRV_GET_NWK_INFO_REQ 3
#define APPSRV_GET_NWK_INFO_RSP 4
#define APPSRV_GET_NWK_INFO_CNF 5
#define APPSRV_GET_DEVICE_ARRAY_REQ 6
#define APPSRV_GET_DEVICE_ARRAY_CNF 7
#define APPSRV_DEVICE_NOTACTIVE_UPDATE_IND 8
#define APPSRV_DEVICE_DATA_RX_IND 9
#define APPSRV_COLLECTOR_STATE_CNG_IND 10
#define APPSRV_SET_JOIN_PERMIT_REQ 11
#define APPSRV_SET_JOIN_PERMIT_CNF 12
#define APPSRV_TX_DATA_REQ 13
#define APPSRV_TX_DATA_CNF 14
#define APPSRV_RMV_DEVICE_REQ 15
#define APPSRV_RMV_DEVICE_RSP 16
#define APPSRV_OAD_FW_FILE_REQ 20
#define APPSRV_OAD_FW_FILE_CNF 21
#define APPSRV_OAD_FW_UPDATE_REQ 22
#define APPSRV_OAD_FW_UPDATE_CNF 23

//Status of APPSRV service
#define COLLECTOR_STATUS_SUCCESS 0
#define COLLECTOR_STATUS_DEV_NOT_FOUND 1
#define COLLECTOR_STATUS_INVALID_STATE 2
#define COLLECTOR_STATUS_INVALID_FILE 3

typedef enum {
    Cmd_Help = 1,
    Cmd_Bye = 2,
    Cmd_ResetPan = 3,
    Cmd_AllowJoin = 4,
    Cmd_NetStat = 5,
    Cmd_ListDev = 6,
    Cmd_RmvDev = 7,
    Cmd_CaliDev = 8,
    Cmd_OadF = 9,
    Cmd_OadU = 10
} CmdId_t;

typedef enum
{
    /*! Configuration message, sent from the collector to the sensor */
    Smsgs_cmdIds_configReq = 1,
    /*! Configuration Response message, sent from the sensor to the collector */
    Smsgs_cmdIds_configRsp = 2,
    /*! Tracking request message, sent from the the collector to the sensor */
    Smsgs_cmdIds_trackingReq = 3,
    /*! Tracking response message, sent from the sensor to the collector */
    Smsgs_cmdIds_trackingRsp = 4,
    /*! Sensor data message, sent from the sensor to the collector */
    Smsgs_cmdIds_sensorData = 5,
    /* Toggle LED message, sent from the collector to the sensor */
    Smsgs_cmdIds_toggleLedReq = 6,
    /* Toggle LED response msg, sent from the sensor to the collector */
    Smsgs_cmdIds_toggleLedRsp = 7,
    /* new data type for ramp data */
    Smsgs_cmdIds_rampdata = 8,
    /*! OAD mesages, sent/received from both collector and sensor */
    Smsgs_cmdIds_oad = 9,
    /* Broadcast control msg, sent from the collector to the sensor */
    Smgs_cmdIds_broadcastCtrlMsg = 10,
    /* KEY Exchange msg, between collector and the sensor */
    Smgs_cmdIds_KeyExchangeMsg = 11,
    /* Identify LED request msg */
    Smsgs_cmdIds_IdentifyLedReq = 12,
    /* Identify LED response msg */
    Smsgs_cmdIds_IdentifyLedRsp = 13,
    /*! SM Commissioning start command sent from collector to the sensor */
    Smgs_cmdIds_CommissionStart = 14,
    /*! SM Commissioning message sent bi-directionally between the collector and sensor */
    Smgs_cmdIds_CommissionMsg = 15,
    /*! Adam: Sensor firmware date code */
    Smsgs_cmdIds_dateCode = 18,
    /*! Adam: Sensor zone status */
    Smsgs_cmdIds_zoneSts = 19,
    /*! Adam: Sensor model id */
    Smsgs_cmdIds_modelId = 20,
    /*! Adam: Sensor calibration request msg */
    Smsgs_cmdIds_calibrateReq = 21,
    /*! Adam: Sensor calibration response msg */
    Smsgs_cmdIds_calibrateRsp = 22
} Smsgs_cmdIds_t;

#define RESOURCE_PATH "/mnt/mtldsk/data/HEM/db"
#define MAX_INP_CMD_LEN 80
#define MAX_FILE_PATH_LEN 60 //consistent with collector api spec
#define MAX_DEV_IN_CMD_LINE 8
#define OAD_LOG_FILE "/mnt/mtldsk/data/ti_oad.log"
int sockfd = 0;
char OadFwFile[MAX_FILE_PATH_LEN+1] = { 0 };

unsigned long int inet_addr(const char *);
int validate_params(int, char **, int);
int send_allow_join(int);
int send_get_network_info(void);
int send_get_device_list(void);
int send_remove_device(int);
int send_calibrate_device(int, unsigned char, int);
int send_oad_file(char*);
int send_oad_query_file();
int recv_network_info(void);
int recv_device_list(void);
int recv_tx_data_cnf(void);
int recv_oad_file_cnf(int *, char *);
unsigned short get_uint16(unsigned char *, int);
unsigned int get_uint32(unsigned char *, int);
void get_mac(unsigned char *, int, unsigned char *);
int create_socket(void);

int main(int argc , char *argv[])
{
    char input_cmd[MAX_INP_CMD_LEN + 20];
    char backup_cmd[MAX_INP_CMD_LEN + 20];
    char *token;
    char *tokens[50];
    char *ptr;
    char linestr[500];
    char c_chan_mask_str[200];
    char c_pan_id_str[100];
    char pan_id_str[10];
    char token_str[10];
    char answ[10];
    char inpc;
    int tok_cnt;
    int i, j, flag, value, ret, status;
    unsigned int pan_id;
    char version[] = "v1.8";

    time_t t;
    FILE *fp, *fp_temp;
    char file_collector_cfg[80];
    char file_temp_cfg[80];
    char file_nv_simulation_bin[80];

    sprintf(file_collector_cfg, "%s/%s", RESOURCE_PATH, "collector.cfg");
    sprintf(file_temp_cfg, "%s/%s", RESOURCE_PATH, "temp.cfg");
    sprintf(file_nv_simulation_bin, "%s/%s", RESOURCE_PATH, "nv-simulation.bin");


    printf("SubgCLI version %s\n", version);
    while(1) {
        printf("subgcli> ");
        input_cmd[0] = '\0';
        scanf("%[^\n]", input_cmd);
        getchar(); //to clean the ending '\n'
        input_cmd[MAX_INP_CMD_LEN] = '\0';
        //printf("%s\n", input_cmd);
        if(strlen(input_cmd)==0) {
            continue;
        }
        strcpy(backup_cmd, input_cmd);
        token = strtok(input_cmd, " ");
        tok_cnt = 0;
        while( token != NULL ) {
              //printf( " %s\n", token ); //printing each token
              tokens[tok_cnt++] = token;
              token = strtok(NULL, " ");
        }

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
                    printf("Restart collector and subgmgr...\n");
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

            fp = fopen(file_collector_cfg, "r");
            if(fp==NULL) {
                printf("Cannot find 'collector.cfg' in directory %s.\n", RESOURCE_PATH);
                continue;
            }
            fp_temp = fopen(file_temp_cfg, "w+");
            if(fp_temp==NULL) {
                printf("Cannot create 'temp.cfg' in directory %s.\n", RESOURCE_PATH);
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
                        //printf("New PAN Id: 0x%04x\n", pan_id);
                    }
                }
                if(flag)
                    fputs(linestr, fp_temp);
            }
            fclose(fp);
            fclose(fp_temp);

            remove(file_collector_cfg);
            rename(file_temp_cfg, file_collector_cfg); //rename the temp file
            remove(file_nv_simulation_bin); //delete the file for simulating non-volatile section

            system("killall -9 monitor_xmgr.app > /dev/null");
            system("subgmgr_init.sh restart");
            system("monitor_xmgr.app subgmgr > /dev/null 2>&1 &");
            printf("New PAN Id: 0x%04x\n", pan_id);

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
            if(send_allow_join((!strcmp(tokens[1], "on"))?1:0)) {
                printf("Command failure\n");
            }
            else {
                if (recv_allow_join_cnf())
                    printf("Command failure\n");
                else
                    printf("Command success\n");
            }
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
            else {
                //From testing result, there is no response from collector.
                sleep(1); //Don't close socket right away, otherwise the collector would give up the task.
                printf("Command success\n");
            }
            close(sockfd);
        } else if((strcmp(tokens[0], "calidev") == 0)) {
            if (validate_params(Cmd_CaliDev, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'calidev d287 t=2530'\n");
                continue;
            }
            if (create_socket())
                continue;
            ptr = tokens[2];
            ptr += 2; //skip 't='
            value = atoi(ptr);
            //printf("calidev value %d\n", value);
            //Curretly, 'calibrate_map' is always 1 which stands for 'calibrate by temperature'.
            if (send_calibrate_device(strtoul(tokens[1], NULL, 16), 1, value)) {
                printf("Command failure\n");
            } else {
                if (recv_tx_data_cnf())
                    printf("Command failure\n");
                else
                    printf("Command success\n");
            }
            close(sockfd);
        } else if ((strcmp(tokens[0], "oadf") == 0)) {
            if (validate_params(Cmd_OadF, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. Set the f/w file:\n'oadf /tmp/ht_sensor_oad_v1.bin'\n");
                printf("     Show the f/w file:\n'oadf'\n");
                continue;
            }
            if (create_socket())
                continue;
            if (tok_cnt == 1) { //show current f/w file path
                flag = 0;
                if (!send_oad_query_file()) {
                    if (!recv_oad_file_cnf(&status, OadFwFile))
                        flag = 1;
                }
                if (flag == 1) {
                    printf("Current f/w file path: '%s'\n", OadFwFile);
                }
                else
                    printf("Command failure\n");
                
                /*
                if(strlen(OadFwFile)>0)
                    printf("Current f/w file path: '%s'\n", OadFwFile);
                else
                    printf("Current f/w file is NOT set.\n");
                */
            } else {
                flag = 0;
                if (!send_oad_file(tokens[1])) {
                    if (!recv_oad_file_cnf(&status, NULL)) {
                        if(status==COLLECTOR_STATUS_SUCCESS)
                            flag = 1;
                    }
                }
                if (flag == 1) {
                    printf("Command success\n");
                    strcpy(OadFwFile, tokens[1]);
                }
                else
                    printf("Command failure\n");
                
                /*
                if (send_oad_file(tokens[1])) {
                    printf("Command failure\n");
                }
                else {
                    if (recv_oad_file_cnf())
                        printf("Command failure\n");
                    else {
                        strcpy(OadFwFile, tokens[1]);
                        printf("Command success\n");
                    }
                }
                */
            }
            close(sockfd);
        } else if ((strcmp(tokens[0], "oadu") == 0)) {
            if (validate_params(Cmd_OadU, tokens, tok_cnt)) {
                printf("Parameter(s) error!\n");
                printf("e.g. 'oadu a34b,78f0,3b12'\n");
                printf("     'oadu l=/tmp/dev_list.json'\n");
                continue;
            }
            if (create_socket())
                continue;
            /*
            if (strlen(OadFwFile) == 0) {
                printf("The f/w file is NOT selected yet!\n");
                continue;
            }
            */
            // To refresh the f/w image file path from collector in case that
            //  the collector ever restarts.
            flag = 0;
            if (!send_oad_query_file()) {
                if (!recv_oad_file_cnf(&status, OadFwFile))
                    flag = 1;
            }
            if (flag == 0) {
                printf("Failed to query collector current image file path.\n");
            }
            else {
                //Double check with the user.
                printf("The f/w image '%s' is going to be updated to devices.\n", OadFwFile);
                printf("Press 'y' to continue, the other keys to abort.\n");
                answ[0] = '\0';
                scanf("%[^\n]", answ);
                getchar(); //to clean the ending '\n'
                if (strcmp(answ, "y")) {
                    printf("'oadu' is aborted.\n");
                }
                else {
                    int len = strlen(backup_cmd);
                    strcpy(backup_cmd + len, " > /dev/null 2>&1 &");
                    strcpy(input_cmd, "nohup ");
                    strcpy(input_cmd, backup_cmd);
                    ret = system(input_cmd); //Launch oadu process in the background.
                    //printf("ret %d errno %d\n", ret, errno);
                    printf("OAD background process is launched.\n");
                    printf("Check %s for the OAD log.\n", OAD_LOG_FILE);
                }
            }
            close(sockfd);
        } else if(strcmp(tokens[0], "help")==0) {
            printf("Supported commands: resetpan, allowjoin, netstat, listdev, rmdev, calidev, oadf, oadu, bye, help\n");
            printf("resetpan: To reset the PAN configuration.\n");
            printf("allowjoin: To open or close joining for new devices.\n");
            printf("netstat: To get the network information.\n");
            printf("listdev: To get the devices list.\n");
            printf("rmdev: To remove a device.\n");
            printf("calidev: To calibrate a device.\n");
            printf("oadf: To set/get current OAD f/w file path.\n");
            printf("oadu: To start the OAD task.\n");
            printf("bye: To exit the CLI program.\n");
        } else if (strcmp(tokens[0], "bye") == 0) {
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
    buff[3] = APPSRV_SET_JOIN_PERMIT_REQ; //permit join command

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



int send_get_network_info()
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x0; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_GET_NWK_INFO_REQ; //get network info command

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
    buff[3] = APPSRV_GET_DEVICE_ARRAY_REQ; //get devices array

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
    buff[3] = APPSRV_RMV_DEVICE_REQ; //remove device

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


int send_calibrate_device(int short_addr, unsigned char calibrate_map, int value)
{
    unsigned char buff[64];
    //unsigned char calibrate_map = 1;
    int sent;

    buff[0] = 0x8; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_TX_DATA_REQ; //tx data request

    buff[4] = Smsgs_cmdIds_calibrateReq;
    buff[5] = short_addr & 0xff;
    buff[6] = short_addr >> 8;

    buff[7] = calibrate_map;
    buff[8] = value & 0xff;
    buff[9] = value >> 8;
    buff[10] = value >> 16;
    buff[11] = value >> 24;

    sent = send(sockfd, buff, 12, 0); // 12 = 4 (header) + 8 (payload)
    if (sent >= 0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //printf("Sent failed\n");
        return(-1);
    }
}


int send_oad_file(char *pathname)
{
    unsigned char buff[MAX_FILE_PATH_LEN + 10];
    int sent;
    int path_len = strlen(pathname);

    buff[0] = path_len + 2; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_OAD_FW_FILE_REQ;

    buff[4] = path_len & 0xff;
    buff[5] = path_len >> 8;

    strcpy(buff + 6, pathname);

    sent = send(sockfd, buff, path_len + 6, 0);
    if (sent >= 0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //printf("Sent failed\n");
        return(-1);
    }
}


int send_oad_query_file()
{
    unsigned char buff[32];
    int sent;
    
    buff[0] = 2; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_OAD_FW_FILE_REQ;

    buff[4] = 0;
    buff[5] = 0;

    sent = send(sockfd, buff, 6, 0);
    if (sent >= 0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //printf("Sent failed\n");
        return(-1);
    }
}




int recv_tx_data_cnf()
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd, status;
    unsigned char addr_mode, rssi;
    unsigned short short_addr;

    count = recv(sockfd, rxbuff, sizeof(rxbuff), 0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */
    if (count < 4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if (cmd != APPSRV_TX_DATA_CNF)
        return(-1);
    else
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
    if(cmd!= APPSRV_GET_NWK_INFO_CNF)
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
    if(cmd!= APPSRV_GET_DEVICE_ARRAY_CNF)
        return(-1);

    status = rxbuff[index++];
    num_dev = get_uint16(rxbuff, index);
    index += 2;
    printf("Status: %s\n", status_str[status]);
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
    printf("------------------\n");
    printf("Number of devices: %d\n", num_dev);
    /*
    printf("%2x %4x %4x [%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x] %d %2x %2x %2x %2x\n", status, pan_id, coord_short_addr,
           coord_mac[0], coord_mac[1], coord_mac[2], coord_mac[3], coord_mac[4], coord_mac[5], coord_mac[6], coord_mac[7],
           channel, fh, security_en, network_mode, state);
    */
    return(0);
}


int recv_allow_join_cnf()
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd, status;

    count = recv(sockfd, rxbuff, sizeof(rxbuff), 0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if (count < 4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if (cmd != APPSRV_COLLECTOR_STATE_CNG_IND)
        return(-1);
    else
        return(0);
}


int recv_oad_file_cnf(int *status, char *pathname)
{
    unsigned char rxbuff[MAX_FILE_PATH_LEN+10];
    int count, i, index, len;
    unsigned char subsystem_id, cmd;

    count = recv(sockfd, rxbuff, sizeof(rxbuff), 0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if (count < 4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if (cmd != APPSRV_OAD_FW_FILE_CNF)
        return(-1);
    else {
        *status = (int)rxbuff[index++];
        if (count > 5) {
            //get current f/w path
            len = get_uint16(rxbuff, index);
            index += 2;
            if (pathname) {
                i = 0;
                while (len-- && i< MAX_FILE_PATH_LEN) {
                    pathname[i++] = rxbuff[index++];
                }
                pathname[i] = '\0';
            }
        }
        return(0);
    }
}




#if 0
int recv_remove_device_rsp()
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd, status;

    count = recv(sockfd, rxbuff, sizeof(rxbuff), 0);

    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n");
    */

    if (count < 4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    printf("len %d cmd %d\n", len, cmd);
    if (cmd != APPSRV_RMV_DEVICE_RSP)
        return(-1);
    else
        return(0);
}
#endif


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
    int result = -1;
    int i, sub_token_cnt;
    int flag, value;
    char *sub_token, *ptr;

    //Caution: tok_cnt includes the command byte. tokens[0] is the command byte.
    switch(command) {
        case(Cmd_ResetPan):
            result = 0;
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
            if(tok_cnt==2 && (strcmp((const char *)tokens[1], "on")==0 || strcmp((const char *)tokens[1], "off")==0))
                result = 0;
        break;
        case(Cmd_NetStat):
            if(tok_cnt==1)
                result = 0;
        break;
        case(Cmd_ListDev):
            if(tok_cnt==1)
                result = 0;
        break;
        case(Cmd_RmvDev):
            if(tok_cnt==2) {
                value = strtoul(tokens[1], NULL, 16); //hex string to integer
                if(value>0 && value<0x10000)
                    result = 0;
            }
        break;
        case(Cmd_CaliDev):
            if (tok_cnt == 3) {
                value = strtoul(tokens[1], NULL, 16); //hex string to integer
                if (value > 0 && value < 0x10000) {
                    unsigned char* ptr = tokens[2];
                    if (ptr[0] == 't' && ptr[1] == '=') {
                        if(isInteger(ptr+2))
                            result = 0;
                    }
                }
            }
        break;
        case(Cmd_OadF):
            if (tok_cnt == 2) {
                if (strlen(tokens[1]) <= MAX_FILE_PATH_LEN) {
                    if (access(tokens[1], F_OK) == 0) {
                        result = 0;
                    }
                    else {
                        printf("The f/w file does NOT exist.\n");
                    }
                }
                else {
                    printf("The pathname max length is %d.\n", MAX_FILE_PATH_LEN);
                }
            }
            else if(tok_cnt == 1)
                result = 0;
        break;
        case(Cmd_OadU):
            result = 0;
            if (tok_cnt == 2) {
                ptr = tokens[1];
                if (ptr[0] == 'l' && ptr[1] == '=') {
                    //validate file path
                    ptr += 2;
                    if (access(ptr, F_OK)) {
                        result = -1;
                        printf("The device list file does NOT exist.\n");
                    }
                }
                else {
                    sub_token = strtok(ptr, ",");
                    sub_token_cnt = 0;
                    while (sub_token != NULL) {
                        //printf( " %s\n", sub_token ); //printing each token
                        value = strtoul(sub_token, NULL, 16); //hex string to integer
                        if (value > 0 && value < 0x10000) {
                            sub_token_cnt++;
                            if (sub_token_cnt > MAX_DEV_IN_CMD_LINE) {
                                result = -1;
                                printf("Max %d devices in the command line.\n", MAX_DEV_IN_CMD_LINE);
                                break;
                            }
                        } else {
                            printf("Invalid device short address is found.\n");
                            result = -1;
                            break;
                        }
                        sub_token = strtok(NULL, ",");
                    }
                }              
            }
            else {
                result = -1;
            }
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


bool isInteger(const char* str)
{
    int cnt = 0;
    while (*str != '\0') {
        if (cnt == 0) {
            if (*str != '-' && (*str < '0' || *str > '9'))
                return(false);
        } else {
            if (*str < '0' || *str > '9')
                return(false);
        }
        str++;
        cnt++;
    }
    return(true);
}

