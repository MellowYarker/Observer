"""
    Insert all addresses from Amazon S3 into local Observer database.
"""
from functions import update_db
import os
import pickle
import getopt
import sys

def setup():
    """
    Parses the command line arguements.

    -f {file} where file is the progress file to use
    """
    try:
        opts, args = getopt.getopt(sys.argv[1:], "f:")
    except getopt.GetoptError as err:
        print(err)
        print("Usage: python3 load_addresses.py -f <progress file>")
        return 0

    progress_file = ""

    # o is the flag, a is the arguement
    for o, a in opts:
        if o == "-f":
            progress_file = a
        else:
            print("Unknown flag {}".format(o))
    if (progress_file == ""):
        return 0
    return progress_file

try:
    progress_file = setup()
    if (progress_file == 0):
        print("Please enter a file to load into the database.")
        exit(1)

    print("Loading addresses from {}".format(progress_file))
    with open(progress_file, "rb") as f:
        data = pickle.load(f)
        count = data["count"]
        addresses = data["addresses"]
        print("Found {} addresses in {}.".format(count, progress_file))
        print("Updating database, this may take some time.")
        update_db(addresses)
        print("Update complete. Removing {}".format(progress_file))
        os.remove(progress_file)
except IOError as e:
    print(e)
except:
    print("Something went wrong. You should re-run the script using the current file.")
finally:
    print("Exiting.")
