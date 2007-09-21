<?
# $Header$
#
# Ptybuffer web based shell - output window
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
#
# $Log$
# Revision 1.3  2007-09-21 11:10:15  tino
# CLL notice now in scripts
#
# Revision 1.2  2007/08/24 19:22:16  tino
# I tried the script to go away on user abort,
# but for some reason on RH9 with PHP4.2.2 and apache1.3
# this does not work.
#
# Revision 1.1  2007/08/24 17:32:21  tino
# first version
?>
<HTML>
<HEAD>
<style>
.l0 { background-color:#ddffdd; color:#000000; font-family:Fixedsys,Courier; }
.l1 { background-color:#ddddff; color:#000000; font-family:Fixedsys,Courier; }
.lc { background-color:#ffffff; color:#000000; font-family:Fixedsys,Courier; }
</style>
<script><!--
var halted=0;
function ready()
{
  halted=1;
  document.body.innerHTML = document.body.innerHTML + "</DIV><P><HR>[terminated]";
}
//-->
</script>
</HEAD>
<BODY BGCOLOR=#8888ff TEXT=#000000 LINK=#880088 VLINK=#0000ff ALINK=#ff0000
 onload="ready()">
<?
$port=$_REQUEST["port"];
if ($port=="") $port="shell";
if (!preg_match("/^[-.a-zA-Z0-9]*$/",$port)) $port="";

echo strftime("%Y-%m-%d %H:%M:%S");

function killme()
{
  GLOBAL $fd;

  fclose($fd);
  exit(0);
}

echo "- LOG START $port<BR>";
$line	= 0;
if (!($fd=fsockopen("sock/$port", 0)))
  echo "cannot connect to sock $port";
else
  {
    register_shutdown_function(killme);
    $needline	= true;
    $buf	= "";
    # For some reason on my RH9 this script does not terminate on user abort
    while (!connection_aborted())
      {
        ignore_user_abort(FALSE);

	if ($buf=="")
	  {
	    # FIX for PHP4.2.2: fread not returns if blocking
	    socket_set_blocking($fd,0);
	    $buf	= fread($fd,8192);
          }
	if ($buf=="")
          {
	    if (feof($fd))
	      break;
	    ob_end_flush();
            flush();
	    socket_set_blocking($fd,1);
	    $buf	= fread($fd,1);
	  }
	if ($needline!==false)
	   {
	     $needline	= false;
     	     $l	= 1-$l;
             echo "<DIV class=l$l style='margin-left=2.61em; text-indent=-2.61em' onmouseover='o(this)' onmouseout='x(this)' onmousedown='c(this)'>";
	     echo str_replace(" ", "&nbsp;", sprintf("%6d: ", ++$line));
	   }
        if (($needline=$o=strpos($buf, "\n"))===false)
	  $o	= strlen($buf);
	$s	= htmlentities(substr($buf,0,$o));
	$buf	= substr($buf,$o+1);

	if (substr($s,0,1)==" ")
	  $s	= "&nbsp;".substr($s,1);
	echo str_replace("  ", " &nbsp;",  $s);

        if ($needline!==false)
	  echo "<BR></DIV>\n";
      }
    fclose($fd);
    echo "<BR></DIV>";
  }
?>
<HR>
<? echo strftime("%Y-%m-%d %H:%M:%S"); ?> - LOG END - <?=$PROG?> <?=$VERS?>
</BODY>
</HTML>
