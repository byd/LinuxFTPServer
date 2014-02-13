#ifndef _NETCMD_H
#define _NETCMD_H

#define MAX_CMD_LEN 256 /*每次最大数据传输量 */ 
#include "string.h"
#include "stdio.h"
#include "msg.h"
#include <netinet/in.h>
#include <netdb.h>
#include "setjmp.h"
#include "signal.h"
#include "sys/wait.h"
#include "sys/stat.h"

static jmp_buf env_alrm;

int sendCmd(int fd, char *cmd);

int recvCmd(int fd, char *cmd);


/*
* 通过peer_fd通信，以被动模式连接并返回数据通道的sockfd
*/
int activeEnd(int peer_fd, struct sockaddr_in *remote_addr);

/*
* 连接服务器的port端口，返回与服务器建立的数据连接sockfd
*/
int passiveEnd(struct sockaddr_in *peer_addr);

/*
* 连接hostAddr:port的地址，返回一个socket描述符，地址信息保存到peer_addr中
*/
int CmdConnect(char *hostAddr, int port, struct sockaddr_in *peer_addr);

static void sig_alarm();

#endif