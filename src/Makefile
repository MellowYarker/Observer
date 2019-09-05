libbloom = ../libbloom
STATIC_BLOOM = ${libbloom}/build
libbtc = ../libbtc
flags = -g -O2 -W -std=gnu99 -Wno-stringop-overflow \
		-Wno-missing-field-initializers
libs = ${libbtc}/libbtc.la -L${STATIC_BLOOM} -lbloom -lsqlite3 -lm
mac_ssl = /usr/local/Cellar/openssl/1.0.2q/include

.PHONY: all clean

# compiles both the gen_keys program and the reader program
all: gen_keys reader

# generates bitcoin addresses
gen_keys: generate_keys.o key_funcs.o
	${libbtc}/libtool --silent --tag=CC --mode=link gcc ${flags} -static -o $@ \
	$^ ${libs}

# collect and parse mempool transcations -- please generate key sets before
# running the reader program!
reader: reader.o reader_funcs.c socket.c
	${libbtc}/libtool --silent --tag=CC --mode=link gcc ${flags} -static -o $@ \
	-I${mac_ssl} $^ ${libs} cjson/cJSON.c -L/usr/local/lib -lwebsockets

%.o: %.c
	gcc -I${libbtc}/include/btc -I${libbloom} -c $< -I${mac_ssl} -o $@

socket.c: socket.c
	mkdir sock_build
	cd sock_build
	cmake .. && make
	cd ..

clean:
	rm -rf *.o
	rm -f reader
	rm -f gen_keys
