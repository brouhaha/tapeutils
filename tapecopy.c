/*
   tapecopy

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

char *buf;

void print_usage (FILE *f)
{
  fprintf (f, "Usage: %s [-v] in out\n", progname);
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

int main (int argc, char *argv[])
{
  int file = 0;
  unsigned long filebytes = 0;
  unsigned long tapebytes = 0;
  int prevlen = -2;
  int lencount = 0;
  int firstrec = 0;
  int len;
  int verbose = 0;
  char *srcfn = NULL;
  char *destfn = NULL;
  tape_handle src = NULL;
  tape_handle dest = NULL;

  progname = argv [0];

  while (++argv, --argc)
    {
      if ((argv [0][0] == '-') && (argv [0][1] != '\0'))
	{
	  if (argv [0][1] == 'v')
	    verbose++;
	  else
	    fatal (1, "unrecognized option '%s'\n", argv [0]);
	}
      else if (! srcfn)
	srcfn = argv [0];
      else if (! destfn)
	destfn = argv [0];
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

  if (destfn)
    {
      dest = opentape (destfn, 1, 1);
      if (! dest)
	fatal (4, "can't open dest tape\n");
    }
  else
    verbose++;

  for (;;)
    {
      len = getrec (src, buf, MAX_REC_LEN);
      if ((lencount != 0) && ((len == 0) || (len != prevlen)))
	{
	  if (verbose)
	    {
	      if (lencount == 1)
		printf ("1 record (%d)\n", firstrec);
	      else
		printf ("%d records (%d..%d)\n", lencount, firstrec,
			firstrec+lencount-1);
	      fflush (stdout);
	    }
	  filebytes += prevlen * lencount;
	  firstrec += lencount;
	  prevlen = -1;
	  lencount = 0;
	}
      if (len != 0)
	{
	  if (lencount == 0)
	    {
	      if (verbose)
		{
		  printf ("file %d record length %d: ", file, len);
		  fflush (stdout);
		}
	    }
	  if (destfn)
	    putrec (dest, buf, len);
	  lencount++;
	}
      else
	{
	  tapebytes += filebytes;
	  if (verbose)
	    {
	      if (prevlen == 0)
		printf ("end of tape, %lu total bytes\n", tapebytes);
	      else
		printf ("end of file %d, %lu bytes\n", file, filebytes);
	      fflush (stdout);
	    }
	  if (destfn)
	    tapemark (dest);
	  if (prevlen == 0)
	    break;
	  file++;
	  lencount = 0;
	  firstrec = 0;
	  filebytes = 0;
	}
      prevlen = len;
    }

  closetape (src);
  if (destfn)
    closetape (dest);

  return (0);
}
