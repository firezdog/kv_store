// db.c
#include "apue_db.h"
#include "apue.h"

#define PTR_SZ 7 /* size of ptr field in hash chain */
#define NHASH_DEF 137 /* default hash table size, remember it is static hashing */

// representation of the database
typedef struct {

} DB;

static DB * _db_alloc(int namelen);

// "..." for optional permissions to use when creating the db
DBHANDLE db_open(const char* pathname, int oflag, ...)
{
    DB *db;
    int len, mode;
    size_t i;
    char asciiptr[PTR_SZ + 1], hash[(NHASH_DEF + 1) * PTR_SZ + 2];  // + 2 for \n and null

    len = strlen(pathname);

    db = _db_alloc(len);

    return NULL;
}

static DB * _db_alloc(int namelen)
{
    DB *db;

    if ((db = calloc(1, sizeof(DB))) == NULL) {
        err_dump("_db_alloc: calloc error for DB");
    }

    return db;
}