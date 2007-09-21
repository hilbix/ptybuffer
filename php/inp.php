<?
# $Header$
#
# Ptybuffer web based shell - input window
#
# This Works is placed under the terms of the Copyright Less License,
# see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
#
# $Log$
# Revision 1.2  2007-09-21 11:10:15  tino
# CLL notice now in scripts
#
# Revision 1.1  2007/08/24 17:32:21  tino
# first version

$autoscroll=$_REQUEST["autoscroll"];
$action=$_REQUEST["action"];
$confirm=$_REQUEST["confirm"];
$shortcut=$_REQUEST["shortcut"];
$direct=$_REQUEST["direct"];
$port=$_REQUEST["port"];
$ok=$_REQUEST["ok"];
$send=$_REQUEST["send"];

if ($autoscroll=="" && !$_REQUEST["inited"]) $autoscroll=1;
if ($port=="") $port="shell";
if (!preg_match("/^[-.a-zA-Z0-9]*$/",$port))
  $port	= "";

function checked($x)
{
  if ($x!="")
    echo "CHECKED";
}
?>
<HTML>
<BODY BGCOLOR=#ccccff TEXT=#000000 LINK=#880088 VLINK=#0000ff ALINK=#ff0000
 marginwidth=0 marginheight=0 topmargin=0 leftmargin=0 onload=init()>
<SCRIPT><!--
var nextevent, scrolltime;

function scroll()
{
  nextevent=500;
  if (parent["out"])
    {
      if (parent["out"].halted)
        {
	  if (confirm("output window disconnected\nreload window? (no stops scroller)"))
            {
              parent["out"].location.reload();
	      nextevent = 2500;
            }
          else
            {
  	      document.frm.autoscroll.checked = 0;
            }
        }
      else
	{
	  parent["out"].scrollBy(0, 5000);
	}
    }
}

function scroller()
{
  if (!scrolltime && document.frm.autoscroll.checked)
    scrolltime = window.setTimeout("doscroll()", nextevent);
}

function doscroll()
{
  scrolltime = 0;
  scroll();
  scroller();
}

function checkscroll()
{
  if (!scrolltime)
    doscroll();
}

function scrollit()
{
  if (document.frm.autoscroll.checked)
    checkscroll();
  return true;
}

function checkcr(arg)
{
  if ((window.event && window.event.keyCode==13) || (arg && arg.which==13))
    document.frm.submit();
  return true;
}

function init()
{
  scrollit();
  if (document.frm.send)
    {
      document.frm.send.focus();
      document.frm.send.select();
    }
}
//-->
</SCRIPT>
<FORM ACTION="inp.php" NAME=frm METHOD=post>
<INPUT TYPE=hidden NAME=inited VALUE=1>
<TABLE CELLSPACING=0 CELLPADDING=0><TR><TD VALIGN=top ALIGN=left>
<NOBR><A TARGET="out" HREF="out.php">^^</A
><INPUT TYPE=checkbox NAME=autoscroll <?=checked($autoscroll)?> onchange="scrollit()"
><?
$ok=0;
if ($action)
  {
    $direct="";
    $send="";
    if ($confirm=="")
      {
	$action	= htmlentities($action);
        echo "$action";
        echo " <INPUT TYPE=hidden NAME=action VALUE=\"$action\">";
        echo " <INPUT TYPE=submit NAME=confirm VALUE=yes>";
        echo " <INPUT TYPE=submit NAME=confirm VALUE=no>";
        echo "</NOBR>";
      }
    else
      {
        if ($confirm=="yes")
	  $direct	= $action;
        $action	= "";
      }
  }
else if ($shortcut)
  $direct	= $shortcut;

if ($action==""):

if ($direct!="")
  {
    $rewrite	= array(
"restart" => "exit\n",
"kill"=>"\3",
"quit"=>"\34",
"xoff"=>"\21",
"xon"=>"\23",
"eof"=>"\4",
"esc"=>"\33",
"escesc"=>"\33\33",
"(return)"=>"",
);
    $ok		= 1;
    $send	= $direct;
    if (isset($rewrite[$direct]))
      $send	= $rewrite[$direct];
  }
if ($port=="")
  $stat = "illegal sock port";
else if ($send=="" && !$ok)
  $stat	= "running $port";
else if ($fd=fsockopen("sock/$port", 0))
  {
    $stat	= "sent: ".htmlentities(addcslashes($send, "\0..\37\177..\237"));
    fwrite($fd, "$send\n");
    fflush($fd);
    flush();
    sleep(1);
    fclose($fd);
  }
else
  $stat	= "cannot open sock ".htmlentities($port);
?>
<INPUT TYPE=text SIZE=40 NAME=send VALUE="<?=htmlentities($send)?>" onkeyup=checkcr
><INPUT TYPE=submit NAME=ok VALUE=Send
><SELECT NAME=shortcut onchange="document.frm.submit()">
<OPTION VALUE=escesc>^[^[ send 2*ESC
<OPTION VALUE=esc>^[ send ESC
<OPTION VALUE=xoff>^Q send XOFF
<OPTION VALUE=xon>^S send XON
<OPTION VALUE=eof>^D send EOF
<OPTION VALUE="" SELECTED>(shortcuts)
<!-- add your favorite quick shell commands here -->
</SELECT
><SELECT NAME=action onchange="document.frm.submit()">
<OPTION VALUE=quit>^\ quit service
<OPTION VALUE=kill>^C kill service
<!-- OPTION>restart -->
<OPTION VALUE="" SELECTED>(actions)
<OPTION>exit
<!-- add additional confirmed shell commands here -->
</SELECT
><!-- INPUT TYPE=submit NAME=direct VALUE=status
--><INPUT TYPE=submit NAME=direct VALUE="(return)">
</NOBR>
</TD><TD VALIGN=top ALIGN=left BGCOLOR=#ddffdd>&nbsp;<?=htmlentities($port)?>&nbsp;
</TD><TD VALIGN=top ALIGN=left>&nbsp;<?=$stat?>
<? endif; ?>
</TD></TR></TABLE>
</FORM>
</BODY>
</HTML>
