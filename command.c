#include "command.h"
#define MAX_CMD_LEN 256
#define DATA_BUF_SIZE 512

int dataPid = -1; // 数据通道的pid，用于在信号处理中进行控制
static jmp_buf env_alrm;

void CmdProcess(int *chldPid, int client_fd){
	int pid = fork();

	if(pid < 0)
		err_exit("data process create failed",1);

	if(pid > 0) //父进程修改其获得到的chldPid值
		*chldPid = pid;
	else if(pid == 0){
		int recvBytes = -1;
		int datafd = -1; // 数据传输的文件描述符
		int pp[2]; // 命令和数据通道通信的管道
		char buf[MAX_CMD_LEN];
		char cmd[32], arg[MAX_CMD_LEN-32];
		char template[] = "tmp/ftp.XXXXXX"; // 在tmp目录下创建临时文件
		char *filename = mktemp(template);

		printf("tmpe filename:%s\n", filename);
		
		creat(filename, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

		msg("cmd connection established");

		if(signal(SIGUSR2, sig_proc_exit) ==  SIG_ERR)
			err_exit("signal data process register failed", 1);
		if(signal(SIGCHLD, sig_chld_halt) ==  SIG_ERR)
			err_exit("signal chld halt register failed", 1);
		if(pipe(pp) < 0) // 打开管道
			err_exit("open pipe failed", 1);

		if ((recvBytes=recv(client_fd, buf, MAX_CMD_LEN, 0)) ==-1)
			err_exit("recv出错！", 1);
		buf[recvBytes] = '\0';
		if(strcmp(buf, "PRT") == 0){
			datafd = portMode(client_fd, 65530);
		}else if(strcmp(buf, "PASV") == 0){
			datafd = pasvMode(client_fd);
		}
		startDataProcess(datafd, client_fd, pp); //启动数据传输进程

		while(1){
			if ((recvBytes=recv(client_fd, buf, MAX_CMD_LEN, 0)) ==-1)
				err_exit("recv出错！", 1);
			if(recvBytes == 0){ //无可用消息或传输已经结束，则返回的是0
				msg("连接已断开");
				kill(dataPid, SIGKILL);
				break;
			}
			buf[recvBytes] = '\0';
			sscanf(buf, "%s%s", cmd, arg);

			printf("Recv From Client, len:%d=body:%s\n", recvBytes, buf);	

			/* 根据buf中的命令执行相应动作，通过给子进程发送管道命令的形式
			 *  1. 客户端的重命名，删除，创建文件夹等文件操作命令，应该由命令进程响应，不需要通过数据通道
			 *  2. 首先应该用pasv或port模式建立数据连接，这样可以得到数据通道的pid和socket_fd
			 */

			// 通过写入pp[1]给数据进程发命令
			if(strcmp(cmd, "PUT") == 0){ // put文件，格式为[PUT 文件名]
				char *p = buf + 4; // 移动4个指针，指向文件名开始处
				if(access(p, W_OK) == 0 || access(p, F_OK) < 0){ //如果文件可写，或者根本不存在
					sendCmd(client_fd, "OK"); // 响应OK
					p -= 1; // 回到空格处
					p[0] = '1';
					printf("pipe cmd：%s\n", p);
					write(pp[1], p, strlen(p)); // 向数据进程管道写入命令
				}else
					sendCmd(client_fd, "File Exists or directory not accessiable");
			}else if(strcmp(cmd, "GET") == 0){ // get文件，同上
				char *p = buf + 4; // 移动4个指针，指向文件名开始处
				msg("接收到get命令");
				if(access(p, R_OK) == 0){ // 如果文件可读
					sendCmd(client_fd, "OK"); // 响应OK
					p -= 1; // 回到空格处
					p[0] = '0';
					printf("pipe cmd：%s\n", p);
					write(pp[1], p, strlen(p)); // 向数据进程管道写入命令
				}else{
					msg("文件访问出错");
					sendCmd(client_fd, "File not readable");
				}
			}else if(strcmp(cmd, "BYE") == 0){
				write(pp[1], "2", 2); // 写入2告知数据进程结束 
				break;
			}else if(strcmp(cmd, "DIR") == 0){
				sprintf(buf, "ls -al > %s", filename);
				if(vfork()==0){
					system(buf);
				}
				buf[0] = '0'; // 表示获取文件
				sprintf(buf+1, "%s", filename);
				printf("pipe cmd：%s\n", buf);
				sendCmd(client_fd, "OK"); // 响应OK
				write(pp[1], buf, strlen(buf));
			}
		}
		close(client_fd);
		unlink(filename);
		printf("命令子进程终止, pid=%d\n", getpid());
		exit(0);
	}
}

/**
 * 完成数据传输功能
 * datafd是数据通道socket
 * cmdfd是命令通道，出错时向客户端发送反馈
 * pipe实现了和命令进程的管道通信，文件打开，返回目录之类的都通过这个传递
 */
void startDataProcess(int datafd, int cmdfd, int *pp){
	dataPid = fork();
	if(dataPid == 0){
		int len;
		unsigned char *dataBuf, *cmdBuf;
		FILE *fp;

		close(pp[1]); // 子进程关闭写端
		dataBuf = (unsigned char*)malloc(sizeof(unsigned char)*(DATA_BUF_SIZE+8)); // 为databuf开辟多一些的空间
		cmdBuf = (unsigned char*)malloc(sizeof(unsigned char)*MAX_CMD_LEN); 
		if(dataBuf == NULL || cmdBuf == NULL)
			err_exit("malloc data  or cmd buf failed", 1);

		msg("数据进程启动，循环等待管道消息");

		/**
		 * 子进程接收并响应父命令进程的命令
		 * 因为数据进程要么是读要么是写
		 * 约定命令进程发来的命令第0位为0表示读，为1表示写，为2表示关闭
		 * 从第1位开始为要读写的文件名，直到字符串结尾遇到\0字符
		 */
		while(1){
			len = read(pp[0], cmdBuf, MAX_CMD_LEN);
			cmdBuf[len] = '\0';
			printf("得到管道命令:%s\n", cmdBuf);
			if(len == 0){
				sleep(1);
				continue;
			}
			
			if(cmdBuf[0] == '0'){ // 读服务器文件发送给数据端
				struct stat buf;
				char temp[30];
				int  cnt = 0;

				FILE *fp = fopen(cmdBuf+1, "rb");
				if(fp == NULL) { msg("读文件打开失败"); continue; }
				if(stat(cmdBuf+1, &buf) < 0) err_exit("获取文件大小失败", 1);
				sprintf(temp, "%ld ", buf.st_size);
				printf("文件长度:%ld ", buf.st_size);
				if(send(datafd, temp, strlen(temp), 0) < 0)
					err_exit("发送文件长度失败", 1);

				while((len = fread(dataBuf, sizeof(unsigned char), DATA_BUF_SIZE, fp)) != 0){
			//		printf("%2d发送数据长度%d 内容:%s\n", cnt++, len, dataBuf);
					if(send(datafd, dataBuf, len, 0) < 0)
						err_exit("文件发送失败", 1);
				}
				msg("文件发送完成");
				fclose(fp);
			}else if(cmdBuf[0] == '1'){// 将用户发来的文件写入到服务器
				long filelength = -1, cnt = 0;
				int index = 0;

				FILE *fp = fopen(cmdBuf+1, "wb");
				if(fp ==  NULL) msg("创建文件打开失败");

				// 读文件，先读文件长度，只要长度足够就停止读取
				while((len = recv(datafd, dataBuf, DATA_BUF_SIZE, 0)) != 0){
					if(len < 0)
						err_exit("接收文件出错", 1);
					
					dataBuf[len] = '\0'; //只是为了标记字符串结尾，不会写入到文件中

					if(filelength == -1){ // 第一次读，先读出文件长度
						while(index < 30 && dataBuf[index++] != ' ');
						if(index == 30) err_exit("读取文件长度失败", 1);
						sscanf(dataBuf, "%ld", &filelength);
						len -= index;
					}else 
						index = 0;

					printf("得到数据长度%d 内容:%s\n", len, dataBuf+index);
					fwrite(dataBuf+index, sizeof(unsigned char), len, fp);
					cnt += len;
					if(cnt >= filelength)
						break;
				}
				fclose(fp);
			}else if(cmdBuf[0] == '2'){
				break;
			}else{
				msg("管道命令出错!");
				continue;
			}
		}
		free(dataBuf);
		free(cmdBuf);
		msg("数据传输监听进程结束");
	}else if(dataPid > 0){
		close(pp[0]); // 父进程关闭读端
	}else
		err_exit("data process create failed", 1);
}

int pasvMode(int client_fd){
	int sockfd, datafd = -1, opt, sin_size;
	int idlePort = 50000; // 空闲端口从此计数
	char buf[30];
	struct sockaddr_in my_addr;
	struct sockaddr_in remote_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		err_exit("data process create data socket failed", 1);
	}
	
	// 将socket设置为可以重复使用
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt));
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(my_addr.sin_zero), 8);
	
	// 获得系统可用的端口号
	while(idlePort++ <= 65534){
		my_addr.sin_port = htons(idlePort);
		if(bind(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) != -1)
			break;
	}
	if(idlePort ==  65535)
		err_exit("no available port for data connection", 1);
	
	// 防止alarm出现竞争条件
	if(signal(SIGALRM, sig_alarm) ==  SIG_ERR)
		err_exit("signal alarm register failed", 0);
	if(setjmp(env_alrm) != 0)
		err_exit("accept time out", 1);

	if(listen(sockfd, 1) == -1)
		err_exit("listen data connection failed", 1);
	
	sin_size = sizeof(struct sockaddr_in); 
	
	// 告诉客户端可以连接该端口了
	sprintf(buf, "PORT %d", idlePort);
	sendCmd(client_fd, buf);

	// 打开端口并监听，如果超时15秒则返回错误
	alarm(15);
	if((datafd = accept(sockfd, (struct sockaddr *)&remote_addr, &sin_size)) == -1)
		err_exit("data connection accept failed", 1);
	alarm(0);

	if(datafd == -1)
		err_exit("client data connection over time", 1);

	printf("get data conection from %s\n", inet_ntoa(remote_addr.sin_addr));
	
	// 获取客户端连接，打开socket描述符，被动模式连接成功
	return datafd;
}

