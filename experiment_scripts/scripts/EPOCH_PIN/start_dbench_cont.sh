#!/bin/bash
main () {	
	echo "Enter the experiment environment: Baseline (B) | OPIMQ w/ Epoch Pin (E) | OPIMQ NO Epoch Pin (NE)"
	read env
	if [[ ${env} == "B" ]] ;then
		env_name="baseline"
	elif [[ ${env} == "E" ]];then
		env_name="opimq+pin"
	elif [[ ${env} == "NE" ]];then
		env_name="opimq+nopin"
	fi
	logdir="./results/${env_name}/dbench"
	mkdir -p $logdir
	n="40"
	exec_containers $n
	check_all_containers $n
	sleep 10

	# Start to run workload
	start_containers $n
}

copy_scripts () {
	for i in $(seq $1);do
		echo "Copy script container $i"
		#docker cp run_workload.sh container${i}:/home 
		#docker cp varmail.f container${i}:/home
		cp run_dbench_workload.sh /mnt/volumes/container${i}/_data
		#cp varmail.f /mnt/volumes/container${i}/_data
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
