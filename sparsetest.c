/* (c) 2015 Flexiant Limited
 *
 * This file is released under the GPL v2
 *
 * See under usage() for a complete description of this program.
 */

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#define NUMCHARS 40


typedef enum
{ ASCENDING, DESCENDING, RANDOM } order_t;

char *orders[] = { "ascending", "descending", "random" };

static off_t blocksize = 512;
static off_t logicalsize = 1024 * 1024 * 1024;
static off_t writeevery = 1024 * 1024;
static order_t order = ASCENDING;
static int initialtruncate = 0;

void
usage ()
{
  fprintf (stderr, "\
sparsetest v0.01\n\
\n\
Usage:\n\
     sparsetest [OPTIONS] FILE\n\
\n\
Options:\n\
     -b, --blocksize SIZE   Use SIZE blocksize in bytes (default 512)\n\
     -s, --size SIZE        Use logical size SIZE\n\
     -w, --writeevery SIZE  Write something every SIZE\n\
     -d, --descending       Write in descending order\n\
     -r, --random           Write in random order\n\
     -i, --initialtruncate  Trunate at the start not the end\n\
     -h, --help             Display usage\n\
\n\
Sparsetest tests a file system's handling of sparse files. The destination path is\n\
overwritten with a sparse file of length specified with the -s parameter. Then,\n\
writes are made to the file at offsets specified by the -w parameter. Finally,\n\
the logical length of the file, and the usage on disk are both printed.\n\
\n\
SIZE can be specified in blocks (default), or use the following suffixes:\n\
     B  Bytes      (2^0  bytes)\n\
     K  Kilobytes  (2^10 bytes)\n\
     M  Megabytes  (2^20 bytes)\n\
     G  Gigabtytes (2^30 bytes)\n\
     T  Terabytes  (2^40 bytes)\n\
     P  Perabytes  (2^50 bytes)\n\
     E  Exabytes   (2^60 bytes)\n\
\n\
Note that blocksize=1024 will set blocksize to 1024 512byte blocks (use 1024B if this is not\n\
what you mean). Also note that disk capacity is often measured using decimal megabytes etc.;\n\
we do not adopt this convention for compatibility with dd.\n");
}

/*
 * getsize() returns the size of an argument that may have a sizing suffix
 */

off_t
getsize (char *arg)
{
  off_t param = 0;
  char *end, *found;
  const char *suffix = "bkmgtpe";
  param = strtoull (arg, &end, 10);

  if (*end == '\0')
    {
      param *= blocksize;
    }
  else if ((found = strchr (suffix, tolower (*end))))
    {
      param <<= (10 * (found - suffix));
    }
  else
    {
      fprintf (stderr, "sparsetest: Bad parameter\n");
      exit (1);
    }
  return param;
}


  /* First parse our options */
