/*
   tapewrite

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
  fprintf (f, "Usage: %s out files...\n", progname);
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
  u32 recordlen = 1024;
  u32 filebytes = 0;
  u32 tapebytes = 0;
  u32 len;
  tape_handle_t dst = NULL;
  FILE *src = NULL;
  char *buf;
  int tape_flags = TF_DEFAULT;

  progname = argv [0];

  while (++argv, --argc)
    {
      if ((argv [0][0] == '-') && (argv [0][1] != '\0'))
	{
	  if (argv [0][1] == 's')
	    tape_flags |= TF_SIMH;
	  else if (argv [0][1] == 'v')
	    print_verbose = 1;
	  else if (argv [0][1] == 'n')
	    {
	      ++argv, --argc;
	      recordlen = atoi(argv [0]);
	    }
	  else
	    fatal (1, "unrecognized option '%s'\n", argv [0]);
	}
      else
	{
	  dst = opentape (argv [0], 1, 1);
	  if (! dst)
	    fatal (3, "can't open destination tape\n");
	  break;
	}
    }

  buf = malloc (MAX_REC_LEN);
  if (! buf)
    fatal (2, "can't allocate buffer\n");

  tapeflags (dst, tape_flags);

  while (++argv, --argc)
    {
      src = fopen(argv [0], "rb");
      file++;
      verbose ("reading from file %s\n", argv [0]);
      for (;;)
	{
	  len = fread (buf, 1, recordlen, src);
	  verbose ("read %u bytes\n", len);
	  if (len > 0)
	    {
	      putrec (dst, buf, recordlen);
	      tapebytes += recordlen;
	      filebytes += recordlen;
	      record++;
	    }
	  if (len < recordlen)
	    {
	      verbose ("end of file, %u records, %u bytes\n", record, filebytes);
	      fclose (src);
	      tapemark (dst);
	      record = 0;
	      filebytes = 0;
	      break;
	    }
	}
    }

  closetape (dst);
  verbose ("end of tape, %u files, %u bytes\n", file, tapebytes);

  return (0);
}
