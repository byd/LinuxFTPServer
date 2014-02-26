#include "command.h"
#define MAX_CMD_LEN 256
#define DATA_BUF_SIZE 512

int dataPid = -1; // 数据通道的pid，用于在信号处理中进行控制

void CmdProcess(int *chldPid, int cmdFd, struct sockaddr_in peer_addr){
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
		char template[] = "/tmp/ftp.XXXXXX"; // 在tmp目录下创建临时文件
		char *filename = mktemp(template);

		msg("cmd connection established");

		creat(filename, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		if(signal(SIGUSR2, sig_proc_exit) ==  SIG_ERR)
			err_exit("signal data process register failed", 1);
		if(signal(SIGCHLD, sig_chld_halt) ==  SIG_ERR)
			err_exit("signal chld halt register failed", 1);
		if(pipe(pp) < 0) // 打开管道
			err_exit("open pipe failed", 1);

		while(1){
			if ((recvBytes=recv(cmdFd, buf, MAX_CMD_LEN, 0)) ==-1)
				err_exit("recv error", 1);
			if(recvBytes == 0){ //无可用消息或传输已经结束，则返回的是0
				msg("Connection Broken Down");
				if(dataPid>0)  // 不要乱杀进程!
					kill(dataPid, SIGKILL);
				break;
			}
			buf[recvBytes] = '\0';
			sprintf(arg, ""); //清空arg的内容
			sscanf(buf, "%s%s", cmd, arg);

			printf("client cmd:%s, cmd=%s, arg=%s\n", buf, cmd, arg);	

			/* 根据buf中的命令执行相应动作，通过给子进程发送管道命令的形式 */
			if(strcmp(cmd, "PORT") == 0){ // 主动模式连接
				int port;
				sscanf(arg, "%d", &port);
				peer_addr.sin_port = htons(port);
				datafd = passiveEnd(&peer_addr);

				if(datafd < 0){
					printf("Active mode connection failed, input [PRT 'port'] or [PASV], errno=%d\n", datafd);
					continue;
				}
				startDataProcess(datafd, cmdFd, pp);
			}else if(strcmp(cmd, "PASV") == 0){ // 服务器打开商品监听
				datafd = activeEnd(cmdFd, &peer_addr);
				if(datafd < 0){
					printf("passive mode connection failed, input [PRT 'port'] or [PASV], errno=%d\n", datafd);
					continue;
				}
				startDataProcess(datafd, cmdFd, pp);
			}else if(strcmp(cmd, "DIR") == 0){
				sprintf(buf, "ls -al %s > %s", arg, filename);
				if(vfork()==0){
					system(buf);
				}
				buf[0] = '0'; // 表示获取文件
				sprintf(buf+1, "%s", filename);
				printf("pipe cmd：%s\n", buf);
				sendCmd(cmdFd, "OK"); // 响应OK
				write(pp[1], buf, strlen(buf));
			}else if(strcmp(cmd, "CD") == 0){
				if(chdir(arg) < 0){
					sendCmd(cmdFd, "ER chdir failed");
				}else{
					buf[2] = '2';
					write(pp[1], buf+2, strlen(buf+2));
					sendCmd(cmdFd, "OK");
				}
			}else if(strcmp(cmd, "GET") == 0){ // get文件，同上
				char *p = buf + 4; // 移动4个指针，指向文件名开始处
				msg("接收到get命令");
				if(access(p, R_OK) == 0){ // 如果文件可读
					sendCmd(cmdFd, "OK"); // 响应OK
					p -= 1; // 回到空格处
					p[0] = '0';
					printf("pipe cmd：%s\n", p);
					write(pp[1], p, strlen(p)); // 向数据进程管道写入命令
				}else{
					msg("文件访问出错");
					sendCmd(cmdFd, "ER File not readable");
				}
			}else if(strcmp(cmd, "PUT") == 0){ // put文件，格式为[PUT 文件名]
				char *p = buf + 4; // 移动4个指针，指向文件名开始处
				if(access(p, W_OK) == 0 || access(p, F_OK) < 0){ //如果文件可写，或者根本不存在
					sendCmd(cmdFd, "OK"); // 响应OK
					p -= 1; // 回到空格处
					p[0] = '1';
					printf("pipe cmd：%s\n", p);
					write(pp[1], p, strlen(p)); // 向数据进程管道写入命令
				}else
					sendCmd(cmdFd, "ER File Exists or directory not accessiable");
			}else if(strcmp(cmd, "DEL") == 0){
				if(remove(arg) == 0)
					sendCmd(cmdFd, "OK");
				else 
					sendCmd(cmdFd, "ER FILE REMOVE FAILED");
			}else if(strcmp(cmd, "BYE") == 0){
				write(pp[1], "3", 2); // 写入3告知数据进程结束 
				break;
			}
		}
		close(cmdFd);
		unlink(filename);
		printf("CMD process terminated, pid=%d\n", getpid());
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
		char cmdBuf[MAX_CMD_LEN];

		close(pp[1]); // 子进程关闭写端

		msg("Cmd process started, waiting for pipe cmd");
		/**
		 * 子进程接收并响应父命令进程的命令
		 * 因为数据进程要么是读要么是写
		 * 约定命令进程发来的命令第0位为0表示读，为1表示写，为2表示关闭
		 * 从第1位开始为要读写的文件名，直到字符串结尾遇到\0字符
		 */
		while(1){
			len = read(pp[0], cmdBuf, MAX_CMD_LEN);
			cmdBuf[len] = '\0';
			printf("receiving pipe cmd :%s\n", cmdBuf);
			
			if(cmdBuf[0] == '0'){ // 读服务器文件发送给数据端
				SendFile(datafd, cmdBuf+1);
			}else if(cmdBuf[0] == '1'){// 将用户发来的文件写入到服务器
				FILE *fp = fopen(cmdBuf+1, "wb");
				if(fp ==  NULL) msg("file create failed");
				RetriveFile(datafd, fp);
			}else if(cmdBuf[0] == '2'){ // 更改目录
				if(chdir(cmdBuf+1) < 0)
					msg("chdir failed");
			}else if(cmdBuf[0] == '3'){ // 退出进程
				break;
			}else{
				msg("pipe cmd error");
				continue;
			}
		}
		msg("data process exit");
	}else if(dataPid > 0){
		close(pp[0]); // 父进程关闭读端
	}else
		err_exit("data process create failed", 1);
}

static void sig_proc_exit(){
	if(dataPid != -1)
		kill(dataPid, SIGKILL);
	waitpid(dataPid, NULL, 0);
	raise(SIGKILL);
}

static void sig_chld_halt(){
	wait(NULL);
	//printf("数据传输进程结束 waitid=%d\n", wait(NULL));
}