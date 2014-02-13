#ifndef _COMMAND_H
#define _COMMAND_H

/**
 * command.h
 * 该程序定义ftp传输的命令通道相关命令
 * 已有：与客户端连接的socket的fd
 * 支持：响应父进程的控制，可中断传输
 *       响应客户端的连接，并根据客户端的命令执行动作
 *       创建ftp数据连接的进程
 *       与数据连接进程的管道通信
 */
#include "lib/msg.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
//#include <netinet/in.h>
#include "unistd.h"
#include "fcntl.h"
#include <sys/socket.h>
#include <sys/types.h>
#include "lib/netcmd.h"
/**
 * 接收用户发来的命令并做出响应，cmdFd是已连接的socket的fd
 */
void CmdProcess(int *chldPid, int cmdFd, struct sockaddr_in peer_addr);


/**
 * 完成数据传输功能，datafd是数据通道socket，cmdfd是命令通道，作用出错时发送反馈，pipe实现了管道通信
 */
void startDataProcess(int datafd, int cmdfd, int *pipe);

static void sig_alarm();

/*
 * 接收到停止信号，将数据连接和命令连接终止，最后进程退出
  */
static void sig_proc_exit();

/**
 * 数据连接断开，回收其资源
 */
static void sig_chld_halt();

#endif
