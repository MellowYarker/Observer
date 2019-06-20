"""
Download and store all bitcoin addresses that have been used.
For now we'll use the Blockchain.com API, though I'd like to use bitcoind's blocks
eventually.

Addresses will be stored in Observer's database.
Once all records have been written, they'll be added to a libbloom bloom filter.
"""
from functions import update_db, get_addresses, check_download
import pickle
import requests
import signal
import sqlite3

block = 0
addresses = set()
new_addresses = set()

# TODO: we need to write to the database too
def signal_handler(signal, frame):
    print("Saving work... last block scanned was {}".format(block))
    update_progress(block, addresses)
    update_db(new_addresses)
    exit(1)


def update_progress(block, addresses):
    try:
        fname = open("progress.b", "wb")
        obj = {'block': block, 'addresses': addresses}
        pickle.dump(obj, fname)
        fname.close()
    except IOError as e:
        print(e)

if __name__ == "__main__":
    flag = False
    try:
        fname = open("progress.b", "rb")
        progress = pickle.load(fname)
        fname.close()
    except IOError as e:
        flag = True
        print("Progress file will be created shortly.")

    if flag == True:
        print("Starting at block 0.")
    else:
        block = progress['block']
        addresses = progress['addresses']
        print("Starting at block {}".format(block))

    # we start handling sigint here
    signal.signal(signal.SIGINT, signal_handler)

    print("Progress will be saved when you press \"ctrl + c\" or after all "
        "addresses have been found. ")


    # make first request, then loop and check response
    url = "https://blockchain.info/block-height/{}?format=json".format(block)
    response = requests.get(url)
    try:
        while (response.json()['blocks'][0]['next_block'] != []):
            get_addresses(addresses, new_addresses, response)
            block += 1
            check_download(block)
            url = "https://blockchain.info/block-height/{}?format=json".format(block)
            response = requests.get(url)
    except InterruptedError as e:
        print(e)
        print("Something went wrong. Saving work and exiting!")

    update_progress(block, addresses)
    update_db(new_addresses)
    print("Finished at block {}".format(block))