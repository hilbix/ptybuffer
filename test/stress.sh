#!/bin/sh
#
# $Header$
#
# Stress test
#
# $Log$
# Revision 1.8  2007-06-01 11:54:24  tino
# Now output works as expected
#
# Revision 1.7  2007/06/01 11:19:22  tino
# Bugfix version (still dunno where the bug is)
#
# Revision 1.6  2007/06/01 10:52:48  tino
# Buggy version .. test/stress.sh does not work anymore!?
#
# Revision 1.5  2006/08/11 22:05:40  tino
# Little improvemtns to tests
#
# Revision 1.4  2006/04/11 23:00:07  tino
# Now working .. now dist
#
# Revision 1.3  2006/04/11 22:44:15  tino
# Well, I was too fast already again.  It does not work.  Looking for error.
#
# Revision 1.2  2004/05/21 10:39:32  tino
# Support for MALLOC_CHECK_ added
#
# Revision 1.1  2004/05/21 09:56:20  tino
# Test script added to find weird bug

cd "`dirname "$0"`" || exit 1
rm -f sock.tmp
export MALLOC_CHECK_=1
../ptybuffer -l- -d sock.tmp ./test.sh &
KPID=$!
unset MALLOC_CHECK_
sleep 1
i=0

trap 'kill $KPID; rm sock.tmp; echo $i; exit' 0
while	accept ">sock.tmp" >/dev/null 2>&1 <&1 ||
	accept ">sock.tmp" >/dev/null 2>&1 <&1

do
	let i=i+1
#	echo -n "$i"
done
