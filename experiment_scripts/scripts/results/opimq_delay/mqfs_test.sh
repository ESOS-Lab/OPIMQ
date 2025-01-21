#!/bin/bash
main () {
MQFS_PATH="/home/oslab/seungwon/mqfs/ccnvme"
MNT_POINT="/mnt"
NCPU=40
DEVICE="/dev/nvme2n1"

	J_SIZE_LIST="128 1024 10240"
	#J_SIZE_LIST="128"
	for j_size in ${J_SIZE_LIST};do
		logdir="./results/mqfs/journal_${j_size}m"
		mkdir -p ${logdir}
		mkdir -p ${logdir}/proc

		init_mqfs $j_size
		cd /home/oslab/jieun/scrips/results/opimq_delay
		sleep 3
		for i in $(seq 2 2 40);do
		#for i in "2";do
			echo "Test $i"
			filebench -f ./varmail_script/varmail_${i}.f > ${logdir}/thread${i}
			cp -r /proc/fs/jbd2/nvme2n1-8/info ${logdir}/proc/info_thread${i}
		done
	done


}

init_device() {
	umount -l /mnt
	mkfs -t ext4 -E lazy_journal_init=0,lazy_itable_init=0 -J size=$1 /dev/${DEVICE}
	mount -t ext4 /dev/${DEVICE} /mnt
}

init_mqfs() {

#systemctl stop docker.service
umount /mnt

sudo rmmod mqfs.ko
sudo rmmod ccnvme.ko
echo "cp ./horae_${1}.h ${MQFS_PATH}/ccnvme/horae.h"
cp  ./horae_${1}.h ${MQFS_PATH}/ccnvme/horae.h

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

cd $(pwd)
}
main
