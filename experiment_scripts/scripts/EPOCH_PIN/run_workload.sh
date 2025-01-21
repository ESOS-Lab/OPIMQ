#!bin/bash

run_varmail () {
	echo "After Trap"
	#/home/filebench-pthdir-throughput/filebench/filebench -f /tmp/varmail.f 
	/home/filebench/filebench -f /tmp/varmail.f 
	exit 1
}

echo My process ID is $$
trap run_varmail SIGUSR1

while true;
do	
#	echo "Test"
	sleep 1
done
