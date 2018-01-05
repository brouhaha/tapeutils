#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

typedef enum {
  HIGH_DENSITY = 1,
  CORE_DUMP,
  ANSI_ASCII,
  SIXBIT
} format_t;

static int bits_per_word[] = { 0, 36, 40, 40, 48 };

static int input_recsize = -1;
static int input_double = -1;
static int input_pad = -1;
static int input_word = HIGH_DENSITY;

static int output_recsize = -1;
static int output_double = -1;
static int output_pad = -1;
static int output_word = HIGH_DENSITY;

static FILE *in;
static int inbuf;
static int inbits = 0;
static int datasize;

static FILE *out;
static int outbuf;
static int outbits = 0;

static void warning (const char *message)
{
  fprintf (stderr, "WARNING: %s\n", message);
}

static void fatal (const char *message)
{
  fprintf (stderr, "ERROR: %s\n", message);
  exit (1);
}

static void verbose (const char *message)
{
  if (0)
    fprintf (stderr, "%s\n", message);
}

static size_t read_recsize (void)
{
  char data[4];
  size_t n, s = 0;
  int i;

  n = fread (data, 1, input_recsize, in);
  if (n != input_recsize)
    fatal ("read");

  for (i = 0; i < input_recsize; i++)
    s += (data[i] << (8 * i));

  fprintf (stderr, "read_recsize: %lu\n", s);
  return s;
}

static void write_recsize (size_t s)
{
  char data[4];
  size_t n;
  int i;

  fprintf (stderr, "write_recsize: %lu\n", s);

  for (i = 0; i < output_recsize; i++) {
    data[i] = (s & 0xFF);
    s >>= 8;
  }

  n = fwrite (data, 1, output_recsize, out);
  if (n != output_recsize)
    fatal ("write");
}

static int read_bits (int n)
{
  unsigned char data;
  int word;

  while (n > inbits) {
    if (fread (&data, 1, 1, in) != 1)
      fatal ("read error");
    fprintf (stderr, "read_bits: data = %x\n", data);
    datasize++;
    inbuf <<= 8;
    inbuf += data;
    inbits += 8;
  }

  fprintf (stderr, "read_bits: %d (%d)\n", n, datasize);
  fprintf (stderr, "read_bits: inbuf = %x, inbits = %d\n", inbuf, inbits);

  word = (inbuf >> (inbits - n));
  fprintf (stderr, "read_bits: bits = %x\n", word);
  inbuf >>= n;
  inbits -= n;
  return word;
}

static void read_zeros (int n)
{
  int data = read_bits (n);
  if (data != 0)
    warning ("not zero");
}

static long long read_word (void)
{
  long long word = 0;
  int x;
  int i;

  fprintf (stderr, "read_word: start\n");

  switch (input_word) {
  case HIGH_DENSITY:
    for (i = 0; i < 4; i++)
      word = (word << 9) + read_bits (9);
    break;
  case CORE_DUMP:
    for (i = 0; i < 4; i++)
      word = (word << 8) + read_bits (8);
    read_zeros (4);
    word = (word << 4) + read_bits (4);
    break;
  case ANSI_ASCII:
    for (i = 0; i < 4; i++) {
      read_zeros (1);
      word = (word << 7) + read_bits (7);
    }
    x = read_bits (1);
    word = (word << 7) + read_bits (7);
    word = (word << 1) + x;
    break;
  case SIXBIT:
    for (i = 0; i < 6; i++) {
      read_zeros (2);
      word = (word << 6) + read_bits (6);
    }
    break;
  default:
    fatal ("bad format");
  }

  fprintf (stderr, "read_word: end %012llo\n", word);

  return word;
}

static void write_bits (int n, unsigned int word)
{
  char data;  

  word &= (1 << n) - 1;

  outbuf <<= n;
  outbuf += word;
  outbits += n;

  fprintf (stderr, "write_bits: %d\n", n);
  while (outbits > 7) {
    data = (outbuf >> (outbits - 8));
    if (fwrite (&data, 1, 1, out) != 1)
      fatal ("read error");
    outbuf >>= 8;
    outbits -= 8;
    fprintf (stderr, "write_bits: octet\n");
  }
  fprintf (stderr, "write_bits: %d remaining\n", outbits);
}

static void write_zeros (int n)
{
  write_bits (n, 0);
}

static void write_word (long long word)
{
  int i;

  switch (output_word) {
  case HIGH_DENSITY:
    for (i = 0; i < 4; i++)
      write_bits (9, word >> ((3-i) * 9));
    break;
  case CORE_DUMP:
    for (i = 0; i < 4; i++)
      write_bits (8, word >> ((3-i) * 8));
    write_zeros (4);
    write_bits (4, word);
    break;
  case ANSI_ASCII:
    for (i = 0; i < 4; i++) {
      write_zeros (1);
      write_bits (7, word >> ((3-i) * 7));
    }
    write_bits (1, word);
    write_bits (7, word >> 1);
    break;
  case SIXBIT:
    for (i = 0; i < 6; i++) {
      write_zeros (2);
      write_bits (7, word >> ((5-i) * 6));
    }
    break;
  default:
    fatal ("bad format");
  }
}

static void copy_data (size_t n)
{
  long long word;

  datasize = 0;

  while (datasize < n) {
    word = read_word ();
    write_word (word);
  }

  fprintf (stderr, "copy_data: %d octets read\n", datasize);
  if (outbits > 0)
    write_zeros (8 - outbits);
}

static size_t copy_record (void)
{
  size_t n1, n2, n3;

  n1 = read_recsize ();
  n2 = (n1 * bits_per_word[output_word]) / bits_per_word[input_word];
  write_recsize (n2);

  copy_data (n1);

  if ((n1 & 1) != 0 && input_pad)
    fgetc (in);

  if (input_double) {
    n3 = read_recsize ();
    if (n1 != n3) {
      fprintf (stderr, "%lu != %lu\n", n1, n3);
      fatal ("recsize mismatch");
    }
  }
    
  if ((n2 & 1) != 0 && output_pad)
    fputc (0, in);

  if (output_double)
    write_recsize (n2);

  return n1;
}

int main (int argc, char **argv)
{
  size_t n, filesize;

  in = stdin;
  out = stdout;

  input_word = CORE_DUMP;
  input_recsize = 4;
  input_double = 1;
  input_pad = 0;

  output_word = CORE_DUMP;
  output_recsize = 4;
  output_double = 1;
  output_pad = 1;

  for (filesize = 0;;) {
    n = copy_record ();
    fprintf (stderr, "record: %lu\n", n);
    filesize += n;
    if (n == 0) {
      fprintf (stderr, "file: %lu\n", filesize);
      if (filesize == 0)
	return 0;
      filesize = 0;
    }      
  }
}
