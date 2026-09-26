#!/bin/sh
#
# Stress test
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

cd "`dirname "$0"`" || exit 1
rm -f stress.sock stress.out stress.log
export MALLOC_CHECK_=1
../ptybuffer -l- -d -po stress.out stress.sock ./test.sh 2>stress.log &
KPID=$!
unset MALLOC_CHECK_
sleep 1
i=0

trap 'kill $KPID; rm stress.sock stress.out stress.log; echo $i; exit' 0
while	../ptybufferconnect -nqqipx stress.sock </dev/null >/dev/null

do
	let i=i+1
	printf '\r%d' "$i"
done
