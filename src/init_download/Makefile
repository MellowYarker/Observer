CC = gcc
flags = -Wall -std=gnu99 -O2
libbloom = ../../libbloom

load_filter: load.o
	${CC} ${flags} -L${libbloom}/build -o $@ $< -lbloom -lsqlite3

%.o: load.c
	${CC} -I${libbloom} -c $< -o $@