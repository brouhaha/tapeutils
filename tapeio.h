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
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


typedef struct mtape_t *tape_handle_t;  /* opaque type */


/* tape flags */
#define TF_DEFAULT	0x000
#define TF_SIMH		0x001


/* open a tape drive */
tape_handle_t opentape (char *name, int create, int writable);

/* close a tape drive */
void closetape (tape_handle_t h);

/* rewind tape */
void posnbot (tape_handle_t h);

/* position tape at EOT (between the two tape marks) */
void posneot (tape_handle_t h);

/* read a tape record, return actual length (0=tape mark) */
int getrec (tape_handle_t h, void *buf, int len);

/* write a tape record */
void putrec (tape_handle_t h, void *buf, int len);

/* write a tape mark */
void tapemark (tape_handle_t h);

/* skip records (negative for reverse) */
void skiprec (tape_handle_t h, int count);

/* skip files (negative for reverse) */
void skipfile (tape_handle_t h, int count);

/* set tape flags */
void tapeflags (tape_handle_t h, int flags);
