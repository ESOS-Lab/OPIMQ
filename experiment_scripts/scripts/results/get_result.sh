#!/bin/bash
main () {
	read i
	dir="./5_18_18_ext4"
	#dir="./5_18_18_opimq"
	#dir="./3_10_0_ext4"
	#for i in $(seq 460 20 600); do for j in $(seq $i);do cat "${dir}/$i-$j" | grep Summary | awk '{print $6}';done | awk '{sum+=$1} END {print sum/1000}';done
	for i in "$i"; do for j in $(seq $i);do cat "${dir}/$i-$j" | grep Summary | awk '{print $6}';done | awk '{sum+=$1} END {print sum/1000}';done
	#for j in $(seq $i);do cat "${dir}/$i-$j" | grep Summary | awk '{print $6}';done | awk '{sum+=$1} END {print sum/1000}'
}

main
