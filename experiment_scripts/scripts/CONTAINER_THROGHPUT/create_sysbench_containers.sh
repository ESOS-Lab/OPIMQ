#!/bin/bash
main () {
	N_CONT=400
	devname="nvme3n1"
	MNT_DIR="/mnt"
	get_kern_env
	for n in ${N_CONT};do
	#docker rm -f $(docker ps -aq)
	#systemctl stop docker.service
	#umount -l /dev/nvme3n1
	#mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=10240 /dev/nvme1n1
	#mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 /dev/nvme3n1
	#mount -t ext4 /dev/nvme3n1 /mnt
	format_and_mount ${env_name}
	systemctl start docker.service
	docker rm -f $(docker ps -aq)
	docker_init
	run_mysql_server
	create_containers $n
	copy_scripts $n
	#exec_containers $n
	#check_all_containers $n
	#start_containers $n
	done
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


docker_init () {
	systemctl start docker.service
	docker load < centos_sysbench.tar
}
run_mysql_server () {

docker load < mysql_8_0_16.tar
sleep 3
docker run -p 3306:3306 --name mysql \
-v /mnt/mydata/mysql/log:/var/log/mysql \
-v /mnt/mydata/mysql/data:/var/lib/mysql \
-v /mnt/mydata/mysql/conf:/etc/mysql/conf.d \
-e MYSQL_ROOT_PASSWORD=root -d mysql:8.0.16

}
create_containers () {
	for i in $(seq $1);do
		docker run -itd --name=container${i} --privileged -v container$i:/tmp --link mysql:sys_db centos:sysbench
	done
}

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"
		#docker cp run_workload.sh container${i}:/home 
		#docker cp varmail.f container${i}:/home
#		cp run_workload.sh /mnt/volumes/container${i}/_data
#		cp varmail.f /mnt/volumes/container${i}/_data
#		cp run_dbench_workload.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/check_db.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/create_sys_db.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/run_sysbench_workload.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/run /mnt/volumes/container${i}/_data
		cp ./sysbench_script/prepare /mnt/volumes/container${i}/_data
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
