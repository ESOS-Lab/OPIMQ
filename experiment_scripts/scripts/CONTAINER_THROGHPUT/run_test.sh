#!/bin/bash
main () {
	#logdir="results/3_10_0_ext4"
	#logdir="results/5_18_18_opimq"
	#logdir="results/5_18_18_ext4"
	#logdir="results/4_18_20_opimq/journal_10g"
	logdir="results/4_18_20_mqfs/optane900p/journal_default/iter2"
	mkdir -p $logdir
	N_CONTS="400"
	INTV="20"
	#docker rm -f $(docker ps -aq)
	#systemctl stop docker.service

	#umount -l /dev/nvme1n1
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=10240 /dev/nvme1n1
	mount -t ext4 /dev/nvme1n1 /mnt

	docker_init
	for n in "${N_CONTS}";do
		create_containers $n
		copy_scripts $n
		#exec_containers $n
		#check_all_containers $n
		#sleep 10
		#start_containers $n
	done
}

docker_init () {
	systemctl start docker.service
	docker load < centos_60s.tar
}

create_containers () {
	start_contnum=`expr $1 - ${INTV} + 1`
	echo "Create container from container${start_contnum} to container$1"
	for i in $(seq ${start_contnum} 1 $1);do
		docker run -itd --name=container${i} --privileged -v container$i:/tmp centos:60s
	done
}

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"
		cp run_workload.sh /mnt/volumes/container${i}/_data
		cp varmail.f /mnt/volumes/container${i}/_data
		sleep 0.1
	done
}



exec_containers () {
	for i in $(seq $1);do
		echo "Exec container $i"
		docker exec container${i} /tmp/run_workload.sh > ${logdir}/$1-${i} &
		sleep 0.2
	done
	echo ${DOCKER_PIDS}
}

check_all_containers () {
	loop="yes"
	while [ "${loop}" == "yes" ]; do
		DOCKER_PIDS=""
		num=0
		for i in $(seq $1);do
			PID=$(docker top container${i} | grep "run_workload" | awk '{print $2}')
			DOCKER_PIDS+=" ${PID}"
			num=`expr $num + $(docker top container${i} | grep "run_workload" | awk '{print $2}' | wc -l)`
			#num=`expr $num + $(docker top container${i} | grep "varmail" | awk '{print $2}' | wc -l)`
			echo "Total Running containers $num"
			if [ $num -eq "$1" ];then
				echo "End Loop"
				loop="no"
				break;
			fi
		done
	done
}

start_containers () {
	echo "Now starts the entire containers"
	echo ${DOCKER_PIDS}
	kill -10 ${DOCKER_PIDS}
	sleep 120
}
main
