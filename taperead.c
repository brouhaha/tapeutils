/*
   taperead

   Copyright 1999, 2000 Eric Smith
   Copyright 2016 Lars Brinkhoff

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

#include "stdio.h"
#include "stdarg.h"
#include "stdlib.h"

#include "tapeio.h"

#define MAX_REC_LEN 32768


typedef unsigned int u32;      /* non-portable!!! */


char *progname;
int print_verbose = 0;

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


void verbose (char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  if (print_verbose)
    vfprintf (stdout, fmt, ap);
  va_end (ap);
}


int main (int argc, char *argv[])
{
  u32 file = 0;
  u32 record = 0;
  u32 filebytes = 0;
  u32 tapebytes = 0;
  u32 len;
  char *srcfn = NULL;
  tape_handle_t src = NULL;
  FILE *dst = NULL;
  char *buf;
  int tape_flags = TF_DEFAULT;
  char filename[100];

  progname = argv [0];

  while (++argv, --argc)
    {
      if ((argv [0][0] == '-') && (argv [0][1] != '\0'))
	{
	  if (argv [0][1] == 's')
	    tape_flags |= TF_SIMH;
	  else if (argv [0][1] == 'v')
	    print_verbose = 1;
	  else
	    fatal (1, "unrecognized option '%s'\n", argv [0]);
	}
      else if (! srcfn)
	srcfn = argv [0];
      else
	fatal (1, NULL);
    }

  if (! srcfn)
    fatal (1, NULL);

  buf = malloc (MAX_REC_LEN);
  if (! buf)
    fatal (2, "can't allocate buffer\n");

  src = opentape (srcfn, 0, 0);
  if (! src)
    fatal (3, "can't open source tape\n");

  tapeflags (src, tape_flags);

  dst = fopen("file0000", "wb");

  for (;;)
    {
      len = getrec (src, buf, MAX_REC_LEN);
      if (len == 0)
	{
	  fclose (dst);

	  if (filebytes == 0)
	    {
	      verbose ("end of tape\n");
	      break;
	    }

	  verbose ("total length of file %d = %d records, %u bytes\n",
		   file, record, filebytes);
	  tapebytes += filebytes;
	  file++;
	  record = 0;
	  filebytes = 0;
	  verbose ("start of file %d\n", file);
	  sprintf (filename, "file%04d", file);
	  dst = fopen(filename, "wb");
	  
	  fflush (stdout);
	}
      else
	{
	  verbose ("file %d record %d: length %d\n", file, record, len);
	  fflush (stdout);
	  fwrite (buf, 1, len, dst);
	  fflush (dst);
	  filebytes += len;
	  record++;
	}
    }

  closetape (src);

  return (0);
}
