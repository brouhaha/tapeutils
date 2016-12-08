/*
   Dump contents of Tops-10 BACKUP tapes, or tape images.

   Original author unknown.
   Modified by Eric Smith to use John Wilson's tapeio library.

   Copyright 1999 Eric Smith

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "t10backup.h"
#include "tapeio.h"

#define bool long
#define false 0
#define true 1

#define MAX_FILE_SPEC 1024

#define RAWSIZE (5*(32+512))

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

long headlh[32], headrh[32];	/* Header block from tape. */
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
char filename[100];		/* file name. */
char fileext[100];		/* extension. */

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

  for (i = 0; i < 32; i++) 
    {
      left = * (rawptr++) << 10;
      left |= * (rawptr++) << 2;
      left |= (c = * (rawptr++)) >> 6;
      right = (c & 077) << 12;
      right |= * (rawptr++) << 4;
      right |= * (rawptr++) & 017;
      headlh [i] = left;
      headrh [i] = right;
      if (verbose > 1)
	{
	  printf("\n%li l=%ld, r=%ld", i, left, right);
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


/* pars_o_name parses an o$name block from data. */

void pars_o_name (long index)
{
  long lastw;

  lastw = index + datarh [index];
  ++index;
  while (index < lastw) 
    {
      switch (datalh [index]) 
	{
	case 0:  index = lastw; break;
	case 1:  pars_asciz (index, filedev);  break;
	case 2:  pars_asciz (index, filename); break;
	case 3:  pars_asciz (index, fileext);  break;
	case 32: pars_asciz (index, filedir);  break;
	case 33: pars_asciz (index, filespec [1]); break;
	case 34: pars_asciz (index, filespec [2]); break;
	case 35: pars_asciz (index, filespec [3]); break;
	case 36: pars_asciz (index, filespec [4]); break;
	case 37: pars_asciz (index, filespec [5]); break;
	}
      index += datarh [index];
    }
}


void pars_o_attr (long index)
{
  /* parse off file attribute block */
  ++index;
  a_bsiz = datarh [index + A_BSIZ];	/* for now... */
  a_alls = datarh [index + A_ALLS];	/* for now... */
  a_mode = datarh [index + A_MODE];	/* for now... */
  a_leng = datarh [index + A_LENG];	/* for now... */
}


void pars_o_dirt (long index)
{
  /* parse off directory attribute block */
}


void pars_o_sysn (long index)
{
  pars_asciz (index, systemname);
}


void pars_o_ssnm (long index)
{
  pars_asciz (index, savesetname);
}


void zerotapeinfo (void)
{
  systemname  [0] = (char) 0;
  savesetname [0] = (char) 0;
}


void zerofileinfo (void)
{
  filedev  [0] = (char) 0;
  filedir  [0] = (char) 0;
  filename [0] = (char) 0;
  fileext  [0] = (char) 0;

  filespec [0][0] = (char) 0;
  filespec [1][0] = (char) 0;
  filespec [2][0] = (char) 0;
  filespec [3][0] = (char) 0;
  filespec [4][0] = (char) 0;
  filespec [5][0] = (char) 0;
  filespec [6][0] = (char) 0;

  cname [0] = (char) 0;
}


/* unpackinfo picks non-data information from data block. */

void unpackinfo (void)
{
  long index;

  unpackdata ();

  index = 0;
  while (index < headrh [G_LND]) 
    {
      switch (datalh [index])
	{
	case 1: pars_o_name (index); break;
	case 2: pars_o_attr (index); break;
	case 3: pars_o_dirt (index); break;
	case 4: pars_o_sysn (index); break;
	case 5: pars_o_ssnm (index); break;
	}
      index += datarh [index];
    }
}


void printtapeinfo (void)
{
  if (verbose) 
    {
      if (* savesetname != (char) 0)
	printf ("Saveset name: %s\n", savesetname);
      if (* systemname != (char) 0)
	printf ("Written on: %s\n", systemname);
    }
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
  unpackheader ();
  return (1);
}


/* Disk file output routines: */

void WriteBlock (void)
{
  char buffer [5*512];
  long bufpos, index;

  unpackdata ();

  for (index = headrh [G_LND], bufpos = 0;
       index < (headrh [G_LND] + headrh [G_SIZE]); index++) 
    {
      pars_5chars (index, &buffer [bufpos]);
      bufpos += 5;
    }

  if (headlh [G_FLAGS] & GF_EOF) 
    {
      for (index = 1; index < (eightbit ? 4 : 5); index++) 
	{
	  if (buffer[bufpos - 1] == (char) 0)
	    bufpos--;
	}
    }

  (void) fwrite (buffer, sizeof (char), bufpos, destination);
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

/* doextract performs the job of "backup -x ..." */

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

      if (headrh [G_SEQ] == prevSEQ)
	continue;

      if (headrh [G_TYPE] == T_FILE)
	{
	  if (headlh [G_FLAGS] & GF_SOF)
	    {
	      currentfilenumber++;
	      zerofileinfo ();
	      unpackinfo ();
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
	      if (headlh [G_FLAGS] & GF_EOF)
		{
		  (void) fclose (destination);
		  extracting = false;
		  if (verbose)
		    printf ("\n");
		  if (argcount == 0)
		    {
		      skipfile (tape, 1);
		      return (1);
		    }
		}
	    }
	}
      prevSEQ = headrh[G_SEQ];
    }
}

/* dodirectory performs the job of "backup -t ..." */

int dodirectory (void)
{
  int got_blocks = 0;
  currentfilenumber = 0;

  for (;;)
    {
      if (! readblock ())
	return (got_blocks);

      got_blocks = 1;

      if (headrh [G_SEQ] == prevSEQ)
	continue;

      if (headrh [G_TYPE] == T_BEGIN)
	{
	  zerotapeinfo ();
	  unpackinfo ();
	  printtapeinfo ();
	}
      if (headrh [G_TYPE] == T_FILE) 
	{
	  if (headlh [G_FLAGS] & GF_SOF) 
	    {
	      ++currentfilenumber;
	      zerofileinfo();
	      unpackinfo();
	      printfileinfo();
	    }
	}
      prevSEQ = headrh [G_SEQ];
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


void parse_range (char *s, int *first, int *last)
{
  char *s2 = s;

  (* first) = (* last) = strtol (s, & s, 0);
  if (! s)
    return;
  if (*(s++) != ',')
    goto badrange;
  if (! *s)
    {
      (* last) = INT_MAX;
      return;
    }
  (* last) = strtol (s, & s, 0);
  if (! s)
    goto badrange;
  if ((* first) > (* last))
    goto badrange;
  return;

 badrange:
  fatal (1, "bad range syntax: '%s'\n", s2);
}


int main (int argc, char *argv[])
{
  long i;
  char action = '\0';
  char* inputname = NULL;
  char *arg;
  int first_save_set = 0;
  int last_save_set = 0;
  int current_save_set = 0;

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
	      case 'i':
		interchange = true;  break;
	      case 's':
		if ((--argc < 0) || ((++argv)[0][0] == '-'))
		  fatal (1, "file skip count missing\n");
		parse_range (argv [0], & first_save_set, & last_save_set);
		/*
		  fprintf (stderr, "save set range %d..%d\n",
		           first_save_set, last_save_set);
                */
		break;
	      case 't':
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

  if (first_save_set > 0)
    {
      skipfile (tape, first_save_set);
      current_save_set = first_save_set;
    }

  while (current_save_set <= last_save_set)
    {
      int ok = 0;
      switch (action)
	{
	case 't': ok = dodirectory (); break;
	case 'x': ok = doextract (); break;
	}
      if (! ok)
	break;
      current_save_set++;
    }

  return (0);
}

