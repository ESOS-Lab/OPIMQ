#!/bin/bash
main () {
	N_CONT=600
	DEV_MODEL="Samsung SSD 980 PRO 1TB"
	devname=$(get_devname)
	for n in ${N_CONT};do
		docker rm -f $(docker ps -aq)
		systemctl stop docker.service
		umount -l /mnt
		mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=10240 /dev/${devname}
		mount -t ext4 /dev/nvme3n1 /mnt
		docker rm -f $(docker ps -aq)
		sleep 2
		docker_init
	    sleep 2
		create_containers $n
		copy_scripts $n
	done
}

docker_init () {
	systemctl start docker.service
	docker load < centos_dbench.tar
}


get_devname () {	# Get the device name corresponding to the target device model

	udisksctl status | grep "${DEV_MODEL}" | awk '{print $7}'

}

create_containers () {
	for i in $(seq $1);do
		docker run -itd --name=container${i} --privileged -v container$i:/tmp centos:dbench
	done
}

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"	
		cp run_dbench_workload.sh /mnt/volumes/container${i}/_data
		sleep 0.1
	done
}

exec_containers () {
	for i in $(seq $1);do
		echo "Exec container $i"
		docker exec container${i} /tmp/run_dbench_workload.sh > ${logdir}/$1-${i} &
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
			PID=$(docker top container${i} | grep "run_dbench_workload" | awk '{print $2}')
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
	sleep 150
}
main
