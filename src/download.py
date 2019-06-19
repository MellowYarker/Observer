"""
Download and store all bitcoin addresses that have been used.
For now we'll use the Blockchain.com API, though I'd like to use bitcoind's blocks
eventually.

Addresses will be stored in Observer's database.
Once all records have been written, they'll be added to a libbloom bloom filter.
"""

import requests
import sqlite3

# current block height
block = 0
addresses = []

# make first request, then loop and check response
url = "https://blockchain.info/block-height/{}?format=json".format(block)
response = requests.get(url)

while (response.json()['blocks'][0]['next_block'] != []):
# for i in range(3):
    # add all output addresses in this block to our used addresses list
    # get the next block
    print("Searching block: {}".format(block))
    block += 1
    for i in response.json()['blocks'][0]['tx']:
        for j in i['out']:
            if 'addr' in j:
                addresses.append(j['addr'])
    url = "https://blockchain.info/block-height/{}?format=json".format(block)
    response = requests.get(url)

# make sure addresses are unique (they definitiely won't be by default)
# build insert queries
for i in addresses:
    print(i)
