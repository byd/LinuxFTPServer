#include "client.h"

#define MAX_CMD_LEN 256 /*每次最大数据传输量 */ 
#define SERVPORT 3333 
#define DATA_BUF_SIZE 512
#define PASVMODE 1

int  cmdFd=-1, dataFd=-1; // 客户端默认数据端口与命令端口相同

int main(int argc, char *argv[]){	
	char hostAddr[128] = "localhost";
	char buf[MAX_CMD_LEN];  //保存用户命令和服务器发送来的命令
	struct sockaddr_in peer_addr;
	
	// 判断参数是否完整，不完整用localhost代替
	if (argc < 2){
		msg("host not specified, use localhost as default");
	}else{
		sprintf(hostAddr, "%s", argv[1]);
	}

	// 数据连接
	if((cmdFd = CmdConnect(hostAddr, SERVPORT, &peer_addr)) < 0)
		err_exit("cmdFd连接失败", cmdFd);

	if(PASVMODE){  // 被动模式
		char temp[50], temp1[10];
		int port;

		// 通知服务器被动连接方式
		sendCmd(cmdFd, "PASV");
		if(recvCmd(cmdFd, temp) != 0)
			sscanf(temp, "%s%d", temp1, &port);
		peer_addr.sin_port = htons(port); // 连接该端口
		if((dataFd = passiveEnd(&peer_addr)) < 0)
			err_exit("被动连接模式失败", dataFd);
	}else{ // 主动模式
		if((dataFd = activeEnd(cmdFd, &peer_addr)) < 0)
			err_exit("主动模式连接失败", dataFd);
	}
	
	while(1){ 
		int i;
		char cmd[32], arg[MAX_CMD_LEN-32];

		//读取命令，以换行结束
		printf("ftp>");
		for(i=0; i<MAX_CMD_LEN; i++){
			buf[i] = getchar();
			if(buf[i] == '\n'){
				buf[i] = '\0';
				break;
			}
		}
		//将命令和参数区分开
		sscanf(buf, "%s%s", cmd, arg);
		
		// 等待用户命令，做出判断
		if(i == 0)
			continue;
		else if(strcmp(cmd, "dir") == 0){ // 打印服务器的当前目录
			sendCmd(cmdFd, "DIR"); // 数据连接必须已经建立
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				msg("服务器接收了命令，开始接收文件");
				RetriveFile(dataFd, stdout); // 将dataFd发来的数据存入到stdout即终端输出上
			}else
				printf("ls error occur, server said:%s\n", buf);
		}else if(strcmp(cmd, "ls") == 0){ // 打印本地目录
				system(buf);
		}else if(strcmp(cmd, "cd") == 0){ // 进入目录
			strcpy(cmd, "CD ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("进入目录%s成功\n", arg);
			}else
				printf("cd error occur, server said:%s\n", buf);
		}else if(strcmp(cmd, "get") == 0){ // 获取服务器文件
			strcpy(cmd, "GET ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				FILE *newFp = fopen(arg, "wb");
				if(newFp == NULL)
					err_exit("文件创建失败", 1);
				RetriveFile(dataFd, newFp);
				fclose(newFp);
			}else
				printf("getfile error occur, server said:%s\n", buf);
		}else if(strcmp(cmd, "put") == 0){ // 上传文件
			strcpy(cmd, "PUT ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				SendFile(dataFd, arg);
			}else
				printf("putfile error occur, server said:%s\n", buf);
		}else if(strcmp(cmd, "del") == 0){ // 删除
			strcpy(cmd, "DEL ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("删除文件%s成功\n", arg);
			}else
				printf("del file error occur, server said:%s\n", buf);
		}else if(strcmp(cmd, "bye") == 0){ // 终止退出
			close(dataFd);
			close(cmdFd);
			
			break;
			//exit(0); // 主动退出
		}else
			msg("未能识别的命令");
	}
	msg("bye bye");
}


/**
  * 向服务器发送文件
  */
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

/**
 * 从socket fd为dataFd的源读取数据，所有的数据存放在文件描述符为fd的文件中
 */
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
