# This script downloads blocks from blockchain.com's api, then parses these blocks
# for output bitcoin addresses.

"""
You can provide a serialized python object of the following format:
	
	obj = {"block": int, "addresses": set()}

to this script if you wish to start downloading at a specific block, or with
an existing set of addresses.

If you want to start from the first block, either:
    1. don't use the "-f" flag
    2. use "-f" and provide a file with the following object {"block": 0, "addresses": set()}

Ex: if I have a file called "progress.b", where "block": 4000 and "addresses"
is the empty set, I can run

	python3 download.py -f progress.b

to start from block 4000 and add to this set. Once the script finishes (can be by keyboard interrupt)
all progress will be saved in "progress.b".

Flags are:
	"-b {int}" the maximum block to iterate to
	"-d" update the database when finished
	"-f {file}" the file to read from and write to

Here's another example:
	"progress.b" has obj = {"block": 500000, "addresses": set()}. If we want to scan
	50,000 blocks and update the database at the end, we run the following.
	
	python3 download.py -f progress.b -b 550000 -d

The script can crash at times for strange reasons, so it's a good idea to call it in a loop.
A simple bash script could be along the lines of, "while the exit code is 1, run the script with these parameters".
"""
# Exit status codes:
#     - 0 process completed (caught up to most recent block)
#     - 1 process exited due to error (more blocks can be found)
#     - 2 process terminated by user (more blocks can be found)
#     - 3 out of memory

# Running this in parallel on different segments of the blockchain is the best way to get results fast.
# I recommend you don't scan more than 50,000 blocks per script unless you have A LOT of RAM (+16GB).


from functions import check_download, get_addresses, update_db, update_progress
import getopt
import pickle
import requests
import signal, struct, sys
import time

def signal_handler(signal, frame):
    """
    Handles a SIGINT signal caused by ctrl + c.
    When the script is killed, all progress will be saved.
    """
    print("\nReceived keyboard interupt signal.")
    raise KeyboardInterrupt()


def save_progress(block, addresses, new_addresses, progress_file, with_db):
    """
    Saves the work we've completed. If with_db is 1, we will write the 
    new_addresses to the database.
    """
    print("Saving progress... do not close/power off computer until process is"
          " complete.")
    update_progress(block, addresses, progress_file)
    if with_db == 1:
        print("Updating database...")
        update_db(new_addresses)
    else:
        print("Skipping database update.")
    print("Progress saved. Last block scanned was {}.".format(block))

def setup():
    """
    Parses the command line arguements.

    -d = update database
    -f {file} where file is the progress file to use
    -b {block} where block is the max block we will scan to
    """
    try:
        opts, args = getopt.getopt(sys.argv[1:], "f:db:")
    except getopt.GetoptError as err:
        print(err)
        print("Usage: python3 {script} -f <progress file> -d")
        sys.exit(2)

    progress_file = ""
    with_db = 0 # by default, do not write to the database
    max_block = -1

    # o is the flag, a is the arguement
    for o, a in opts:
        if o == "-f":
            progress_file = a
        elif o == "-d":
            with_db = 1 # update the database when script exits
        elif o == "-b":
            max_block = int(a)
        else:
            print("Unknown flag {}".format(o))
    
    return progress_file, with_db, max_block


if __name__ == "__main__":
    progress_file, with_db, max_block = setup()

    # try to load previous progress (if there is any)
    try:
        with open(progress_file, "rb") as f:
            progress = pickle.load(f)
            block = progress['block']
            addresses = progress['addresses']
    except IOError as e:
        block = 0
        addresses = set()
        progress_file = "temp_progress.b"
    print("Starting at block {}".format(block))

    if with_db == 1:
        print("Database will be updated when new addresses are found.")
    else:
        print("No database operations will be performed.")
    new_addresses = set()

    # we start handling keyboard interrupts here
    signal.signal(signal.SIGINT, signal_handler)

    print("\nYou can save and quit by pressing ctrl + c. If an error occurs, your"
          " progress will be saved.\nOnce the most recent block is found, the"
          " script will exit.")

    # make first request, then loop and check response
    url = "https://blockchain.info/block-height/{}?format=json"
    response = requests.get(url.format(block))
    try:
        if (max_block == -1):
            max_block = block + 50000 # scan 50,000 blocks unless otherwise specified
        print("Scanning from block {} to {}".format(block, max_block))

        # true if we will iterate until the most recent block
        new_loop = False
        for _ in range(max_block - block):
            # triggers when close to current block, so that we don't try to scan past it
            if (block >= 584000):
                new_loop = True
                break

            get_addresses(addresses, new_addresses, response)
            block += 1
            check_download(block) # prints progress every 100 blocks
            response = requests.get(url.format(block))

        if (new_loop):
            while (response.json()['blocks'][0]['next_block'] != []):
                get_addresses(addresses, new_addresses, response)
                block += 1
                check_download(block) # prints progress every 100 blocks
                response = requests.get(url.format(block))

    except KeyboardInterrupt:
        save_progress(block, addresses, new_addresses, progress_file, with_db)
        exit(2)
    except MemoryError as e:
        print("Out of memory! Will not overwrite (for saftey).")
        print(e)
        exit(3)
    except:
        print("Exiting because an error has occured, you should "
        "run the script again to continue from block {}.".format(block))
        exit(1)

    save_progress(block, addresses, new_addresses, progress_file, with_db)
    print("Download completed at block {}. This is the most recent block.".format(block))
    exit(0)
