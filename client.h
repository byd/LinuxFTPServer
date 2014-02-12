#ifndef _CLIENT_H
#define _CLIENT_H

/**
 * client.h
 * ftp的客户端程序调试
 */
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "stdio.h"
#include "stdlib.h"
#include "lib/msg.h"
#include "lib/netcmd.h"

/**
 * 从socket fd为dataFd的源读取数据，所有的数据存放在文件描述符为fd的文件中
 */
void RetriveFile(int dataFd, FILE* fp);

/**
 * 以被动模式建立数据连接
 */
int pasvMode();

#endif
