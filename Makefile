all: db test

db:
	gcc -I./src/include -Wall -g -c ./src/db/db.c -o ./build/db.o
	ar rsv ./build/libapue_db.a ./build/db.o

test:
	gcc -I./src/include -Wall -g -c ./src/test/test.c -o ./build/test.o
	gcc -L./build -o ./build/test ./build/test.o -lapue_db

clean:
	rm -rf build/*

.PHONY: all clean