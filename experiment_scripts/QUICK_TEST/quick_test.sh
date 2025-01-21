#!/bin/bash
main () {
	LOGDIR="./LOGS"
	TARGET="/mnt"	# Target mount point.
	DEV_MODEL="Samsung SSD 980"	# Target Device Model
	DEV_NAME="$(get_devname)"
	TRACING_ON="1"	# If this value is "1", collecting logs by ftrace is activate.
	PYTHON_SCRIPT="parse_ftrace_log.py"
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
		echo 1 > /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable
		echo 563200 > /sys/kernel/debug/tracing/buffer_size_kb
		cat /dev/null > /sys/kernel/debug/tracing/trace
		echo 1 > /sys/kernel/debug/tracing/tracing_on
	fi
	fio ./fio_quick_test	# 4KB write size, random write followed by fsync for 1M file.

	if [[ ${TRACING_ON} == "1" ]];then
		echo 0 > /sys/kernel/debug/tracing/tracing_on
		echo 0 > /sys/kernel/debug/tracing/events/nvme/nvme_setup_cmd/enable
		echo "Copy the collected ftrace log to ${LOGDIR} ..."
		cp /sys/kernel/debug/tracing/trace ${LOGDIR}
	fi
}

parse_ftrace () {
	LOG_FILE="./LOGS/trace"
	python3 "$PYTHON_SCRIPT" "$LOG_FILE"

}
main
