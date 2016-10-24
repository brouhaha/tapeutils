/*
  Routines to do magtape I/O to local tapes, remote tapes, and tape image
  files.

  Copyright 1998, 1999 John Wilson and Eric Smith

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


typedef struct mtape_t *tape_handle;  /* opaque type */


/* open a tape drive */
tape_handle opentape (char *name, int create, int writable);

/* close a tape drive */
void closetape (tape_handle h);

/* rewind tape */
void posnbot (tape_handle h);

/* position tape at EOT (between the two tape marks) */
void posneot (tape_handle h);

/* read a tape record, return actual length (0=tape mark) */
int getrec (tape_handle h, char *buf, int len);

/* write a tape record */
void putrec (tape_handle h, char *buf, int len);

/* write a tape mark */
void tapemark (tape_handle h);

/* skip records (negative for reverse) */
void skiprec (tape_handle h, int count);

/* skip files (negative for reverse) */
void skipfile (tape_handle h, int count);

