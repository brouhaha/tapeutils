/*
   tapedump

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

#include "stdio.h"
#include "stdarg.h"
#include "stdlib.h"

#include "tapeio.h"

#define MAX_REC_LEN 32768


typedef unsigned int u32;      /* non-portable!!! */
typedef unsigned char uchar;


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

void dump (FILE *f, uchar *buf, u32 len)
{
  u32 i, j;

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
	    uchar c = buf [i+j] & 0x7f;
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

u32 get16 (uchar *buf, u32 offset)
{
  return ((buf [offset] & 0xff) << 8) | (buf [offset+1] & 0xff);
}

char * file_flag_names [16] =
{
  "unrestricted",
  "protected",
  "locked",
  "private",
  "unused",
  "unused",
  "unused",
  "unused",
  "unused",
  "unused",
  "unused",
  "fcp",
  "mwa",
  "pfa",
  "output",
  "input"
};

void dump_hp_2000_directory_entry (FILE *f, uchar *buf)
{
  u32 uid;
  u32 flags;
  char uids [6];
  char filename [MAX_FN_LEN + 1];
  int i;

  uid = get16 (buf, 0);
  if (uid == 0xffff)
    {
      fprintf (f, "End of directory\n");
      return;
    }
  uids [0] = (uid / 1024) + '@';
  sprintf (& uids [1], "%03d", uid & 0x3ff);
  for (i = 0; i < MAX_FN_LEN; i++)
    filename [i] = buf [i+2] & 0x7f;
  filename [sizeof (filename) - 1] = '\0';
  fprintf (f, "ID %s filename '%s'", uids, filename);
  if (buf [2] & 0x80)
    fprintf (f, " ASCII");
  if (buf [4] & 0x80)
    fprintf (f, " file");
  if (buf [2] & 0x80)
    fprintf (f, " semi-compiled");
  flags = get16 (buf, 14);
  for (i = 15; i >= 0; i--)
    {
      if (flags & (1 << i))
	fprintf (f, " %s", file_flag_names [i]);
    }
  fprintf (f, "\n");
}

void dump_hp_2000_hibernate_file_header (FILE *f, uchar *buf, u32 len)
{
  if (len < 24)
    {
      fprintf (f, "file header too short for directory entry\n");
      return;
    }

  dump_hp_2000_directory_entry (f, buf);
}

void dump_hp_2000_hibernate (FILE *f, u32 file, u32 record, uchar *buf, u32 len)
{
  if ((file > 0) && (record == 0))
    dump_hp_2000_hibernate_file_header (f, buf, len);
}


static int mcp_file_type = 0;

void dump_hp_2000_mcp_file_header (FILE *f, uchar *buf, u32 len)
{
  u32 checksum = 0;
  u32 info;
  int i;

  if (len != 10)
    {
      fprintf (f, "wrong MCP file header length\n");
      return;
    }
  if ((get16 (buf, 0) != 01000) || (get16 (buf, 2) != 02001))
    {
      fprintf (f, "wrong MCP file header magic numbers\n");
      return;
    }
  fprintf (f, "MCP file ID word %d ", get16 (buf, 4));
  info = get16 (buf, 6);
  switch (info)
    {
    case 0x0000:
      fprintf (f, "abs");
      break;
    case 0x8000:
      fprintf (f, "rel");
      break;
    default:
      fprintf (f, "info word %d", info);
      break;
    }
  mcp_file_type = info;
  for (i = 2; i < 8; i += 2)
    checksum += get16 (buf, i);
  checksum &= 0xffff;
  if (checksum != get16 (buf, 8))
    fprintf (f, ", bad MCP file header checksum");
  fprintf (f, "\n");
}

void dump_hp_2000_abs_record (FILE *f, uchar *buf, u32 len)
{
  u32 l;
  u32 checksum = 0;
  u32 tape_checksum;
  u32 i;

  l= buf [0] * 2 + 6;
  if ((l != len) || (buf [1] != 0))
    {
      fprintf (f, "first word of record is not length (expected %d)\n", l);
      return;
    }
  for (i = 2; i < len - 2; i += 2)
    checksum += get16 (buf, i);
  checksum &= 0xffff;
  tape_checksum = get16 (buf, len - 2);
  if (checksum != tape_checksum)
    fprintf (f, "computed checksum %04x, tape checksum %04x\n", checksum,
	     tape_checksum);
}

void dump_hp_2000_rel_record (FILE *f, uchar *buf, u32 len)
{
  u32 l;

  l= buf [0] * 2;
  if ((l != len) || (buf [1] != 0))
    {
      fprintf (f, "first word of record is not length (expected %d)\n", l);
      return;
    }
}

void dump_hp_2000_mcp (FILE *f, u32 file, u32 record, uchar *buf, u32 len)
{
  if ((len == 10) && (buf [0] == 0x02) && (buf [1] == 0x00) &&
      (buf [2] == 0x04) && (buf [3] == 0x01))
    {
      dump_hp_2000_mcp_file_header (f, buf, len);
    }
  else switch (mcp_file_type)
    {
    case 0x0000:
      dump_hp_2000_abs_record (f, buf, len);
      break;
    case 0x8000:
      dump_hp_2000_rel_record (f, buf, len);
      break;
    default:
      fprintf (f, "unknown file type\n");
    }
}

#endif /* HP_2000_SUPPORT */


typedef enum {
  generic,
#ifdef HP_2000_SUPPORT
  hp_2000_hibernate,
  hp_2000_mcp,
#endif /* HP_2000_SUPPORT */
} t_tape_type;


int main (int argc, char *argv[])
{
  u32 file = 0;
  u32 record = 0;
  u32 filebytes = 0;
  u32 tapebytes = 0;
  u32 len;
  char *srcfn = NULL;
  tape_handle_t src = NULL;
  uchar *buf;
  t_tape_type tape_type = generic;

  progname = argv [0];

  while (++argv, --argc)
    {
      if ((argv [0][0] == '-') && (argv [0][1] != '\0'))
	{
#ifdef HP_2000_SUPPORT
	  if (argv [0][1] == 'h')
	    tape_type = hp_2000_hibernate;
	  else if (argv [0][1] == 'm')
	    tape_type = hp_2000_mcp;
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

  buf = (uchar *) malloc (MAX_REC_LEN);
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
	  printf ("total length of file %d = %d records, %u bytes\n",
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
	  switch (tape_type)
	    {
#ifdef HP_2000_SUPPORT
	      case hp_2000_hibernate:
		dump_hp_2000_hibernate (stdout, file, record, buf, len);
		break;
	      case hp_2000_mcp:
		dump_hp_2000_mcp (stdout, file, record, buf, len);
		break;
#endif /* HP_2000_SUPPORT */
	    default:
	      break;
	    }
	  dump (stdout, buf, len);
	  fflush (stdout);
	  filebytes += len;
	  record++;
	}
    }

  closetape (src);

  return (0);
}
