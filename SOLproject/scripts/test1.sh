#!/bin/bash

#SERVER_CMD=./server
#CLIENT_CMD=./client
CONFIG=server_config.txt
# ALTERNATIVO per qualsiasi estensione: 
# out=${1%.*}-capital.txt

echo -e "SOCKNAME=\nMAX_FILE=100\nSTORAGE_CAPACITY=1280000\nCLIENTS_EXPECTED=20\nCORES=4\nTHREAD_WORKERS=1" > ${CONFIG}

sleep 1s

valgrind --track-origins=yes --leak-check=full ./server & # avvio il server in background
pid=$!

sleep 5s

./client -h 
./client -f cs_sock -v ./fileTest1/ade.txt -p -t200 -r ./fileTest1/ade.txt -d fileSavedCLIENT1 -v ./fileTest1/jinglebells.txt,./fileTest1/dai.txt
./client -f cs_sock -p -t300 -d fileSavedCLIENT2 -a ./fileTest1/ade.txt@" ciao" -a ./fileTest1/jinglebells.txt@" AYY" -R0

#for word in {1...3}; do
#    $CLIENT_CMD $word >> $out #invoco il client e salvo il risultato nel file $out
#done

sleep 5s

kill -s SIGHUP $pid # termino il server
wait $pid