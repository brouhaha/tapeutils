/*
   tapedump

   This program uses a modified version of John Wilson's tapeio library to
   copy tapes between any of the following:
  
       local tape drives              - pathname starting with /dev/
       remote tape drives (rmt)       - pathname containing a colon
       Wilson-format tape image files - any other pathname
  
   Copyright 1999, 2000 Eric Smith

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as published
   by the Free Software Foundation.  Note that permission is not granted
   to redistribute this program under the terms of any other version of the
   General Public License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "tapeio.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define MAX_REC_LEN 32768

char *progname;

void print_usage (FILE *f)
{
  fprintf (f, "Usage: %s in\n", progname);
}

void fatal (int retval, char *fmt, ...)
{
  va_list ap;

  if (fmt)
    {
      fprintf (stderr, "%s: ", progname);
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
    }

  if (retval == 1)
    print_usage (stderr);

  exit (retval);
}


#define BYTES_PER_LINE 16

void dump (FILE *f, char *buf, int len)
{
  int i, j;

  for (i = 0; i < len; i += BYTES_PER_LINE)
    {
      fprintf (f, "  %04x: ", i);
      for (j = 0; j < BYTES_PER_LINE; j++)
	if ((i + j) < len)
	  fprintf (f, "%02x ", 0xff & buf [i+j]);
	else
	  fprintf (f, "   ");
      for (j = 0; j < BYTES_PER_LINE; j++)
	if ((i + j) < len)
	  {
	    char c = buf [i+j];
	    if ((c >= ' ') && (c <= '~'))
	      fprintf (f, "%c", c);
	    else
	      fprintf (f, ".");
	  }
	else
	  fprintf (f, " ");
      fprintf (f, "\n");
    }
}


#ifdef HP_2000_SUPPORT

#define MAX_FN_LEN 6

void dump_hp_2000_file_header (FILE *f, char *buf)
{
  unsigned long uid;
  char uids [6];
  char filename [MAX_FN_LEN + 1];
  int i;
  uid = ((buf [0] & 0xff) << 8) | (buf [1] & 0xff);
  uids [0] = (uid / 1024) + '@';
  sprintf (& uids [1], "%03d", uid & 0x3ff);
  for (i = 0; i < MAX_FN_LEN; i++)
    filename [i] = buf [i+2] & 0x7f;
  filename [sizeof (filename) - 1] = '\0';
  fprintf (f, "  id %s filename '%s'\n", uids, filename);
}

#endif /* HP_2000_SUPPORT */

int main (int argc, char *argv[])
{
  int file = 0;
  int record = 0;
  unsigned long filebytes = 0;
  unsigned long tapebytes = 0;
  int len;
  char *srcfn = NULL;
  tape_handle src = NULL;
  char *buf;
#ifdef HP_2000_SUPPORT
  int hp_2000_hibernate = 0;
#endif /* HP_2000_SUPPORT */

  progname = argv [0];

  while (++argv, --argc)
    {
      if ((argv [0][0] == '-') && (argv [0][1] != '\0'))
	{
#ifdef HP_2000_SUPPORT
	  if (argv [0][1] == 'h')
	    hp_2000_hibernate++;
	  else
#endif /* HP_2000_SUPPORT */
	    fatal (1, "unrecognized option '%s'\n", argv [0]);
	}
      else if (! srcfn)
	srcfn = argv [0];
      else
	fatal (1, NULL);
    }

  if (! srcfn)
    fatal (1, NULL);

  buf = (char *) malloc (MAX_REC_LEN);
  if (! buf)
    fatal (2, "can't allocate buffer\n");

  src = opentape (srcfn, 0, 0);
  if (! src)
    fatal (3, "can't open source tape\n");

  for (;;)
    {
      len = getrec (src, buf, MAX_REC_LEN);
      if (len == 0)
	{
	  printf ("total length of file %d = %d records, %lu bytes\n",
		  file, record, filebytes);
	  tapebytes += filebytes;
	  file++;
	  record = 0;
	  filebytes = 0;
	  printf ("start of file %d\n", file);
	  fflush (stdout);
	}
      else
	{
	  printf ("file %d record %d: length %d\n", file, record, len);
#ifdef HP_2000_SUPPORT
	  if (hp_2000_hibernate && (file > 0) && (record == 0) && (len >= 24))
	    dump_hp_2000_file_header (stdout, buf);
#endif /* HP_2000_SUPPORT */
	  dump (stdout, buf, len);
	  fflush (stdout);
	  filebytes += len;
	  record++;
	}
    }

  closetape (src);

  return (0);
}
