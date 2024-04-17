/* private header from book -- this will include functions needed for the db library only */
#ifndef _APUE_H
#define _APUE_H

#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/termios.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAXLINE 4096

void err_dump(const char *fmt, ...);

#endif