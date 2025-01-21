#!/bin/bash
main () {

	J_SIZE_LIST="1024"
	#J_SIZE_LIST="128"
	for j_size in ${J_SIZE_LIST};do
	logdir="./results/opimq/test/journal_${j_size}m"
	mkdir -p ${logdir}
	#thread_list="1 2 6 10 14 18 22 26 30 34 38 40"
	thread_list="40"
	init_device $j_size

#	echo 0 > /proc/sys/kernel/lock_stat
	for i in ${thread_list};do
#	for i in $(seq 40 -2 2); do
#		echo 0 > /proc/sys/kernel/lock_stat
		echo "Test $i"
#		cat /dev/null > /proc/lock_stat

#		echo 1 > /proc/sys/kernel/lock_stat
		filebench -f ./varmail_script/varmail_${i}.f > ${logdir}/thread${i}
#		echo 0 > /proc/sys/kernel/lock_stat
#		cp /proc/lock_stat  ${logdir}/lockstat_thread$i

		cp -r /proc/fs/jbd2/nvme2n1-8/info ${logdir}/info_thread${i}
		cat ${logdir}/thread${i} | grep Summary | awk '{print $6/1000}'
	done
	done


}

init_device() {
	umount -l /mnt
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=$1 /dev/nvme2n1
	mount -t ext4 /dev/nvme2n1 /mnt
}
main
