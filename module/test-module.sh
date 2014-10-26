#!/bin/bash
set -euo pipefail

make clean
(lsmod | grep genocide_prober) && rmmod genocide_prober
make
insmod genocide-prober.ko
pushd ../program
make
popd
first=$(../program/killcounter)
if [ "$first" != "0" ]; then
	echo "First run: expected 0 got $first"
	exit 1
fi
sleep 100 &
kill -9 $(pidof sleep)
second=$(../program/killcounter)
if [ "$second" != "1" ]; then
	echo "Second run: expected 1 got $second"
	exit 1
fi
echo "Test passed!"
