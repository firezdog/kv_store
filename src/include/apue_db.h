/* public database header */
#ifndef APUE_DB_H
#define APUE_DB_H

/*
generic pointer abstracts implementation from users
allows different c implementations of the same header
probably the reason for ... in the function signatures as well
a particular implementation might take other args
*/
typedef void * DBHANDLE;

// the char represents the path, the int represents the open mode
DBHANDLE db_open(const char *, int, ...);

#endif