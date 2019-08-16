libbloom = ../libbloom
STATIC_BLOOM = ${libbloom}/build
libbtc = ../libbtc
flags = -g -O2 -W -std=gnu99 -pedantic -Wno-unused-function -Wno-long-long \
		-Wno-overlength-strings
libs = ${libbtc}/libbtc.la -L${STATIC_BLOOM} -lbloom -lsqlite3 -lm

# generate bitcoin addresses
gen_keys: generate_keys.o key_funcs.o
	${libbtc}/libtool --silent --tag=CC --mode=link gcc ${flags} -static -o $@ \
	$^ ${libs}

# collect and parse mempool transcations
reader: reader.o reader_funcs.c socket.c
	${libbtc}/libtool --silent --tag=CC --mode=link gcc ${flags} -static -o $@ \
	$^ ${libs} -L/usr/local/libs -lwebsockets

%.o: %.c
	gcc -I${libbtc}/include/btc -I${libbloom} -c $< -o $@

socket.c: socket.c
	mkdir sock_build
	cd sock_build
	cmake .. && make
	cd ..