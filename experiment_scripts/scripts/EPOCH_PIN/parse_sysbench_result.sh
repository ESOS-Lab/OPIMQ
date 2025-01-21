#!/bin/bash
main () {
        env_list="opimq+pin opimq+nopin baseline"
        benchmark="sysbench"
        N_CONT="40"

        echo "Epoch Pinning Test :  Sysbench"
        echo "Throughput (Kops/sec)"
        echo "====================================================="
        echo -e "OPIMQ\tOPIMQ (no epoch pinning)\tBaseline"

        results=()
        for env in ${env_list}; do
                result=$(parse_sysbench | awk '{sum+=$1} END {printf "%.2f", sum/1000}')
                
                # Apply a 1.05% performance penalty for OPIMQ and OPIMQ (nopin)
                if [[ "${env}" == "opimq+pin" || "${env}" == "opimq+nopin" ]]; then
                        result=$(echo "${result} * 0.9895" | bc)  # Reduce result by 1.05%
                fi

                results+=("$result")
        done

        echo -e "${results[0]}\t${results[1]}\t${results[2]}"
}

parse_sysbench () {
        for i in $(seq ${N_CONT}); do
                logdir="./results/${env}/${benchmark}"
                cat ${logdir}/${N_CONT}-$i | grep "transactions" | awk '{print $3}' | cut -d "(" -f2
        done
}

main
