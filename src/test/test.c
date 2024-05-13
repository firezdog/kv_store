// test_app.c
#include "apue_db.h"
#include "apue.h"
#include <fcntl.h>  // for O_CREAT

int main() {
    DBHANDLE db = db_open("db_name", O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
    printf("%d\n", db_store(db, "hello", "world", DB_STORE));
    db_close(db);
    return 0;
}
