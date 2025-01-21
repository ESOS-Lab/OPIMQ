#!/bin/bash
main () {
	TARGET="/mnt"	# Target mount point.
	DEV_MODEL="Samsung SSD 980"	# Target Device Model
	DEV_NAME="$(get_devname)"
	TRACING_ON="1"	# If this value is "1", collecting logs by ftrace is activate.
	systemctl stop docker.service
	echo "Enter the experiment environment: OPEXT4 (O) or BASELINE (B)"
	read env
	
	if [[ ${env} == "O" ]];then
		echo "OPEXT4 Experiment Running..."
		LOGDIR="./LOGS/OPEXT4"
	fi

	if [[ ${env} == "B" ]];then
		echo "Baseline Experiment Running ..."
		LOGDIR="./LOGS/EXT4"
	fi

	mkdir -p ${LOGDIR}
	# Initialize LOGDIR
	if [[ -d $LOGDIR ]];then
		# Delete all files in the directory
		rm -f "$LOGDIR"/*
		echo "All files in '$LOGDIR' have been deleted."
	fi


	echo "Step 1. Format the target device with FS and mount to the TARGET directory"
	echo "mkfs -t ext4 /dev/${DEV_NAME}"
	echo "mount -t ext4 /dev/${DEV_NAME} ${TARGET}"
	format_fs_and_mount

	
	echo "Step 2. Run basic workload w/ FIO benchmark: 4KB random write followed by fsync."
	run_workload

	echo "Step 3. Parse the raw data of ftrace"
	parse_ftrace

}

get_devname () {	# Get the device name corresponding to the target device model

	udisksctl status | grep "${DEV_MODEL}" | awk '{print $7}'

}

format_fs_and_mount () {	# Format the target device and mount.
	umount /mnt
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 /dev/${DEV_NAME} > /dev/null	# OP-EXT4 in OPIMQ kernel, EXT4 in Vanilla Kernel (Baseline).
	mount -t ext4 /dev/${DEV_NAME} ${TARGET} > /dev/null
}

run_workload () {
	if [[ ${TRACING_ON} == "1" ]];then
		echo 563200 > /sys/kernel/debug/tracing/buffer_size_kb
		echo 1 > /sys/kernel/debug/tracing/events/ext4/ext4_sync_file_enter/enable	
		echo 1 > /sys/kernel/debug/tracing/events/ext4/ext4_sync_file_exit/enable	
		echo 1 > /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable	
		echo 1 > /sys/kernel/debug/tracing/events/nvme/nvme_sq/enable	
		cat /dev/null > /sys/kernel/debug/tracing/trace
		echo 1 > /sys/kernel/debug/tracing/tracing_on
	fi
	fio ./fio_basic_test	# 4KB write size, random write followed by fsync for 1M file.

	if [[ ${TRACING_ON} == "1" ]];then
		echo 0 > /sys/kernel/debug/tracing/events/ext4/ext4_sync_file_exit/enable	
		echo 0 > /sys/kernel/debug/tracing/tracing_on
		echo 0 > /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable
		echo "Copy the collected ftrace log to ${LOGDIR} ..."
		cp /sys/kernel/debug/tracing/trace ${LOGDIR}
	fi
}

parse_ftrace () {
	LOG_FILE="${LOGDIR}/trace"
	if [[ ${env} == "O" ]];then
		fsync_latency=$(cat ${LOG_FILE} | grep "exit" | awk '{print $11/1000/1000}')
		TDMA_HOST=$(cat  ${LOG_FILE}  | grep "nvme_cmd_write" | grep "iolat" | awk '{print $13/1000/1000}' | awk '{sum+=$1}END{print sum}')
		Tflush=$(cat  ${LOG_FILE}  | grep "nvme_cmd_flush" | grep "iolat" | awk '{print $13/1000/1000}' | awk '{sum+=$1}END{print sum}')
		TFUA="0"
	fi

	if [[ ${env} == "B" ]];then
		fsync_latency=$(cat ${LOG_FILE} | grep "exit" | awk '{print $13/1000/1000}')
		TDMA_HOST=$(cat ${LOG_FILE} | grep "nvme_cmd_write" | grep "iolat" | head -n 3 | awk '{sum+=$7}END{print sum/1000/1000}')
		Tflush=$(cat ${LOG_FILE}  | grep "nvme_cmd_flush" | grep "iolat" | awk '{print $7/1000/1000}' | awk '{sum+=$1}END{print sum}')
		TFUA=$(cat ${LOG_FILE}  | grep "nvme_cmd_write" | grep "iolat" | tail -n 1 | awk '{sum+=$7}END{print sum/1000/1000}')
	fi

	
	outfile="${LOGDIR}/output"
	touch ${outfile}
	cat /dev/null > ${outfile}

	if [[ ${env} == "O" ]];then
		echo "OPEXT4 Result" >> ${outfile}
	fi

	if [[ ${env} == "B" ]];then
		echo "EXT4 Result" >> ${outfile}
	fi

	echo "Total fsync latency: ${fsync_latency} (msec)" >> ${outfile}
	echo "TDMA + THOST: ${TDMA_HOST} (msec)" >> ${outfile}
	echo "TFlush: ${Tflush} (msec)" >> ${outfile}
	echo "TFUA: ${TFUA} (msec)" >> ${outfile}
}
main
