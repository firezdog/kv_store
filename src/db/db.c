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

        this implements a "first write wins" (??) policy for opening db --
            if A and B come in and B opens after A, whatever work A did will be erased
            but if A did no work and B goes and does its work, then when it comes time for A,
            A will fstat and see the size is not 0, so it will cancel trying to init the db

            "first write wins" -- not sure, because it is the last CREATE that wins
    */
    if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) {
        if (writew_lock(db->idxfd, 0, SEEK_SET, 0) < 0) {
            err_dump("db_open: writew_lock error");
        }

        if (fstat(db->idxfd, &statbuff) < 0) {
            err_sys("db_open: fstat error");
        }

        if (statbuff.st_size == 0) {
            /* build list of (NHASH_DEF + 1) chain ptrs with value of 0, + 1 is for free list pointer preceding hash table */
            /* 
                ensure that asciiptr contains stringified 0 and is prefixed with enough empty characters to fill it 
                "every pointer string we write to the database occupies exactly PTR_SZ characters"
                then fill the hash table with pointer strings

                in example, PTR_SZ is set to 4 and NHASH_DEF is set to 3 -- we will be able to address up to offset 9999
                    in the data file for the 10000 bytes mentioned in the book
            */
            sprintf(asciiptr, "%*d", PTR_SZ, 0);
            hash[0] = 0;
            for (i = 0; i < NHASH_DEF + 1; i++) {
                strcat(hash, asciiptr);
            }
            strcat(hash, "\n");

            /* actually write the pointer strings into the database file -- ensure that everything is written */
            i = strlen(hash);
            if (write(db->idxfd, hash, i) != i) {
                err_dump("db_open: index file init write error");
            }
        }

        if (un_lock(db->idxfd, 0, SEEK_SET, 0) < 0) {
            err_dump("db_open: error releasing db lock");
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