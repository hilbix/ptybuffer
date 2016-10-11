/* ptybuffer: daemonize interactive tty line driven programs with output history
 *
 * Copyright (C)2004-2016 Valentin Hilbig <webmaster@scylla-charybdis.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */
#ifndef PTYBUFFER_HISTORY_LENGTH
#define PTYBUFFER_HISTORY_LENGTH	1000
#endif

#include "tino/file.h"
#include "tino/debug.h"
#include "tino/fatal.h"
#include "tino/alloc.h"
#include "tino/sock_select.h"
#include "tino/slist.h"
#include "tino/getopt.h"
#include "tino/proc.h"
#include "tino/filetool.h"
#include "tino/sock_tool.h"

#include <setjmp.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <pty.h>
#include <utmp.h>

#include "ptybuffer_version.h"

static const char	*outfile, *logfile;
static int		doecho, dotimestamp, doquiet;
static pid_t		mypid;

static FILE *
file_open(FILE *fd, const char *name)
{
  if (!name)
    return 0;
  if (strcmp(name, "-"))
    fd	= tino_file_fopenE(name, "a+");
  return fd;
}

static void
file_flush_close(FILE *fd)
{
  if (fd==stdout || fd==stderr)
    fflush(fd);
  else
    tino_file_fcloseE(fd);
}

static void
file_timestamp(FILE *fd, int showpid)
{
  struct tm	tm;
  time_t	tim;

  time(&tim);
  gmtime_r(&tim, &tm);
  fprintf(fd,
	  "%4d-%02d-%02d %02d:%02d:%02d ",
	  1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday,
	  tm.tm_hour, tm.tm_min, tm.tm_sec);
  if (showpid)
    {
      if (!mypid)
	mypid	= getpid();
      fprintf(fd, "%ld: ", (long)mypid);
    }
}

/* Do you have a better idea?
 *
 * The file must be always flushed and I want you to be able to always
 * rotate the file without anything to keep in mind.
 */
static void
file_out(void *_ptr, size_t len)
{
  static int	in_line;
  FILE		*fd;
  char		*ptr=_ptr;

  if ((fd=file_open(stdout, outfile))==0)
    return;
  if (!dotimestamp)
    tino_file_fwriteE(fd, ptr, len);
  else
    while (len)
      {
	int	i;

	if (!in_line)
	  file_timestamp(fd, 0);
	in_line	= 1;
	for (i=0; i<len; i++)
	  if (((char *)ptr)[i]=='\n')
	    {
	      in_line	= 0;
	      i++;
	      break;
	    }
	tino_file_fwriteE(fd, ptr, i);
	ptr	+= i;
	len	-= i;
      }
  file_flush_close(fd);
}

/* do not call this before the last fork
 */
static void
file_log(const char *s, ...)
{
  FILE		*fd;
  va_list	list;
  int		e;

  e	= errno;
  if ((fd=file_open(stderr, ((logfile || doquiet) ? logfile : "-")))==0)
    {
      errno	= e;
      return;
    }

  file_timestamp(fd, 1);

  va_start(list, s);
  vfprintf(fd, s, list);
  va_end(list);
  fputc('\n', fd);

  file_flush_close(fd);

  errno	= e;
}

/* main parent:
 * inform caller if everything is OK
 *
 * This is the main program if daemonization is done!
 */
static int
parent(pid_t pid, int *fds)
{
  char	buf[BUFSIZ];
  int	got;

  if (pid==(pid_t)-1)
    tino_exit("fork");

  /* close the writing pipe
   */
  tino_file_close_ignO(fds[1]);

  file_log("parent: waiting for OK from main %ld", (long)pid);

  /* wait until the child is ready
   */
  while ((got=read(fds[0], buf, sizeof buf-1))==-1)
    if (errno!=EINTR && errno!=EAGAIN)
      {
	perror("read");
	got	= 0;
	break;
      }
  tino_file_close_ignO(fds[0]);

  /* child sends OK in case everything is ok
   */
  if (got==2 && !strncmp(buf, "OK", 2))
    {
      file_log("parent: got OK");
      return 0;
    }

  buf[got]	= 0;
  if (!got)
    strcpy(buf, "unexpected error");

  /* We have some error.
   * Be sure to kill off the child.
   */
  file_log("parent: killing main %ld, got error '%s'", (long)pid, buf);
  kill(pid, 9);
  tino_exit("error: %s", buf);
  return -1;
}

