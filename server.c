#include "server.h"

#define SERVPORT 20 /*服务器监听端口号 */ 
#define BACKLOG 10 /* 最大同时连接请求数 */ 


void FtpServe(){
	if((cid = fork()) < 0)
		err_exit("fork failed", 1);
	if(cid > 0)
		msg("Server started");

	if(cid == 0){ // 在这里完成子进程的任务

		int client_fd, sin_size, opt; /*sock_fd：监听socket；client_fd：数据传输socket */ 
		struct sockaddr_in my_addr; /* 本机地址信息 */ 
		struct sockaddr_in remote_addr; /* 客户端地址信息 */ 
		
		InitChldPidPool(); // 初始化子进程pid池

		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { 
			err_exit("socket创建出错！",1);
		}
	
		// 将socket设置为可以重复使用，这个很有必要，不然就会出现调试过程中无法绑定端口的问题
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,&opt,sizeof(&opt));

		// 设置地址的参数
		my_addr.sin_family=AF_INET; 
		my_addr.sin_port=htons(SERVPORT); 
		my_addr.sin_addr.s_addr = INADDR_ANY; 
		bzero(&(my_addr.sin_zero),8); 

		// 将socket与地址绑定
		if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) { 
			err_exit("bind出错！",1); 
		}
	
		// 监听端口
		if (listen(sockfd, BACKLOG) == -1) { 
			err_exit("listen出错！",1); 
		}

		// 响应信号
		if(signal(SIGUSR1, sig_close_server) ==  SIG_ERR)
			err_exit("signal stop register failed", 1);
		if(signal(SIGCHLD, sig_chld_exit) ==  SIG_ERR)
			err_exit("signal child exit register failed", 1);

		// 循环监听客户端的连接
		int *p;
		while(1){
			sin_size = sizeof(struct sockaddr_in); 
			if ((client_fd = accept(sockfd, (struct sockaddr *)&remote_addr, &sin_size)) == -1) {
				printf("accept出错"); 
				continue;  // 如果连接过多，在这里会直接跳过
			}
			printf("received a connection from %s\n", inet_ntoa(remote_addr.sin_addr)); 
			p = getAvilablePid();
			CmdProcess(p, client_fd, remote_addr);
		}

		// while退出，服务进程作服务器的善后工作
		msg("shuting down...");
		DeInitChldPidPool();
		exit(0);
	}
}

// 等待子进程结束，并将其pid值重置为-1
static void sig_chld_exit(){
	int wid = wait(NULL);
	int i;
	printf("ftpserve函数wait得到退出命令进程id=%d\n", wid);
	for(i=0; i<BACKLOG; i++)
		if(chldPidPool[i] == wid)
			chldPidPool[i] = -1;
}

// 主动向所有子进程发送结束信号
static void sig_close_server(){
	int i;
	msg("正在注销所有程序");
	for (i = 0; i < BACKLOG; ++i)
	{
		if(chldPidPool[i] != -1)
			kill(chldPidPool[i], SIGUSR2);
	}

	close(sockfd); // 释放占用的socket资源，但这里好像总不能释放

	// 结束自己
	raise(SIGKILL);
}

void InitChldPidPool(){
	int i;

	chldPidPool = (int*)malloc(BACKLOG * sizeof(int));
	for (i = 0; i < BACKLOG; ++i){
		chldPidPool[i] = -1;
	}
}

void DeInitChldPidPool(){
	free(chldPidPool);
	msg("deinit chlid pool");
}

int *getAvilablePid(){
	int i; 
	for(i=0; i<BACKLOG; i++)
		if(chldPidPool[i] == -1)
			return chldPidPool+i;
	return 0;
}