int
parse_command_line (int argc, char **argv)
{
  /* We can't evaluate these in the while() loop
   * As blocksize might not have been altered yet
   */
  char *writeevery_arg = NULL;
  char *blocksize_arg = NULL;
  char *logicalsize_arg = NULL;
  int dest = -1;

  while (1)
    {
      static struct option long_options[] = {
	{"help", no_argument, 0, 'h'},
	{"blocksize", required_argument, 0, 'b'},
	{"logicalsize", required_argument, 0, 's'},
	{"writeevery", required_argument, 0, 'w'},
	{"initialtruncate", no_argument, 0, 'i'},
	{"descending", no_argument, 0, 'd'},
	{"random", no_argument, 0, 'r'},
	{0, 0, 0, 0}
      };
      /* getopt_long stores the option index here. */
      int option_index = 0;
      int c;

      c = getopt_long (argc, argv, "hb:s:w:idr", long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
	break;

      switch (c)
	{
	case 0:
	  /* If this option set a flag, do nothing else now. */
	  break;

	case 'h':
	  usage ();
	  exit (0);
	  break;

	case 'b':
	  blocksize_arg = strdup (optarg);
	  break;

	case 's':
	  logicalsize_arg = strdup (optarg);
	  break;

	case 'w':
	  writeevery_arg = strdup (optarg);
	  break;

	case 'i':
	  initialtruncate = 1;
	  break;

	case 'd':
	  order = DESCENDING;
	  break;

	case 'r':
	  order = RANDOM;
	  break;

	default:
	  usage ();
	  exit (1);
	}
    }

  /* Do blocksize first */
  if (blocksize_arg)
    {
      blocksize = getsize (blocksize_arg);
      if ((blocksize <= 0) || (blocksize % sizeof (int)))
	{
	  fprintf (stderr, "sparsetest: Bad block size %lld\n",
		   (long long int) blocksize);
	  exit (1);
	}
      free (blocksize_arg);
    }

  if (logicalsize_arg)
    {
      logicalsize = getsize (logicalsize_arg);
      free (logicalsize_arg);
    }
  else if (logicalsize < blocksize * 4)
    logicalsize = blocksize * 4;

  if (logicalsize < blocksize)
    {
      fprintf (stderr,
	       "sparsetest: Bad final size %lld - cannot be less than blocksize %lld\n",
	       (long long int) logicalsize, (long long int) blocksize);
      exit (1);
    }

  if (writeevery_arg)
    {
      writeevery = getsize (writeevery_arg);
      free (writeevery_arg);
    }
  else if (writeevery < blocksize * 2)
    writeevery = blocksize * 2;

  if (writeevery < blocksize)
    {
      fprintf (stderr,
	       "sparsetest: Bad write-every %lld - cannot be less than blocksize %lld\n",
	       (long long int) writeevery, (long long int) blocksize);
      exit (1);
    }

  /* Check we have exactly 1 remaining parameter */
  if ((optind + 1) != argc)
    {
      usage ();
      exit (1);
    }

  if (-1 == (dest = open (argv[optind], O_RDWR | O_CREAT | O_TRUNC, 0666)))
    {
      perror ("open() Could not open destination file");
      exit (3);
    }
  return dest;
}

void
showlen (const char *label, off_t len)
{
  printf ("%23s: %15llu bytes; %15llu M; %15llu blocks of %llu bytes\n",
	  label,
	  (unsigned long long int) len,
	  (unsigned long long int) (len / (1024 * 1024)),
	  (unsigned long long int) (len / blocksize),
	  (unsigned long long int) blocksize);
}

void
shuffle (off_t * offsets, long long unsigned int n)
{
  long long unsigned int i;
  if (n <= 1)
    return;

  for (i = 0; i < n - 1; i++)
    {
      long long unsigned int j = i + random () / (RAND_MAX / (n - i) + 1);
      off_t t = offsets[j];
      offsets[j] = offsets[i];
      offsets[i] = t;
    }
}

void
reverse (off_t * offsets, long long unsigned int n)
{
  long long unsigned int i;
  if (n <= 1)
    return;

  for (i = 0; i < n / 2; i++)
    {
      long long unsigned int j = n - 1 - i;
      off_t t = offsets[j];
      offsets[j] = offsets[i];
      offsets[i] = t;
    }
}

int
main (int argc, char **argv)
{
  int *junk = NULL;
  off_t offset = 0;
  off_t *offsets = NULL;
  int dest = -1;
  off_t optimumpsize = 0;
  long long unsigned int count = 0;
  long long unsigned int offnum = 0;
  struct stat sb;

  srandom (time (NULL));

  dest = parse_command_line (argc, argv);

  if (!(junk = calloc (1, blocksize))
      || !(offsets = calloc (sizeof (off_t), logicalsize / writeevery + 2)))
    {
      perror ("calloc()/malloc() failed");
      exit (5);
    }

  if (initialtruncate && ftruncate (dest, logicalsize) < 0)
    {
      perror ("ftruncate() failed");
      exit (6);
    }


  for (offset = 0; offset + blocksize <= logicalsize; offset += writeevery)
    {
      optimumpsize += blocksize;
      offsets[count++] = offset;
    }

  switch (order)
    {
    case RANDOM:
      shuffle (offsets, count);
      break;
    case DESCENDING:
      reverse (offsets, count);
      break;
    default:
      break;
    }

  for (offnum = 0; offnum < count; offnum++)
    {
      int i;
      offset = offsets[offnum];
      for (i = 0; i < blocksize / sizeof (int); i++)
	junk[i] = (int) random ();

      if (pwrite (dest, junk, blocksize, offset) < blocksize)
	{
	  perror ("write(dest) failed");
	  exit (9);
	}
    }

  if (!initialtruncate && ftruncate (dest, logicalsize) < 0)
    {
      perror ("ftruncate() failed");
      exit (6);
    }

  if (-1 == fstat (dest, &sb))
    {
      perror ("stat failed");
      exit (9);
    }

  off_t finallsize = sb.st_size;
  off_t finalpsize = (off_t) (sb.st_blocks) * 512;

  if (logicalsize != finallsize)
    {
      fprintf (stderr,
	       "ERROR: final size (%lld) did not equal logical size requested (%lld)- something has gone wrong\n",
	       (long long int) finallsize, (long long int) logicalsize);
      exit (10);
    }

  printf ("Results:\n");
  showlen ("Intended logical size", logicalsize);
  showlen ("Optimum physical size", optimumpsize);
  showlen ("Actual physical size", finalpsize);
  printf ("\nUsed %llu writes of %llu bytes every %llu bytes in %s order\n", count,
	  (unsigned long long) blocksize, (unsigned long long) writeevery, orders[order]);
  printf ("Created %llu 512 byte blocks on disk\n",
	  (unsigned long long) sb.st_blocks);
  printf ("Density as %% of actual physical size over logical size: %f %%\n",
	  finalpsize * 100.0 / (1.0 * (logicalsize ? logicalsize : 1)));
  printf
    ("Efficiency as %% of optimum physical size over actual: %f %%\n",
     optimumpsize * 100.0 / (1.0 * (finalpsize ? finalpsize : 1)));

  close (dest);
  free (junk);
  exit (0);
}
