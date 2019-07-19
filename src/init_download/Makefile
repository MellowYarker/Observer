CC = gcc
flags = -Wall -std=gnu99 -O2
libbloom = ../../libbloom

load_filter: load.o
	${CC} ${flags} -o $@ -L${libbloom}/build $< -lbloom -lm -lsqlite3

load.o: load.c
	${CC} ${flags} -I${libbloom} -c $< -o $@
