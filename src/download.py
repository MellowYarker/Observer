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
# use a set because we don't want to waste time sorting and making a list unique
addresses = set()

# make first request, then loop and check response
url = "https://blockchain.info/block-height/{}?format=json".format(block)
response = requests.get(url)

while (response.json()['blocks'][0]['next_block'] != []):
# for i in range(40): # uncomment for a test loop
    # add all output addresses in this block to our used addresses set
    # get the next block
    block += 1
    for i in response.json()['blocks'][0]['tx']:
        for j in i['out']:
            if 'addr' in j:
                # we could check if in set then try to add to db here
                addresses.add(j['addr'])
    url = "https://blockchain.info/block-height/{}?format=json".format(block)
    response = requests.get(url)

# add to database
db = "../db/observer.db"
try:
    conn = sqlite3.connect(db)
    cur = conn.cursor()

    # build query
    start = "BEGIN;"
    cur.execute(start)
    for i in addresses:
        sql = "INSERT INTO usedAddresses VALUES ('{}');".format(i)
        cur.execute(sql)

    end = "COMMIT;"
    cur.execute(end)
    conn.close()

except ValueError as e:
    print(e)
