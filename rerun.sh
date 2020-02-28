#!/bin/bash
pid=$(netstat -tlnp| grep 26000| grep -o [0-9]*/|grep -o [0-9]*)
kill -9 $pid
cd ~/Documents/MIT-6.828/lab
make clean
make qemu
