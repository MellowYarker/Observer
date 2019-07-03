for i in storage/*; do
    python3 load_addresses.py -f $i
done