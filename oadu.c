 /*
  * FILE NAME: oadu.c
  *
  * DESCRIPTION: This is an independent process for executing
  *              oad update procedure.
  *
  * NOTES:
  *
  * CREATOR: Adam Wu, 2020/10/20
  *
  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <jansson.h>

typedef int bool;
#define false 0  
#define true  1  

//APPSRV service command code
#define APPSRV_OAD_FW_UPDATE_REQ 22
#define APPSRV_OAD_FW_UPDATE_CNF 23
#define APPSRV_OAD_GET_STATE_REQ 24
#define APPSRV_OAD_GET_STATE_CNF 25

//Status of APPSRV service
#define COLLECTOR_STATUS_SUCCESS 0
#define COLLECTOR_STATUS_DEV_NOT_FOUND 1
#define COLLECTOR_STATUS_INVALID_STATE 2
#define COLLECTOR_STATUS_INVALID_FILE 3

//OAD state of the collector
#define COLLECTOR_OAD_STATE_IDLE 0
#define COLLECTOR_OAD_STATE_BUSY 1

//Max consecutive times of OAD progress frozen checked
#define MAX_FREEZE_CNT 7
#define CHECK_OAD_PROGRESS_INTERVAL 24 //in seconds

#ifdef __mips__
#define OAD_LOG_FILE "/mnt/mtldsk/data/ti_oad.log"
#else
#define OAD_LOG_FILE "./ti_oad.log"
#endif
#define LOG_PRINT(fmt, ...) {printf("%s: ",get_ctime());printf(fmt, __VA_ARGS__);\
                             fprintf(LogFp, "%s: ",get_ctime()); fprintf(LogFp, fmt, __VA_ARGS__); \
                             fflush(LogFp);}

int sockfd = 0;

unsigned long int inet_addr(const char *);
int send_oad_update(int);
int recv_oad_update_cnf(int*);
//int send_oad_check_busy();
//int recv_oad_check_busy(bool *);
//int check_collector_oad_busy(bool*);
//void poll_oad_free();
void wait_oad_done(int);
int send_oad_check_progress(int);
int recv_oad_check_progress(int *, int*, int*);
int check_oad_progress(int, int*, int*, int*);

int oad_start(int);

bool check_single_instance();
unsigned short get_uint16(unsigned char *, int);
unsigned int get_uint32(unsigned char *, int);
void get_mac(unsigned char *, int, unsigned char *);
int create_socket(void);
char* get_ctime();
FILE* LogFp;

int main(int argc, char* argv[])
{
    int address, status;
    json_t* devlist_p = NULL;
    json_t* dev_p;
    json_error_t error;
    bool busy;
    int i, result = 0;
    char* sub_token, * ptr;

    LogFp = fopen(OAD_LOG_FILE, "a+");
    LOG_PRINT("Start OAD task\n", NULL);
    if (!check_single_instance()) {
        LOG_PRINT("Another instance of %s is already running!\n", argv[0]);
        fclose(LogFp);
        exit(-1);
    }
    
    //Always assume the program is launched by subgcli, thus the parameter integrity
    //is validated there.
    if (argc == 2) {
        ptr = argv[1];
        if (ptr[0] == 'l' && ptr[1] == '=') {
            ptr += 2;
            LOG_PRINT("Load JSON file %s\n", ptr);
            devlist_p = json_load_file(ptr, JSON_ENCODE_ANY, &error);
            if (devlist_p != NULL) {
                if (json_is_array(devlist_p)) {
                    for (i = 0; i < (int)json_array_size(devlist_p); i++) {
                        dev_p = json_array_get(devlist_p, i);
                        address = (int)strtoul(json_string_value(dev_p), NULL, 16);
                        if(oad_start(address)==0)
                            wait_oad_done(address);
                    }
                }
            }
            else {
                LOG_PRINT("Load JSON file error!\n", NULL);
                result = -1;
            }
        }
        else {
            sub_token = strtok(ptr, ",");
            while (sub_token != NULL) {
                //printf( " %s\n", sub_token ); //printing each token
                address = (int)strtoul(sub_token, NULL, 16); //hex string to integer
                if(oad_start(address)==0)
                    wait_oad_done(address);
                sub_token = strtok(NULL, ",");
            }
        }
    }
    else {
        LOG_PRINT("Invalid arguments\n", NULL);
        result = -1;
    }
    LOG_PRINT("OAD task terminated\n", NULL);
    fclose(LogFp);
    exit(result);
}
 

int oad_start(int address)
{
    int status;
    int result = -1;
    
    //To use a new socket connection per device, otherwise, recv_oad_update_cnf 
    //will catch trouble.
    if (create_socket()) {
        LOG_PRINT("Failed to build a connection to the collector!\n", NULL);
        return(-1);
    }
    LOG_PRINT("oad update 0x%04x ...\n", address);
    if (send_oad_update(address)) {
        LOG_PRINT("Failed to send oad update request.\n", NULL);
    }
    else {
        if (recv_oad_update_cnf(&status)) {
            LOG_PRINT("Failed to receive oad update confirmation.\n", NULL);
        }
        else {
            if (status == COLLECTOR_STATUS_DEV_NOT_FOUND) {
                //Invalid device address
                LOG_PRINT("Invalid device address.\n", NULL);
            }
            else if (status == COLLECTOR_STATUS_INVALID_FILE) {
                //f/w image file does not exist
                LOG_PRINT("Firmware image file is not found.\n", NULL);
            }
            else {
                result = 0;
            }
        }
    }
    close(sockfd);
    return(result);
}

#if 0
void poll_oad_free()
{
    int status;
    bool busy;
    if (create_socket()) {
        LOG_PRINT("Failed to build a connection to the collector!\n", NULL);
        fclose(LogFp);
        exit(-1);
    }
    while (1) {
        status = check_collector_oad_busy(&busy);
        if (status || busy)
            sleep(8);
        else
            break;
    }
    close(sockfd);
}
#endif


void wait_oad_done(int address)
{
    int freeze_cnt = 0, status;
    int progress = 0, prev_sent_blocks = 0;
    int sent_blocks, total_blocks;

    /*
    if (create_socket()) {
        LOG_PRINT("Failed to build a connection to the collector!\n", NULL);
        fclose(LogFp);
        exit(-1);
    }
    */
    while (1) {
        sleep(CHECK_OAD_PROGRESS_INTERVAL);
        if (check_oad_progress(address, &status, &sent_blocks, &total_blocks) == 0) {
            //LOG_PRINT("L10 %d %d\n", sent_blocks, total_blocks);
            if (total_blocks) {
                progress = ((float)sent_blocks / total_blocks) * 100;
                if (sent_blocks != prev_sent_blocks) {
                    LOG_PRINT("%04x oad progress %d %%\n", address, progress);
                    prev_sent_blocks = sent_blocks;
                    freeze_cnt = 0;
                }
                else {
                    //LOG_PRINT("L30\n", NULL);
                    freeze_cnt++;
                }
            }
            else if (prev_sent_blocks && status==0 && (sent_blocks+total_blocks)==0) {
                //You may not poll 100% at the precise moment
                LOG_PRINT("%04x oad progress 100 %%\n", address);
                break;
            }
            else {
                //LOG_PRINT("L40\n", NULL);
                freeze_cnt++;
            }
        }
        else {
            //treat as frozen
            //LOG_PRINT("L50\n", NULL);
            freeze_cnt++;
        }
        if (progress == 100 || freeze_cnt >= MAX_FREEZE_CNT) {
            break;
        }
    }
    //close(sockfd);
}