int portMode(int client_fd, int port){
	// 根据客户端发来的端口号发起连接。此时客户端已打开了该端口
	int datafd = -1, add_len;
	struct sockaddr_in client_addr;

	if((datafd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		err_exit("port mode socket create failed", 1);
	}

	// 通过client_fd得到对方的ip，再组合port参数进行连接
	// sockaddr_in和sockaddr结构体具有通用性，取到之后修改port可以使用
	if(getpeername(client_fd, (struct sockaddr*)&client_addr, &add_len) == -1)
		err_exit("get peername failed", 1);

	client_addr.sin_port = htons(port);

	// 如果返回错误或无法连接，则报错。
	if(connect(datafd, (struct sockaddr*)&client_addr, sizeof(struct sockaddr)) == -1)
		err_exit("passive mode data connection failed", 0);

	// 连接成功，打开socket文件描述符，主动模式连接成功
	return datafd;
}
	


static void sig_alarm(){
	longjmp(env_alrm, 1);
}

static void sig_proc_exit(){
	if(dataPid != -1)
		kill(dataPid, SIGKILL);
	//sleep(1); // 等待子进程退出
	waitpid(dataPid, NULL, 0);
	raise(SIGKILL);
}

static void sig_chld_halt(){
	printf("数据传输进程结束 waitid=%d\n", wait(NULL));
}