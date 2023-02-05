PORT=$(shuf -i 3000-4000 -n 1)
ADDRESS=$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')
ONE=1
PORTMASTER=$(($1 - $ONE))

echo "SLAVE SERVER ON ADDRESS $ADDRESS AND PORT $1"
echo "PLEASE USE MASTER SERVER PORT ON $PORTMASTER WITH THE SAME ADDRESS"

./tsdslave -p $1 $ -i $ADDRESS