int send_oad_update(int short_addr)
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0x2; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_OAD_FW_UPDATE_REQ;

    buff[4] = short_addr & 0xff;
    buff[5] = short_addr >> 8;

    sent = send(sockfd, buff, 6, 0);
    if (sent >= 0) {
        //print("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //print("Sent failed\n");
        return(-1);
    }
}






int recv_oad_update_cnf(int *status)
{
    unsigned char rxbuff[32];
    int count, i, index, len;
    unsigned char subsystem_id, cmd;

    count = recv(sockfd, rxbuff, sizeof(rxbuff), 0);
    /*
    for(i=0;i<count;i++)
    {
        printf("%02x ", (unsigned char)rxbuff[i]);
    }
    printf("\n", NULL);
    */
    
    if (count < 4)
        return(-1);
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if (cmd != APPSRV_OAD_FW_UPDATE_CNF)
        return(-1);
    else {
        *status = (int)rxbuff[index];
        return(0);
    }
}


#if 0
int send_oad_check_busy()
{
    unsigned char buff[64];
    int sent;

    buff[0] = 0; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_OAD_GET_STATE_REQ;

    sent = send(sockfd, buff, 4, 0);
    if (sent >= 0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //printf("Sent failed\n");
        return(-1);
    }
}




int recv_oad_check_busy(bool* busy)
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
    if (cmd != APPSRV_OAD_GET_STATE_CNF)
        return(-1);
    else {
        *busy = (rxbuff[index] == COLLECTOR_OAD_STATE_BUSY);
        return(0);
    }
}




