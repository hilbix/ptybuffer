/* $Header$
 *
 * ptybuffer: daemonize interactive tty line driven programs with output history
 * Copyright (C)2004 Valentin Hilbig, webmaster@scylla-charybdis.com
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
 * Revision 1.8  2004-05-23 12:25:58  tino
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
#if 0
#define ECHO_SEND
#endif
#define HISTORY_LENGTH	1000
#if 0
#define DEBUG_INTERACTIVE
#endif

#include "tino_common.h"
#include "tino/sock.h"
#include "tino/slist.h"

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <pty.h>
#include <utmp.h>

/* main parent:
 * inform caller if everything is OK
 */
static int
parent(pid_t pid, int *fds)
{
  char	buf[BUFSIZ];
  int	got;

  if (pid==(pid_t)-1)
    ex("fork");

  /* close the writing pipe
   */
  close(fds[1]);

  /* wait until the child is ready
   */
  while ((got=read(fds[0], buf, sizeof buf-1))==-1)
    if (errno!=EINTR && errno!=EAGAIN)
      {
	got	= 0;
	break;
      }
  close(fds[0]);

  /* child sends OK in case everything is ok
   */
  if (got==2 && !strncmp(buf, "OK", 2))
    return 0;

  buf[got]	= 0;
  if (!got)
    strcpy(buf, "unexpected error");

  /* We have some error.
   * Be sure to kill off the child.
   */
  kill(pid, 9);
  ex("error: %s", buf);
  return -1;
}

struct ptybuffer
  {
    TINO_SOCK		sock, pty;
    int			forcepoll;
    TINO_GLIST		send;
    TINO_GLIST		screen;
    long long		count;
    int			outlen, outpos;
    const char		*out;
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
    case TINO_SOCK_PROC_CLOSE:
      return TINO_SOCK_FREE;

    case TINO_SOCK_PROC_POLL:
      xDP(("connect_process() poll"));
      return c->outpos<c->outfill || c->screenpos<c->p->count ? 3 : 1;

    case TINO_SOCK_PROC_READ:
      xDP(("connect_process() read"));
      if (c->infill>=sizeof c->in)
	{
	  c->discard	= 1;	/* line too long	*/
	  c->infill	= 0;
	}
      got	= read(tino_sock_fd(sock),
		       c->in+c->infill, sizeof c->in-c->infill);
      xDP(("connect_process() read %d", got));
      if (got<=0)
	return got;
      c->infill	+= got;
      while (c->infill && (pos=memchr(c->in, '\n', (size_t)c->infill))!=0)
	{
	  pos++;
	  cnt	= pos-c->in;
	  xDP(("connect_process() cnt %d max %d", (int)cnt, (int)c->infill));
	  FATAL(cnt>c->infill);
#if 0
	  if (memchr(c->in, 0, cnt))
	    c->discard	= 1;	/* line must not contain \0	*/
#endif
	  if (!c->discard)
	    {
	      xDP(("connect_process() add %.*s", (int)cnt, c->in));
	      tino_glist_add_n(c->p->send, c->in, cnt);
	      tino_sock_poll(c->p->pty);
	    }
	  c->discard	= 0;
	  if ((c->infill-=cnt)>0)
	    memmove(c->in, pos, c->infill);
	}
      return got;

    case TINO_SOCK_PROC_WRITE:
      xDP(("connect_process() write"));
      if (c->outpos>=c->outfill)
	{
	  TINO_GLIST_ENT	list;
	  int			min;

	  c->outpos	= 0;
	  c->outfill	= 0;
	  min		= c->p->count-c->p->screen->count;
	  if (min>c->screenpos)
	    {
	      c->outfill	= sprintf(c->out,
					  "\n[[%Ld infoblocks skipped]]\n",
					  min-c->screenpos);
	      c->screenpos	= min;
	    }
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

    case TINO_SOCK_PROC_ACCEPT:
      FATAL("TINO_SOCK_PROC_ACCEPT");
    }
  FATAL("connect_process");
  return 0;
}

/* Process PTY information
 */
