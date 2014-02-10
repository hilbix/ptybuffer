#!/bin/bash
#
# Example script for ~/autostart/
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.

# How to use:
#
# - make; sudo make install
# - cp scripts/autostart.sh ~/bin/
# - crontab -e # add autostart.sh
# - cp scripts/helloworld.sh ~/autostart/NAME.sh
# - ln -s YOUR-REAL-PROGRAM-TO-START ~/autorun/NAME.sh
# - mkdir ~/.rc
# - ~/bin/autostart.sh
# - socat - unix:/var/tmp/autostart/$LOGNAME/NAME.sock
# - save
# - quit
# - vim ~/.rc/settings-NAME.sh
#
# That's it, absolutely straight forward

# Configuration is at the end
# Start of function list

log()
{
eval 'echo "`date +%Y%m%d-%H%M%S` $*" >'"$LOG"
}

OOPS()
{
log "OOPS $CMD: $*"
echo "OOPS $CMD: $*" >&2
exit 1
}

# Run external commands, closing FD 3
x()
{
log "execute: $*"
"$@" 3>&-
ret=$?
log "status$ret: $*"
return $ret
}

# $# minargs maxargs "error code"
# If maxargs < minargs then unlimited args
args()
{
[ $1 -ge ${2:-0} ] && [ ${2:-0} -gt ${3:-0} -o $1 -le ${3:-0} ] && return
case "$2$3" in
(''|0|00)	OOPS "command takes no arguments";;
($2$2)		OOPS "command takes exactly $2 arguments";;
(*)		OOPS "${4:-command takes ${2:-0} to ${3:-0} arguments}";;
esac
}

# number value
# Check that value is a number
number()
{
  [ ".$1" = ".$(printf "%d" "$1")" ] || OOPS "not a number: $1"
}

# Inject some command into main shell,
# to set/alter global variables etc.
inject()
{
  echo "$*" >&3 || OOPS "cannot inject: $*"
}

cmd-help()
{
  : "help [command]: list known commands or explain a command"
  args $# 0 1
  if [ 1 = $# ]
  then
  	declare -f "cmd-$1"
	return
  fi
  echo "Commands known:"
  while read -ru6 name ignore
  do
  	printf '%-20s %s\n' "$name" "$(declare -f "cmd-$name" | sed -n 's/^[[:space:]]*: "\(.*\)";$/\1/p')"
  done 6< <(declare -F | sed -n 's/^declare -f cmd-//p')
}

cmd-quit()
{
  : "Terminate everything"
  inject exit
}

cmd-stop()
{
  : "stop automatic timeout"
  args $#
  inject TIMEOUT=
  #inject save
}

cmd-start()
{
  : "start [timeout]: start or set automatic timeout"
  args $# 0 1

  t="${1:-60}"
  number "$t"
  inject TIMEOUT=$t
  #inject save
}

cmd-load()
{
  : "load settings from $SETTINGS"
  args $#
  [ -f "$SETTINGS" ] || OOPS "missing settings file: $SETTINGS"
  inject load
}

cmd-save()
{
  : "save settings to $SETTINGS"
  args $#
  inject save
}

cmd-settings()
{
  : "show settings"
  settings
}

# Message on timeout
# Falls through to the same as return or EOF on input
timeout()
{
  echo "$TIMEOUT seconds passed (or EOF), running default action"
}

loop()
{
while	[ -z "$TIMEOUT" ] || echo "(autorun in $TIMEOUT seconds)"
	echo -n "Give ommand, press return to run: "	# bail out if terminal goes away
do
	if [ -n "$TIMEOUT" ]
	then
		read -rt"$TIMEOUT" CMD args || timeout
	else
		read -r CMD args
	fi
	[ -n "$CMD" ] || CMD="run"
	run="cmd-$CMD"
	if	fn="$(type -t "$run")" && [ function = "$fn" ]
	then
		exec 4>&1
		log "command: $run $args"
		catch="$("$run" $args 3>&1 >&4 4>&-;)"
		ret=$?
		log "return$ret: $run $args"
		if	[ 0 = $ret ]
		then
			# This enables scripts to inject something into this shell
			[ -z "$catch" ] && continue
			log "process: $catch"
			eval "$catch"
		else
			echo "command status: $ret"
		fi
	else
		case "$cmd" in
		(*\'*)	echo "Unknown command, perhaps leave away the quotes";;
		(*)	echo "unknown command '$CMD', try 'help' for help";;
		esac
	fi
done
}

save()
{
  settings > "$SETTINGS.new" &&
  mv -f "$SETTINGS.new" "$SETTINGS"
}

load()
{
  [ -f "$SETTINGS" ] && . "$SETTINGS"
}

settings()
{
  for a in $VARS
  do
	declare -p "$a" | sed '1s/^declare -- //'
  done
}


# End of function list

# Change following function to your needs
# The default is to try to run a command
# found in/autorun/ which is named after
# this script's name.
#
# If this is run from ~/autostart/ then
# the directory is ~/autorun/

cmd-run()
{
  : "run default program (with additional args)"
  args $# 0 $MAXARGS

  # Per default runs a command in ~/autorun/
  x "autorun/${0##*/}" "$@"
}

# START OF CONFIGURATION

VARS="VARS SETTINGS TIMEOUT LOG MAXARGS"
SETTINGS="$HOME/.rc/settings-${0##*/}"
TIMEOUT=60
LOG="&1"	# give as ">'FILENAME'" to append to a file
MAXARGS=

#cd
log start $0
load
loop
log end

