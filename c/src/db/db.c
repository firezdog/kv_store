// db.c
#include "apue_db.h"
#include "apue.h"
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

// page 755
#define IDXLEN_SZ 4  /* index record length (ASCII chars) */
#define NEWLINE '\n' /* newline char */
#define SEP ':'

#define PTR_SZ 7        /* size of ptr field in hash chain */
#define PTR_MAX 9999999 /* max file offset = 10 ** PTR_SZ - 1 */
#define NHASH_DEF 137   /* default hash table size, remember it is static hashing */
#define HASH_OFF PTR_SZ /* size of ptr field in hash chain */

typedef unsigned long DBHASH; // hash values

// representation of the database
typedef struct DB
{
    int idxfd;      // index file descriptor
    int datfd;      // data file descriptor
    char *name;     // db name
    char *idxbuf;   // index record buffer
    char *datbuf;   // data record buffer
    off_t ptrval;   // contents of chain ptr in index record
    off_t hashoff;  // offset in index file of hash table
    off_t idxoff;   // offset for current record in hash table
    off_t chainoff; // offset of hash chain for this index record
    off_t ptroff;   // chain ptr offset pointing to this idx record
    off_t datoff;   // offset of data record
    size_t datlen;  // size of data record
    DBHASH nhash;   // current hash table size
} DB;

static DB *_db_alloc(int);
static void _db_free(DB *);
static int _db_find_and_lock(DB *, const char *, int);
static off_t _db_readptr(DB *, off_t);
static off_t _db_readidx(DB *, off_t);
static DBHASH _db_hash(DB *, const char *);
static void _db_writedat(DB *, const char *, off_t, int);
static void _db_writeidx(DB *, const char *, off_t, int, off_t);
static void _db_writeptr(DB *, off_t, off_t);

// "..." for optional permissions to use when creating the db
DBHANDLE db_open(const char *pathname, int oflag, ...)
{
    DB *db;
    int len;
    int mode;
    size_t i;
    char asciiptr[PTR_SZ + 1];
    char hash[(NHASH_DEF + 1) * PTR_SZ + 2]; // + 2 for \n and null
    struct stat statbuff;

    len = strlen(pathname);
    if ((db = _db_alloc(len)) == NULL)
    {
        err_dump("db_open: error allocating memory for DB");
    }

    db->nhash = NHASH_DEF;  // hash table size
    db->hashoff = HASH_OFF; // offset of hashtable in index file

    /* set db name and add ".idx" */
    strcpy(db->name, pathname);
    strcat(db->name, ".idx");

    /*
        open the database:
            (1) create it if "create" mode was specified
     */
    if (oflag & O_CREAT)
    {
        /* find optional third argument (mode) of type int*/
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, int);
        va_end(ap);

        /* open index and data file --
            add O_EXCL flag to avoid opening a database that already exists in truncate mode
            otherwise the locking that occurs below is useless
        */
        db->idxfd = open(db->name, oflag | O_EXCL, mode);
        strcpy(db->name + len, ".dat");
        db->datfd = open(db->name, oflag | O_EXCL, mode);
    }

    if (db->idxfd < 0 || db->datfd < 0)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "apue_db: error while creating db -- %s already exists\n", pathname);
        }
        _db_free(db);
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
    if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))
    {
        if (writew_lock(db->idxfd, 0, SEEK_SET, 0) < 0)
        {
            err_dump("db_open: writew_lock error");
        }

        if (fstat(db->idxfd, &statbuff) < 0)
        {
            err_sys("db_open: fstat error");
        }

        if (statbuff.st_size == 0)
        {
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
            for (i = 0; i < NHASH_DEF + 1; i++)
            {
                strcat(hash, asciiptr);
            }
            strcat(hash, "\n");

            /* actually write the pointer strings into the database file -- ensure that everything is written */
            i = strlen(hash);
            if (write(db->idxfd, hash, i) != i)
            {
                err_dump("db_open: index file init write error");
            }
        }

        if (un_lock(db->idxfd, 0, SEEK_SET, 0) < 0)
        {
            err_dump("db_open: error releasing db lock");
        }
    }

    db_rewind(db);
    return db;
}

void db_rewind(DBHANDLE h)
{
    DB *db = h; // cast generic pointer to our pointer.
    size_t offset = (db->nhash + 1) * PTR_SZ + 1;

    if ((db->idxoff = lseek(db->idxfd, offset, SEEK_SET)) == -1)
    {
        err_dump("db_rewind: lseek error");
    }
}

void db_close(DBHANDLE h)
{
    DB *db = h;
    _db_free(db);
}

