# Reference

Advanced Unix Programming Ch. 20

# Features

* open a database in one of the following modes:
    * create
    * read
    * read and update
* manage db contents / query
    * store a kv pair in the database
        * insert (error if it already exists)
        * update (error if it does not exist)
        * upsert (error if it already exists?)
    * delete a kv pair from the database, if it exists
    * get the value for a key in the database, if it exists
* read db sequentially
    * rewind
    * next

# Implementation

* two files, index and data file
    * index file: B-tree or hashing -- here hashing
    * store records as character strings to avoid binary format issues
        * i.e. even integers
        * disk space sacrificed for portability
    * only support a single record per key, no secondary keys
* index file
    * chain pointers to offsets in the data table
    * pointer to next, length of data, key, separator, data offset, data length, terminator
        * so we can determine where the dat acorresponding to the key starts and where it ends
* get
    * calculate hash value of key
    * get from hash in hash table
    * follow hash chain to the record we want (linked list when there are collisions)
* example (idx)
    * 0 53 35 0
        * empty free list
        * three hash chain pointers
    * 0 10Alpha:0:6
        * 0 is the chain pointer
        * 10 is the length
            * read fixed sized chain pointer and length (i.e. 4 chars for length)
            * read variable length portion
            * Alpha is the key, beginning at 0 and going 6 chars
            * ":" cannot appear in the key (separator)
            * terminate with newline to make text file easier to operate on
    * to produce example
        * initialize db in read / write / create / truncate / file mode (??)
        * insert (Alpha, data1)
        * insert (beta, data for beta)
        * store (gamma, record for gamma)
        * close db
        * exit
    * order of keys in index files matches order of insert
    * fast access for small hash chains
        * dynamic hashing: locate any record w/ two disk accesses
        * b-tree: traverse database in key order (more functionality for nextrec)

# Centralization

* centralized: processes communicate with db manager via ipc, and db manager does all i/o
    * customer tuning (priorities) vs. OS decisions
    * recovery (no cleanup for outstanding transactions)
* decentralized: processes lock and do their own i/o
    * faster (no ipc, no kernel trap)
    * record locking and i/o per process
        * the db is just the files and the conventions for inserting etc.
    * our choice (simpler to implement?)

# Concurrency

* coarse grained locking
    * byte range locking semantics on the files
    * multiple readers, one writer
        * read lock: get, next
        * write lock: delete, store
    * limited concurrency
* fine grained
    * read / write lock on hash chain
        * multiple readers, one writer
    * operations:
        read, readv, write, writev

# Online Resource
* https://github.com/roktas/apue2e/blob/master/db/apue_db.h
    * source code and makefile for this book

# Implementation Notes

* db_open
    * allocate the db
    * opened with create flag?
        * yes: open with flag + mode (permissions)
        * else: open with flag
    * successfully opened?
        * yes: continue
        * no: free and return
    * initialize if opened with create flag
        * write lock
        * get file size
        * if not yet created (file size == 0)
            * copy out first row (the hash)
            * check we copied out all chars
        * unlock
    * rewind pointer to start of db

* db_rewind
    * set db structure's index offset to first index record
        * this is 
            * the number of pointers * pointer size 
            * \+ free list pointer * pointer size 
            * \+ 1 for newline

* db_store
    * inputs: db handle, key + data, flag (insert, replace, store)
        * i think replace is update and insert is put, store is upsert that covers both cases
    * validate flag, validate data length
    * determine where the data item goes in the hash table, change the table entry (put new entry at head)
        * _db_find_and_lock
            * need to write lock the hash chain when updating (specified via flag)
            * returns -1 if there is an error
            * side effect -- sets db->chainoff to the place this record should be
        * if we don't find the entry and we wanted to update, return an error
            * we also record the number of store errors associated with the db for some reason
        * otherwise
            * get to first index on hash chain
            * find a free empty record
                * if we can't, record goes to the end ? -- but doesn't this ruin the hashing scheme??
                    * no, the hash chain marks where it is, we just optimize by looking for a free record b/f we append
                    * ^ remember that the hash chain is just telling us where we go to in what amounts to a log
                * otherwise, record replaces the existing record, and that goes to front of hash chain
                    * so it's ordered by most recent mod?
            * use of _db_writedat, _db_writeidx, _db_writeptr
            * analytics (cnt_stor2, errors, etc.)
    * doreturn label: unlocks hash chain at the end of the function
    * will need to try to simplify by first unwinding the code for a simple insert
        * i.e. first we will only support put, then we will add update?

* _db_find_and_lock (db handle, key, lock flag)
    * used by insert and other methods
    * find record for a given key
        * convert key to hash value -- get starting address of hash chain (chainoff)
        * wait for lock if requested, only lock first byte of hash chain start to increase parallelism
        * read first pointer in chain -- 0 means empty hash chain
        * iterate and compare keys, read records until we reach an empty record
            * at end offset = 0 means we found no key
            * otherwise ptroff contains address of prev record, datoff of data record, datlen size
                * helpful to know previous record when deleting

* _db_hash (db handle, key)
    * multiply each char times 1-index, divide by nhash entries
        * uses primes for good hash space distribution (see Knuth)

* locking race condition on create
    * currently the way things are set up, two requests to create a db are in a race
    * a lock is acquired for the initialization during create, but locking requires an fd
        * lockfile would be the solution
        * another option: just supplement the request with requirement that the file not exist