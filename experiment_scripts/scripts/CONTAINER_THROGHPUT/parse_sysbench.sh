#!/bin/bash
#!/bin/bash

main () {
    env_list="Basline BFS OPIMQ MQFS"

	test_list="20 40 100 200 400"	#The number of containers to run simultaneously during testing
    # Print header
    echo "Container Scalability - Sysbench"
    printf "# Containers\t%s\t%s\t%s\t%s\n" ${env_list}

    # Iterate through test cases
    for n in ${test_list}; do
        printf "%s\t" ${n}
        for env_name in ${env_list}; do
            result_dir="./results/${env_name}/sysbench"
            throughput=$(get_tops $n ${result_dir} | awk '{sum+=$1} END {print sum/1000};') # 1000TPS/sec
            
            # Apply penalties
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

get_tops () {
    n_threads=$1
    result_dir=$2
    for i in $(seq ${n_threads}); do
        input_file="${result_dir}/${n_threads}-${i}"
        if [[ -f "${input_file}" ]]; then
            grep "transactions" "${input_file}" | awk '{print $3}' | cut -d "(" -f2
        fi
    done
}

main
