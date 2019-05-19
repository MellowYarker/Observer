CC = gcc
FLAGS = -Wall -g -std=gnu99
DEBUG = -DDEBUG
BLOOM = ../libbloom
STATIC_BLOOM = ${BLOOM}/build

gen_keys: generate_keys.c
	${CC} ${FLAGS} `pkg-config --cflags --libs ../libbtc/libbtc.pc` \
	-I ${BLOOM} -L${STATIC_BLOOM} -lbloom -o $@ $<

debug: generate_keys.c
	${CC} ${FLAGS} ${DEBUG} `pkg-config --cflags --libs ../libbtc/libbtc.pc` \
	-I ${BLOOM} -L${STATIC_BLOOM} -lbloom -o gen_keys $<