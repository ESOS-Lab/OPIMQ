
#if [ $# != 0 ] && [ $# != 1 ]; then
#	echo "Usage $0 [None|number]"
#	exit -1
#fi

get_kernel_list () {
	if [ $1 -eq 0 ]; then
		sudo grep ^menuentry /etc/grub2-efi.cfg \
		| cut -d "'" -f2 | cat -n 
	else
		sudo grep ^menuentry /etc/grub2-efi.cfg \
		| cut -d "'" -f2 | sed -n ${1}p
	fi	
}

echo "Please enter the kernel name to execute:"
echo "  B  - Baseline"
echo "  O  - OPIMQ"
echo "  M  - MQFS"
echo "  BFS - BFS with SQ"
echo "  N   - OPIMQ without epoch pinning"
read target_kern
case "${target_kern}" in
	"O")
	kern_name="5.18.18-opext4+opimq)"
	;;
	"M")	
	kern_name="MQFS)"
	;;
	"B")
	kern_name="5.18.18+original)"
	;;
	"BFS")
	kern_name="bfs+sq)"
	;;
	"N")
	kern_name="opext4+opimq+nopin)"
	;;
esac


num=$( sudo grep ^menuentry /etc/grub2-efi.cfg | cut -d "'" -f2 | cat -n | grep "${kern_name}" |  awk '{print $1}')
kernel=`get_kernel_list $num`
echo grub2-set-default \"$kernel\"
sudo grub2-set-default "${kernel}"

exit;


case $1 in
	0)
	sudo grep ^menuentry /boot/grub2/grub.cfg | cut -d "'" -f2 ;;
	1)
	grub2-set-default "CentOS Linux (3.10.0-693.el7.x86_64) 7 (Core)" ;;
	2)
	grub2-set-default "CentOS Linux (3.10.0+) 7 (Core)" ;;
	3)
	grub2-set-default "CentOS Linux (3.10.0-xfs-fbarrier) 7 (Core)" ;;
	4)
	grub2-set-default "CentOS Linux (3.10.0-barrier) 7 (Core)" ;;
	5)
	grub2-set-default "CentOS Linux (3.10.0-barrier.old) 7 (Core)" ;;
	6)
	grub2-set-default "CentOS Linux (3.10.0-barrier-lockstat) 7 (Core)" ;;
	*)
	echo wrong number;;
esac
