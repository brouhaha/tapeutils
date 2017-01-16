/*
   Dump contents of TAPEX tapes, or tape images.  Based on Eric Smith's
   modified t10backup.c.

   Copyright 1999 Eric Smith
   Copyright 2017 Lars Brinkhoff

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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "tapeio.h"

#define bool long
#define false 0
#define true 1

#define MAX_FILE_SPEC 1024

#define RAWSIZE (5*(512))

#define endof(s) (strchr(s, (char) 0))

char *progname;


tape_handle_t tape;			/* Source "tape". */


bool eightbit = false;		/* Status of -8 (eight-bit) flag. */
bool buildtree = false;		/* Status of -d (build trees) flag. */
bool interchange = false;	/* Status of -i (interchange) flag. */
long verbose = 0;		/* Status of -v (verbose) flag. */


char* argfiles [MAX_FILE_SPEC];		/* File spec's to extract. */
long argcount;			/* Number of them. */


unsigned char rawdata[RAWSIZE];	/* Raw data for a tape block. */

long headlh[512], headrh[512];	/* Header block from tape. */
long datalh[512], datarh[512];	/* Data block from tape. */

long prevSEQ;			/* SEQ number of previous block. */
long currentfilenumber;

long defercount;			/* Count of defered output bytes. */

bool extracting;
FILE* destination;

/* Tape information: */

char systemname[100];
char savesetname[100];

/* File information: */

long a_bsiz;				/* For now. */
long a_alls;
long a_mode;
long a_leng;

char filedev[100];		/* Device: */
char filedir[100];		/* [ufd] */
char filename[1000];		/* file name. */
char fileext[100];		/* extension. */
int file_bytesize;
int file_blocks;
long long file_bytes;

char filespec[7][100];		/* [0]: device:ufd. */
				/* [1-5]: sfd's, stored directly here. */
				/* [6]: file.ext */

char cname[100];		/* Canonical name. */


