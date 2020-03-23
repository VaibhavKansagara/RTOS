# RTOS

## Compile
gcc server.c -o server -lpthread -lpulse -lpulse-simple <br/>
gcc client.c -o client -lpthread -lpulse -lpulse-simple <br/>

## Run
./server <server_address> <port_no> voice_chat = 0/1 <br/>
./client <server_address> <port_no> voice_chat = 0/1 <br/>