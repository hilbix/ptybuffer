#!/bin/bash
#
# This is an autostarter script for use with ptybuffer.
# Run this from cron like:
# * * * * * bin/autostart.sh >/dev/null
#
# Place all scripts to automatically start in following directory:
#	mkdir "$HOME/autostart/"
# Don't forget to give them exec rights.
#
# Option to ptybuffer can be given in the script name as follows:
#	scriptname.-NT10Yhello_world-.sh
# sets options -n 10 -t 10 -y 'hello_world'.
# Letters L and O are special as they suppress the standard option
# and letter Y must not be followed by uppercase letters.
#
# You can add softlinks to other directories containing
# scripts to autostart as well into this directory.
#
# To connect to STDIN of the forked scripts, do:
#	socat - unix:/var/tmp/autostart/$USER/$SCRIPT.sock
# $USER is your username, $SCRIPT the scriptname without .sh ext
#
# If you are not happy with /var/tmp/autostart then
#	sudo mkdir -m1777 /var/log/autostart
#
# Don't forget to rotate the files as they will grow.
#
# Note that according to FSSTD /var/tmp/ survives reboot while
# /tmp/ should not.  /var/tmp/autostart can be a softlink.
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY. 

cd || exit

PATH="$HOME/bin:/usr/local/bin:$PATH"

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

# checks if $check matches -*$1*- sequence
# if not, fail.  Else:
# $rest becomes everything of the 2nd *
# $n becomes the first number found in $rest
check()
{
case "$check" in
(-*"$1"*-)	;;
(*)		return 1;;
esac
rest="${check##*"$1"}"
rest="${rest%-}"
n="${rest%%[0-9]*}"
n="${rest#"$n"}"
n="${n%%[^0-9]*}"
return 0
}

ex=0
for a in autostart/* autostart/*/*
do
	case "$a" in
	*~)	continue;;
	esac
	[ -f "$a" ] || continue
	[ -x "$a" ] || continue
	b="`basename "$a" .sh`"
	args=()
	check="${b##*.}"
	check A &&	args+=(-a)
	check E &&	args+=(-e)
	check G &&	args+=(-g)
	check I &&	args+=(-i)
	check K &&	args+=(-k)
	check L ||	args+=(-l "$RUNDIR/$b.log")
	check N &&	args+=(-n "${n:-1}")
	check O ||	args+=(-o "$RUNDIR/$b.out")
	check P &&	args+=(-p)
	check Q &&	args+=(-q)
	check T &&	args+=(-t "${n:-0}")
	check U &&	args+=(-u "${n:-077}")
	check W &&	args+=(-w)
	check Y &&	args+=(-y "$rest")
	ptybuffer "${args[@]}" -cf "$RUNDIR/$b.sock" "$a"
	ret="$?"
	case "$ret" in
	0)	log "started $b" >&2; [ -z "$1" ] || sleep "$1";;
	42)	log "ok $b";;
	*)	log "OOPS $b ($ret)" >&2; ex=1;;
	esac
done
exit $ex
