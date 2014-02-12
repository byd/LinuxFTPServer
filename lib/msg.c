#include "msg.h"
#include <stdio.h>
#include <stdlib.h>

void err_exit(char *s, int errno){
	printf("Err:%s\tErrno:%d\n", s, errno);
	exit(errno);
}

void msg(char *s){
	printf("%s\n", s);
}