struct ptybuffer_params
  {
    int			first_connect;	/* first connect (dupped stdin)	*/
    long		history_length;
    long		history_tail;
    int			immediate, kill_incomplete;
    int			keepopen;
  };
struct ptybuffer
  {
    TINO_SOCK		sock, pty;
    int			forcepoll;
    TINO_GLIST		send;
    TINO_GLIST		screen;
    long long		blockcount, skipbytes, bytecount;
    int			outlen, outpos;
    const char		*out;
    long		history_length;
    long		history_tail;
    int			immediate, kill_incomplete;
    int			keepopen;
  };

#define	PTYBUFFER_MAX_INPUTLINE		10240	/* if you change this value, change the next one, too!	*/
#define	PTYBUFFER_MAX_INPUTLINE_STR	"10240"

struct ptybuffer_connect
  {
    struct ptybuffer	*p;
    int			infill, discard;
    char		in[PTYBUFFER_MAX_INPUTLINE];
    int			outfill, outpos;
    char		out[BUFSIZ+BUFSIZ];
    long long		screenpos, screenbytes;
    long long		minscreenpos;
  };

static void
send_to_pty(struct ptybuffer_connect *c, int cnt)
{
  xDP(("(%p,%d) max %d", c, cnt, (int)c->infill));
  tino_FATAL(cnt>c->infill);
#if 0
  if (memchr(c->in, 0, cnt))
    c->discard	= 1;	/* line must not contain \0	*/
#endif
  if (!c->discard)
    {
      xDP(("() add %.*s", (int)cnt, c->in));
      tino_glist_add_n(c->p->send, c->in, cnt);
      tino_sock_pollOn(c->p->pty);
#if 0
      /* This is too dangerous.  If passwords are entered with
       * 'stty noecho', then they shall not be recorded.
       * (Well, we can perhaps auto-switch logging later.)
       */
      file_log("input %d: %.*s", tino_sock_fdO(sock), (int)cnt-1, c->in);
#endif
    }
  c->discard	= 0;
  if ((c->infill-=cnt)>0)
    memmove(c->in, c->in+cnt, c->infill);
}

/* Handle data to a connected socket:
 * Send incoming to terminal,
 * Output buffer to socket.
 *
 * If some data cannot be sent to the remote in time,
 * tell how much lines were dropped.
 */
