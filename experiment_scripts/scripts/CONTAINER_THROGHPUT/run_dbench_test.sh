#!/bin/bash
main () {
	get_kern_env
	echo "Test ${env_name}"
	logdir="./results/${env_name}/dbench"
	mkdir -p $logdir
	DEV_MODEL="Samsung SSD 980 PRO 1TB"
	MNT_DIR="/mnt"
	devname=$(get_devname)
	N_CONT="600"	# Create N_CONT containers
	test_list="40 100 200 400 600"	#The number of containers to run simultaneously during testing

	echo "Format FS and Mount"
	format_and_mount ${env_name}
	echo -e "Mount Done."
	create_containers ${N_CONT}

	echo -e "Exec Containers..."
	for n in ${test_list};do
		exec_containers $n
		check_all_containers $n
	sleep 10

	# Start to run workload
	start_containers $n

	sleep 1
	done
	echo -e "\n"
	echo "Experiment Done. The results are saved in ${logdir}"
}

get_kern_env () {

	cur_kern=$(uname -r)
	case "${cur_kern}" in
		*MQFS*)
			env_name="MQFS"
			;;

		*opimq*)
			env_name="OPIMQ"
			;;
		*bfs*)
			env_name="BFS"
			;;
		*original*)
			env_name="Basline"
			;;
	esac
}

format_and_mount () {
	systemctl start docker.service
	echo -e "Remove remaining containers...\n"
	docker rm -f $(docker ps -aq) > /dev/null
	systemctl stop docker.service
	umount -l ${MNT_DIR} > /dev/null

	if [[ "$1" == "MQFS" ]];then
		init_mqfs
	else
		mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=10240 /dev/${devname}
		echo -e "Mount /mnt"
		mount -t ext4 /dev/${devname} ${MNT_DIR}
	fi
}

init_mqfs () {
	MQFS_PATH="/home/oslab/seungwon/mqfs/ccnvme"
	NCPU=40
	DEVICE="/dev/${devname}"
	CUR_PATH=$(pwd)

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
	mount -t mqfs ${DEVICE} -o nr_streams=${NCPU} ${MNT_DIR}
	cd ${CUR_PATH}
}


create_containers () {
	echo -e "Start docker.service and load docker image\n"
	docker_init
	sleep 2
	load_containers $1
	copy_scripts $1
}

docker_init () {
	systemctl start docker.service
	docker load < centos_dbench.tar
}


get_devname () {	# Get the device name corresponding to the target device model

	udisksctl status | grep "${DEV_MODEL}" | awk '{print $7}'

}

load_containers () {
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
		#docker exec container${i} /tmp/run_dbench_workload.sh > ${logdir}/$1-${i} &
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
			num=`expr $num + $(docker top container${i} | grep "run_dbench_workload" | awk '{print $2}' | wc -l)`
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
	sleep 100
}
main
