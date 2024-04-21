// db.c
#include "apue_db.h"
#include "apue.h"
#include <fcntl.h>
#include <stdarg.h>

#define PTR_SZ 7        /* size of ptr field in hash chain */
#define NHASH_DEF 137   /* default hash table size, remember it is static hashing */
#define HASH_OFF PTR_SZ  /* size of ptr field in hash chain */

typedef unsigned long DBHASH;   // hash values

// representation of the database
typedef struct {
    int idxfd;      // index file descriptor
    int datfd;      // data file descriptor
    char *name;     // db name
    char *idxbuf;   // index record buffer
    char *datbuf;   // data record buffer
    off_t hashoff;  // offset in index file of hash table
    DBHASH nhash;   // current hash table size
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

    db->nhash = NHASH_DEF;      // hash table size
    db->hashoff = HASH_OFF;     // offset of hashtable in index file

    /* set db name and add ".idx" */
    strcpy(db->name, pathname);
    strcat(db->name, ".idx");

    /* 
        open the database:
            (1) create it if "create" mode was specified
     */
    if (oflag & O_CREAT) {
        /* find optional third argument (mode) of type int*/
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);

        /* open index and data file */
        db->idxfd = open(db->name, oflag, mode);
        strcpy(db->name + len, ".dat");
        db->datfd = open(db->name, oflag, mode);
    }

    if (db->idxfd < 0 || db->datfd < 0) {
        // TODO: create function to free db
        printf("database should be freed");
        return NULL;
    }

    /* initialize db if created -- requires write lock, otherwise
        process A opens db and is interrupted
        process B opens db, initializes, and writes a record
        process A initializes and the record is lost
    */
    if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) {
        if (writew_lock(db->idxfd, 0, SEEK_SET, 0) < 0) {
            err_dump("db_open: writew_lock error");
        }
    }

    return db;
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