#!/bin/bash
#for i in $(seq 10 10 300);do
main () {
	read n
	logdir="./results/mqfs/test/journal_${n}m"
	result=$(mktemp --suffix=".dat")
	index=$(mktemp --suffix=".dat")
	#for i in $(seq 20 20 600);do
	#for i in "600";do
	#for i in $n;do
	for i in $(seq 2 2 40);do
		do_parse $i | awk '{sum+=$1} END {print sum/1000/1000/60}' >> ${result}
		echo $i >> ${index}
	done
	
	paste ${index} ${result}
} 
do_parse () {
	for j in $(seq $1);do
		file="${logdir}/thread$1"
		if [ -e ${file} ];then
			start_line=$( cat ${file} | awk '/Flush/ {print NR}'); 
			end_line=$( cat ${file} | awk '/NTCreateX/ {print NR}');
			#echo ${start_line} 
			#echo ${end_line} 
			#start_line="88"
			#end_line="101"
			cat ${file} | awk 'NR>='${start_line}' && NR <='${end_line}' {print $2}'
		fi
	done
}
main

