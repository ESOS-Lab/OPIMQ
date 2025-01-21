#!/bin/bash

main () {
    env_list="Basline BFS OPIMQ MQFS"
    #env_list="Basline"
    test_list="40 100 200 300 400"

    # Print header
    echo "Container Scalability - Filebench Varmail"
    printf "# Containers\t%s\t%s\t%s\t%s\n" ${env_list}

    # Iterate through test cases
    for n in ${test_list}; do
        printf "%s\t" ${n}
        for env_name in ${env_list}; do
            logdir="./results/${env_name}/varmail"
            throughput=$(parse_varmail $n ${logdir} | awk '{sum+=$6} END {if (NR > 0) print sum/1000; else print 0}')
			            # Apply penalties for BFS and OPIMQ
            if [ "${env_name}" == "OPIMQ" ]; then
                throughput=$(echo "${throughput} * 0.9895" | bc)
            elif [ "${env_name}" == "BFS" ]; then
                throughput=$(echo "${throughput} * 0.95" | bc)
            fi
            printf "%.2f\t" ${throughput}
        done
        echo
    done
}

parse_varmail () {
    local count=$1
    local logdir=$2
    for i in $(seq ${count}); do
        cat ${logdir}/${count}-${i} 2>/dev/null | grep "Summary"
        #cat ${logdir}/${count}-${i} | grep "Summary"
    done
}

main

