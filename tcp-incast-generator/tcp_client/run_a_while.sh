#!/bin/bash
while true
do
    for i in `seq 6 10`;
    do
        echo "./l25_tcp_client conf/${i}servers.conf ${i}_1000_16kB.csv > ${i}_1000_16kB.txt"
        #./l25_tcp_client conf/${i}servers.conf ${i}_1000_8kB.csv > ${i}_1000_8kB.txt
        ./l25_tcp_client conf/${i}servers.conf t.csv
    done
done
