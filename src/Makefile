BLOOM = ../libbloom
STATIC_BLOOM = ${BLOOM}/build
libbtc = ../libbtc

gen_keys: generate_keys.c key_funcs.c
	${libbtc}/libtool --silent --tag=CC --mode=link gcc -I${libbtc}/include/btc \
	-I${BLOOM} -g -O2 -W -std=gnu99 -pedantic -Wno-unused-function \
	-Wno-long-long -Wno-overlength-strings -static -o $@ $^ \
	${libbtc}/libbtc.la -L${STATIC_BLOOM} -lbloom -lsqlite3 -lm

