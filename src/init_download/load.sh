# Note, used.sql is not in a folder, we download it straight from S3
$db="observer.db"

echo "Inserting used addresses into database. This will take a while."

sudo sqlite3 ../../db/$db < used.sql
echo "Done! Inserting all elements into bloom filter..."

sudo make load_filter
sudo ./load_filter
echo "Done! You can now compare your generated addresses to over 530 million
      used addresses!"
