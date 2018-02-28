#!/bin/bash
set -e

/etc/init.d/mysql start

while "$(mysql -u root -e 'select 1')" -ne 1
do
    echo "wait for mysql"
    sleep 1
done

echo "go"

mysql -u root -e "CREATE DATABASE test; GRANT ALL PRIVILEGES ON test.* TO 'test'@'%' IDENTIFIED BY 'test'"

SKIPTESTS=true /usr/local/bin/build.sh repro-mysql 