#ifndef _MSG_H
#define _MSG_H

/**
 * 打印出错消息，并使程序退出
 */
void err_exit(char *s, int errno);

/**
 * 打印消息，并以换行结束
 */
void msg(char *s);
#endif
