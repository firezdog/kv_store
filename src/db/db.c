// db.c
#include "apue_db.h"
#include "apue.h"

#define PTR_SZ 7        /* size of ptr field in hash chain */
#define NHASH_DEF 137   /* default hash table size, remember it is static hashing */

// representation of the database
typedef struct {
    int idxfd;      // index file descriptor
    int datfd;      // data file descriptor
    char *name;     // db name
    char *idxbuf;   // index record buffer
    char *datbuf;   // data record buffer
} DB;

static DB * _db_alloc(int namelen);

// "..." for optional permissions to use when creating the db
DBHANDLE db_open(const char* pathname, int oflag, ...)
{
    DB *db;
    int len;
    int mode;
    size_t i;
    char asciiptr[PTR_SZ + 1];
    char hash[(NHASH_DEF + 1) * PTR_SZ + 2];  // + 2 for \n and null
    struct stat statbuff;

    len = strlen(pathname);

    if ((db = _db_alloc(len)) == NULL) {
        err_dump("db_open: error allocating memory for DB");
    }

    return NULL;
}

/* allocation of DB structure and its buffers */
static DB * _db_alloc(int namelen)
{
    DB *db;

    if ((db = calloc(1, sizeof(DB))) == NULL) {
        err_dump("_db_alloc: calloc error for DB");
    }

    // reset file descriptors with sentinel values
    db->idxfd = -1;
    db->datfd = -1;

    // DB name is the name specified + 4 for ".idx" / ".dat" + null terminator
    if ((db->name = malloc(namelen + 5)) == NULL) {
        err_dump("_db_alloc: malloc error when allocating name");
    }

    /* 
        allocate index  and data buffers -- +2 for newline & null term
        TODO: could we allow these to dynamically expand?
            track size and call realloc as needed. 
    */
    if ((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL) {
        err_dump("_db_alloc: malloc error for index buffer");
    }
    if ((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL) {
        err_dump("_db_alloc: malloc error for data buffer");
    }

    return db;
}