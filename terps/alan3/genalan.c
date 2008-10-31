/*----------------------------------------------------------------------*\

  genalan.c

  A small C program to generate a large Alan program...

\*----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int objs;
static int locs;
static int exts;
static int acts;
static int scrs;
static int evts;
static int ruls;


static long rnd(long to, long from)
{
  if (to > from)
    return rand()%(to-from+1)+from;
  else
    return rand()%(from-to+1)+to;
}


#define OBJS (objs/2+rnd(1,objs))

static void geobjs(char *initloc)
{
  int objmax = OBJS;
  int o;

  for (o = 1; o <= objmax; o++) {
    printf("OBJECT o%d%s%s%s\n", o, initloc?initloc:"", initloc?" AT ":"", initloc?initloc:"");
    printf("END OBJECT o%d%s.\n\n", o, initloc?initloc:"");
  }
}


#define EXTS (exts/2+rnd(1, exts))

static void geexts(int maxloc)
{
  int extmax = EXTS;
  int e;

  for (e = 1; e <= extmax; e++)
    printf("  EXIT e%d TO l%ld.\n", e, rnd(1, maxloc));
}


#define LOCS (locs/2+rnd(1,locs))

static void gelocs(void)
{
  int locmax = LOCS;
  int l;
  char initloc[30];

  for (l = 1; l<=locmax; l++) {
    printf("LOCATION l%d\n", l);
    geexts(locmax);
    printf("END LOCATION l%d.\n\n", l);
    sprintf(initloc, "l%d", l);
    geobjs(initloc);
  }
}



/* SPA Option handling */

#include "spa.h"


static SPA_FUN(usage)
{
  printf("Purpose:\nGenerate random Alan source\n\nUsage:\nGENALAN [-help] [options]\n");
}


static SPA_ERRFUN(paramError)
{
  char *sevstr;

  switch (sev) {
  case 'E': sevstr = "error"; break;
  case 'W': sevstr = "warning"; break;
  default: sevstr = "internal error"; break;
  }
  printf("Parameter %s: %s, %s\n", sevstr, msg, add);
  usage(NULL, NULL, 0);
  exit(EXIT_FAILURE);
}

static SPA_FUN(extraArg)
{
  printf("Extra argument: '%s'\n", rawName);
  usage(NULL, NULL, 0);
  exit(EXIT_FAILURE);
}

static SPA_FUN(xit) {exit(EXIT_SUCCESS);}

static SPA_DECLARE(arguments)
     SPA_FUNCTION("", "extra argument", extraArg)
SPA_END

static SPA_DECLARE(options)
     SPA_HELP("help", "this help", usage, xit)
     SPA_INTEGER("objects", "nominal number of objects at each location", objs, 10, NULL)
     SPA_INTEGER("locations", "nominal number of locations", locs, 10, NULL)
     SPA_INTEGER("exits", "nominal number of exits in each location", exts, 2, NULL)
     SPA_INTEGER("actors", "nominal number of actors", acts, 10, NULL)
     SPA_INTEGER("scripts", "nominal number of scripts for each actor", scrs, 10, NULL)
     SPA_INTEGER("events", "nominal number of events", evts, 10, NULL)
     SPA_INTEGER("rules", "nominal number of rules", ruls, 10, NULL)
SPA_END


int main(int argc, char **argv)
{
  int nArgs;

  nArgs = spaProcess(argc, argv, arguments, options, paramError);
  if (nArgs > 0)
    exit(EXIT_FAILURE);
  
  srand(time(0));

  geobjs(0);
  gelocs();

  printf("START AT l1.\n");
  exit(EXIT_SUCCESS);
}


