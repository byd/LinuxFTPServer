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
#include "signal.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "setjmp.h"
#include <netinet/in.h>
#include "sys/wait.h"
#include "sys/stat.h"
#include "unistd.h"
#include "fcntl.h"
#include <sys/socket.h>
#include <sys/types.h>
/**
 * 接收用户发来的命令并做出响应，cmdFd是已连接的socket的fd
 */
void CmdProcess(int *chldPid, int cmdFd);


/**
 * 完成数据传输功能，datafd是数据通道socket，cmdfd是命令通道，作用出错时发送反馈，pipe实现了管道通信
 */
void startDataProcess(int datafd, int cmdfd, int *pipe);

/**
 * 选择一个高位端口打开，并监听客户连接请求，将该通道作为文件传输通道
 */
int pasvMode(int client_fd);

/**
 * 打开客户端发来的端口，将该通道作为文件传输通道
 */
int portMode(int client_fd, int port);

/**
 * 避免等待超时
 */
static void sig_alarm();

/*
 * 接收到停止信号，将数据连接和命令连接终止，最后进程退出
  */
static void sig_proc_exit();

/**
 * 数据连接断开，回收其资源
 */
static void sig_chld_halt();

// 其他客户端命令
extern int sendCmd(int fd, char *cmd);
extern int recvCmd(int fd, char *cmd);

#endif
