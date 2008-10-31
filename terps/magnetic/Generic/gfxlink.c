#define PRGNAME "gfxlink"
#define PRGVERSION "1.5"

/*
   gfxlink.c

   - combines Magnetic Scrolls picture files (Amiga format)
     to gfxfile (compatible with the MAG interpreter)
   - does not currently work with Collection & Wonderland pictures

   Author: Paul David Doherty <h0142kdd@rz.hu-berlin.de>

   v1.0:  12 Feb 1997
   v1.1:  17 Apr 1997  Amigafied & cleverised
   v1.2:  19 Apr 1997  itoa() replaced
   v1.3:  22 Apr 1997  cleaned up; name changed to 'gfxlink'
   v1.4:   8 May 1997  might even work under UNIX now
   v1.5:  25 Nov 1997  UNIX fixes supplied by Robert Bihlmeyer
 */

#if defined(AZTEC_C) || defined(LATTICE)
#define AMIGA
#endif

#define FILENAMELENGTH 256
#define MAXPICTURES 35

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#ifdef __TURBOC__
#include <io.h>
#include <sys\stat.h>
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#endif
#ifdef __GNUC__
#include <sys/stat.h>
#endif
#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#endif

#if defined(__TURBOC__) || defined(__GNUC__) && !defined(__unix__)
#define O_SCRIVERE O_WRONLY|O_BINARY
#define O_LEGGERE O_RDONLY|O_BINARY
#else
#define O_SCRIVERE O_WRONLY
#define O_LEGGERE O_RDONLY
#endif

#define TRUE 1
#define FALSE 0

#ifdef __TURBOC__
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

typedef unsigned char type8;
typedef unsigned short type16;
typedef unsigned long int type32;

int fdi, fdo_temp, fdo_gfx;
char infilemask[FILENAMELENGTH];
char infile[FILENAMELENGTH];
char gfxfile[FILENAMELENGTH];
char picnum[5];

#ifdef AMIGA
#define TEMPFILE "ram:pics.tmp"
#else
#define TEMPFILE "pics.tmp"
#endif

char *magic = "MaPi";

type8 inputbuf[256];
type8 endfile_reached = FALSE;
type8 offsetcounter = 1;
type8 detected;
type16 buflength;
type32 offset[MAXPICTURES + 2];
type32 addoffset;
type32 partlength;
type32 alllength;

struct lookuptab
  {
    char *game;
    char *filemask;
    type16 par[10];
  }
lookup[] =
{
  "The Pawn", "Pawn",
    1, 15, 17, 21, 23, 25, 27, 30, 32, 34,
    "The Pawn", "pawn",
    1, 15, 17, 21, 23, 25, 27, 30, 32, 34,
    "The Pawn", "ThePawn/Pawn",
    1, 15, 17, 21, 23, 25, 27, 30, 32, 34,
    "The Guild of Thieves", "guild",
    4, 32, 0, 0, 0, 0, 0, 0, 0, 0,
    "The Guild of Thieves", "The Guild Of Thieves/guild",
    4, 32, 0, 0, 0, 0, 0, 0, 0, 0,
    "Jinxter", "j",
    3, 30, 73, 73, 0, 0, 0, 0, 0, 0,
    "Corruption", "c",
    16, 37, 41, 41, 38, 40, 0, 0, 0, 0,
    "Fish!", "f",
    14, 40, 0, 0, 0, 0, 0, 0, 0, 0,
    "Myth", "m",
    16, 19, 0, 0, 0, 0, 0, 0, 0, 0
};

void
write_32 (type32 numb)
{
  inputbuf[3] = numb & 0xff;
  numb >>= 8;
  inputbuf[2] = numb & 0xff;
  numb >>= 8;
  inputbuf[1] = numb & 0xff;
  numb >>= 8;
  inputbuf[0] = numb;
  write (fdo_gfx, inputbuf, 4);
}

void
ex (char *error)
{
  fprintf (stderr, PRGNAME ": %s\n", error);
  close (fdi);
  close (fdo_temp);
  close (fdo_gfx);
  remove (TEMPFILE);
  exit (1);
}

void
itoa_krns (type16 n, char s[])
{
  int k, l, m;
  k = 0;
  do
    {
      s[k++] = n % 10 + '0';
    }
  while ((n /= 10) > 0);
  s[k] = '\0';
  for (k = 0, l = strlen (s) - 1; k < l; k++, l--)
    {
      m = s[k];
      s[k] = s[l];
      s[l] = m;
    }
}

