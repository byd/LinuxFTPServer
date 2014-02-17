#include "stdio.h"
#include "lib/msg.h"
#include "server.h"
#include "signal.h"
#include "sys/resource.h"
#include "fcntl.h"

int daemonize(){
	struct rlimit rl;
	struct sigaction sa;
	int pid, i, fd0, fd1, fd2;

	getrlimit(RLIMIT_NOFILE, &rl);

	pid = fork();
	if(pid == -1){
		msg("daemon 1st fork failed");
		return -1;
	}else if(pid > 0) // parent
		exit(0);

	if(setsid() == -1){
		msg("set sid failed");
		return -2;
	}

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if(sigaction(SIGHUP, &sa, NULL) < 0)
		err_exit("can not ignore SIGHUP", 1);

	pid = fork();
	if(pid == -1){
		msg("daemon 2nd fork failed");
		return -3;
	}else if(pid > 0) // parent
		exit(0);

//	chdir("/");
//	printf("ftp server pid %d\n", getpid());
	
	if(rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for(i=0; i<rl.rlim_max; i++)
		close(i);
/*	
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);
*/	
	return 0;
}


void main(){
	int eno;
	
	umask(0);
	if((eno = daemonize()) < 0)
		err_exit("daemonize failed", eno);
//	freopen("ftp.log", "w", stdout); // 将所有输出重定向到当前目录下的ftp.log文件中
	FtpServe();
}

