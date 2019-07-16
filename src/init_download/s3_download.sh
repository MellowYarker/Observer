range=35
mkdir address_backup

for ((i=0;i<=$range;i++)); do
	filename="result_${i}.sql"
	sudo wget -P address_backup "https://observer-test-bucket.s3.amazonaws.com/sql/${filename}"
done;
