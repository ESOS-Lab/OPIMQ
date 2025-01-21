#!/bin/bash
main () {
	N_CONT=40
	for n in ${N_CONT};do
	DEV_MODEL="Samsung SSD 980"
	devname=$(get_devname)
	DOCKER_IMG_PATH="./"
	docker rm -f $(docker ps -aq)
	systemctl stop docker.service
	umount -l /mnt
	#mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=10240 /dev/nvme1n1
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 /dev/${devname}
	mount -t ext4 /dev/${devname} /mnt
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

docker_init () {
	systemctl start docker.service
	docker load < ./centos_sysbench.tar
}

get_devname () {	# Get the device name corresponding to the target device model

	udisksctl status | grep "${DEV_MODEL}" | awk '{print $7}'

}

run_mysql_server () {

docker load < ${DOCKER_IMG_PATH}/mysql_8_0_16.tar
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


main