static int
connect_process(TINO_SOCK sock, enum tino_sock_proctype type)
{
  struct ptybuffer_connect	*c	= tino_sock_userO(sock);
  char				*pos;
  int				got, put;

  xDP(("(%p[%d], %d)", sock, tino_sock_fdO(sock), type));
  switch (type)
    {
    case TINO_SOCK_PROC_EOF:
      xDP(("connect_process() eof"));
      return TINO_SOCK_FREE;

    case TINO_SOCK_PROC_CLOSE:
      /* Good bye to the other side
       */
      xDP(("() close"));
      file_log("close %d: %d sockets",
	       tino_sock_fdO(sock), tino_sock_useO()-1);
      /* propagate close of stdin main socket
       * in case we use sockfile=-
       */
      if (c->p->sock==sock)
	{
	  c->p->sock	= 0;
	  c->p->forcepoll	= 1;
	}
      if (c->infill && !c->p->kill_incomplete)
	send_to_pty(c, c->infill);
      return TINO_SOCK_FREE;

    case TINO_SOCK_PROC_POLL:
      xDP(("() poll"));
      /* If something is waiting to be written
       * mark this side as readwrite, else only read.
       * Terminate after everything has been written if pty was closed.
       */
      xDP((" %d, %d, %d, %d @ %p", c->outpos, c->outfill, c->screenpos, c->p->blockcount, c->p->pty));
      if (c->p->pty)
	return c->outpos<c->outfill || c->screenpos<c->p->blockcount
		? TINO_SOCK_READWRITE
		: TINO_SOCK_READ;
      /* XXX TODO XXX
       * half close socket here?
       */
      if (c->outpos<c->outfill || c->screenpos<c->p->blockcount)
	return TINO_SOCK_WRITE;
      file_log("closing %d: all data written");
      return TINO_SOCK_EOF;

    case TINO_SOCK_PROC_READ:
      xDP(("() read"));
      /* Read data lines comming in.
       * Only send full lines to the termina.
       *
       * For safety, the line length is limited.
       */
      if (c->infill>=sizeof c->in)
	{
	  c->discard	= 1;	/* line too long	*/
	  c->infill	= 0;
	}
      got	= read(tino_sock_fdO(sock),
		       c->in+c->infill, sizeof c->in-c->infill);
      xDP(("() read %d", got));
      if (got<=0)
	return got;	/* proper error handling done by upstream	*/
      c->infill	+= got;

      if (c->p->immediate)
	send_to_pty(c, c->infill);
      else
	{
	  /* Send the lines to the terminal
	   * One by one.
	   */
	  while (c->infill && (pos=memchr(c->in, '\n', (size_t)c->infill))!=0)
	    {
	      pos++;
	      send_to_pty(c, pos-c->in);
	    }
	}
      return got;

    case TINO_SOCK_PROC_WRITE:
      xDP(("() write"));
      /* Socket is ready to be written to
       *
       * We have it all.
       */
      if (c->outpos>=c->outfill)
	{
	  TINO_GLIST_ENT	list;
	  long long		min;

	  c->outpos	= 0;
	  c->outfill	= 0;
	  /* always >0, only <0 on overruns
	   */
	  min		= c->p->blockcount-c->p->screen->count;
	  if (c->minscreenpos && min<c->minscreenpos)
	    {
	      min		= c->minscreenpos;
	      c->minscreenpos	= 0;
	    }
	  if (min>c->screenpos)
	    {
	      c->outfill	= snprintf(c->out, sizeof c->out,
					   "\n[[%Ld bytes (%Ld infoblocks) skipped]]\n",
					   c->p->skipbytes-c->screenbytes, min-c->screenpos);
	      c->screenpos	= min;
	      c->screenbytes	= c->p->skipbytes;
	    }
	  /* Find the proper infoblock
	   *
	   * Well, I should optimize this,
	   * but ptybuffer was not written to handle enourmous
	   * amounts of data and enourmous number of connects.
	   */
	  list	= c->p->screen->list;
	  for (; min<c->screenpos && list; min++)
	    list	= list->next;
	  for (; list; c->screenpos++, list=list->next)
	    {
	      int	max;

	      max	= sizeof c->out-c->outfill;
	      if (list->len>max)
		break;
	      memcpy(c->out+c->outfill, list->data, list->len);
	      c->outfill	+= list->len;
	    }
	}
      /* This should not happen, but ..
       */
      if (!c->outfill)
	{
	  xDP(("() write cancel"));
	  return 1;
	}
      xDP(("() write(%d)", (int)(c->outfill-c->outpos)));
      put	= write(tino_sock_fdO(sock),
			c->out+c->outpos, c->outfill-c->outpos);
      xDP(("() write %d", put));
      if (put>0)
	c->outpos	+= put;
      return put;

    default:
      break;
    }
  tino_fatal("connect_process");
  return 0;
}

/* Process information comming from PTY
 */