static int
master_process(TINO_SOCK sock, enum tino_sock_proctype type)
{
  struct ptybuffer	*p=tino_sock_user(sock);
  char			buf[BUFSIZ];
  int			got, put;

  xDP(("master_process(%p[%d], %d)", sock, tino_sock_fd(sock), type));
  if (p->screen->count>HISTORY_LENGTH)
    tino_glist_free(tino_glist_get(p->screen));
  switch (type)
    {
    case TINO_SOCK_PROC_CLOSE:
      xDP(("master_process() CLOSE"));
      FATAL(sock!=p->pty);
      p->pty	= 0;
      return TINO_SOCK_CLOSE;

    case TINO_SOCK_PROC_EOF:
      xDP(("master_process() EOF"));
      return TINO_SOCK_ERR;

    case TINO_SOCK_PROC_POLL:
      xDP(("master_process() poll %d", p->send->count));
      return p->send->count ? TINO_SOCK_READWRITE : TINO_SOCK_READ;

    case TINO_SOCK_PROC_READ:
      xDP(("master_process() read"));
      got	= read(tino_sock_fd(sock), buf, sizeof buf);
      xDP(("master_process() read %d", got));
      if (got<0)
	return -1;
      if (got>0)
	{
	  tino_glist_add_n(p->screen, buf, (size_t)got);
	  p->count++;
	  p->forcepoll	= 1;
	}
      return got;

    case TINO_SOCK_PROC_WRITE:
      xDP(("master_process() write"));
      if (p->out && p->outpos>=p->outlen)
	{
	  free((void *)p->out);
	  p->out	= 0;
	}
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
      put	= write(tino_sock_fd(sock),
			p->out+p->outpos, p->outlen-p->outpos);
      xDP(("master_process() write %d", put));
      if (put>0)
	{
#ifdef ECHO_SEND
	  TINO_GLIST_ENT	ent;

	  ent	= tino_glist_add_n(p->screen, NULL, 10+put);
	  strcpy(ent->data, "\n[sent: ");
	  memcpy(ent->data+8, p->out+p->outpos, (size_t)put);
	  memcpy(ent->data+8+put, "]\n", (size_t)2);
	  p->count++;
	  p->forcepoll	= 1;
#endif
	  p->outpos	+= put;
	}
      return put;

    case TINO_SOCK_PROC_ACCEPT:
      FATAL("TINO_SOCK_PROC_ACCEPT");
    }
  FATAL("master_process");
  return 0;
}

/* Accept new connections from socket
 */
static int
sock_process(TINO_SOCK sock, enum tino_sock_proctype type)
{
  struct ptybuffer		*p=tino_sock_user(sock);
  struct ptybuffer_connect	*buf;
  int				fd;

  xDP(("sock_process(%p[%d], %d)", sock, tino_sock_fd(sock), type));
  switch (type)
    {
    case TINO_SOCK_PROC_CLOSE:
      xDP(("sock_process() CLOSE"));
      FATAL(sock!=p->sock);
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
      buf	= tino_alloc0(sizeof *buf);
      buf->p	= p;
      tino_sock_poll(tino_sock_new_fd(fd, connect_process, buf));
      break;
    }
  xDP(("sock_process() end"));
  return 0;
}

/* Run the main loop
 * in deamon mode
 */
static int
deamonloop(int sock, int master)
{
  struct ptybuffer	work = { 0 };

  xDP(("deamonloop(%d,%d)", sock, master));
  work.sock		= tino_sock_new_fd(sock,   sock_process,   &work);
  work.pty		= tino_sock_new_fd(master, master_process, &work);
  work.screen		= tino_glist_new(0);
  work.send		= tino_glist_new(0);
  work.forcepoll	= 1;
  while (work.pty && work.sock)
    {
      int	tmp;

      tmp		= work.forcepoll;
      work.forcepoll	= 0;
      if (tino_sock_select(tmp)<0)
	ex("select");
    }
  return 0;
}

int
main(int argc, char **argv)
{
  pid_t	pid;
  int	master, sock, fd;
#ifndef DEBUG_INTERACTIVE
  int	fds[2];
#endif

  signal(SIGPIPE, SIG_IGN);

#if 0
  struct termios	tio;
  struct winsize	winsz;
  tcgetattr(0, &tio);
  ioctl(0, TIOCGWINSZ, (char *)&winsz);
#endif

  if (argc<3)
    {
      fprintf(stderr, "Usage: %s sockfile command args...\n", argv[0]);
      return 1;
    }

#ifndef DEBUG_INTERACTIVE
  /* Open a communication pipe
   * such that errors are propagated to the caller
   */
  if (pipe(fds))
    ex("pipe");

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
#endif

  /* Open some file descriptors
   * Do it here such that errors can be intercepted
   * before we fork off the PTY
   */
  sock	= tino_sock_unix_listen(argv[1]);
#ifndef DEBUG_INTERACTIVE
  if ((fd=open("/dev/null", O_RDWR))<0)
    ex("/dev/null");
#endif

  /* Now fork off the PTY thread
   */
  if ((pid=forkpty(&master, NULL, NULL, NULL))==0)
    {
      xDP(("[child]"));
      /* Close the writing pipe
       * and all other not needed file descriptors.
       * Then exec the wanted program
       */
      close(sock);
#ifndef DEBUG_INTERACTIVE
      close(fd);
      close(fds[1]);
#endif
      execvp(argv[2], argv+2);
      /* Tell about the error
       */
      ex("execvp %s", argv[2]);
    }
  if (pid==(pid_t)-1)
    ex("forkpty");

  chdir("/");

#ifndef DEBUG_INTERACTIVE
  /* Close the controlling terminal
   */
  dup2(fd, 0);
  dup2(fd, 1);
  dup2(fd, 2);
  close(fd);

  /* Tell OK to the caller
   */
  write(fds[1], "OK", 2);
  close(fds[1]);
#endif

  return deamonloop(sock, master);
}
