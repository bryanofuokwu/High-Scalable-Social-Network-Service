PORT=$(shuf -i 3000-4000 -n 1)
ADDRESS=$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')
ONE=1
PORTSLAVE=$(($1 + $ONE))

echo "ROUTING SERVER ADDRESS TO USE FOR 3 MASTERS AND CLIENTS $ADDRESS:$1"
echo "USE THE ABOVE INFO FOR THE CLIENT AND MASTER CONNECTIONS"
echo ""
echo "PLEASE USE SLAVE SERVER PORT ON $PORTSLAVE WITH THE SAME ADDRESS"

./tsd -p $1 -i $ADDRESS -s routing

