CC = gcc
FLAGS = -Wall -g -std=gnu99 -O
DEBUG = -DDEBUG
BLOOM = ../libbloom
STATIC_BLOOM = ${BLOOM}/build

gen_keys: generate_keys.c key_funcs.c
	${CC} ${FLAGS} `pkg-config --cflags --libs ../libbtc/libbtc.pc` -lsqlite3 \
	-I ${BLOOM} -L${STATIC_BLOOM} -lbloom -o $@ $^

debug: generate_keys.c key_funcs.c
	${CC} ${FLAGS} ${DEBUG} `pkg-config --cflags --libs ../libbtc/libbtc.pc` \
	-lsqlite3 -I ${BLOOM} -L${STATIC_BLOOM} -lbloom -o gen_keys $^