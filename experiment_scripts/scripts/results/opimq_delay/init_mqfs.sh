#!/bin/bash
MQFS_PATH="/home/oslab/seungwon/mqfs/ccnvme"
MNT_POINT="/mnt"
NCPU=40
DEVICE="/dev/nvme2n1"

systemctl stop docker.service
umount /mnt

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
mount -t mqfs ${DEVICE} -o nr_streams=${NCPU} ${MNT_POINT}