static int
master_process(TINO_SOCK sock, enum tino_sock_proctype type)
{
  struct ptybuffer	*p=tino_sock_userO(sock);
  char			buf[BUFSIZ];
  int			got, put;

  xDP(("master_process(%p[%d], %d)", sock, tino_sock_fdO(sock), type));
  /* Do a cleanup step each time we come here.
   */
  if (p->screen->count>p->history_length)
    {
      p->skipbytes	+= p->screen->list->len;
      tino_glist_free(tino_glist_get(p->screen));
    }
  switch (type)
    {
    case TINO_SOCK_PROC_CLOSE:
      xDP(("master_process() CLOSE"));
      /* The terminal was closed.
       *
       * Flag the close, as we have to wait for
       * all others to have read the data.
       *
       * Actually, for now, ptybuffer does not wait at all, sorry.
       */
      tino_FATAL(sock!=p->pty);
      p->pty	= 0;
      p->forcepoll	= 1;
      file_log("main: close master");
      return TINO_SOCK_CLOSE;

    case TINO_SOCK_PROC_EOF:
      xDP(("master_process() EOF"));
      /* This does a close in the next cycle
       */
      return TINO_SOCK_ERR;

    case TINO_SOCK_PROC_POLL:
      xDP(("master_process() poll %d", p->send->count));
      /* If something is waiting to be written
       * mark this side as readwrite, else only read.
       */
      if (p->sock)
        return p->send->count || (p->out && p->outpos<p->outlen)
		? TINO_SOCK_READWRITE
		: TINO_SOCK_READ;
      /* XXX TODO XXX
       * half close pty here?  Is this supported?
       */
      if (p->send->count || (p->out && p->outpos<p->outlen))
	return TINO_SOCK_WRITE;
      file_log("main: closing master, all data written");
      return TINO_SOCK_EOF;

    case TINO_SOCK_PROC_READ:
      /* Save output from the terminal in our internal buffer
       *
       * Cleanup of lines is done above.
       */
      xDP(("master_process() read"));
      got	= read(tino_sock_fdO(sock), buf, sizeof buf);
      xDP(("master_process() read %d", got));
      if (got<0)
	return -1;
      if (got>0)
	{
	  file_out(buf, (size_t)got);
	  tino_glist_add_n(p->screen, buf, (size_t)got);
	  p->bytecount	+= got;
	  p->blockcount++;
	  p->forcepoll	= 1;
	}
      return got;

    case TINO_SOCK_PROC_WRITE:
      xDP(("master_process() write"));
      /* Write data to the terminal which is due.
       *
       * First: Free old used up output pointers
       */
      if (p->out && p->outpos>=p->outlen)
	{
	  free((void *)p->out);
	  p->out	= 0;
	}
      /* fetch some more data which is waiting
       * to be written to the terminal
       */
      if (!p->out)
	{
	  TINO_GLIST_ENT	ent;

	  xDP(("master_process() write new"));
	  ent		= tino_glist_get(p->send);
	  p->outpos	= 0;
	  p->outlen	= ent->len;
	  p->out	= ent->data;
	  ent->data	= 0;
	  tino_glist_free(ent);
	}
      /* Write something to the terminal
       */
      put	= write(tino_sock_fdO(sock),
			p->out+p->outpos, p->outlen-p->outpos);
      xDP(("master_process() write %d", put));
      if (put>0)
	{
	  if (doecho)
	    {
	      TINO_GLIST_ENT	ent;

	      ent	= tino_glist_add_n(p->screen, NULL, 10+put);
	      strcpy(ent->data, "\n[sent: ");
	      memcpy(ent->data+8, p->out+p->outpos, (size_t)put);
	      memcpy(ent->data+8+put, "]\n", (size_t)2);
	      file_out(ent->data, 10+put);
	      p->bytecount	= 10+put;
	      p->blockcount++;
	      p->forcepoll	= 1;
	    }
	  /* Remember which data was written.
	   * Note that cleanup is done above.
	   */
	  p->outpos	+= put;
	}
      return put;

    default:
      break;
    }
  tino_fatal("master_process");
  return 0;
}

static TINO_SOCK
ptybuffer_new_fd(struct ptybuffer *p, int fd)
{
  struct ptybuffer_connect	*buf;
  TINO_SOCK			sock;

  buf		= tino_alloc0O(sizeof *buf);
  buf->p	= p;
  if (p->history_tail>=0 && p->blockcount>p->history_tail)
    buf->minscreenpos	= p->blockcount-p->history_tail;
  sock		= tino_sock_new_fdAn(fd, connect_process, buf);

  file_log("connect %d: %d sockets", fd, tino_sock_useO());
  return sock;
}

/* Accept new connections from socket
 * This is:
 * A new client comes in and wants to see the data we have
 * buffered from the tty.
 */
