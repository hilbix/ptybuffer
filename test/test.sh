#!/bin/sh
# $Header$
#
# Test script for ptybuffer:
# Use this script this like that:
#
# rm -f sock.tmp; ptybuffer sock.tmp ./test.sh
#
# Then run a program capable to connect to the unix socket, sock.tmp:
# access ">sock.tmp"
#
# You can find access-2.x where you found ptybuffer:
# http://www.scylla-charybdis.com/download/
#
# $Log$
# Revision 1.1  2004-05-19 20:22:23  tino
# Test script added
#

while date
do
	echo "==============================================================================="
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	echo "-------------------------------------------------------------------------------"
	echo "*******************************************************************************"
	echo "-------------------------------------------------------------------------------"
	echo "==============================================================================="
	sleep 1
done
