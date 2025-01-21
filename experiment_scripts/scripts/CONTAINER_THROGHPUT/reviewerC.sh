
echo "$(date) varmail - BEGINs" >> reviewerC.log
./run_varmail_test.sh
echo "$(date) varmail - ENDss" >> reviewerC.log

sleep 120

echo "$(date) varmail - BEGINs" >> reviewerC.log
./run_dbench_test.sh
echo "$(date) Dbench - ENDs" >> reviewerC.log


sleep 120

echo "$(date) Sysbench - BEGINs" >> reviewerC.log
./create_sysbench_containers.sh
sleep 120
./start_sysbench.sh
echo "$(date) Sysbench - ENDs" >> reviewerC.log