void print_usage (FILE *f)
{
  fprintf (f, "Usage: %s [options] [filespecs...]\n", progname);
  fprintf (f, "Options:\n"
	      "    -t          list directory\n"
              "    -x          extract files\n"
              "    -f <file>   read input from file\n"
              "                    /dev/*     tape drive\n"
              "                    host:file  rmt server\n"
              "                    file       tape image file\n"
              "                    -          tape image from stdin\n"
              "    -s <range>  use save sets in range\n"
              "                e.g. '-s 2' for only save set 2\n"
              "                     '-s 2,' for save set 2 through EOT\n"
              "                     '-s 3,5' for save sets 3 through 5\n"
	      "    -v          verbose\n"
	      "    -vv         very verbose\n"
              "    -8          eight bit mode\n"
              "    -i          interchange mode\n"
              "    -d          build directory tree\n");
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


void warning (char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: ", progname);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}


/* unpackheader unpacks the header block from the raw stream. */

void unpackheader (void)
{
  unsigned char* rawptr;
  long i, left, right;
  unsigned char c;

  rawptr = & rawdata [0];

  for (i = 0; i < 512; i++) 
    {
      left = * (rawptr++) << 10;
      left |= * (rawptr++) << 2;
      left |= (c = * (rawptr++)) >> 6;
      right = (c & 077) << 12;
      right |= * (rawptr++) << 4;
      right |= * (rawptr++) & 017;
      headlh [i] = left;
      headrh [i] = right;
    }
}

void unpack_filename(void)
{
  long long x;
  char *s = filename;
  int i, j;

  for (i = 2; i < 512; i++) {
    x = headlh[i];
    x <<= 18;
    x += headrh[i];
    if (x == 0)
      break;
    for (j = 0; j < 5; j++) {
      *s++ = ((x >> 29) & 0177);
      x <<= 7;
    }
  }
}


/* unpackdata unpacks the data block from the raw stream. */

void unpackdata (void)
{
  unsigned char* rawptr;
  long i, left, right;
  unsigned char c;

  rawptr = & rawdata [32*5];

  for (i = 0; i < 512; i++) 
    {
      left = * (rawptr++) << 10;
      left |= * (rawptr++) << 2;
      left |= (c = * (rawptr++)) >> 6;
      right = (c & 077) << 12;
      right |= * (rawptr++) << 4;
      right |= * (rawptr++) & 017;
      datalh [i] = left;
      datarh [i] = right;
    }
}

void unpackinfo (void)
{
  file_bytesize = (headlh[0] >> 6) & 077;
  file_blocks = headrh[0];
  file_bytes = headlh[1];
  file_bytes <<= 18;
  file_bytes += headrh[1];
  unpack_filename();
}

/* pars_5chars reads five ASCII chars from a machine word. */

void pars_5chars (long index, char *store)
{
  long l, r;

  l = datalh [index];
  r = datarh [index];

  store [0] = (0177 & (l >> 11));
  store [1] = (0177 & (l >> 4));
  store [2] = (0177 & ((l << 3) | ((r >> 15) & 017)));
  store [3] = (0177 & (r >> 8));
  store [4] = (0177 & (r >> 1));
}


/* pars_asciz stores asciz text from data */

void pars_asciz (long index, char *store)
{
  long words;

  words = datarh [index++];
  while ((words--) > 0)
    {
      pars_5chars (index++, store);
      store += 5;
    }
  * store = (char) 0;
}


void zerotapeinfo (void)
{
}

void zerofileinfo (void)
{
  file_bytesize = 0;
  file_blocks = 0;
  file_bytes = 0;
  filename[0] = 0;
}


void printtapeinfo (void)
{
  if (!verbose)
    return;

  fprintf (stderr, "%s", filename);
  if (verbose > 1)
    fprintf (stderr, "   %d blocks; %lld %d-bit bytes\n",
	     file_blocks, file_bytes, file_bytesize);
  else
    fputc ('\n', stderr);

  if (verbose > 2 && (headlh[0] & 0770077) != 0)
    fprintf (stderr, "MYSTERY HEADER: %06lo%06lo %06lo%06lo\n",
	     headlh[0], headrh[0], headlh[1], headrh[1]);
}


void downcase (char *s)
{
  while (*s != (char) 0) {
    if (isupper(*s)) *s = tolower(*s);
    s++;
  }
}


void buildfilenames (void)
{
  long i;

  if (* filedev != (char) 0)
    sprintf (filespec [0], "%s:%s", filedev, filedir);
  else
    sprintf (filespec [0], "%s", filedir);

  sprintf (filespec [6], "%s.%s", filename, fileext);

  for (i = 0; i < 7; i++)
    downcase (filespec [i]);

  sprintf (cname, "%s", filespec [0]);
  for (i = 1; i < 6; i++) 
    {
      if (* filespec [i] != (char) 0)
	sprintf (endof (cname), ".%s", filespec [i]);
    }
  if (* cname != (char) 0)
    sprintf (endof (cname), "..%s", filespec [6]);
  else
    sprintf (cname, "%s", filespec [6]);
}


void printfileinfo (void)
{
  buildfilenames ();
  printf ("%3ld  %s", currentfilenumber, cname);
  if (verbose)
    {
      printf (" (%ld) alloc:%ld, mode:%lo, len:%ld", a_bsiz, a_alls, a_mode, a_leng);
    }
  printf ("\n");
}


/* readblock reads one logical block from the input stream. */
/* The header is unpacked into head{l,r}; the data is not. */

int readblock (void)
{
  long i;
  i = getrec (tape, rawdata, RAWSIZE);
  if (i == 0)
    return (0);
  if (i != RAWSIZE)
    {
      fprintf (stderr, "record length %ld, expected %d\n", i, RAWSIZE);
      while (i++ < RAWSIZE) rawdata [i] = (char) 0;
    }
  return (1);
}


/* Disk file output routines: */

void WriteBlock (void)
{
}


/* OpenOutput opens the output file, according to -d and -i flags. */

bool OpenOutput (void)
{
  struct stat statbuf;
  char oname [100];
  long i;

  defercount = 0;

  if (interchange) 
    destination = fopen (filespec [6], "w");
  else if (! buildtree)
    destination = fopen(cname, "w");
  else
    {
      for (i = 0, oname [0] = (char) 0; i < 6; i++) 
	{
	  if (* filespec [i] == (char) 0)
	    break;
	  sprintf (endof (oname), "%s", filespec [i]);
	  if (stat (oname, & statbuf) != 0) 
	    {
	      if (mkdir (oname, 0777) != 0) 
		{
		  warning ("cannot create %s/\n", oname);
		  return (false);
		}
	    }
	  sprintf (endof (oname), "/");
	}
      sprintf (endof (oname), "%s", filespec [6]);
      destination = fopen (oname, "w");
    }

  return (destination != NULL);
}

void CloseOutput (void)
{
  /* Close output file after us. */
}

/* Argmatch checks if the current file matches the given argument: */

bool argmatch (char *arg)
{
  long target;
  char* f;
  char* p;
  char* s;

  if (*arg == '#') 
    {
      (void) sscanf (arg, "#%ld", & target);
      return (target == currentfilenumber);
    }

  if (*arg == '*')
    return (1);

  for (f = cname; *f != (char) 0; f++)
    {
      for (p = f, s = arg; (*s != (char) 0) && (*p == *s); p++, s++)
	;
      if (*s == (char) 0)
	return (true);
    }
  return (false);
}

/* doextract performs the job of "tapex -x ..." */

int doextract (void)
{
  int got_blocks = 0;
  long i;

  currentfilenumber = 0;
  extracting = false;

  for (;;)
    {
      if (! readblock ())
	return (got_blocks);

      got_blocks = 1;

      currentfilenumber++;
      zerofileinfo ();
      buildfilenames ();
      for (i = 0; i < argcount; i++) 
	{
	  if (argmatch (argfiles [i])) 
	    {
	      if (*argfiles[i] == '#') 
		{
		  /* Maybe do a pure shift here? */
		  argfiles [i] = argfiles [--argcount];
		}
	      extracting = true;
	      break;
	    }
	}
      if (extracting) 
	{
	  if (OpenOutput ())
	    {
	      if (verbose) 
		{
		  printf ("Extracting %s", cname);
		  fflush (stdout);
		}
	    }
	  else 
	    {
	      warning ("can't open %s for output\n", cname);
	      extracting = false;
	    }
	}
    }

  if (extracting) 
    {
      WriteBlock ();
    }
}

/* dodirectory performs the job of "backup -t ..." */

int dodirectory (void)
{
  int end_of_tape = 0;
  int n;

  for (;;)
    {
      n = readblock ();
      if (n == 0) {
	if (end_of_tape)
	  return 0;
	end_of_tape = 1;
	continue;
      }

      end_of_tape = 0;
      unpackheader ();
      unpackinfo ();
      zerotapeinfo ();
      printtapeinfo ();

      while (file_blocks-- > 0)
	{
	  readblock ();
	}
    }
}

/* command decoder and dispatcher */

void checkarg (char *arg)
{
  long i;
  char c;

  if (*arg == '#') 
    {
      if (sscanf (arg, "#%ld%c", & i, & c) != 1) 
	fatal (1, "bad argument: '%s'\n", arg);
    }
}


int main (int argc, char *argv[])
{
  long i;
  char action = '\0';
  char* inputname = NULL;
  char *arg;

  progname = argv [0];
  argcount = 0;

  while (--argc > 0)
    {
      ++argv;
      arg = argv [0];
      if (arg [0] == '-')
	{
	  while (*++arg)
	    switch (*arg)
	      {
	      case '8':
		eightbit = true;  break;
	      case 'd':
		buildtree = true;  break;
	      case 'f':
		if (--argc < 0)
		  fatal (1, "input file name missing\n");
		inputname = (++argv) [0];
		break;
	      case 't':
		verbose++;
		/* Fall through. */
	      case 'x':
		action = *arg; break;
	      case 'v':
		verbose++;  break;
	      default:
		fatal (1, "bad option %c\n", arg [1]);
	      }
	}
      else
	{
	  if (argcount >= MAX_FILE_SPEC)
	    fatal (1, "too many file specs\n");
	  argfiles [argcount++] = argv [0];
	}
    }

  if (! action)
    fatal (1, "either -t or -x must be specified\n");

  for (i = 0; i < argcount; i++)
    checkarg (argfiles[i]);

  if (inputname == NULL) 
    fatal (1, "no input file given\n");

  tape = opentape (inputname, 0, 0);
  if (! tape)
    fatal (1, "can't open %s for input\n", inputname);

  switch (action) {
  case 't': dodirectory (); break;
  case 'x': doextract (); break;
  }

  return 0;
}
