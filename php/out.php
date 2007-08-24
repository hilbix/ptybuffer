<?
# $Header$
#
# Ptybuffer web based shell - output window
#
# $Log$
# Revision 1.1  2007-08-24 17:32:21  tino
# first version
#
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

echo "- LOG START $port<BR>";
$line	= 0;
if (!($fd=fsockopen("sock/$port", 0)))
  echo "cannot connect to sock $port";
else
  {
    $needline	= true;
    $buf	= "";
    for (;;)
      {
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
	    ob_flush();
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
    echo "<BR></DIV>";
  }
?>
<HR>
<? echo strftime("%Y-%m-%d %H:%M:%S"); ?> - LOG END - <?=$PROG?> <?=$VERS?>
</BODY>
</HTML>
