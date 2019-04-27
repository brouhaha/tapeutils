/*
 *  Program to read Tops-20 Dumper format tapes
 *
 *                   Jim Guyton,  Rand Corporation
 *                   Original       10/20/82
 *                   jdg:   -n added 6/11/83
 *                   jdg:    can now extract 8-bit-byte files  2/9/86
 *
 * Lot of mods by Jay Lepreau, Univ of Utah, 1-2/87.
 * See the RCS log for details.
 *
 * Modified by Eric Smith <eric@brouhaha.com>, 4-DEC-2000, to use
 * tapeio library, in order to use tape image files.  Also changed
 * to use environment variable TAPE if set, otherwise default to /dev/nst0.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define _REGEX_RE_COMP
#include <regex.h>

#include "dumper.h"
#include "tapeio.h"

#define LOGFILE "Logfile"		/* logfile should be changeable */


tape_handle_t tape_handle;

char tapeblocka[TAPEBLK];        /* One logical record from tape */
FILE *fpFile;                   /* Output file handle on extracts */
int debug = 0;
int textflg = 0;                /* Non-zero if retr binary files as text */
int numflg = 0;                 /* Non-zero if using numeric filenames */
int keepcr = 0;			/* Keep CR's in CRLF pairs in text files */
int dodir = 0;				/* directory listing */
int xflg = 0;				/* extract */
int verbose = 0;
int genflg;				/* keep generation number */
int nselect;		/* number of files still to be selected by number */
int doallflag;		/* act on all files cause no args given */

int number;                     /* Current output file "number" */

#define TAPE "/dev/nst0"        /* Default input tape */

int  bytesize;          /* Number of bits/byte in current file */
long  numbytes;          /* Number of bytes in current file */
int  pgcount;           /* Number of twenex pages in file */
long pageno, tapeno, ssno, filenum;

unsigned tprot;		/* Tops-20 protection */
char *timeptr;

struct utimbuf timep;

int offline, archived, invisible;
int apgcount, tp1, tp2, ss1, ss2, tf1, tf2;

char topsname[130];
char sunixname[300];

struct want {
     unsigned short ssnum;
     unsigned short fnum;
} want[10000];				/* limited by 20000 char arglist */

int cursswant;

char **patterns = 0;     /* Filename match patterns */
int numpats = 0;         /* Number of patterns */
char *expression = 0;
char *re_comp_error;     /* Error message from re_comp() */
extern char *re_comp();

#if defined(__APPLE__) || defined(__OpenBSD__)
static regex_t re_regexp;

char *re_comp(char *s)
{
  if (regcomp(&re_regexp, s, 0) == 0)
    return NULL;
  else
    return "error";
}

static int re_exec(char *s)
{
  return regexec(&re_regexp, s, 0, 0, 0);
}
#endif

/*
	read20  [-f tapefile] [-t] [-c] [-T] [-n number] pattern

	no tapefile ==> /dev/rmt8
	-t == directory listing
	-n == use numeric filenames in extracts, number is 1st name
	-T == pretend 36 bit files are 7-bit ascii
	-c == keep CR's in CRLF pairs.
	-g == keep generation numbers
*/



