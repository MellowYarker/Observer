#!/bin/bash

# todo: rename dynamic libs to force gcc to use static ones

# set up, compile, and install libbtc
echo "Installing libbtc..."
cd libbtc
sudo ./autogen.sh
sudo ./configure --disable-net # looks like we cannot --disable-wallet on linux
sudo make
# sudo make check # double check this
# make install # we don't need to install it anymore

# compile libbloom
echo "Installing libbloom..."
cd ../libbloom
make
# could delete symlinks and just rename the .so to what ld wants.

# create sqlite3 database with keys table
echo "Creating Observer database..."
cd ../db
db=observer.db

if [ -f "$db" ]
then
    echo "$db already exists."
else
    sqlite3 $db < configure.sql
fi
