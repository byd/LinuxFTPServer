#ifndef _NETCMD_H
#define _NETCMD_H

#define MAX_CMD_LEN 256 /*每次最大数据传输量 */ 
#include "string.h"
#include <netdb.h>
#include "stdio.h"
#include "msg.h"

int sendCmd(int fd, char *cmd){
	int len = 0;
	if(fd == -1){
		msg("连接尚未建立");
		return;
	}
	len = send(fd, cmd, strlen(cmd), 0);
	if(len == -1)
		err_exit("send  cmd error, exit now", 1);
	return len;
}

int recvCmd(int fd, char *cmd){
	int len = recv(fd, cmd, MAX_CMD_LEN, 0);
	if (len ==-1) 
		err_exit("recv出错！", 1);
	if(!len)
		msg("服务器传输结束");  // 当返回0时表示无可用消息，或传输已结束
	
	cmd[len] = '\0';
	return len;
}
#endif