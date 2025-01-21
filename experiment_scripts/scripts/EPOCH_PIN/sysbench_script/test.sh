#!bin/bash
trap run_varmail SIGUSR1

run_varmail () {
	echo "Run varmail workload"
	filebench -f /tmp/varmail.f 
	exit 0
}

while true;
do
	echo "Waiting Signal"
	sleep 1
done
