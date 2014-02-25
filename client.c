#include "client.h"

#define MAX_CMD_LEN 256 /*每次最大数据传输量 */ 
#define SERVPORT 20 
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
				printf("ls error occured, server said:%s\n", buf);
		}else if(strcmp(cmd, "ls") == 0){ // 打印本地目录
				system(buf);
		}else if(strcmp(cmd, "cd") == 0){ // 进入目录
			strcpy(cmd, "CD ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("进入目录%s成功\n", arg);
			}else
				printf("cd error occured, server said:%s\n", buf);
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
				printf("getfile error occured, server said:%s\n", buf);
		}else if(strcmp(cmd, "put") == 0){ // 上传文件
			strcpy(cmd, "PUT ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				SendFile(dataFd, arg);
			}else
				printf("putfile error occured, server said:%s\n", buf);
		}else if(strcmp(cmd, "del") == 0){ // 删除
			strcpy(cmd, "DEL ");
			sendCmd(cmdFd, strcat(cmd, arg));
			if(recvCmd(cmdFd, buf)!=0 && strcmp(buf, "OK")==0){
				printf("删除文件'%s'成功\n", arg);
			}else
				printf("del file error occured, server said:%s\n", buf);
		}else if(strcmp(cmd, "bye") == 0){ // 终止退出
			close(dataFd);
			close(cmdFd);
			break;
		}else
			msg("未能识别的命令");
	}
	msg("bye bye");
}


