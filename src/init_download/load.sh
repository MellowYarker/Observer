for i in address_sets/*; do
    python3 load_addresses.py -f $i
done
