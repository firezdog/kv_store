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

    db_store(db, "hello", "data1", DB_INSERT);
    db_store(db, "goodbye", "data2", DB_INSERT);
    db_close(db);
    return 0;
}
