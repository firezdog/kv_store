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
