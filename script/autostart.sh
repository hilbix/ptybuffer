#!/bin/sh
# $Header$
#
# This is an autostarter script for use with ptybuffer.
# You can run this each minute from cron.
#
# Create a directory $HOME/autostart/ and place all scripts
# which shall be autostarted by CRON in this directory.
# Don't forget to give them exec rights.
#
# The scripts then will be connectable to RUNDIR/$USER/$SCRIPT.sock
# where RUNDIR is /var/log/autostart, /var/tmp/autostart or /tmp/autostart
# (depends on which directory is present and usable).
#
# Don't forget to rotete the outputs in this directory!
#
# Public Domain as long as nobody else claims a Copyright on this.
# Written by Valentin Hilbig <webmaster@scylla-charybdis.com>
#
# $Log$
# Revision 1.3  2007-03-31 21:17:06  tino
# Forgot to change "echo" into "log" calls
#
# Revision 1.2  2007/03/25 23:33:02  tino
# Fixed + better logging
#
# Revision 1.1  2007/03/04 02:49:18  tino
# Commit for dist, see ChanegLog

cd || exit

RUNDIR="/var/log/autostart"
[ -w "$RUNDIR" ] || RUNDIR="/var/tmp/autostart"
[ -w "$RUNDIR" ] || mkdir -m1777 "$RUNDIR" 2>/dev/null || { RUNDIR="/tmp/autostart" && mkdir -m1777 "$RUNDIR"; } || exit

RUNDIR="$RUNDIR/$LOGNAME"
[ -d "$RUNDIR" ] || mkdir -m700 "$RUNDIR" || exit
chmod 700 "$RUNDIR" || exit

log()
{
lnow="`date +%Y%m%d-%H%M%S` $*"
echo "$lnow" >> "$RUNDIR/autostart.log"
echo "$lnow"
}

ex=0
for a in autostart/*
do
	case "$a" in
	*~)	continue;;
	esac
	[ -f "$a" ] || continue
	[ -x "$a" ] || continue
	b="`basename "$a" .sh`"
	ptybuffer -l "$RUNDIR/$b.log" -o "$RUNDIR/$b.out" -cf "$RUNDIR/$b.sock" "$a"
	ret="$?"
	case "$ret" in
	0)	log "startd $b"; [ -z "$1" ] || sleep "$1";;
	42)	log "ok $b";;
	*)	log "OOPS $b ($ret)" >&2; ex=1;;
	esac
done
exit $ex
