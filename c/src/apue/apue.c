#include "apue.h"
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>

static void err_doit(int, int, const char *, va_list);

void err_dump(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    abort();    // dumps core and terminates
    exit(1);    // should never be called
}

void err_sys(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

/* fatal error unrelated to system call -- print message and terminate */
void err_quit(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    err_doit(0, 0, fmt, ap);
    va_end(ap);
    exit(1);
}

/* print messasge and return to caller -- caller specifies errno flag */
static void err_doit(int errnoflag, int error, const char *fmt, va_list ap)
{
    char buf[MAXLINE];

    vsnprintf(buf, MAXLINE-1, fmt, ap);
    if (errnoflag) {
        snprintf(buf+strlen(buf), MAXLINE-strlen(buf)-1, ": %s", strerror(error));
        strcat(buf, "\n");
        fflush(stdout);
        fputs(buf, stderr);
        fflush(NULL);
    }
}

int lock_reg(int fd, int cmd, int type,  off_t offset, int whence, off_t len)
{
    struct flock lock;
    lock.l_type = type;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = offset;  /* byte offset, relative to l_whence */
    lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = len;       /* num bytes (0 means to EOF) */

    return (fcntl(fd, cmd, &lock));
}