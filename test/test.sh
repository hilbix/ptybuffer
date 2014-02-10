#!/bin/sh
#
# Test script for ptybuffer: Use this script like that:
#	rm -f sock.tmp; ptybuffer sock.tmp ./test.sh
# Then run a program capable to connect to the unix socket, sock.tmp:
#	socat - unix:sock.tmp
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

unset MALLOC_CHECK_
while	echo "==============================================================================="
do
	set
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	stty -a
	echo "-------------------------------------------------------------------------------"
	tty
	echo "==============================================================================="
	date
	sleep 1
done
