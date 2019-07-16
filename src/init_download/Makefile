CC = gcc
flags = -Wall -std=gnu99 -O2
libbloom = ../../libbloom

load_filter: load.o
	${CC} ${flags} -o $@ $< -L${libbloom}/build -l:libbloom.a -lm -lsqlite3

load.o: load.c
	${CC} -I${libbloom} -c $< -o $@
