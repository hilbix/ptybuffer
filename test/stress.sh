#!/bin/sh
#
# $Header$
#
# Stress test
#
# $Log$
# Revision 1.2  2004-05-21 10:39:32  tino
# Support for MALLOC_CHECK_ added
#
# Revision 1.1  2004/05/21 09:56:20  tino
# Test script added to find weird bug
#

rm -f sock.tmp
export MALLOC_CHECK_=1
./ptybuffer sock.tmp ./test.sh &
export MALLOC_CHECK_=0
sleep 1
i=0

trap 'echo $i; exit' 0
while accept ">sock.tmp" >/dev/null 2>&1 <&1
do
	let i=i+1
#	echo -n "$i"
done
