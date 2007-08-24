#!/bin/bash
#
# $Header$
#
# Start script to spawn a web based shell
#
# To autostart the shell you can run this script from
# cron each minute with the switch -cron.
#
# BE SURE TO PROPERLY PROTECT ACCESS TO THIS SHELL!
# YOUR MACHINE MIGHT BE COMPROMIZED IF YOU RUN IT THIS WAY!
#
# $Log$
# Revision 1.1  2007-08-24 17:32:33  tino
# first version
#

cd "`dirname "$0"`" || exit

enter()
{
echo "WARNING! THIS SCRIPT IS DANGEROUS!" >&2
echo "To start it, enter $*" >&2
read risk || exit
[ ".$*" = ".$risk" ] || exit
}

case "$1" in
-cron)	;;
*)	enter RISKY BUSINESS;;
esac

ptybuffer -qcf sock/shell /bin/sh
ret="$?"
case "$ret" in
42)	ret=0;
esac
exit $ret
