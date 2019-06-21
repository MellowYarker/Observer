""" This script takes all the records in addresses.txt and stores them in binary
    so that the download.py script can store them in Observers database.

    This script should only be ran once. After completion we can probably delete
    addresses.txt to free up some space, since address info is in progress.b

    The addresses stored in addresses.txt are all unique bitcoin addresses that
    have been found on the blockchain. Storing them here means you don't have to
    search the blockchain for all the addresses that have ever been used.
"""
from functions import update_progress
import os
import pickle

try:
    with open("addresses.txt", "r") as f:
        block = int(f.readline())
        addresses = set()
        for i in f.readlines():
            addresses.add(i)
        update_progress(block, addresses)
        os.remove("addresses.txt")
except:
    print("Something went wrong.")