"""
Download and store all bitcoin addresses that have been used since the first 
block. The script gets the blocks from blockchain.com's api.

There are a lot of addresses, and finding them by iterating blocks is pretty 
slow, so it's a good idea to run this every few thousand blocks.

To speed the process up, addresses.txt stores all the addresses found in the 
first 550000+ blocks, and will be continuously updated. These are added to the 
database and progress.b on set up.
"""
from functions import check_download, get_addresses, update_db, update_progress
import pickle
import requests
import signal

block = 0
addresses = set()
new_addresses = set()

def signal_handler(signal, frame):
    """
    Handles a SIGINT signal caused by ctrl + c.
    When the script is killed, all progress will be saved.
    """
    print("\nReceived keyboard interupt signal.")
    save_progress(block, addresses, new_addresses)
    raise KeyboardInterrupt()

def save_progress(block, addresses, new_addresses):
    """
    Saves the work we've completed.
    """
    print("Saving progress... last block scanned was {}.".format(block))
    update_progress(block, addresses)
    update_db(new_addresses)

if __name__ == "__main__":
    flag = False
    # try to load previous progress (if there is any)
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

    # we start handling keyboard interrupts here
    signal.signal(signal.SIGINT, signal_handler)

    print("You can save and quit by pressing ctrl + c. If an error occurs, your"
          " progress will be saved. Once the most recent block is found, the"
          " script will exit.")


    # make first request, then loop and check response
    url = "https://blockchain.info/block-height/{}?format=json"
    response = requests.get(url.format(block))
    try:
        while (response.json()['blocks'][0]['next_block'] != []):
            get_addresses(addresses, new_addresses, response)
            block += 1
            check_download(block) # prints progress every 100 blocks
            response = requests.get(url.format(block))
    except KeyboardInterrupt:
        exit(1)
    except:
        save_progress(block, addresses, new_addresses)
        print("Your progress has been saved. An error has occured, you should "
        "run the script again to continue from block {}.".format(block))
        exit(1)

    save_progress(block, addresses, new_addresses)
    print("Download completed at block {}. This is the most recent block.".format(block))
    exit(0)