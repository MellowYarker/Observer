import sqlite3
import requests
cpdef update_db(set new_addresses):
    db = "../db/observer.db"
    try:
        conn = sqlite3.connect(db)
        cur = conn.cursor()

        # build query
        start = "BEGIN;"
        cur.execute(start)
        for i in new_addresses:
            sql = "INSERT OR IGNORE INTO usedAddresses VALUES ('{}');".format(i)
            cur.execute(sql)

        end = "COMMIT;"
        cur.execute(end)
        conn.close()
    except ValueError as e:
        print(e)


cpdef get_addresses(set addresses, set new_addresses, response):
    cdef dict i
    cdef dict j
    for i in response.json()['blocks'][0]['tx']:
        for j in i['out']:
            if 'addr' in j:
                # we could check if in set then try to add to db here
                if j['addr'] not in addresses:
                    new_addresses.add(j['addr'])
                    addresses.add(j['addr'])


cpdef check_download(int block):
    if block % 100 == 0:
        print("Downloaded block {}".format(block))