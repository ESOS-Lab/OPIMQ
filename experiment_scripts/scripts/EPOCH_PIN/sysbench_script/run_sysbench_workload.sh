#!bin/bash

run_sysbench () {
	echo "After Trap"
	#/home/filebench-pthdir-throughput/filebench/filebench -f /tmp/varmail.f 
	#/home/filebench/filebench -f /tmp/varmail.f 
	#dbench --directory=/tmp --sync-dir --clients-per-process=1 -t 60 1
	./tmp/run
	exit 1
}

echo My process ID is $$
trap run_sysbench SIGUSR1

while true;
do	
#	echo "Test"
	sleep 1
done
