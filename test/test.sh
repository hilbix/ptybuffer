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
# $Log$
# Revision 1.5  2004-10-22 01:09:09  tino
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
