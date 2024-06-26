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
void db_rewind(DBHANDLE);
void db_close(DBHANDLE);
int db_store(DBHANDLE, const char *, const char *, int);

/* implementation limits */
#define IDXLEN_MIN 6    /* key, separator, start, separator, length, \n */
#define IDXLEN_MAX 1024 /* arbitrary */
#define DATLEN_MIN 2    /* data byte, newline */
#define DATLEN_MAX 1024 /* arbitrary */

/* flags for DB store, see p. 754 */
#define DB_INSERT 1     /* insert new record */
#define DB_REPLACE 2    /* replace existing record */
#define DB_STORE 3      /* replace or insert (upsert) */

#endif