// see page 744
int db_store(DBHANDLE h, const char *key, const char *data, int flag)
{
    enum RETURN_CODE
    {
        OK, // store succeeded no problem
    };
    enum RETURN_CODE code = OK;

    /* validate user request for store op */
    int valid = (flag == DB_INSERT || flag == DB_REPLACE || flag == DB_STORE);
    if (!valid)
    {
        errno = EINVAL;
        return -1;
    }

    // TODO: delete
    if (flag != DB_INSERT)
    {
        errno = EINVAL;
        err_dump("REPLACE / STORE not implemented");
    }

    /* validate data length (length of data + newline) */
    int datlen = strlen(data) + 1;
    if (datlen < DATLEN_MIN || datlen > DATLEN_MAX)
    {
        errno = EINVAL;
        err_dump("db_store: data length out of bounds");
    }

    DB *db = h;
    int IS_LOCK = 1;
    if (_db_find_and_lock(db, key, IS_LOCK) < 0)
    { /* record not found */
        // TODO: replace + error handling if we try to replace a nonexistent record

        off_t ptrval = _db_readptr(db, db->chainoff);

        // TODO: reuse a free record if available
        /* append new record to ends of index and data files
            we write the data *first* b/c if we wrote a key that had no data, the db would be in an inconsistent state
            whereas an extra data record that nothing points to just wastes space
        */
        _db_writedat(db, data, 0, SEEK_END);
        _db_writeidx(db, key, 0, SEEK_END, ptrval);
        /* _db_writeidx sets db->idxoff, new record goes to front of chain */
        _db_writeptr(db, db->chainoff, db->idxoff);
    }

    return code;
}

static void _db_writedat(DB *db, const char *data, off_t offset, int whence)
{
    struct iovec iov[2];
    static char newline = NEWLINE;

    /* lock if appending */
    if (whence == SEEK_END)
    {
        int is_locked = writew_lock(db->datfd, 0, SEEK_SET, 0);
        if (is_locked < 0)
        {
            err_dump("_db_writedat: writew_lock error");
        }
    }

    off_t datoff = lseek(db->datfd, offset, whence);
    if (datoff == -1)
    {
        err_dump("_db_writedat: lseek error");
    }
    size_t datlen = strlen(data);

    db->datoff = datoff;
    db->datlen = datlen + 1; // + 1 for newline

    // write data first, then newline using separate io vector
    iov[0].iov_base = (char *)data;
    iov[0].iov_len = datlen;
    iov[1].iov_base = &newline;
    iov[1].iov_len = 1;

    // write both vectors
    int written = writev(db->datfd, &iov[0], 2);
    if (written != db->datlen)
    {
        err_dump("_db_writedat: error when writing data record");
    }

    // unlock if appending
    if (whence == SEEK_END)
    {
        int unlocked = un_lock(db->datfd, 0, SEEK_SET, 0);
        if (unlocked < 0)
        {
            err_dump("_db_writedat: error unlocking file");
        }
    }
}

// pg. 772
static void _db_writeidx(DB *db, const char *key, off_t offset, int whence, off_t ptrval)
{
    struct iovec payload[2]; // the data we need to write for this index record
    char asciiptrlen[PTR_SZ + IDXLEN_SZ + 1];

    // validate that the pointer points to an index entry we can write to, then write to state
    if (ptrval < 0 || ptrval > PTR_MAX)
    {
        err_quit("_db_writeidx: invalid ptr: %d", ptrval);
    }
    db->ptrval = ptrval;

    // populate index buffer for this entry: key|offset|length
    sprintf(db->idxbuf, "%s%c%lld%c%ld\n", key, SEP, (long long)db->datoff, SEP, (long)db->datlen);
    int len = strlen(db->idxbuf);
    if (len < IDXLEN_MIN || len > IDXLEN_MAX)
    {
        err_dump("_db_writeidx: invalid length");
    }
    // add pointer value and pointer length ensuring that fixed size is respected: chain_ptr|idx_len
    // the two above give us the following: chain_ptr|idx_len|key|offset|length
    sprintf(asciiptrlen, "%*lld%*d", PTR_SZ, (long long)ptrval, IDXLEN_SZ, len);

    /* as elsewhere, ensure append is atomic (not necessary for overwrite) */
    if (whence == SEEK_END)
    {
        int locked = writew_lock(db->idxfd, ((db->nhash + 1) * PTR_SZ) + 1, SEEK_SET, 0);
        if (locked < 0)
        {
            err_dump("_db_writeidx: writew_lock error");
        }
    }

    /* determine where to put the index entry */
    off_t idxoff = lseek(db->idxfd, offset, whence);
    if (idxoff == -1)
    {
        err_dump("_db_writeidx: lseek error");
    }
    db->idxoff = idxoff;

    /* write the payload */
    payload[0].iov_base = asciiptrlen;
    payload[0].iov_len = PTR_SZ + IDXLEN_SZ;
    payload[1].iov_base = db->idxbuf;
    payload[1].iov_len = len;

    int written = writev(db->idxfd, &payload[0], 2);
    if (written != PTR_SZ + IDXLEN_SZ + len)
    {
        err_dump("_db_writeidx: writev error of index record");
    }

    /* unlock */
    if (whence == SEEK_END)
    {
        int unlocked = un_lock(db->idxfd, ((db->nhash + 1) * PTR_SZ) + 1, SEEK_SET, 0);
        if (unlocked < 0)
        {
            err_dump("_db_writeidx: unlock error");
        }
    }
}

