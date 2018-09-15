> THIS IS RELEASE EARLY CODE!  Maybe that the latest version introduces bugs.

[![PtyBuffer Build Status](https://api.cirrus-ci.com/github/hilbix/ptybuffer.svg?branch=master)](https://cirrus-ci.com/github/hilbix/ptybuffer/master)


# ptybuffer

## For the impatient

	git clone https://github.com/hilbix/ptybuffer.git
	cd ptybuffer
	git submodule update --init
	make
	sudo make install

Example how to autostart and control processes in background:

	mkdir -p ~/bin ~/autostart ~/autorun
	cp scripts/autostart.sh ~/bin/
	cp scripts/helloworld.sh ~/autostart/
	cp autorun/helloworld.sh ~/autorun/
	{ crontab -l; echo '* * * * * bin/autostart.sh >/dev/null'; } | crontab -
	~/bin/autostart.sh

	socat - unix:/var/tmp/autostart/$LOGNAME/helloworld.sock
	socat - unix:/var/tmp/autostart/$LOGNAME/helloworld.sock

More info:

	./ptybuffer -h
	head test/test.sh

See also:

- https://github.com/hilbix/watcher/ is designed to display running
  autostarts: `watcher.py /var/tmp/autostart/$LOGNAME/*.sock` and
  in future it perhaps even can control them.


## About

`ptybuffer`

- daemonizes.  With option -d it runs in foreground

- calls a process (arg2 and above) with a `PTY`.  Stdlib usually
  makes output line buffered this way.

- gathers the output history of the pty for later view.  Currently
  this is 1000 reads (not neccessarily lines, see option -n).

- listens for connects to the unix socket (command line arg 1)

- outputs the `PTY` history to the connected unix socket and
  continuously sends all new output to the connected unix socket.

- reads (full) lines from the connected unix sockets and send them to
  the `PTY` (line can be max. 10K bytes long or so).

- allows multiple concurrent connects to the unix socket.  This way
  several persons in parallel can control one process.

This is better than running background tasks using screen or tmux:

- Screen blocks all connects if one session freezes.  ptybuffer is
  thought to always let the program run, regardless what mess the
  socket connections might do.  If a connect stalls, it just discards
  data to the frozen connect and if the connect catches up, it tells
  the fact.

- Concurrent access is no problem.  Lines are always sent as full lines.
  (Except when option -i is used.)

- History of output is clearly defined, not just something like a
  window.

- Naturally scrolling logfile support.  Eases development in case you
  have excessive debugging active and want to see the output as soon
  as it comes from the program, even without shell access.

- You can control where the socket is.  This improves oversight in
  case you have running several commands in parallel.

- Easy to use from web scripting languages like PHP which can directly
  connect to a Unix Domain Socket.  Really no need to use error prone
  constructs like expect.

- If the central process of screen or tmux dies, all background tasks
  die as well.  Ptybuffer runs independently for each task.

- Ptybuffer does not interpret the terminal.  So no complicated code
  which might hide bugs.


# Bugs

- Very long lines are discarded.  Use option -i if this is a problem.
  Then, however, no line buffering is done, which might cause
  confusion if more than one input socket is open.

- ptybuffer does not unlink() the socket if option -f and -c are
  missing.  This is a feature.

- New version have not been tested much before release.

- If you send data to ptybuffer and close the socket very fast, the
  EOF on the write side closes the socket before everything can be
  read from the read side by the select-loop.  Luckily, usually not
  much is sent to the controlled process this way (one line or so), so
  a short sleep should be enough for ptybuffer to gather the data.  To
  fix this BUG I must implement the socket flags first and restructure
  the the EOF processing a bit - no time for it now, it currently
  works good enough for me.


# Example

See introductory comment in test/test.sh

See scripts/autostart.sh with example scripts/helloworld.sh

For a PHP usage example see the php/ subdirectory.

You need a program capable to send data to unix sockets to control
the program running in ptybuffer.  `socat` is your friend.


# DISCLAIMER

USE AT YOUR OWN RISK!  I CANNOT ACCEPT ANY LIABLILITY FOR ANYTHING!
THIS *IS* RELEASE EARLY CODE!  IT IS CONSIDERED TO BE INSTABLE!

DON'T TRUST SOURCES!  READ THEM!  READ THEM AGAIN!  AND BE SURE THERE
ARE NO KNOWN BACKDOORS IN THEM!  IT'S OPEN SOURCE, SO YOU CAN CHECK!

I tried my best.  However I am human.  So I make mistakes.  Be prepared.
All I guarantee is, that I never do anything to harm *you* by purpose.

Copyright (C)2004-2018 by Valentin Hilbig

ptybuffer may be distributed freely under the conditions of the
GPL2 (GNU General Public License version 2) or higher.

(Note: In future I might re-release it more permissively under CLL)
