#!/bin/sh
#
# $Header$
#
# Stress test
#
# $Log$
# Revision 1.1  2004-05-21 09:56:20  tino
# Test script added to find weird bug
#

rm -f sock.tmp
./ptybuffer sock.tmp ./test.sh &
sleep 1
i=0

while accept ">sock.tmp" >/dev/null 2>&1 <&1
do
	let i=i+1
	echo -n "$i"
done

echo $i