void punt (int prterrno, char *fmt, ...)
{
    va_list ap;

    va_start (ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end (ap);
    if (prterrno) {
	fprintf(stderr, ": %s\n", strerror(errno));
    }
    else
	fprintf(stderr, "\n");
    exit(1);
}


/*  fold  --  perform case folding
 *
 *  Usage:  p = fold (out,in,whichway);
 *	    p = foldup (out,in);
 *	    p = folddown (out,in);
 *	char *p,*in,*out;
 *	enum {FOLDUP, FOLDDOWN} whichway;
 *
 *  Fold performs case-folding, moving string "in" to
 *  "out" and folding one case to another en route.
 *  Folding may be upper-to-lower case (folddown) or
 *  lower-to-upper case.
 *  Foldup folds to upper case; folddown folds to lower case.
 *  The same string may be specified as both "in" and "out".
 *  The address of "out" is returned for convenience.
 *
 *  HISTORY
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Rewritten for VAX; now uses enumerated type for fold().  The
 *	foldup() and folddown() routines are new.
 *
 */

typedef enum {
	FOLDUP, FOLDDOWN} 
FOLDMODE;

char *fold (char *out, char *in, FOLDMODE whichway)
{
	register char *i,*o;
	register char lower;
	char upper;
	int delta;

	switch (whichway)
	{
	case FOLDUP:
		lower = 'a';		/* lower bound of range to change */
		upper = 'z';		/* upper bound of range */
		delta = 'A' - 'a';	/* amount of change */
		break;
	case FOLDDOWN:
	default:
		lower = 'A';
		upper = 'Z';
		delta = 'a' - 'A';
	}

	i = in;
	o = out;
	do {
		if (*i >= lower && *i <= upper)		*o++ = *i++ + delta;
		else					*o++ = *i++;
	} 
	while (*i);
	*o = '\0';
	return (out);
}

char *foldup (char *out, char *in)
{
	return (fold(out,in,FOLDUP));
}

char *folddown (char *out, char *in)
{
	return (fold(out,in,FOLDDOWN));
}



int   masks[32] =       /* bitmasks for different length fields */
{              0x00000001, 0x00000003, 0x00000007,
   0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
   0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
   0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
   0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
   0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
   0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
   0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
   0xffffffff
};

long getfield (char *block,  /* Tape block record */
	       int wordoff,  /* 36-bit word offset */
	       int bitoff,   /* Bit offset of field (from msb) */
	       int bitlen)   /* Bit length of field */
{
	register char *p;                /* Used to point into record */
	register long w32;           /* First 32 bits of the 36 bit word */
	int   w4;               /* Last 4 bits of the 36 bit word */
	long  w = 0;            /* the word to return */

				/* First, the "illegal" kludge */
	if (bitoff == 0 && bitlen == 36) {
		bitoff = 4;
		bitlen = 32;
	}
	if (bitlen > 32)
		punt(0, "Can't get that large a field = %d!", bitlen);

	/* A PDP-10 (or 20) 36-bit word is laid out with the first 32 bits
	   as the first 4 bytes and the last 4 bits are the low order 4 bits
	   of the 5th byte.   The high 4 bits of that byte should be zero */

	p = block + (5*wordoff);        /* Get ptr to word of interest */
	w32 = *p++ & 0377;                      /* First byte */
	w32 = (w32 << 8) | (*p++ & 0377);       /* 2nd */
	w32 = (w32 << 8) | (*p++ & 0377);       /* 3rd */
	w32 = (w32 << 8) | (*p++ & 0377);       /* 4th */
	w4  = *p;                               /* 5th */
	if (w4 > 017)
		punt(0, "Not a PDP-10 tape!  w4 = octal %o", w4);


	/* Get the field right justified in the word "w".
	   There are three cases that I have to handle:
	      [1] field is contained in w32
	      [2] field crosses w32 and w4
	      [3] field is contained in w4
	*/

	if (bitoff+bitlen <= 32)        /* [1] field is contained in w32 */
	{
		w = w32 >> (32 - (bitoff+bitlen));
	}
	else if (bitoff <= 32)          /* [2] field crosses boundary */
	{
		w =  (w32 << (bitoff+bitlen-32))
		   | (w4  >> (36 - (bitoff+bitlen)));
	}
	else                            /* [3] field is contained in w4 */
	{
		w = w4 >> (36 - (bitoff+bitlen));
	}
	w = w & masks[bitlen-1];          /* Trim to proper size */
	return(w);
}


int lastc = 0;

/*
 * Unpack into buffer 's' the 7 bit string stored in 'block' and
 * append a null char.  Optionally strip CR's from CRLF pairs.  'max'
 * is the max number of 7-bit chars to unpack from 'block', not the
 * max to put into 's' (that's important!).  This only works if
 * getstring() is always called with 'max' mod 5 == 0, except for the
 * last call on "contiguous" blocks.
 * Returns number of chars stored in output buffer.
 */
int getstring (char *block,  /* Tape block */
	       char *s       /* Destination string buffer */,
	       int wordoff,  /* 36-bit offset from start of tape block */
	       int max)      /* Max number of characters to xfer into s */
{
	register int i;         /* Counter for five characters per word */
	int ct = 0;             /* Number of characters loaded so far */
	char *orig = s;         /* Save for debugging */
	int c;

	while (ct < max)
	{
		for (i = 0; i < 5; i++)
		{
			c = getfield(block, wordoff, i*7, 7);
			if (lastc == '\r' && c != '\n')
				*s++ = '\r';
			if (c != '\r' || keepcr)
				*s++ = c;
			if (!keepcr)
				lastc = c;
			if ((ct + i + 1) == max)
				return (s - orig);
		}
		wordoff++;
		ct += 5;
	}
	printf("Fall thru in getfield\n");
	fflush(stdout);
	*s = '\0';
	return (s - orig);
}


/*
 * pendstring - return any character pending output after
 * last call to getstring().  Also zeros `lastc'.
 * Can only return '\r' or 0 for none.
 */
int pendstring (void)
{
	int olastc = lastc;

	lastc = 0;
	return (olastc == '\r') ? '\r' : 0;
}	


/* getbytes: like getstring, but ...
   1) uses 8 bit bytes
   2) doesn't stop on a zero
*/
void getbytes (char *block,  /* Tape block */
	       char *s,      /* Destination string buffer */
	       int wordoff,  /* 36-bit offset from start of tape block */
	       int max)      /* Max number of characters to xfer into s */
{
	register int i;         /* Counter for five characters per word */

	int ct = 0;             /* Number of characters loaded so far */
	/* char *orig = s; */         /* Save for debugging */

	while (ct < max)
	{
		for (i = 0; i < 4; i++)
		{
			*s = getfield(block, wordoff, i*8, 8);
		   /*   if (*s == 0) return;    */
			s++;
		}
		wordoff++;
		ct += 4;
	}
   /**     punt(0, "String greater than %d characters.", max);   **/
}


#define SecPerTick  (24.*60.*60.)/0777777
#define DayBaseDelta 0117213            /* Unix day 0 in Tenex format */

/*
 * This screws up on some of the atime's we see, including, yielding, e.g.
 * Fri Dec 23 23:28:16 1994
 * Fri Dec 23 23:28:16 1994
 * Tue Jan 13 07:57:03 1987
 */
long unixtime(char *block, int wordoff)
{
	long int t, s;

	t = getfield(block, wordoff, 0, 18);    /* First half is day */
	t -= DayBaseDelta;                      /* Switch to unix base */
						/* Now has # days since */
						/* Jan 1, 1970 */

	s = getfield(block, wordoff, 18, 18);   /* 2nd half is fraction day */
	s = s * SecPerTick;                     /* Turn into seconds */

	s += t*24*60*60;                        /* Add day base */
	return(s);
}


char *unixname (char *name)
{
	static FILE *log = NULL;
	register char *t, *p;
	static char lastdir[64];
	struct stat stb;
	int mask;
	register int newdir = 0;

	if (numflg) {             /* If numeric filenames */
		if (log == NULL) log = fopen(LOGFILE, "a");
		fprintf(log, "%d is %s\n", number, name);
		sprintf(sunixname, "%d", number++);
		return(sunixname);
	}

	strcpy(sunixname, index(name, '<') + 1); /* trim off device */
	t = rindex(sunixname, '>');        	 /* find end of directory */
	*t = '.';

	if (strncmp(lastdir, sunixname, t - sunixname)) {/* maybe new dir */
	    strncpy(lastdir, sunixname, t - sunixname);	/* remember it */
	    newdir = 1;
	}
	for (p = sunixname; p <= t; p++)
	    if (*p == '.') {
		if (newdir) {
		    *p = '\0';			/* temporarily null it off */
		    if (stat(sunixname, &stb) < 0) {
			mask = umask(2);
			if (mkdir(sunixname, 0777) < 0)
			    punt(1, "mkdir %s failed", sunixname);
			umask(mask);
		    }
		}
		*p = '/';
	    }
	
	if (!genflg) {
		t = rindex(sunixname, '.');	/* find last . */
		*t = 0;				/* zap it out */
	}
	return(sunixname);
}


void doDatablock (char *block)
{
					    /* max is 5 bytes per word */
	static char buf[(512*5)+1];         /* A page of characters */
	int ct;
	int maxperblock;
	int nout;

	if (debug > 10)
		printf("*");
	if (fpFile == NULL)
		return;
					
	switch (bytesize) {		/* only handle 7 and 8 bit bytes */
	   case 7:      maxperblock = 512*5; break;
	   case 8:      maxperblock = 512*4; break;
	   default:     return;
	}

	if (numbytes > maxperblock)
		ct = maxperblock;
	else
		ct = numbytes;

	if (bytesize == 7) {
		nout = getstring(block, buf, 6, ct);
		fwrite(buf, 1, nout, fpFile);
	}
	else {          /* if not 7, then 8bit */
		getbytes(block, buf, 6, ct);
		fwrite(buf, 1, ct, fpFile);
	}
	if (ferror(fpFile))
		punt(1, "Error writing %s", sunixname);
	numbytes -= ct;
}


void doSaveset (char *block, int contflag)
{
	static char name[102];
	static char ss[2];
	long t;

	if (debug > 10) printf("\nSaveset header:");
	tapeno = getfield(block, WdoffTapeNum, BtoffTapeNum, BtlenTapeNum);
	ssno = getfield(block, WdoffSaveSetNum, BtoffSaveSetNum,
	    BtlenSaveSetNum);
	getstring(block, name, WdoffSSName, sizeof(name));
	ss[0] = pendstring();		/* superfluous */
	(void) strcat(name, ss);
	
	t = unixtime(block, WdoffSSDate);
	if (dodir || verbose)
		printf("%sSaveset '%s' %s\n", contflag ? "Continued " : "",
		    name, ctime(&t));

}


/* Return 1 if topsname matches any of the "extraction" strings. */
int patternmatch (void)
{
	register int i;

	for (i = 0; i < numpats; i++)
		if (strstr(topsname, patterns[i]))
			return (1);
	return (0);
}



/* Return 1 if topsname matches the regular expression. */
int expmatch (void)
{
	register int match;

	if (expression) {
		if ((match = re_exec(topsname)) == -1)
			punt(0, "re_exec: internal error on %s", topsname);
		else
			return (match);
	}
	return (0);
}


/* Return 1 if current file number matches one selected by arg line. */
int fmatch (void)
{
    static int widx;
    while (want[widx].ssnum < ssno)
	widx++;
    if (want[widx].ssnum > ssno)
	return 0;
    while (want[widx].fnum < filenum)
	widx++;
    if (want[widx].fnum > filenum)
	return 0;
    return 1;
}


/*
 * Sets a bunch of global variables to info from the fdb.
 * For some reason the archive tape info is garbage.
 */
void getfdbinfo (char *block)
{
	
	timep.modtime = unixtime(block, WdoffFDB_Wrt);
	timep.actime = unixtime(block, WdoffFDB_Ref);
	timeptr = ctime(& timep.modtime) + 4;	/* Skip over day-name field */
	timeptr[20] = '\0';			/* Chop off \n at end */

	bytesize = getfield(block, WdoffFDB_BSZ, BtoffFDB_BSZ, BtlenFDB_BSZ);
	numbytes = getfield(block, WdoffFDB_Size, BtoffFDB_Size,BtlenFDB_Size);
	pgcount  = getfield(block, WdoffFDB_PGC, BtoffFDB_PGC, BtlenFDB_PGC);
	tprot = getfield(block, WdoffFDB_PRT, BtoffFDB_PRT, BtlenFDB_PRT);

	archived = getfield(block, WdoffFDB_CTL, BtoffFDB_Arc, BtlenFDB_Arc);
	invisible = getfield(block, WdoffFDB_CTL, BtoffFDB_Inv, BtlenFDB_Inv);
	offline = getfield(block, WdoffFDB_CTL, BtoffFDB_Off, BtlenFDB_Off);
	apgcount = getfield(block, WdoffFDB_PGC_A, BtoffFDB_PGC, BtlenFDB_PGC);
	/* The rest is bogus. */
	tp1 = getfield(block, WdoffFDB_TP1, 0, 36);
	tp2 = getfield(block, WdoffFDB_TP2, 0, 36);
	ss1 = getfield(block, WdoffFDB_SS1, BtoffFDB_SS, BtlenFDB_SS);
	ss2 = getfield(block, WdoffFDB_SS2, BtoffFDB_SS, BtlenFDB_SS);
	tf1 = getfield(block, WdoffFDB_TF1, BtoffFDB_TF, BtlenFDB_TF);
	tf2 = getfield(block, WdoffFDB_TF2, BtoffFDB_TF, BtlenFDB_TF);
}


int t2uprot (unsigned int prot)
{
    register unsigned tprot, uprot;
    register int tshift;

#ifdef notdef
    if (f->FB_dir) {			/* THIS WON'T WORK! */
    	punt(0, "Can't handle directory %s", topsname);
	prot = gtdirprot(_dirnm(jfn));	/* returns 20 fmt protection */
	for (tshift=12, uprot=0; tshift >= 0; tshift -= 6) {
	    tprot = prot >> tshift;	/* pick up next field */
	    uprot <<= 3;
	    if (tprot & DP_rd)
		uprot |= WREAD|WEXEC;	/* world read, world execute */
	    if (tprot & (DP_cn|DP_cf))	/* allow write for either conn. */
		uprot |= WWRITE;	/*   access or add files access */
	}
    }
    else
#endif
    {  /* do it this way so easily modified-- i know it could be faster */
	for (tshift=12, uprot=0; tshift >= 0; tshift -= 6) {
	    tprot = prot >> tshift;
	    uprot <<= 3;
	    uprot |= (tprot >> 3) & 07;		/* just r,w,x */
	}
    }
    return uprot;
}


void doFileHeader (char *block)
{
    char *ts;
    static char prt_ar[2] = {'-', 'A'};
    static char prt_inv[2] = {'-', 'I'};
    static char prt_off[2] = {'-', 'O'};

    if (debug > 5)
	printf("File Header block:\n");

    filenum = getfield(block, WdoffFileNum, BtoffFileNum, BtlenFileNum);
    getstring(block, topsname, WdoffFLName, sizeof(topsname));
    ts = index(topsname, ';');		/* Chop off ;Pprotection;Aacct */
    *ts++ = pendstring();		/* superfluous */
    *ts = 0;
    folddown(topsname, topsname);

    fpFile = NULL;

    if ( doallflag ||
         (patterns && patternmatch()) ||
	 (expression && expmatch()) ||
	 (nselect && fmatch()) ) {
	getfdbinfo(block);
	pageno = getfield(block, WdoffPageNum, BtoffPageNum, BtlenPageNum);

	if (dodir || verbose) {
	    if (verbose)
		printf("%3ld%6ld ", ssno, filenum);
	    printf("%c%c%c", prt_ar[archived], prt_off[offline],
	      prt_inv[invisible]);
	    printf("%5d%9ld %2d %o %s %s", offline ? apgcount : pgcount,
	      numbytes, bytesize, tprot, timeptr, topsname);
	    if (archived && verbose >= 2)
		printf(" %x%4d%5d %x%4d%5d", tp1, ss1, tf1, tp2, ss2, tf2);
	    if (pageno != 0)
		printf(" Split file, part 2");
	}

	if (xflg) {
	    /* Special hack for bad files */
	    if (textflg && bytesize != 7) {
		if (bytesize == 0 || bytesize == 36) {
		    bytesize = 7;
		    numbytes *= 5;
		}
	    }
	    if ((bytesize == 7 || bytesize == 8) && !offline) {
		if (pageno != 0) {	/* continued file */
		    int missing = pageno * 512 * (bytesize == 7 ? 5 : 4);

		    numbytes -= missing;
		    if (!(dodir || verbose))
		    	printf("%s: Split file, part 2", topsname);
		    printf(": %d raw bytes missing.", missing);
		    if (!(dodir || verbose))
			putchar('\n');
		}
		fpFile = fopen(unixname(topsname), "w");
		if (fpFile == NULL)
		    punt(1, "Can't open %s for write", sunixname);
		else if (verbose)
		    printf(" Extracted.");
		if (fchmod(fileno(fpFile), t2uprot(tprot) & ~0111) < 0)
		    punt(1, "fchmod on %s", sunixname);
	    } else if (verbose)
		printf(" Skipping -- %s file.",
		  offline ? "offline" : "binary");
	}

	if (dodir || verbose)
	    putchar('\n');
    }
}


/*ARGSUSED*/
void doFileTrailer (char *block)
{
	if (debug > 10) printf(" File trailer\n");
	if (fpFile != NULL) {
		if (pendstring() == '\r')
			putc('\r', fpFile);
		if (fclose(fpFile) == EOF)
			punt(1, "fclose: write error on %s", sunixname);
		fpFile = NULL;
		utime(sunixname, & timep);
		if (numbytes != 0)
			printf("%s: Split file, part 1: %ld raw bytes left\n",
			    topsname, numbytes);
	}
}


/*ARGSUSED*/
void doTapeTrailer (char *block)
{
	if (debug > 10) printf("Tape Trailer");
}


int compwant(const void *wa1, const void *wa2)
{
  const struct want *w1 = wa1;
  const struct want *w2 = wa2;
    int sdif;

    if ((sdif = w1->ssnum - w2->ssnum))
	return sdif;
    return (w1->fnum - w2->fnum);
}


int main (int argc, char *argv[])
{
	char *tape;              /* Pathname for tape device/file */
	char	*tapeblock;
	int rc;
	int rtype;

	if (! (tape = getenv ("TAPE")))
	  tape = TAPE;

	/* Do switch parsing */

	while(argc>1 && argv[1][0] == '-'){
		switch(argv[1][1]){
		case 'f':
			if (argc <= 2)
				punt(0, "Need filename after -f");
			tape = argv[2];
			argc--; argv++;
			break;
		case 'T':             /* Force text mode on "binary" files */
			textflg = 1;
			break;
		case 't':             /* directory listing */
			dodir = 1;
			break;
		case 'x':             /* extract */
			xflg = 1;
			break;
		case 'v':             /* verbosity */
			verbose++;
			break;
		case 'g':             /* keep gen number */
			genflg++;
			break;
		case 'd':
			debug = atoi(&argv[1][2]);
			fprintf(stderr, "Debug value set to %d\n", debug);
			break;
		case 'n':               /* numeric output filenames */
			if (argc <= 2)
				punt(0, "Need number after -n");
			number = atoi(argv[2]);         /* First file name */
			numflg = 1;
			argc--; argv++;
			break;
		case 'c':		/* keep CR`s in CR/LF pairs */
			keepcr++;
			break;
		case 'e':               /* regular expression */
			if (argc <= 2)
				punt(0, "Need expression after -e");
			if (expression)
				punt(0, "Only one regexp allowed");
			expression = argv[2];
			if ((re_comp_error = re_comp(expression)) != 0)
				punt(0, "re_comp: %s", re_comp_error);
			argc--; argv++;
			break;
		case 'S':		/* selected save set number */
			if (argc <= 2)
				punt(0, "Need save set number after -S");
			cursswant = atoi(argv[2]);
			argc--; argv++;
			break;
		case 'F':		/* selected file numbers */
			if (argc <= 2)
				punt(0, "Need file number(s) after -F");
			for (argc -= 2, argv += 2;
			  argc && isdigit(**argv);
			  argc--, argv++, nselect++) {
			      want[nselect].ssnum = cursswant;
			      want[nselect].fnum = atoi(*argv);
			}
			argc += 2; argv -= 2;
			break;
		default:
			punt(0, "unknown flag %s", argv[1]);
		}
		argc--;  argv++;
	}

	if (!xflg && !dodir)
		punt(0, "Need either '-x' or '-t' option.");

	if (argc > 1) {
		patterns = &argv[1];
		numpats = argc - 1;
	}
	doallflag = !(patterns || expression || nselect);
	if (nselect)
		qsort((char *)want, nselect, sizeof (struct want), compwant);

	tape_handle = opentape (tape, 0, 0);
	if (! tape_handle)
		punt(1, "Can't open tape '%s'", tape);

	rc = 0;
	for ( ; ; )             /* Loop till end of tape */
	{
					 /*** Read a block ***/
		if (rc == 0) {
		        rc = getrec (tape_handle, tapeblocka, TAPEBLK);
			if (debug > 99)
				printf("rc=%d\n", rc);
			if ((rc % (518*5)) != 0) {
				if (rc != 0)
				punt(1, "Oops.  Read block len = %d", rc);
			}
			if (rc == 0) {
				if (verbose)
					printf("\nEnd of tape.\n");
				exit(0);        /* Normal exit */
			}
			tapeblock = tapeblocka;
			rc = rc - 518*5;
		}
		else {
			tapeblock = tapeblock + 518*5;
			rc = rc - 518*5;
		}

					/*** Do something with it ***/
		switch(rtype = -getfield(tapeblock,
		    WdoffRectype, BtoffRectype, BtlenRectype))
		{
		  case RectypeData:             /* Data block */
			doDatablock(tapeblock);
			break;

		  case RectypeTphd:             /* Saveset header */
			doSaveset(tapeblock, 0);
			break;

		  case RectypeFlhd:             /* File header */
			doFileHeader(tapeblock);
			break;

		  case RectypeFltr:             /* File trailer */
			doFileTrailer(tapeblock);
			break;

		  case RectypeTptr:             /* Tape trailer */
			doTapeTrailer(tapeblock);
			break;

		  case RectypeUsr:              /* User directory info ? */
  			if (verbose >= 3)
				fprintf(stderr, "Directory record skipped\n");
			break;

		  case RectypeCtph:             /* Continued saveset hdr */
			doSaveset(tapeblock, 1);
			break;

		  case RectypeFill:             /* Fill record */
		  	if (verbose >= 3)
				fprintf(stderr, "Fill record skipped\n");
			break;

		  default:
			punt(0, "Unknown record type 0x%x", rtype);
			break;
		}
	}
}



#ifdef notdef
#define HOUR 3600
#define DAY  (HOUR*24)
#define DAY0 40587     /* number of days between tops20 0 day and Unix 0 day */

#define makeword(l, r)	( ((l) << 18) | (r) )
#define getright(b)	( (b) & 0777777 )
#define getleft(b)	( (b) >> 18 )

/* Convert Tops-20 to Unix time -- curently incomplete due to 32 < 36 bits */
int
_t2utim(t)
unsigned t;
{
    register ticks, rh, secs;

    ticks = t - makeword(DAY0, 0);
    rh = getright(ticks) * DAY;
    secs = rh >> 18;
    if (rh % makeword(1,0) > 0400000)
	secs++;				/* round up */
    return (getleft(ticks) * DAY) + secs;
}
#endif