int check_collector_oad_busy(bool *busy)
{
    *busy = false;

    if (send_oad_check_busy()==0) {
        return(recv_oad_check_busy(busy));      
    }
    return(-1);
}
#endif


int send_oad_check_progress(int short_addr)
{
    unsigned char buff[32];
    int sent;

    buff[0] = 2; //two bytes len
    buff[1] = 0x0;
    buff[2] = 0xa; //subsystem id
    buff[3] = APPSRV_OAD_GET_STATE_REQ;
    buff[4] = short_addr & 0xff;
    buff[5] = short_addr >> 8;

    sent = send(sockfd, buff, 6, 0);
    if (sent >= 0) {
        //printf("Sent %d bytes\n", sent);
        return(0);
    }
    else {
        //printf("Sent failed\n");
        //printf("send_oad_check_progress cmd fail\n");
        return(-1);
    }
}


int recv_oad_check_progress(int *status, int* sent_blocks, int* total_blocks)
{
    unsigned char rxbuff[32];
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

    if (count < 4) {
        //printf("recv_oad_check_progress fail\n");
        return(-1);
    }
    index = 0;
    len = get_uint16(rxbuff, index);
    index += 2;
    subsystem_id = rxbuff[index++];
    cmd = rxbuff[index++];
    if (cmd != APPSRV_OAD_GET_STATE_CNF) {
        //printf("recv_oad_check_progress cmd fail: cmd: %x\n", cmd);
        return(-1);
    }
    else {
        *status = rxbuff[index++];
        *sent_blocks = get_uint16(rxbuff, index);
        index += 2;
        *total_blocks = get_uint16(rxbuff, index);
        //printf("%d %d %d\n", *status, *sent_blocks, *total_blocks);
        return(0);
    }
}


int check_oad_progress(int address, int *status, int* sent_blocks, int* total_blocks)
{
    int result = -1;
    if (create_socket()) {
        LOG_PRINT("Failed to build a connection to the collector!\n", NULL);
        return(-1);
    }
    if (send_oad_check_progress(address) == 0) {
        result = recv_oad_check_progress(status, sent_blocks, total_blocks);
    }
    close(sockfd);
    return(result);
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



int create_socket()
{
    int err;
    struct sockaddr_in info;
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd == -1){
        LOG_PRINT("Fail to create a socket.\n", NULL);
        return(-1);
    }

    bzero(&info,sizeof(info));
    info.sin_family = PF_INET;

    info.sin_addr.s_addr = inet_addr("127.0.0.1");
    info.sin_port = htons(5000);

    err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
    if(err==-1){
        LOG_PRINT("Connect collector socket error.\n", NULL);
        close(sockfd);
        return(-1);
    }
    return(0);
}




bool check_single_instance(void)
{
    bool single = true;
    int pid_file = open("/var/run/oadu.pid", O_CREAT | O_RDWR, 0666);
    int rc = flock(pid_file, LOCK_EX | LOCK_NB);
    if (rc && (EWOULDBLOCK == errno))
    {
        single = false;
    }

    return single;
}


char* get_ctime()
{
    time_t current_time;
    char* c_time_string;

    /* Obtain current time. */
    current_time = time(NULL);

    if (current_time == ((time_t)-1))
    {
        LOG_PRINT("Failure to obtain the current time.\n", NULL);
        return(NULL);
    }

    /* Convert to local time format. */
    c_time_string = ctime(&current_time);

    if (c_time_string == NULL)
    {
        LOG_PRINT("Failure to convert the current time.\n", NULL);
        return(NULL);
    }
    //c_time_string points to static memory pool,
    // thus no need to free.
    c_time_string[strlen(c_time_string) - 1] = '\0'; //cut the ending '\n'
    return(c_time_string + 4); //skip 'day' part
}
