#!/bin/bash

# todo: rename dynamic libs to force gcc to use static ones

# set up, compile, and install libbtc
cd libbtc
./autogen.sh
./configure --disable-wallet --disable-tools
make check
make install

# compile libbloom
cd ../libbloom
make

#   *note* since all these keys are really just integers, see if it's worth it to
#   store them in the DB as numbers rather than varchars. Since we're at risk of
#   performing a lot of DB reads, it might make look up faster.

# create sqlite3 database with keys table
cd ../db
sqlite3 observer.db < configure.sql
