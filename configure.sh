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

# could delete symlinks and just rename the .so/.dylib to what ld wants.

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
# make db writeable
sudo chmod 664 $db

cd ../src/init_download
pip3 install cython
echo "Compiling Cython.."
python3 setup.py build_ext --inplace
# if you only care about UTxO, then you can skip this (eventually!)
read -r -p "Do you want to download all addresses that have been used in the last ~584,000 blocks? (y/n):" response
case "$response" in
    [yY][eE][sS]|[yY]) 
        mkdir address_sets
        echo "Downloading addresses from S3, this will take some time."
        sh s3_download.sh
        # load the addresses into the database
        echo "Done! Loading addresses into sqlite3, this will take a while so it will be run in the background."
        echo "If at any point you want to quit, just find the process and kill it via"
        echo "kill -KILL <pid>"
        echo "For example, ps aux | grep load.sh"
        echo "Then ps aux | grep load_adresses.py"
        sh load.sh &do_something
        ;;
    *)
        echo "Skipping download. You can do this at a later time, just read over the configure.sh script."
        ;;
esac

echo "Done."