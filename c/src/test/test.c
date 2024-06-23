// test_app.c
#include "apue_db.h"
#include "apue.h"
#include <fcntl.h>  // for O_CREAT

int main() {
    DBHANDLE db = db_open("db_name", O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
    if (db == NULL) {
        fprintf(stderr, "could not create db -- already exists");
        return -1; 
    }

    db_store(db, "apple", "1", DB_INSERT);
    db_store(db, "boy", "2", DB_INSERT);
    db_store(db, "clover", "3", DB_INSERT);
    db_close(db);
    return 0;
}
