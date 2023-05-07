/* ptybuffer: connect to ptybuffer from shell
 *
 * This Works is placed under the terms of the Copyright Less License,
 * see file COPYRIGHT.CLL.  USE AT OWN RISK, ABSOLUTELY NO WARRANTY.
 */

#if 0
#define	TINO_DP_sock	TINO_DP_ON
#endif

#include "tino/file.h"
#include "tino/sockbuf.h"
#include "tino/getopt.h"
#include "tino/codec.h"

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

/* I consider it bad design that such wrappers are needed.  There
 * should be some more easy way to do it directly.  Read: I am not
 * happy with sockbuf today.
 */
static void
ignore_fn(TINO_SOCKBUF sb, int len)
{
  tino_buf_resetO(tino_sockbuf_inO(sb));
}

static void
out_fn(TINO_SOCKBUF sb, int len)
{
  int	err, *ign = tino_sockbuf_userO(sb);

  if (!tino_buf_get_lenO(tino_sockbuf_inO(sb)))
    return;

  err	= tino_buf_write_away_allE(tino_sockbuf_inO(sb), 1, -1);
  if (err)
    {
      if (err>0)
        exit(1);
      tino_exit("error writing stdout");
    }
  if (*ign)
    {
      tino_sock_shutdownE(tino_sockbuf_fdO(sb), SHUT_RD);
    }
}

static void
term_fn(TINO_SOCKBUF sb, int len)
{
  if (!tino_buf_get_lenO(tino_sockbuf_outO(sb)))
    tino_sock_shutdownE(tino_sockbuf_fdO(sb), SHUT_WR);
}

struct terminal_info
  {
    int			len;
    int			tty;
    TINO_SOCKBUF	out;
    unsigned char	escape[];
  };

static void
terminal(TINO_SOCKBUF out, const char *esc)
{
  struct terminal_info	*info;
  int			len, tty;
  TINO_SOCKBUF		sb;

  tty	= isatty(0);
  if (!esc)
    esc	= tty ? "1B1B1B1B1B" : "";
  len	= strlen(esc);
  if (len&1)
    tino_exit("escape sequence must be hex, hence have a even number of characters");
  len	/= 2;

  info		= tino_alloc0O(len + sizeof *info);
  info->len	= len;
  info->tty	= tty;
  info->out	= out;

  if (len != tino_dec_hexO(info->escape, len, esc))
    tino_exit("escape sequence cannot be decoded from hex");

  sb	= tino_sockbuf_newOn(0, tty ? "TTY" : "(stdin)", &info);

  tino_exit("interactive not yet implemented");
}

int
main(int argc, char **argv)
{
  TINO_SOCKBUF	sb;
  int		argn, ignore, term, interactive;
  int		no_lf;
  char		*send;
  char		*esc;

  argn	= tino_getopt(argc, argv, 1, 1,
                      TINO_GETOPT_VERSION(PTYBUFFER_VERSION)
                      " socket\n"
                      "	Connect to ptybuffer socket"
                      ,

                      TINO_GETOPT_FLAG
                      TINO_GETOPT_MAX
                      "b	batch mode (read data from stdin)"
                      "		default if stdin is a tty"
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
                      TINO_GETOPT_MAX
                      "q	quiesce output (data read from socket not printed)\n"
                      "		Give two times to only read the first packet from socket"
                      , &ignore,
                      2,

#if 0
                      TINO_GETOPT_FLAG
                      TINO_GETOPT_FLAG
                      "u	unescape input (backslash escaped as in C)"
                      "		give twice to unescape -p and stdin"
                      , &unescape,
                      2,
#endif
                      TINO_GETOPT_STRING
                      "x str	set the escape sequence in hexadecimal\n"
                      "		If received within a second, interactive mode is interrupted\n"
                      "		NUL is supported (as 00), set to empty string to disable.\n"
                      "		On TTY this defaults to 5 times ESC, else empty string"
                      , &esc,
                      NULL
                      );

  if (argn<=0)
    return 1;

  if (!interactive)
    interactive	= isatty(0) ? 1 : -1;

  sb	= tino_sockbuf_newOn(tino_sock_unix_connect(argv[argn]), argv[argn], &ignore);
  if (send)
    {
      tino_buf_add_sO(tino_sockbuf_outO(sb), send);
      if (!no_lf)
        tino_buf_add_cO(tino_sockbuf_outO(sb), '\n');
    }
  TINO_SOCKBUF_SET(sb, TINO_SOCKBUF_READ_HOOK, ignore&1 ? ignore_fn : out_fn);
  if (term)
    TINO_SOCKBUF_SET(sb, TINO_SOCKBUF_WRITE_HOOK, term_fn);
  if (interactive>0)
    terminal(sb, esc);
  return tino_sock_select_loopA();
}

