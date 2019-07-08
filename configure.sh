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
# if you only care about UTxO, then you can skip this (eventually!)
echo "Do you want to download all addresses that have been used in the last ~584,000 blocks? (y/n):"
read response
if [ $response = "y" ]
then
    mkdir address_sets
    echo "Downloading addresses from S3, this will take some time."
    sh s3_download.sh
    # load the addresses into the database
    echo "Done! Loading addresses into sqlite3."
    sh load.sh
    echo "Addresses loaded!"
else
    echo "Skipping download. You can do this at a later time, just read over the configure.sh script."