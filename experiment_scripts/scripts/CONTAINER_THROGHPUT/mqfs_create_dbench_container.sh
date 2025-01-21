#!/bin/bash
main () {
	N_CONT=600
	for n in ${N_CONT};do
	docker rm -f $(docker ps -aq)
	init_mqfs
    sleep 2
	docker_init
    sleep 2
	create_containers $n
	copy_scripts $n
	#exec_containers $n
	#check_all_containers $n
	#start_containers $n
	done
}

docker_init () {
	systemctl start docker.service
	docker load < centos_dbench.tar
}

init_mqfs () {

	MQFS_PATH="/home/oslab/seungwon/mqfs/ccnvme"
	MNT_POINT="/mnt"
	NCPU=40
	DEVICE="/dev/nvme3n1"
	CUR_PATH=$(pwd)
	systemctl stop docker.service
	umount /mnt

	sudo rmmod mqfs.ko
	sudo rmmod ccnvme.ko
	cd ${MQFS_PATH} 
	cd nvme-host
	make -j ${NCPU}
	./reset_nvme.sh
	sleep 3

	cd ${MQFS_PATH}/ccnvme
	sleep 3
	insmod ccnvme.ko cp_device=${DEVICE} nr_streams=${NCPU}
	cd ..

	cd ${MQFS_PATH}/mqfs
	make -j ${NCPU}
	sleep 3
	insmod mqfs.ko

	mkfs.ext4 -E lazy_itable_init=0 -O ^has_journal ${DEVICE}
	mount -t mqfs ${DEVICE} -o nr_streams=${NCPU} ${MNT_POINT}
	cd ${CUR_PATH}
}


create_containers () {
	for i in $(seq $1);do
		docker run -itd --name=container${i} --privileged -v container$i:/tmp centos:dbench
	done
}

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"
		#docker cp run_workload.sh container${i}:/home 
		#docker cp varmail.f container${i}:/home
#		cp run_workload.sh /mnt/volumes/container${i}/_data
#		cp varmail.f /mnt/volumes/container${i}/_data

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
