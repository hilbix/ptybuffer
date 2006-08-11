/* $Header$
 *
 * ptybuffer: daemonize interactive tty line driven programs with output history
 * Copyright (C)2004-2006 Valentin Hilbig, webmaster@scylla-charybdis.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Log$
 * Revision 1.16  2006-08-11 22:09:07  tino
 * Bugfix (for missing option -c)
 * child status is logged (and returned for option -d)
 *
 * Revision 1.15  2006/07/26 12:20:22  tino
 * options -c, -f and release
 *
 * Revision 1.14  2006/07/26 11:41:14  tino
 * Option -t
 *
 * Revision 1.13  2006/04/11 23:00:07  tino
 * Now working .. now dist
 *
 * Revision 1.12  2006/04/11 22:44:15  tino
 * Well, I was too fast already again.  It does not work.  Looking for error.
 *
 * Revision 1.11  2006/04/11 22:08:12  tino
 * new release
 *
 * Revision 1.10  2004/11/12 04:45:00  tino
 * NULL pointer dereference corrected and minor logging improvements
 *
 * Revision 1.9  2004/10/22 01:14:12  tino
 * new release
 *
 * Revision 1.8  2004/05/23 12:25:58  tino
 * Thou shalt not forget to test compile before checkin ;)
 *
 * Revision 1.7  2004/05/23 12:22:21  tino
 * closedown problem in ptybuffer elliminated
 *
 * Revision 1.6  2004/05/23 10:12:23  tino
 * new upload for NWNadm
 *
 * Revision 1.5  2004/05/21 02:23:35  tino
 * minor issue fixed: free() of "work" pointer which is a stack variable
 *
 * Revision 1.4  2004/05/20 04:59:00  tino
 * master can be 0, which closes stdin.  close in the slave part removed.
 *
 * Revision 1.3  2004/05/20 04:22:22  tino
 * Forgot to add the initial poll to the new socket
 *
 * Revision 1.2  2004/05/20 02:05:43  tino
 * History now has a define and is 1000 lines, not 10 like for testing
 *
 * Revision 1.1  2004/05/19 20:22:30  tino
 * first commit
 */
#ifndef PTYBUFFER_HISTORY_LENGTH
#define PTYBUFFER_HISTORY_LENGTH	1000
#endif

#include "tino/file.h"
#include "tino/debug.h"
#include "tino/fatal.h"
#include "tino/alloc.h"
#include "tino/sock.h"
#include "tino/slist.h"
#include "tino/getopt.h"
#include "tino/proc.h"

#include <setjmp.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <pty.h>
#include <utmp.h>

#include "ptybuffer_version.h"

static char	*outfile, *logfile;
static int	doecho;

/* Do you have a better idea?
 *
 * The file must be always flushed and I want you to be able to always
 * rotate the file without anything to keep in mind.
 */
static __inline__ void
file_out(void *ptr, size_t len)
{
  FILE	*fd;

  fd	= stdout;
  if (!outfile ||
      (strcmp(outfile, "-") && (fd=fopen(outfile, "a+"))==0))
    return;

  fwrite(ptr, len, 1, fd);
  if (fd==stdout)
    fflush(fd);
  else
    fclose(fd);
}

/* do not call this before the last fork
 */
static void
file_log(const char *s, ...)
{
  FILE		*fd;
  va_list	list;
  struct tm	tm;
  time_t	tim;
  static pid_t	pid;

  fd	= stderr;
  if (!logfile ||
      (strcmp(logfile, "-") && (fd=fopen(logfile, "a+"))==0))
    return;

  time(&tim);
  gmtime_r(&tim, &tm);
  if (!pid)
    pid	= getpid();
  fprintf(fd,
	  "%4d-%02d-%02d %02d:%02d:%02d %ld: ",
	  1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday,
	  tm.tm_hour, tm.tm_min, tm.tm_sec,
	  (long)pid);
  va_start(list, s);
  vfprintf(fd, s, list);
  va_end(list);
  fputc('\n', fd);
  if (fd==stderr)
    fflush(fd);
  else
    fclose(fd);
  return;
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
  close(fds[1]);

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
  close(fds[0]);

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
  };
