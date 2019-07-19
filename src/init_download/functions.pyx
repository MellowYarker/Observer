import pickle
import requests
import sqlite3

cpdef update_db(set new_addresses):
    db = "../../db/observer.db"
    cdef int size
    cdef str address

    try:
        conn = sqlite3.connect(db)
        cur = conn.cursor()
        size = len(new_addresses)

        # build query
        cur.execute("BEGIN;")
        for i in range(size):
            address = new_addresses.pop()
            value = (address,)
            cur.execute("INSERT OR IGNORE INTO usedAddresses VALUES (?)", value)
        cur.execute("COMMIT;")
        conn.close()
    except ValueError as e:
        print(e)
    except:
        cur.execute("ROLLBACK;")


cpdef get_addresses(set addresses, set new_addresses, response):
    cdef list transactions = response.json()['blocks'][0]['tx']
    cdef dict i
    cdef dict j
    cdef str candidate

    for i in transactions:
        for j in i['out']:
            if 'addr' in j:
                candidate = j['addr']
                if candidate not in addresses:
                    new_addresses.add(candidate)
                    addresses.add(candidate)

cpdef check_download(int block):
    if block % 100 == 0:
        print("Downloaded block {}".format(block))

cpdef update_progress(int block, set addresses, str progress):
    """
    block [int]: the latest block we've scanned
    addresses [set]: the current set of unique addresses

    Serialize the current block and address set for later use.
    """
    try:
        fname = open(progress, "wb")
        obj = {'block': block, 'addresses': addresses}
        pickle.dump(obj, fname, pickle.HIGHEST_PROTOCOL)
        fname.close()
    except IOError as e:
        print(e)