void
kick (type16 picnumber)
{
  itoa_krns (picnumber, picnum);
  strcpy (infile, infilemask);
  strcat (infile, picnum);

  if ((fdi = open (infile, O_LEGGERE, 0)) == -1)
    {
      printf ("File not found: %s\n", infile);
      ex ("picture file(s) missing; can't create gfxfile");
    }
  else
    {
      endfile_reached = FALSE;
      partlength = 0;
      while (endfile_reached == FALSE)
	{
	  buflength = read (fdi, inputbuf, 256);
	  if (buflength != 256)
	    endfile_reached = TRUE;
	  write (fdo_temp, inputbuf, (int) buflength);
	  partlength = partlength + buflength;
	}
      offset[offsetcounter] = offset[offsetcounter - 1] + partlength;
      offsetcounter++;
    }
  close (fdi);
}

void
usage (void)
{
  fprintf (stderr, PRGNAME " v" PRGVERSION ": ");
  fprintf (stderr, "creates Magnetic Scrolls graphics file.\n");
  fprintf (stderr, "(c) 1997 by Paul David Doherty <h0142kdd@rz.hu-berlin.de>\n\n");
  fprintf (stderr, "Usage: " PRGNAME " [path] gfxfile\n");
  fprintf (stderr, "  e.g. " PRGNAME " df0: pawn.gfx\n");
  exit (1);
}

int
main (int argc, char **argv)
{
  int i, j;

  if ((argc < 2) || (argc > 3))
    usage ();

  strcpy (gfxfile, argv[(argc == 2) ? 1 : 2]);
  if (argc == 3)
    {
      strcpy (infilemask, argv[1]);
      if ((infilemask[strlen (infilemask) - 1] != ':')
	  && (infilemask[strlen (infilemask) - 1] != '/')
	  && (infilemask[strlen (infilemask) - 1] != '\\'))
	strcat (infilemask, DIRSEP);
    }

/* detection */
  for (i = 0; i < (sizeof (lookup) / sizeof (struct lookuptab)); i++)
    {
      itoa_krns (lookup[i].par[1], picnum);
      strcpy (infile, infilemask);
      strcat (infile, lookup[i].filemask);
      strcat (infile, picnum);
      if ((fdi = open (infile, O_LEGGERE, 0)) != -1)
	break;
    }
  detected = i;
  if (detected >= (sizeof (lookup) / sizeof (struct lookuptab)))
      ex ("no picture files detected");
  close (fdi);
  printf ("Game detected: %s\n", lookup[detected].game);

/* generation of temp file & offsets */
  if ((fdo_temp = open (TEMPFILE,
		  O_SCRIVERE | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
    ex ("could not create temp file");

  offset[0] = 0;
  strcat (infilemask, lookup[detected].filemask);
  for (i = 0; i <= 8; i = i + 2)
    for (j = lookup[detected].par[i]; j <= lookup[detected].par[i + 1]; j++)
      if (j != 0)
	kick ((type16) j);
  close (fdo_temp);
  offsetcounter--;
  printf ("Number of pictures: %d\n", offsetcounter);

/* writing offsets into gfx file */
  if ((fdo_gfx = open (gfxfile,
		  O_SCRIVERE | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
    ex ("could not create output file");

  addoffset = (((offsetcounter + 2) * 4) + strlen (magic));
  for (i = 0; i < offsetcounter; i++)
    offset[i] = offset[i] + addoffset;
  alllength = offset[offsetcounter] + addoffset;
  offset[offsetcounter] = 0;
  write (fdo_gfx, magic, strlen (magic));
  write_32 (alllength);
  for (i = 0; i <= offsetcounter; i++)
    write_32 (offset[i]);

/* copying tempfile into gfx file */
  if ((fdi = open (TEMPFILE, O_LEGGERE, 0)) == -1)
    ex ("could not reopen temp file");

  endfile_reached = FALSE;
  while (endfile_reached == FALSE)
    {
      buflength = read (fdi, inputbuf, 256);
      if (buflength != 256)
	endfile_reached = TRUE;
      write (fdo_gfx, inputbuf, (int) buflength);
    }
  close (fdi);
  close (fdo_gfx);
  if (remove (TEMPFILE) != 0)
    ex ("couldn't delete temp file");
  return 0;
}
