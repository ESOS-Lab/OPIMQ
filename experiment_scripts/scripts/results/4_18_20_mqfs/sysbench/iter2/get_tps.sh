#!/bin/bash
#read num
read n
for num in "$n";do 
#for num in $(seq 20 20 420);do 
	cat ${num}-* | grep transactions | awk '{print $3}' | cut -d "(" -f2 | awk '{sum+=$1}END {print sum/1000}'
done
