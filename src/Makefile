libbloom = ../libbloom
STATIC_BLOOM = ${libbloom}/build
libbtc = ../libbtc
flags = -g -O2 -W -std=gnu99 -pedantic -Wno-unused-function -Wno-long-long \
		-Wno-overlength-strings
libs = ${libbtc}/libbtc.la -L${STATIC_BLOOM} -lbloom -lsqlite3 -lm

gen_keys: generate_keys.o key_funcs.o
	${libbtc}/libtool --silent --tag=CC --mode=link gcc ${flags} -static \
	-o $@ $^ ${libs}

%.o: %.c
	gcc -I${libbtc}/include/btc -I${libbloom} -c $< -o $@