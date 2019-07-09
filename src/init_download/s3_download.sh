a=4
b=4
c=5
d=8
e=7
f=5

get="wget -P address_sets https://observer-test-bucket.s3.amazonaws.com/address_sets/"
for ((i=0;i<=a;i++)); do
    $get"a_$i.b"
done;
for ((i=0;i<=b;i++)); do
    $get"b_$i.b"
done;
for ((i=0;i<=c;i++)); do
    $get"c_$i.b"
done;
for ((i=0;i<=d;i++)); do
    $get"d_$i.b"
done;
for ((i=0;i<=e;i++)); do
    $get"e_$i.b"
done;
for ((i=0;i<=f;i++)); do
    $get"f_$i.b"
done;
