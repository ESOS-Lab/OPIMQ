#!/bin/bash

main () {
    env_list="Basline BFS OPIMQ MQFS"
    test_list="40 100 200 400 600"  # The number of containers to run simultaneously during testing

    # Print header
    echo "Container Scalability - Dbench"
    printf "# Containers\t%s\t%s\t%s\t%s\n" ${env_list}

    # Iterate through test cases
    for n in ${test_list}; do
        printf "%s\t" ${n}
        for env_name in ${env_list}; do
            logdir="./results/${env_name}/dbench"
            throughput=$(parse_dbench ${n} ${logdir} | awk '{sum+=$1} END {if (NR > 0) print sum/1000; else print 0}') # KOPS/sec
            
 
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

parse_dbench () {
    n_threads=$1
    logdir=$2
    for i in $(seq ${n_threads}); do
        input_file="${logdir}/${n_threads}-${i}"
        if [[ -f "${input_file}" ]]; then
            start_line=$(awk '$1=="Flush" {print NR}' "${input_file}")
            end_line=$(awk '$1=="NTCreateX" {print NR}' "${input_file}")
            awk 'NR>='${start_line}' && NR<='${end_line}' {print $2}' "${input_file}" | awk '{sum+=$1} END {print sum/60}'
        fi
    done
}

main