// page 770
/* write chain pointer field in appropriate location */
static void _db_writeptr(DB *db, off_t offset, off_t ptrval)
{
    // check bounds
    if (ptrval < 0 || ptrval > PTR_MAX)
    {
        err_quit("_db_writeptr: invalid ptr: %d", ptrval);
    }

    // copy pointer value into fix-sized buffer
    char asciiptr[PTR_SZ + 1];
    sprintf(asciiptr, "%*lld", PTR_SZ, (long long)ptrval);

    // determine where to write and write
    int found = lseek(db->idxfd, offset, SEEK_SET);
    if (found == -1)
    {
        err_dump("_db_writeptr: lseek error to ptr field");
    }

    int written = write(db->idxfd, asciiptr, PTR_SZ);
    if (written != PTR_SZ)
    {
        err_dump("_db_writeptr: write error of ptr field");
    }
}

static void _db_free(DB *db)
{
    /* close the files if they were opened */
    if (db->idxfd > -1)
    {
        close(db->idxfd);
    }
    if (db->datfd > -1)
    {
        close(db->datfd);
    }

    /* null checks are not necessary but good practice */
    if (db->idxbuf != NULL)
    {
        free(db->idxbuf);
    }
    if (db->datbuf != NULL)
    {
        free(db->datbuf);
    }
    if (db->name != NULL)
    {
        free(db->name);
    }

    free(db);
}

// page 763
static int _db_find_and_lock(DB *db, const char *key, int is_lock)
{
    /*
        calculate expected key hash, then look for chain pointer in entry
        lock the first byte of the hash chain -- write lock or read lock depending on flag
        CALLER MUST UNLOCK!
    */
    db->chainoff = (_db_hash(db, key) * PTR_SZ) + db->hashoff;
    if (is_lock)
    {
        if (writew_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
        {
            err_dump("_db_find_and_lock: writew_lock error");
        }
    }
    else if (readw_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
    {
        err_dump("_db_find_and_lock: readw_lock error");
    }

    db->ptroff = db->chainoff;
    off_t offset = _db_readptr(db, db->ptroff);
    while (offset != 0) {
        off_t nextoffset = _db_readidx(db, offset);
        if (strcmp(db->idxbuf, key) == 0) { break; } // found a match }
        db->ptroff = offset; // move to next record
        offset = nextoffset;
    }

    return offset == 0 ? -1 : 0;
}

static off_t _db_readidx(DB *db, off_t offset) {
    // TODO
    return 0;
}

static DBHASH _db_hash(DB *db, const char *key)
{
    /* calculate hash as contribution from each char based on index + 1 */
    DBHASH hash = 0;
    for (int i = 0; key[i] != '\0'; i++)
    {
        int contribution = i + 1;
        hash += (DBHASH)key[i] * contribution;
    }

    // TODO: static for testing that chains work properly
    // return hash % db->nhash;
    return 5;
}

static off_t _db_readptr(DB *db, off_t offset)
{
    /* retrieve stored chain pointer starting at offset; returns zero if the pointer does not exist */
    // find the requested offset
    if (lseek(db->idxfd, offset, SEEK_SET) == -1)
    {
        err_dump("_db_readptr: unable to find offset");
    }
    // read contents of hash chain into buffer and null terminate
    char asciiptr[PTR_SZ + 1];
    if (read(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ)
    {
        err_dump("_db_readptr: error retrieving pointer from file");
    }
    asciiptr[PTR_SZ] = 0;

    // return result as long
    return atol(asciiptr);
}

/* allocation of DB structure and its buffers */
static DB *_db_alloc(int namelen)
{
    DB *db;

    if ((db = calloc(1, sizeof(DB))) == NULL)
    {
        err_dump("_db_alloc: calloc error for DB");
    }

    // reset file descriptors with sentinel values
    db->idxfd = -1;
    db->datfd = -1;

    // DB name is the name specified + 4 for ".idx" / ".dat" + null terminator
    if ((db->name = malloc(namelen + 5)) == NULL)
    {
        err_dump("_db_alloc: malloc error when allocating name");
    }

    /*
        allocate index  and data buffers -- +2 for newline & null term
        TODO: could we allow these to dynamically expand?
            track size and call realloc as needed.
    */
    if ((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL)
    {
        err_dump("_db_alloc: malloc error for index buffer");
    }
    if ((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL)
    {
        err_dump("_db_alloc: malloc error for data buffer");
    }

    return db;
}