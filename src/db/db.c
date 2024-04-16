// db.c
#include "apue_db.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define PTR_SZ 7 /* size of ptr field in hash chain */
#define NHASH_DEF 137 /* default hash table size, remember it is static hashing */

// representation of the database
typedef struct {

} DB;

// ... for optional permissions to use when creating the db
DBHANDLE db_open(const char* pathname, int oflag, ...)
{
    printf("build database\n");
    DB *db;
    int len, mode;
    size_t i;
    char asciiptr[PTR_SZ + 1], hash[(NHASH_DEF + 1) * PTR_SZ + 2];  // + 2 for \n and null

    len = strlen(pathname);

    return NULL;
}

static DB * _db_alloc(int namelen)
{
    DB *db;

    if ((db = calloc(1, sizeof(DB))) == NULL) {
        // TODO: we will need to define this in the common header probably
        err_dump("_db_alloc: calloc error for DB");
    }
}