static int
sock_process(TINO_SOCK sock, enum tino_sock_proctype type)
{
  struct ptybuffer		*p=tino_sock_userO(sock);
  int				fd;

  xDP(("sock_process(%p[%d], %d)", sock, tino_sock_fdO(sock), type));
  switch (type)
    {
    default:
      tino_fatal("sock_process");

    case TINO_SOCK_PROC_CLOSE:
      file_log("close socket");
      xDP(("sock_process() CLOSE"));
      tino_FATAL(sock!=p->sock);
      p->sock	= 0;
      return TINO_SOCK_CLOSE;
      
    case TINO_SOCK_PROC_EOF:
    case TINO_SOCK_PROC_POLL:
      if (tino_sock_useO()>1 && (p->keepopen || p->pty>0))
        {
          xDP(("sock_process() ACCEPT"));
          return TINO_SOCK_ACCEPT;
	}
      xDP(("sock_process() EOF"));
      return TINO_SOCK_EOF;

    case TINO_SOCK_PROC_READ:
    case TINO_SOCK_PROC_WRITE:
    case TINO_SOCK_PROC_ACCEPT:
      xDP(("sock_process() accept"));
      fd	= tino_sock_acceptI(tino_sock_fdO(sock));
      xDP(("sock_process() accept %d", fd));
      if (fd<0)
	{
	  file_log("accept: returned error %s", strerror(errno));
	  return -1;
	}
      tino_sock_pollOn(ptybuffer_new_fd(p, fd));
      break;
    }
  xDP(("sock_process() end"));
  return 0;
}

/* Run the main loop
 * in daemon mode
 *
 * Actually with option -d this is the main program.
 */
static void
daemonloop(int sock, int master, struct ptybuffer_params *params)
{
  struct ptybuffer	work = { 0 };

  file_log("main: starting loop");

  xDP(("daemonloop(%d,%d)", sock, master));

  work.pty		= tino_sock_new_fdAn(master, master_process, &work);
  work.screen		= tino_glist_new(0);
  work.send		= tino_glist_new(0);
  work.forcepoll	= 1;

  work.history_length	= params->history_length<=0 ? 1000 : params->history_length;
  work.history_tail	= params->history_tail;
  work.immediate	= params->immediate;
  work.kill_incomplete	= params->kill_incomplete;
  work.keepopen		= params->keepopen;

  /* Treat stdin as the first connect?
   * This is also set if sock==0 (see main())
   */
  if (params->first_connect)
    work.sock		= ptybuffer_new_fd(&work, params->first_connect);

  if (sock)
    work.sock		= tino_sock_new_fdAn(sock, sock_process, &work);

  while (tino_sock_useO())
    {
      int	tmp;

      tmp		= work.forcepoll;
      work.forcepoll	= 0;
      xDP((" pty=%p sock=%p forcepoll=%d", work.pty, work.sock, tmp));
      if (tino_sock_selectEn(tmp)<0)
	tino_exit("select");
    }
}

/* Exit 42 if socket appears living
 *
 * else return (which is a mess).
 */
static void
do_check(const char *name)
{
  int	fd;

  tino_sock_error_fn	= tino_sock_error_fn_ignore;
  fd	= tino_sock_unix_connect(name);
  tino_sock_error_fn	= 0;
  if (fd>=0)
    {
      file_log("check: socket up: %s", name);
      exit(42);
    }
}

static int
log_childstatus(pid_t pid)
{
  char	*buf;
  int	ret;

  file_log("main: waiting for child: %ld", (long)pid);
  ret	= tino_wait_child_exact(pid, &buf);
  file_log("main: child %s", buf);
  return ret;
}

/* This routine is too long
 */
