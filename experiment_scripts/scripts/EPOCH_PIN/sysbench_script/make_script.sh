#!/bin/bash
SET=$(seq 2 2 40);
for i in ${SET};
do
  script="run_${i}"
  cat /dev/null > ${script}
  cp temp run_${i}
  sed -i "s/tempthread/${i}/g" run_${i}

done
