/**
 * terminal.c
 * 用于服务启动的主程序，运行ftp服务器需要启动该程序。
 * 它首先创建一个子进程，子进程执行服务端初始化的操作（包括打开端口，监听对创建用户连接的通信进程）
 * 其主进程用于接收用户的操作，可以接收用户输入关闭服务器。
 *
 */
#include "terminal.h"

int main(){
	char buf[128] ; // 实现启动程序即启动服务进程
	int i = sizeof(buf);

	umask(0);

	if(signal(SIGCHLD, sig_chld) == SIG_ERR)
		msg("signal child halt register failed");

	// 默认启动
	FtpServe();
	
	while(1){
		printf("input>");
		for(i=0; i<128; i++){
			buf[i] = getchar();
			if(buf[i] == '\n'){
				buf[i] = '\0';
				break; 
			}
		}
		// actions 
		if(i==0)
			continue;
		else if(strcmp(buf, "start") == 0){
			if(cid != -1)
				printf("当前正在运行, pid=%d\n", cid);
			else
				FtpServe();
		}else if(strcmp(buf, "stop") == 0){
			if(cid != -1)
				kill(cid, SIGUSR1);
			else
				msg("Server not running");
		}else if(strcmp(buf, "exit") == 0){
			if(cid != -1)
				kill(cid, SIGUSR1);
			while(cid != -1) sleep(1); // wait for child process to terminate
			break;
		}else
			msg("未能识别的命令");

	}
	msg("bye");
}

static void sig_chld(){
	printf("子进程退出, 原id=%d,waitid=%d\n", cid, wait(NULL));
	cid = -1;
}
