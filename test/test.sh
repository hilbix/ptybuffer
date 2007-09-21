#!/bin/sh
# $Header$
#
# Test script for ptybuffer: Use this script like that:
#	rm -f sock.tmp; ptybuffer sock.tmp ./test.sh
# Then run a program capable to connect to the unix socket, sock.tmp:
#	accept ">sock.tmp"
# You can find accept-2.x where you found ptybuffer:
#	http://www.scylla-charybdis.com/download/
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
#
# $Log$
# Revision 1.7  2007-09-21 11:10:15  tino
# CLL notice now in scripts
#
# Revision 1.6  2006/08/11 22:05:40  tino
# Little improvemtns to tests
#
# Revision 1.5  2004/10/22 01:09:09  tino
# now shows environment
#
# Revision 1.4  2004/05/23 10:12:23  tino
# new upload for NWNadm
#
# Revision 1.3  2004/05/20 04:59:00  tino
# master can be 0, which closes stdin.  close in the slave part removed.
#
# Revision 1.2  2004/05/20 02:03:59  tino
# typo corrected
#
# Revision 1.1  2004/05/19 20:22:23  tino
# Test script added

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
