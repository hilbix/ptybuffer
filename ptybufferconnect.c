/* $Header$
 *
 * ptybuffer: connect to ptybuffer from shell
 * Copyright (C)2006 Valentin Hilbig, webmaster@scylla-charybdis.com
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
 * Revision 1.1  2006-07-26 12:20:22  tino
 * options -c, -f and release
 *
 */

#include "tino/file.h"
#include "tino/sock.h"

static int	sock;

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

int
main(int argc, char **argv)
{
  char	buf[BUFSIZ];
  int	n;

  printf("not ready yet!\n");
  if (argc!=2)
    {
      fprintf(stderr,
	      "Usage: %s socket\n"
	      "\tConnect to ptybuffer transparently"
	      , argv[0]);
      return 1;
    }
  sock	= tino_sock_unix_connect(argv[1]);
  while ((n=read(sock, buf, sizeof buf))>0)
    put(buf, n, 1);
  000;
  return 0;
}
