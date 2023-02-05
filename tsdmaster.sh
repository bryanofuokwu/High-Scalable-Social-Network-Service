PORT=$(shuf -i 3000-4000 -n 1)
ADDRESS=$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')
ONE=1
PORTSLAVE=$(($PORT + $ONE))

echo "MASTER SERVER ON $ADDRESS:$PORT"
echo "MAKE SURE SLAVE SERVER IS PORT ON $PORTSLAVE WITH THE SAME ADDRESS"
./tsd -p $1 -i $ADDRESS -s master -r $2

