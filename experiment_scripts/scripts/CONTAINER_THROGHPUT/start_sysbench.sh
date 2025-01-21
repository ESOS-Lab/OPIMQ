#!/bin/bash
main () {
	get_kern_env

	logdir="./results/${env_name}/sysbench"
	mkdir -p $logdir
	test_list="20 40 100 200 400"
	for n in ${test_list};do
	check_db_connect 1
	exec_containers $n

	sleep 2
	check_all_containers $n
	sleep 10


	start_containers $n
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

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"
		#docker cp run_workload.sh container${i}:/home 
		#docker cp varmail.f container${i}:/home
		cp ./sysbench_script/run_sysbench_workload.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/create_sys_db.sh /mnt/volumes/container${i}/_data
		cp ./sysbench_script/prepare /mnt/volumes/container${i}/_data
		#cp varmail.f /mnt/volumes/container${i}/_data
		sleep 0.1
	done
}

check_db_connect () {
	for i in $(seq $1);do
		echo "Check container $i"
		docker exec container${i} /tmp/check_db.sh
		sleep 0.2
		echo "Create sys_db"
		docker exec container$i /tmp/create_sys_db.sh
		echo "Prepare tables"
		docker exec container${i} /tmp/prepare
	done
	echo ${DOCKER_PIDS}
}



exec_containers () {
	for i in $(seq $1);do
		echo "Exec container $i"
		docker exec container${i} /tmp/run_sysbench_workload.sh > ${logdir}/$1-${i} &
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
			PID=$(docker top container${i} | grep "run_sysbench_workload" | awk '{print $2}')
			DOCKER_PIDS+=" ${PID}"
			num=`expr $num + $(docker top container${i} | grep "run_sysbench_workload" | awk '{print $2}' | wc -l)`
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

get_tx () {

	touch temp_tx
	touch ${logdir}/log/tx_${1}.dat
while true;
do
	cat /proc/fs/mqfs/nvme1n1-8/olayer_overhead > ./temp_tx
	cat ./temp_tx >> ${logdir}/log/tx_${1}.dat
	if [ "`cat ./temp_tx | grep Total`" == Total ]
	then
		break
	fi
done
	rm -rf temp_tx
}
main