struct ptybuffer
  {
    TINO_SOCK		sock, pty;
    int			forcepoll;
    TINO_GLIST		send;
    TINO_GLIST		screen;
    long long		count;
    int			outlen, outpos;
    const char		*out;
    long		history_length;
    long		history_tail;
  };

struct ptybuffer_connect
  {
    struct ptybuffer	*p;
    int			infill, discard;
    char		in[BUFSIZ];
    int			outfill, outpos;
    char		out[BUFSIZ+BUFSIZ];
    long long		screenpos;
  };

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
  struct ptybuffer_connect	*c	= tino_sock_user(sock);
  char				*pos;
  int				got, put, cnt;

  xDP(("connect_process(%p[%d], %d)", sock, tino_sock_fd(sock), type));
  switch (type)
    {
    case TINO_SOCK_PROC_EOF:
      xDP(("connect_process() eof"));
      return TINO_SOCK_FREE;

    case TINO_SOCK_PROC_CLOSE:
      /* Good bye to the other side
       */
      xDP(("connect_process() close"));
      file_log("close %d: %d sockets",
	       tino_sock_fd(sock), tino_sock_use());
      /* propagate close of stdin main socket
       * in case we use sockfile=-
       */
      if (c->p->sock==sock)
	c->p->sock	= 0;
      return TINO_SOCK_FREE;

    case TINO_SOCK_PROC_POLL:
      xDP(("connect_process() poll"));
      /* If something is waiting to be written
       * mark this side as readwrite, else only read.
       */
      return (c->outpos<c->outfill || c->screenpos<c->p->count
	      ? TINO_SOCK_READWRITE
	      : TINO_SOCK_READ);

    case TINO_SOCK_PROC_READ:
      xDP(("connect_process() read"));
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
      got	= read(tino_sock_fd(sock),
		       c->in+c->infill, sizeof c->in-c->infill);
      xDP(("connect_process() read %d", got));
      if (got<=0)
	return got;	/* proper error handling done by upstream	*/
      c->infill	+= got;
      /* Send the lines to the terminal
       * One by one.
       */
      while (c->infill && (pos=memchr(c->in, '\n', (size_t)c->infill))!=0)
	{
	  pos++;
	  cnt	= pos-c->in;
	  xDP(("connect_process() cnt %d max %d", (int)cnt, (int)c->infill));
	  tino_FATAL(cnt>c->infill);
#if 0
	  if (memchr(c->in, 0, cnt))
	    c->discard	= 1;	/* line must not contain \0	*/
#endif
	  if (!c->discard)
	    {
	      xDP(("connect_process() add %.*s", (int)cnt, c->in));
	      tino_glist_add_n(c->p->send, c->in, cnt);
	      tino_sock_poll(c->p->pty);
#if 0
	      /* This is too dangerous.  If passwords are entered with
	       * 'stty noecho', then they shall not be recorded.
	       * (Well, we can perhaps auto-switch logging later.)
	       */
	      file_log("input %d: %.*s",
		       tino_sock_fd(sock), (int)cnt-1, c->in);
#endif
	    }
	  c->discard	= 0;
	  if ((c->infill-=cnt)>0)
	    memmove(c->in, pos, c->infill);
	}
      return got;

    case TINO_SOCK_PROC_WRITE:
      xDP(("connect_process() write"));
      /* Socket is ready to be written to
       *
       * We have it all.
       */
      if (c->outpos>=c->outfill)
	{
	  TINO_GLIST_ENT	list;
	  int			min;

	  c->outpos	= 0;
	  c->outfill	= 0;
	  min		= c->p->count-c->p->screen->count;
	  if (min>c->screenpos)
	    {
	      c->outfill	= snprintf(c->out, sizeof c->out,
					   "\n[[%Ld infoblocks skipped]]\n",
					   min-c->screenpos);
	      c->screenpos	= min;
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
	  xDP(("connect_process() write cancel"));
	  return 1;
	}
      xDP(("connect_process() write(%d)", (int)(c->outfill-c->outpos)));
      put	= write(tino_sock_fd(sock),
			c->out+c->outpos, c->outfill-c->outpos);
      xDP(("connect_process() write %d", put));
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
  struct ptybuffer	*p=tino_sock_user(sock);
  char			buf[BUFSIZ];
  int			got, put;

  xDP(("master_process(%p[%d], %d)", sock, tino_sock_fd(sock), type));
  /* Do a cleanup step each time we come here.
   */
  if (p->screen->count>p->history_length)
    tino_glist_free(tino_glist_get(p->screen));
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
      file_log("close master");
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
      return (p->send->count || (p->out && p->outpos<p->outlen)
	      ? TINO_SOCK_READWRITE
	      : TINO_SOCK_READ);

    case TINO_SOCK_PROC_READ:
      /* Save output from the terminal in our internal buffer
       *
       * Cleanup of lines is done above.
       */
      xDP(("master_process() read"));
      got	= read(tino_sock_fd(sock), buf, sizeof buf);
      xDP(("master_process() read %d", got));
      if (got<0)
	return -1;
      if (got>0)
	{
	  file_out(buf, (size_t)got);
	  tino_glist_add_n(p->screen, buf, (size_t)got);
	  p->count++;
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
      put	= write(tino_sock_fd(sock),
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
	      p->count++;
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

  buf		= tino_alloc0(sizeof *buf);
  buf->p	= p;
  if (p->history_tail>=0 && p->history_tail>p->count)
    buf->screenpos	= p->count-p->history_tail;
  sock		= tino_sock_new_fd(fd, connect_process, buf);

  file_log("connect %d: %d sockets", fd, tino_sock_use());
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
  struct ptybuffer		*p=tino_sock_user(sock);
  int				fd;

  xDP(("sock_process(%p[%d], %d)", sock, tino_sock_fd(sock), type));
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
      xDP(("sock_process() ACCEPT"));
      return TINO_SOCK_ACCEPT;

    case TINO_SOCK_PROC_READ:
    case TINO_SOCK_PROC_WRITE:
    case TINO_SOCK_PROC_ACCEPT:
      xDP(("sock_process() accept"));
      fd	= accept(tino_sock_fd(sock), NULL, NULL);
      xDP(("sock_process() accept %d", fd));
      if (fd<0)
	return -1;
      tino_sock_poll(ptybuffer_new_fd(p, fd));
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

  /* Treat stdin as the first connect?
   * This is also set if sock==0
   * stdin_use is the duped fd (as stdin might be redirected).
   */
  if (params->first_connect)
    work.sock		= ptybuffer_new_fd(&work, params->first_connect);

  if (sock)
    work.sock		= tino_sock_new_fd(sock, sock_process, &work);
  work.pty		= tino_sock_new_fd(master, master_process, &work);
  work.screen		= tino_glist_new(0);
  work.send		= tino_glist_new(0);
  work.forcepoll	= 1;

  work.history_length	= params->history_length<=0 ? 1000 : params->history_length;
  work.history_tail	= params->history_tail;

  /* Actually I should extend this:
   *
   * As long as there is a socket, loop.
   *
   * If terminal is closed, then close work.sock, too.
   *
   * Any socket which tries to write to the closed terminal is closed,
   * too.
   *
   * If terminal is closed and all data was written, close the write
   * side and linger until socket is closed.
   */
  while (work.pty && work.sock)
    {
      int	tmp;

      tmp		= work.forcepoll;
      work.forcepoll	= 0;
      if (tino_sock_select(tmp)<0)
	tino_exit("select");
    }
}

static jmp_buf do_check_jmp;

static void
do_check_hook(const char *err, va_list list)
{
  longjmp(do_check_jmp, 1);
}

/* Exit 42 if socket apperas living
 *
 * else return (which is a mess).
 */
static void
do_check(const char *name)
{
  if (!setjmp(do_check_jmp))
    {
      tino_sock_error_fn	= do_check_hook;
      tino_sock_unix_connect(name);
      /* always a success if we come here
       */
      file_log("check: socket up: %s", name);
      exit(42);
    }
  tino_sock_error_fn	= 0;
}

static int
log_childstatus(pid_t pid)
{
  char	*buf;
  int	ret;

  ret	= tino_wait_child_exact(pid, &buf);
  file_log("child: %s", buf);
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

  signal(SIGPIPE, SIG_IGN);

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
		      "	if sockfile=- then connection comes from stdin.",
#if 0
		      TINO_GETOPT_STRING
		      "b brand	Branding running process name.\n"
		      "		You can do 'killall brand' afterwards"
		      , &brand, 
#endif
		      TINO_GETOPT_FLAG
		      "c	Check if the socket appears to be living.\n"
		      "		returns 42 if so, 24 if not and no command.\n"
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

#if 0
		      TINO_GETOPT_FLAG
		      "i	immediate mode, do not wait for full lines.\n"
		      "		Without this option ptybuffer waits, until a full line is\n"
		      "		received from clients until this line is sent to the terminal"
		      , &immediate,
#endif

		      TINO_GETOPT_STRING
		      "l file	write activity log to file (- to stderr)\n"
		      "		When daemonizing use an absolute path!"
		      , &logfile,

		      TINO_GETOPT_LONGINT
		      TINO_GETOPT_DEFAULT
#if 0
		      TINO_GETOPT_INT_MIN
		      TINO_GETOPT_INT_MIN_REF
#endif
		      "n nr	number of history buckets to allocate\n"
		      "		This is read()s and not lines."
#if 0
		      , 0
		      , &params.history_tail
#endif
		      , &params.history_length,
		      PTYBUFFER_HISTORY_LENGTH,

		      TINO_GETOPT_FLAG
		      "s	use stdin as first connected socket.\n"
		      "		This is similar to sockfile=- but ptybuffer continues on EOF"
		      , &params.first_connect,

		      TINO_GETOPT_INT
		      TINO_GETOPT_DEFAULT
#if 0
		      TINO_GETOPT_INT_MIN
		      TINO_GETOPT_INT_MAX_REF
#endif
		      "t nr	number of tail buckets to print on connect\n"
		      "		-1=all, else must be less than -n option."
#if 0
		      , -1
		      , &params.history_length
#endif
		      , &params.history_tail,
		      -1,

		      TINO_GETOPT_STRING
		      "o file	write terminal output to file (- to stdout)\n"
		      "		When daemonizing use an absolute path!"
		      , &outfile,

		      NULL
		      );
  xDP(("[argn=%d]", argn));
  if (argn<=0)
    return 1;

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

      /* child:
       * Close the reading pipe
       * Start a new process session
       */
      close(fds[0]);
      setsid();
    }

  /* Open some file descriptors
   * Do it here such that errors can be intercepted
   * before we fork off the PTY
   * If sock is - then stdin is the controlling socket.
   */
  sock	= 0;
  if (strcmp(argv[argn], "-"))
    {
      if (force && !tino_file_notsocket(argv[argn]))
	unlink(argv[argn]);
      sock	= tino_sock_unix_listen(argv[argn]);
    }
  argn++;
  fd	= -1;	/* only to skip a warning */
  if (!foreground)
    if ((fd=open("/dev/null", O_RDWR))<0)
      tino_exit("/dev/null");

  /* Now fork off the PTY thread
   *
   * This closes stderr, however perhaps we need it later again.
   */
  stderr_saved	= dup(2);
  if (!sock || params.first_connect)
    params.first_connect	= dup(0);
  if ((pid=forkpty(&master, NULL, NULL, NULL))==0)
    {
      char	*env;

      /* Close the writing pipe
       * and all other not needed file descriptors.
       * Then exec the wanted program
       */
      close(sock);
      if (!foreground)
	{
	  close(fd);
	  close(fds[1]);
	}
      if (logfile && !strcmp(logfile, "-"))
	dup2(stderr_saved, 2);
      close(stderr_saved);

      /* Put the current PID into the environment.
       *
       * I don't even trust my eyes, but 120 shall be long enough ever.
       */
      env	= tino_alloc(120);
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
  close(stderr_saved);
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
      close(fd);

      setsid();
      chdir("/");

      /* Tell OK to the caller
       */
      write(fds[1], "OK", 2);
      close(fds[1]);
    }
  if (outfile)
    file_log("main: output log: %s", outfile);

  daemonloop(sock, master, &params);

  return log_childstatus(pid);
}
