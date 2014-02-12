#ifndef _TERMINAL_H
#define _TERMINAL_H


#include <unistd.h>
#include <stdio.h>
#include "lib/msg.h"
#include "lib/netcmd.h"
#include "server.h"
#include "fcntl.h"

static void sig_chld();

int cid = -1; //  child pid

#endif