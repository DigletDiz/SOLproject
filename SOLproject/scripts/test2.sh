#!/bin/bash

echo "Welcome to Test2"; 

SERVER_CMD=./server #Eseguibile del server
CLIENT_CMD=./client #Eseguibile del client
CONFIG=server_config.txt

echo -e "SOCKNAME=\nMAX_FILE=10\nSTORAGE_CAPACITY=1000000\nCLIENTS_EXPECTED=10\nCORES=4\nTHREAD_WORKERS=4" > ${CONFIG}

sleep 1s

valgrind --leak-check=full $SERVER_CMD & #Avvia il server in background con valgrind e i flag settati
pid=$!
sleep 5s

#Scrivo 11 file che superano il limite MAXSIZE
for i in {1..11}; do
  $CLIENT_CMD -v ./fileTest2/file$i.txt
done
#$CLIENT_CMD -v ./fileTest2/file1.txt

sleep 2s

kill -s SIGHUP $pid
wait $pid