int
main(int argc, char **argv)
{
  struct ptybuffer_params	params;
  pid_t	pid;
  int	master, sock, fd, stderr_saved;
  int	foreground, check, force;
  int	fds[2];
  int	argn;

  tino_sigign(SIGPIPE);		/* make sure we do not get this signal	*/
  tino_sigdummy(SIGCHLD);	/* make sure we get EINTR on SIGCHLD	*/

#if 0
  struct termios	tio;
  struct winsize	winsz;
  tcgetattr(0, &tio);
  ioctl(0, TIOCGWINSZ, (char *)&winsz);
#endif

  argn	= tino_getopt(argc, argv, 1, 0,
		      TINO_GETOPT_VERSION(PTYBUFFER_VERSION)
#if 0
		      TINO_GETOPT_DEBUG
#endif
		      " sockfile command [args...]\n"
		      "	if sockfile=- then connection comes from stdin (implies -s).\n"
		      "	Note that lines longer than " PTYBUFFER_MAX_INPUTLINE_STR " send to the\n"
		      "	pty are silently discarded if option -i is not present."
		      ,

		      TINO_GETOPT_USAGE
		      "h	This help"
		      ,
#if 0
		      TINO_GETOPT_STRING
		      "b brand	Branding running process name.\n"
		      "		You can do 'killall brand' afterwards (use - for default)"
		      , &brand, 
#endif
		      TINO_GETOPT_FLAG
		      "c	Check if the socket appears to be living.\n"
		      "		returns 42 if so, 24 if not and no command.\n"
		      "		Checks the PTY to be alive, unless option -w is given\n"
		      "		Use with option -f to re-create the socket."
		      , &check,

		      TINO_GETOPT_FLAG
		      "d	ptybuffer runs in foreground (see -s)\n"
		      "		Else daemonizes: It detaches from the\n"
		      "		current terminal and changes working dir to /"
		      , &foreground,

		      TINO_GETOPT_FLAG
		      "e	Echo input to terminal output.\n"
		      "		Try this if 'stty echo' fails."
		      , &doecho,

		      TINO_GETOPT_FLAG
		      "f	force socket creation.  (Also see -c)"
		      , &force,

		      TINO_GETOPT_FLAG
		      "i	immediate mode, do not wait for full lines.\n"
		      "		Without this option ptybuffer waits, until a full line is\n"
		      "		received from clients, then it sends the line to the terminal.\n"
		      "		If you are concerned that the pty never sees lines longer\n"
		      "		than " PTYBUFFER_MAX_INPUTLINE_STR " then do not use this option\n"
		      "		and set option -k!  See also option -k"
		      , &params.immediate,

		      TINO_GETOPT_FLAG
		      "k	kill incomplete lines on socket disconnect\n"
		      "		This was the old behavior of ptybuffer before 0.6.0.\n"
		      "		Note:  When option -i and -k are used together, the behavior\n"
		      "		might change in future to truncate overlong lines!"
		      , &params.kill_incomplete,

		      TINO_GETOPT_STRING
		      "l file	write activity log to file.  If file is - then:\n"
		      "		- log goes to stderr\n"
		      "		- and stderr of the child is not connected to the pty"
		      , &logfile,

		      TINO_GETOPT_MIN_PTR
		      TINO_GETOPT_LONGINT
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN
		      "n nr	number of history buckets to allocate\n"
		      "		This is read()s and not lines."
		      , &params.history_tail
		      , &params.history_length,
		      PTYBUFFER_HISTORY_LENGTH,
		      1,

		      TINO_GETOPT_STRING
		      "o file	write terminal output to file (- to stdout)"
		      , &outfile,

		      TINO_GETOPT_FLAG
		      "p	prefix output (option -o) with timestamp"
		      ,	&dotimestamp,

		      TINO_GETOPT_FLAG
		      "q	quiet operation (suppress start banners if no -l)"
		      , &doquiet,

		      TINO_GETOPT_FLAG
		      "s	use stdin as first connected socket.\n"
		      "		This is similar to sockfile=- but ptybuffer continues on EOF"
		      , &params.first_connect,

		      TINO_GETOPT_MAX_PTR
		      TINO_GETOPT_LONGINT
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN
		      "t nr	number of tail buckets to print on connect\n"
		      "		-1=all, else must be less than -n option."
		      , &params.history_length
		      , &params.history_tail,
		      -1,
		      -1,

		      TINO_GETOPT_FLAG
		      "w	wait for all connections to terminate before closing\n"
		      "		before closing the main socket.\n"
		      "		Option -c then no more checks if command is alive."
		      , &params.keepopen,


		      NULL
		      );
  xDP(("[argn=%d]", argn));
  if (argn<=0)
    return 1;

  if (logfile)
    {
      if (strcmp(logfile, "-"))
	logfile	= tino_file_realpathE(logfile);
      file_log("status log: %s", logfile);
    }
  if (outfile)
    {
      if (strcmp(outfile, "-"))
	outfile	= tino_file_realpathE(outfile);
      file_log("output log: %s", outfile);
    }

  if (check)
    {
      do_check(argv[argn]);
      if (argn+1>=argc)
	{
	  file_log("check: socket down: %s", argv[argn]);
	  return 24;
	}
    }
  else if (argn+1>=argc)
    tino_exit("missing command");

  if (!foreground)
    {
      xDP(("[daemonizing]"));
      /* Open a communication pipe
       * such that errors are propagated to the caller
       */
      if (pipe(fds))
	tino_exit("pipe");

      /* fork off a child
       */
      if ((pid=fork())!=0)
	return parent(pid, fds);

      mypid	= 0;

      /* child:
       * Close the reading pipe
       * Start a new process session
       */
      tino_file_close_ignO(fds[0]);
      setsid();

      /* XXX TODO Bug?
       *
       * We are the session group leader.  Is there a way to attach a
       * controlling terminal to it?  Following suggests this can happen:
       * http://www.unixguide.net/unix/programming/1.7.shtml
       *
       * For beeing a tty driver, it perhaps makes sense to be a
       * session group leader ..  Unclear ..
       */
    }

  /* Open some file descriptors
   * Do it here such that errors can be intercepted
   * before we fork off the PTY
   * If sock is - then stdin is the controlling socket.
   */
  sock	= 0;
  if (strcmp(argv[argn], "-"))
    {
      if (force && !tino_file_notsocketE(argv[argn]))
	unlink(argv[argn]);
      sock	= tino_sock_unix_listen(argv[argn]);
    }
  argn++;
  fd	= -1;	/* only to skip a warning */
  if (!foreground)	/* we want to do this here, before the fork() */
    if ((fd=open("/dev/null", O_RDWR))<0)
      tino_exit("/dev/null");

  /* Now fork off the PTY thread
   *
   * This closes stderr, however perhaps we need it later again.
   */
  stderr_saved	= dup(2);
  if (!sock || params.first_connect)
    params.first_connect	= tino_sock_wrapO(0, 1, TINO_SOCK_WRAP_SHAPE_NORMAL);
  if ((pid=forkpty(&master, NULL, NULL, NULL))==0)
    {
      char	*env;

      /* forkpty() created fds 0,1,2 for us	*/
      mypid	= 0;

      /* Close the writing pipe
       * and all other not needed file descriptors.
       * Then exec the wanted program
       */
      if (sock)	/* if sock=0 (see above) do not close it, as it is from forkpty()	*/
        tino_file_close_ignO(sock);
      if (!foreground)
	{
	  close(fd);
	  close(fds[1]);
	}
      /* Special: keep stderr of child on stderr	*/
      if (logfile && !strcmp(logfile, "-"))
	dup2(stderr_saved, 2);
      tino_file_close_ignO(stderr_saved);

      /* Put the current PID into the environment.
       *
       * I don't even trust my eyes, but 120 shall be long enough ever.
       */
      env	= tino_allocO(120);
      snprintf(env, 120, "PTYBUFFER_PID=%ld", (long)getppid());
      env[119]	= 0;
      putenv(env);

      file_log("child: starting %s", argv[argn]);
      execvp(argv[argn], argv+argn);

      /* Tell about the error
       */
      file_log("child: exec failed");
      tino_exit("execvp %s", argv[argn]);
    }
  if (pid==(pid_t)-1)
    tino_exit("forkpty");
  tino_file_close_ignO(stderr_saved);
  file_log("main: forked child %ld", (long)pid);

  if (!foreground)
    {
      file_log("main: daemonizing, cd /");

      /* Close the controlling terminal.
       * Only close fd if it's not needed.
       * Well, better don't try stdout/stderr logging
       * within daemon mode, it's stupid.
       */
      dup2(fd, 0);
      if (!outfile || strcmp(outfile, "-"))
	dup2(fd, 1);
      if (!logfile || strcmp(logfile, "-"))
	dup2(fd, 2);
      tino_file_close_ignO(fd);

      setsid();
      tino_file_chdirE("/");

      /* Tell OK to the caller
       */
      tino_file_writeE(fds[1], "OK", 2);
      tino_file_close_ignO(fds[1]);

      doquiet	= 1;	/* suppress further default child output	*/
    }

  daemonloop(sock, master, &params);

  return log_childstatus(pid);
}
