/* $Header$
 *
 * ptybuffer: connect to ptybuffer from shell
 * Copyright (C)2006-2007 Valentin Hilbig <webmaster@scylla-charybdis.com>
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
 *
 * $Log$
 * Revision 1.3  2007-08-29 20:30:12  tino
 * Bugfix (int -> long long) and ptybufferconnect -i
 *
 */

#if 0
#define	TINO_DP_sock	TINO_DP_ON
#endif

#include "tino/file.h"
#include "tino/sockbuf.h"
#include "tino/getopt.h"

#include "ptybuffer_version.h"

#if 0
static void
put(const char *buf, int n, int noesc)
{
  while (--n>=0)
    {
      unsigned char	c;

      c	= *buf++;
      if (c<32)
	printf("[%02x]", c);
      else
	putchar(c);
    }
}
#endif

/* I consider it bad that such a wrapper is needed
 */
static void
out_fn(TINO_SOCKBUF sb, int len)
{
  tino_buf_write_away(tino_sockbuf_inO(sb), 0, -1);
}

static void
term_fn(TINO_SOCKBUF sb, int len)
{
  if (!tino_buf_get_len(tino_sockbuf_outO(sb)))
    tino_sock_shutdownE(tino_sockbuf_fdO(sb), SHUT_WR);
}

int
main(int argc, char **argv)
{
  TINO_SOCKBUF	sb;
  int		argn, ignore, term, interactive;
  int		no_lf;
  char		*send;

  argn	= tino_getopt(argc, argv, 1, 1,
		      TINO_GETOPT_VERSION(PTYBUFFER_VERSION)
		      " socket\n"
		      "	Connect to ptybuffer socket"
		      ,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "b	batch mode (read data from stdin)"
		      , &interactive,
		      1,
#if 0
		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "c	control characters are printed as escapes"
		      "		Give twice to supress output of raw characters, too"
		      , &control_chars,
		      2,
#endif
		      TINO_GETOPT_FLAG
		      "e	exit if data is transferred (see -p)"
		      , &term,

		      TINO_GETOPT_USAGE
		      "h	This help"
		      ,

		      TINO_GETOPT_FLAG
		      "i	Do not append a linefeed to string given with -p\n"
		      "		This corresponds to ptybuffer's option -i"
		      , &no_lf,

		      TINO_GETOPT_FLAG
		      TINO_GETOPT_MAX
		      "n	noninteractive, ignore stdin\n"
		      "		default if stdin is not a tty"
		      , &interactive,
		      -1,

		      TINO_GETOPT_STRING
		      "p str	prepend string to output to socket"
		      , &send,

		      TINO_GETOPT_FLAG
		      "q	quiesce output (data read from socket not printed)"
		      , &ignore,

#if 0
		      TINO_GETOPT_FLAG
		      TINO_GETOPT_FLAG
		      "u	unescape input (backslash escaped as in C)"
		      "		give twice to unescape -p and stdin"
		      , &unescape,
		      2,
#endif

		      NULL
		      );

  if (argn<=0)
    return 1;

  if (!interactive)
    interactive	= isatty(0) ? 1 : -1;

  sb	= tino_sockbuf_newO(tino_sock_unix_connect(argv[argn]), argv[argn], NULL);
  if (send)
    {
      tino_buf_add_s(tino_sockbuf_outO(sb), send);
      if (!no_lf)
	tino_buf_add_c(tino_sockbuf_outO(sb), '\n');
    }
  if (!ignore)
    TINO_SOCKBUF_SET(sb, TINO_SOCKBUF_READ_HOOK, out_fn);
  if (term)
    TINO_SOCKBUF_SET(sb, TINO_SOCKBUF_WRITE_HOOK, term_fn);
  if (interactive>0)
    {
      fprintf(stderr, "interactive mode not yet implemented, use -n\n");
      return -1;
    }
  return tino_sock_select_loopA();
}
