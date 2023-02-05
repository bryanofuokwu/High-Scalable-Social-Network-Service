
Compile the code using the provided makefile:

`make`

To clear the directory (and remove .txt files):
   
`make clean`


**Please follow steps to run program** 

1. Open all VMs
2. First run slave bash script for the VM1 with the routing server:

    VM1 `sh tsdslave.sh 3011`
3. Run script for routing-master server (note it has to have port number one less than the server)

    VM1 `sh tsdrouting.sh 3010`
    
    After doing this you will receive the credentials for the routing server that you will need for step **5**. 
4. On the next three VMS run the slave with whatever port number. I provided example port numbers
   
    VM2
    `sh tsdslave.sh 3012`
    
    VM3
    `sh tsdslave.sh 3014`
    
    VM4
    `sh tsdslave.sh 3016`
5. Next, run all of the masters with port number one less than its respective slave server port number. 
You need to pass the credentials of the routing server to it as well. 
If my master was running in IP `10.0.2.15` and port `3010`, then I need to 
pass in `10.0.2.15:3010`. This is needed because the masters have a bidirectional stream with the routing server. 
Thus the script needs to know the routing server correct IP and port which is dependent on VM's IP.

    VM2
    `sh tsdmaster.sh 3011 10.0.2.15:3010`
    
    VM3
    `sh tsdmaster.sh 3013 10.0.2.15:3010`
    
    VM4
    `sh tsdmaster.sh 3015 10.0.2.15:3010`
    
    
    
**WHEN CRASHING A SERVER**

You dont have to wait 30 seconds. The resurrection happens instantaneously. To choose connect to another server please use
the command `list` **twice** inside the client. That will trigger the client to connect to the elected server.


