#ifndef _SERVER_H
#define _SERVER_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "lib/msg.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * ftp服务器的主进程，该进程监听21端口，并对客户端的连接响应，创建一个新的进程
 */
void FtpServe();

/**
 * 得到可用的子进程id，方便对限定数目内的子进程进程管理 
  */
int *getAvilablePid();
void InitChldPidPool();
void DeInitChldPidPool();

/** 
 * 主程序发送信号，服务进程收到后先向其所有子进程发送kill信号，然后自己注销
 */
static void sig_close_server();

/**
 * 子进程退出，父进程回收子进程的资源
  */
static void sig_chld_exit();

#endif