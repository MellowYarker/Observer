#!/bin/bash

# todo: rename dynamic libs to force gcc to use static ones

# set up, compile, and install libbtc
echo "Installing libbtc..."
cd libbtc
./autogen.sh
./configure --disable-wallet --disable-tools
make check
make install

# compile and install secp256k1
echo "Installing secp256k1..."
cd src/secp256k1
make
make install

# compile libbloom
echo "Installing libbloom..."
cd ../../../libbloom
make
cd build
rm *.dylib

#   *note* since all these keys are really just integers, see if it's worth it to
#   store them in the DB as numbers rather than varchars. Since we're at risk of
#   performing a lot of DB reads, it might make look up faster.

# create sqlite3 database with keys table
echo "Creating Observer database..."
cd ../../db
db=observer.db

if [ -f "$db" ]
then
    echo "$db already exists."
else
    sqlite3 `$db` < configure.sql
fi

cd ../src
pip3 install cython
echo "Compiling Cython.."
python3 setup.py build_ext --inplace
echo "Loading all used bitcoin addresses..."
python3 load_addresses.py
echo "Addresses loaded!"
# nohup python3 download.py &