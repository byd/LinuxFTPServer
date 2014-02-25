#include "netcmd.h"

int sendCmd(int fd, char *cmd){
	int len = 0;
	if(fd < 0){
		msg("连接尚未建立");
		return -1;
	}
	len = send(fd, cmd, strlen(cmd), 0);
	if(len == -1)
		err_exit("send cmd error, exit now", 1);
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

void SendFile(int dataFd, char *filename){
	struct stat filestat;
	char temp[30];
	int  cnt = 0, len;
	char *dataBuf;

	FILE *fp = fopen(filename, "rb");
	if(fp == NULL) { msg("文件读取失败"); return; }
	
	dataBuf = (unsigned char*)malloc(sizeof(unsigned char)*(DATA_BUF_SIZE+8)); // 为databuf开辟多一些的空间

	if(stat(filename, &filestat) < 0) err_exit("获取文件大小失败", 1);
	sprintf(temp, "%ld ", filestat.st_size);
	printf("文件长度:%ld ", filestat.st_size);
	if(send(dataFd, temp, strlen(temp), 0) < 0)
		err_exit("发送文件长度失败", 1);
	
	while((len = fread(dataBuf, sizeof(unsigned char), DATA_BUF_SIZE, fp)) != 0){
		if(send(dataFd, dataBuf, len, 0) < 0)
			err_exit("文件发送失败", 1);
	}
	msg("文件发送完成");
	free(dataBuf);
	fclose(fp);
}

void RetriveFile(int dataFd, FILE* fp){
	unsigned char *dataBuf;
	long len, filelength=-1, cnt=0;
	int index = 0;

	// 创建比需要更大的dataBuf
	dataBuf = (unsigned char*)malloc(sizeof(unsigned char)*(DATA_BUF_SIZE+8));
	if(dataBuf == NULL)
		err_exit("malloc data buf failed", 1);

	// 读文件，先读文件长度，只要长度足够就停止读取
	while((len = recv(dataFd, dataBuf, DATA_BUF_SIZE, 0)) != 0){
		if(len < 0)
			err_exit("接收文件出错", 1);
		
		dataBuf[len] = '\0'; //只是为了标记字符串结尾，不会写入到文件中

		if(filelength == -1){
			while(index < 30 && dataBuf[index++] != ' ');
			if(index == 30) err_exit("读取文件长度失败", 1);
			sscanf(dataBuf, "%ld", &filelength);
			len -= index; // 更新已读取长度
		}else
			index = 0;

		//printf("得到数据长度%ld 内容:%s\n", len, dataBuf+index);
		fwrite(dataBuf+index, sizeof(unsigned char), len, fp); // 将剩余的字符写入到文件
		cnt += len;
		if(cnt >= filelength) // 读取到了足够的数据，跳出循环
			break;
	}
	free(dataBuf);
}


/*
* 监听可用端口，通过peer_fd通过对方，返回数据连接
* 对方的地址信息保存在remote_addr中
*/
int activeEnd(int peer_fd, struct sockaddr_in *remote_addr){
	int sockfd, datafd = -1, opt, sin_size;
	int idlePort = 50000; // 空闲端口从此计数
	char buf[30];
	struct sockaddr_in my_addr;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -2;

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
		return -3;

	// 防止alarm出现竞争条件
	if(signal(SIGALRM, sig_alarm) ==  SIG_ERR)
		return -4;
	if(setjmp(env_alrm) != 0)
		return -5;
		//err_exit("accept time out", 1);

	if(listen(sockfd, 1) == -1)
		return -6;
		//err_exit("listen data connection failed", 1);
	
	sin_size = sizeof(struct sockaddr_in); 

	// 告诉客户端可以连接该端口了
	sprintf(buf, "PORT %d", idlePort);
	sendCmd(peer_fd, buf);

	// 打开端口并监听，如果超时15秒则返回错误
	alarm(15);
	if((datafd = accept(sockfd, (struct sockaddr *)remote_addr, &sin_size)) == -1)
		return -7;
	//err_exit("data connection accept failed", 1);
	alarm(0);

	if(datafd == -1)
		return -8;
		//err_exit("client data connection over time", 1);

	printf("get data conection from %s\n", inet_ntoa(remote_addr->sin_addr));

	return datafd;
}

/*
* 连接服务器的port端口，返回与服务器建立的数据连接sockfd
*/
int passiveEnd(struct sockaddr_in *peer_addr){
	int dataFd;

	if((dataFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return -2;

	if(connect(dataFd, (struct sockaddr *)peer_addr, sizeof(struct sockaddr)) == -1)
		return -3;

	msg("passive connect ok");
	return dataFd;
}

/*
* 连接hostAddr:port的地址，返回一个socket描述符，连接结果保存在peer_addr里
*/
int CmdConnect(char *hostAddr, int port, struct sockaddr_in *serv_addr){
	struct hostent *host;

	if((host=gethostbyname(hostAddr))==NULL)
		return -2;
	serv_addr->sin_family = AF_INET; 
	serv_addr->sin_port = htons(port); 
	serv_addr->sin_addr = *((struct in_addr *)host->h_addr); 
	bzero(&(serv_addr->sin_zero),8);

	return passiveEnd(serv_addr);
}

static void sig_alarm(){
	longjmp(env_alrm, 1);
}

