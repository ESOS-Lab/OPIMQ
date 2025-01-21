#!/bin/bash
main () {
	#J_SIZE_LIST="128 1024 10240"
	J_SIZE_LIST="128"
	#J_SIZE_LIST="128"
	for j_size in ${J_SIZE_LIST};do
	logdir="/home/oslab/jieun/temp_data_dir/ext4/proc_stat/iter2/journal_${j_size}m"
	mkdir -p ${logdir}
	mkdir -p ${logdir}/proc_dat
	#thread_list="1 2 6 10 14 18 22 26 30 34 38 40"
	thread_list="1 2 20 40"
	init_device $j_size

#	echo 0 > /proc/sys/kernel/lock_stat
	for i in ${thread_list};do
#	for i in $(seq 2 2 40); do
#		echo 0 > /proc/sys/kernel/lock_stat
		echo "Test $i"
#		cat /dev/null > /proc/lock_stat

#		echo 1 > /proc/sys/kernel/lock_stat
#		filebench -f ./varmail_script/varmail_${i}.f > ${logdir}/thread${i}
		dbench --directory=/mnt --sync-dir --clients-per-process=1 -t 60 $i > ${logdir}/thread${i}
		cat ${logdir}/thread${i} | grep Throughput
		echo "get op"
		get_op $i
		echo "get tx"
		get_tx $i
		echo "get chk"
		get_chk $i
	done
	done


}

init_device() {
	umount -l /mnt
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=$1 /dev/nvme2n1
	mount -t ext4 /dev/nvme2n1 /mnt
}

get_op (){
	touch temp_op
	touch ${logdir}/proc_dat/op_${1}.dat
while true;
do
	cat /proc/fs/jbd2/nvme2n1-8/op > ./temp_op
	cat ./temp_op >> ${logdir}/proc_dat/op_${1}.dat
	if [ "`cat ./temp_op | grep END`" == END ]
	then
		break
	fi
done
	rm -rf temp_op

}

get_tx () {

	touch temp_tx
	touch ${logdir}/proc_dat/tx_${1}.dat
while true;
do
	cat /proc/fs/jbd2/nvme2n1-8/tx > ./temp_tx
	cat ./temp_tx >> ${logdir}/proc_dat/tx_${1}.dat
	if [ "`cat ./temp_tx | grep END`" == END ]
	then
		break
	fi
done
	rm -rf temp_tx

}

get_chk () {

	touch temp_chk
	touch ${logdir}/proc_dat/chk_${1}.dat
while true;
do
	cat /proc/fs/jbd2/nvme2n1-8/chk > ./temp_chk
	cat ./temp_chk >> ${logdir}/proc_dat/chk_${1}.dat
	if [ "`cat ./temp_chk | grep END`" == END ]
	then
		break
	fi
done
	rm -rf temp_chk

}
main
