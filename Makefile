all: clean apue db test

apue:
	gcc -I./src/include -Wall -g -c ./src/apue/apue.c -o ./build/apue.o
	ar rsv ./build/libapue.a ./build/apue.o

db: apue
	gcc -I./src/include -Wall -g -c ./src/db/db.c -o ./build/db.o
	ar rsv ./build/libapue_db.a ./build/db.o

test: db
	gcc -I./src/include -Wall -g -c ./src/test/test.c -o ./build/test.o
	gcc -L./build -o ./build/test ./build/test.o -lapue_db -lapue

clean:
	rm -rf build/*
	rm *.dat *.idx

.PHONY: all clean