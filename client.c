#include "client.h"

#define MAX_CMD_LEN 256 /*每次最大数据传输量 */ 
#define SERVPORT 3333 
#define DATA_BUF_SIZE 512

int  cmdFd=-1, dataFd=-1; // 客户端默认数据端口与命令端口相同
struct sockaddr_in serv_addr; // 保存服务器地址和端口的结构体，使用时要进行修改
char buf[MAX_CMD_LEN];  //保存用户命令和服务器发送来的命令
char tempBuf[32]; //函数中临时保存命令数据的变量

int main(int argc, char *argv[]){ 	
	int i;
	struct hostent *host;	
	char hostAddr[] = "localhost";

	// 判断参数是否完整
/*	if (argc < 2) 
		err_exit("Please enter the server's hostname", 1); 
*/	
	if((host=gethostbyname(hostAddr))==NULL) 
		err_exit("gethostbyname出错！", 1); 
	if ((cmdFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_exit("socket创建出错！", 1); 
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(SERVPORT); 
	serv_addr.sin_addr = *((struct in_addr *)host->h_addr); 
	bzero(&(serv_addr.sin_zero),8); 
	if (connect(cmdFd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) 
		err_exit("connect出错！", 1);

	msg("连接服务器成功，命令通道打开");

	// 这里要实现打开数据通道，不管有多麻烦，数据通道都一次打开，通信完成之后再关闭
	pasvMode();

	while(1){ 
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
		else if(strcmp(cmd, "ls") == 0){ // 打印服务器的当前目录
			sendCmd(cmdFd, "DIR"); // 数据连接必须已经建立
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				msg("服务器接收了命令，开始接收文件");
				RetriveFile(dataFd, stdout); // 将dataFd发来的数据存入到stdout即终端输出上
			}else
				msg("dir命令出错");
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
				msg("GET文件出错");
		}else if(strcmp(cmd, "cd") == 0){ // 进入目录
			strcpy(cmd, "CD ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("进入目录%s成功\n", arg);
			}else
				msg("cd到目录出错");	
		}else if(strcmp(cmd, "del") == 0){ // 删除
			strcpy(cmd, "DEL ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("删除文件%s成功\n", arg);
			}else
				msg("del文件出错");
		}else if(strcmp(cmd, "put") == 0){ // 上传文件
			msg("Not implemented");  // 尚未完成的功能
		}else if(strcmp(cmd, "bye") == 0){ // 终止退出
			close(dataFd);
			close(cmdFd);
			msg("bye bye");
			exit(0); // 主动退出
		}else
			msg("未能识别的命令");
	}
}

/**
 * 以被动模式建立数据连接
 */
int pasvMode(){
	int servPort = -1;
	sendCmd(cmdFd, "PASV"); // 被动传输模式
	if(recvCmd(cmdFd, buf) != 0){
		printf("得到服务器返回消息%s\n", buf);
		sscanf(buf, "%s%d", tempBuf, &servPort); //通过服务器的返回数据得到port
		if((dataFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			err_exit("数据通道socket创建失败", 1);
		serv_addr.sin_port = htons(servPort);
		printf("等待服务器端口%d\n", servPort);
		if(connect(dataFd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1)
			err_exit("data chanel connect failed", 1);
		msg("passive mode connect ok");
	}
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
	msg("写入完成，关闭文件");
}