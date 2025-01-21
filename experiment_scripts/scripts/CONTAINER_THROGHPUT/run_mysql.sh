#!/bin/bash
docker load < mysql_8_0_16.tar
sleep 3
docker run -it -p 3306:3306 --name mysql \
-v /mnt/mydata/mysql/log:/var/log/mysql \
-v /mnt/mydata/mysql/data:/var/lib/mysql \
-v /mnt/mydata/mysql/conf:/etc/mysql/conf.d \
-e MYSQL_ROOT_PASSWORD=root -d mysql:8.0.16
