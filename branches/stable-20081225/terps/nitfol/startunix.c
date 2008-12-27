#line 228 "opt2glkc.pl"
#include "nitfol.h"
#include "glkstart.h"
#ifdef DEBUGGING
#include <signal.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

static char *game_filename = NULL;

static void set_game_filename(const char *name)
{
  n_free(game_filename);
  game_filename = 0;

#if defined(_GNU_SOURCE)
  game_filename = canonicalize_file_name(name);
#else
#if defined(_BSD_SOURCE) || defined(_XOPEN_SOURCE)
  game_filename = (char *) n_malloc(PATH_MAX);
  if(!realpath(name, game_filename)) {
    n_free(game_filename);
    game_filename = 0;
  }
#else
#ifdef __DJGPP__
  game_filename = (char *) n_malloc(FILENAME_MAX);
  _fixpath(name, game_filename);
#endif
#endif
#endif

  if(!game_filename)
    game_filename = n_strdup(name);
}


strid_t startup_findfile(void)
{
  static DIR *dir = NULL;
  static char *pathstart = NULL;
  static char *path = NULL;
  strid_t stream;
  struct dirent *d;
  char *name = NULL;

  if(!pathstart) {
    char *p = search_path;
    if(!p)
      return 0;
    pathstart = n_strdup(p);
    if(!(path = n_strtok(pathstart, ":"))) {
      n_free(pathstart);
      pathstart = 0;
      return 0;
    }
  }

  do {
    if(!dir) {
      dir = opendir(path);
      if(!dir) {
	n_free(pathstart);
	pathstart = 0;
	return 0;
      }
    }
    d = readdir(dir);
    if(!d) {
      closedir(dir);
      dir = NULL;
      if(!(path = n_strtok(NULL, ":"))) {
	n_free(pathstart);
	pathstart = 0;
	return 0;
      }
    }
  } while(!dir);

  name = (char *) n_malloc(n_strlen(path) + n_strlen(d->d_name) + 2);
  n_strcpy(name, path);
  n_strcat(name, "/");
  n_strcat(name, d->d_name);
  stream = glkunix_stream_open_pathname(name, fileusage_Data |
					fileusage_BinaryMode, 0);
  if(stream)
    set_game_filename(name);
  n_free(name);
  return stream;
}


strid_t intd_filehandle_open(strid_t savefile, glui32 operating_id,
                             glui32 contents_id, glui32 interp_id,
                             glui32 length)
{
  char *name;
  strid_t str;
  if(operating_id != 0x554e4958 /* 'UNIX' */)
    return 0;
  if(contents_id != 0)
    return 0;
  if(interp_id != 0x20202020 /* '    ' */)
    return 0;

  name = (char *) n_malloc(length+1);
  glk_get_buffer_stream(savefile, name, length);
  name[length] = 0;
  str = glkunix_stream_open_pathname(name, fileusage_Data |
				     fileusage_BinaryMode, 0);
  if(str)
    set_game_filename(name);
  n_free(name);
  return str;
}

void intd_filehandle_make(strid_t savefile)
{
  if(!game_filename)
    return;
  w_glk_put_string_stream(savefile, "UNIX");
  glk_put_char_stream(savefile, b00000010); /* Flags */
  glk_put_char_stream(savefile, 0);         /* Contents ID */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  w_glk_put_string_stream(savefile, "    "); /* Interpreter ID */
  w_glk_put_string_stream(savefile, game_filename);
}

glui32 intd_get_size(void)
{
  if(!game_filename)
    return 0;
  return n_strlen(game_filename) + 12;
}

strid_t startup_open(const char *name)
{
  strid_t str;
  char *s;

  str = glkunix_stream_open_pathname((char *) name, fileusage_Data | fileusage_BinaryMode, 0);
  if(str) {
    set_game_filename(name);
    s = strrchr(name, '\\');
    if (!s) s = strrchr(name, '/');
    garglk_set_story_name(s ? s + 1 : name);
  } else {
    char *path = search_path;
    if(path) {
      char *p;
      char *newname = (char *) n_malloc(strlen(path) + strlen(name) + 2);
      path = n_strdup(path);
      for(p = n_strtok(path, ":"); p; p = n_strtok(NULL, ":")) {
	n_strcpy(newname, p);
	n_strcat(newname, "/");
	n_strcat(newname, name);
	str = glkunix_stream_open_pathname((char *) newname, fileusage_Data |
					   fileusage_BinaryMode, 0);
	if(str) {
	  set_game_filename(newname);
	  s = strrchr(newname, '\\');
	  if (!s) s = strrchr(newname, '/');
	  garglk_set_story_name(s ? s + 1 : newname);
	  break;
        }
      }
      n_free(path);
    }
  }

  if(!str)
    fprintf(stderr, "Cannot open '%s'\n", name);

  return str;
}

#line 717 "opt2glkc.pl"
static strid_t startup_wopen(const char *name)
{
  return n_file_name(fileusage_Data | fileusage_BinaryMode,
		     filemode_Write, name);
}
glkunix_argumentlist_t glkunix_arguments[] = {
  { (char *) "", glkunix_arg_ValueCanFollow, (char *) "filename	file to load" },
  { (char *) "-help", glkunix_arg_NoValue, (char *) "list command-line options" },
  { (char *) "--help", glkunix_arg_NoValue, (char *) "list command-line options" },
  { (char *) "-version", glkunix_arg_NoValue, (char *) "get version number" },
  { (char *) "--version", glkunix_arg_NoValue, (char *) "get version number" },
  { (char *) "-i", glkunix_arg_NoValue, (char *) "-i" },
  { (char *) "-no-ignore", glkunix_arg_NoValue, (char *) "-no-ignore" },
  { (char *) "--no-ignore", glkunix_arg_NoValue, (char *) "--no-ignore" },
  { (char *) "-ignore", glkunix_arg_NoValue, (char *) "-ignore" },
  { (char *) "--ignore", glkunix_arg_NoValue, (char *) "--ignore	Ignore Z-machine strictness errors" },
  { (char *) "-f", glkunix_arg_NoValue, (char *) "-f" },
  { (char *) "-no-fullname", glkunix_arg_NoValue, (char *) "-no-fullname" },
  { (char *) "--no-fullname", glkunix_arg_NoValue, (char *) "--no-fullname" },
  { (char *) "-fullname", glkunix_arg_NoValue, (char *) "-fullname" },
  { (char *) "--fullname", glkunix_arg_NoValue, (char *) "--fullname	For running under Emacs or DDD" },
  { (char *) "-x", glkunix_arg_ValueFollows, (char *) "-x" },
  { (char *) "-command", glkunix_arg_ValueFollows, (char *) "-command" },
  { (char *) "--command", glkunix_arg_ValueFollows, (char *) "--command	Read commands from this file" },
  { (char *) "-P", glkunix_arg_NoValue, (char *) "-P" },
  { (char *) "-no-pirate", glkunix_arg_NoValue, (char *) "-no-pirate" },
  { (char *) "--no-pirate", glkunix_arg_NoValue, (char *) "--no-pirate" },
  { (char *) "-pirate", glkunix_arg_NoValue, (char *) "-pirate" },
  { (char *) "--pirate", glkunix_arg_NoValue, (char *) "--pirate	Aye, matey" },
  { (char *) "-q", glkunix_arg_NoValue, (char *) "-q" },
  { (char *) "-no-quiet", glkunix_arg_NoValue, (char *) "-no-quiet" },
  { (char *) "--no-quiet", glkunix_arg_NoValue, (char *) "--no-quiet" },
  { (char *) "-quiet", glkunix_arg_NoValue, (char *) "-quiet" },
  { (char *) "--quiet", glkunix_arg_NoValue, (char *) "--quiet	Do not print introductory messages" },
  { (char *) "-no-spell", glkunix_arg_NoValue, (char *) "-no-spell" },
  { (char *) "--no-spell", glkunix_arg_NoValue, (char *) "--no-spell" },
  { (char *) "-spell", glkunix_arg_NoValue, (char *) "-spell" },
  { (char *) "--spell", glkunix_arg_NoValue, (char *) "--spell	Perform spelling correction" },
  { (char *) "-no-expand", glkunix_arg_NoValue, (char *) "-no-expand" },
  { (char *) "--no-expand", glkunix_arg_NoValue, (char *) "--no-expand" },
  { (char *) "-expand", glkunix_arg_NoValue, (char *) "-expand" },
  { (char *) "--expand", glkunix_arg_NoValue, (char *) "--expand	Expand one letter abbreviations" },
  { (char *) "-s", glkunix_arg_ValueFollows, (char *) "-s" },
  { (char *) "-symbols", glkunix_arg_ValueFollows, (char *) "-symbols" },
  { (char *) "--symbols", glkunix_arg_ValueFollows, (char *) "--symbols	Specify symbol file for game" },
  { (char *) "-t", glkunix_arg_NoValue, (char *) "-t" },
  { (char *) "-no-tandy", glkunix_arg_NoValue, (char *) "-no-tandy" },
  { (char *) "--no-tandy", glkunix_arg_NoValue, (char *) "--no-tandy" },
  { (char *) "-tandy", glkunix_arg_NoValue, (char *) "-tandy" },
  { (char *) "--tandy", glkunix_arg_NoValue, (char *) "--tandy	Censors some Infocom games" },
  { (char *) "-T", glkunix_arg_ValueFollows, (char *) "-T" },
  { (char *) "-transcript", glkunix_arg_ValueFollows, (char *) "-transcript" },
  { (char *) "--transcript", glkunix_arg_ValueFollows, (char *) "--transcript	Write transcript to this file" },
  { (char *) "-d", glkunix_arg_NoValue, (char *) "-d" },
  { (char *) "-no-debug", glkunix_arg_NoValue, (char *) "-no-debug" },
  { (char *) "--no-debug", glkunix_arg_NoValue, (char *) "--no-debug" },
  { (char *) "-debug", glkunix_arg_NoValue, (char *) "-debug" },
  { (char *) "--debug", glkunix_arg_NoValue, (char *) "--debug	Enter debugger immediatly" },
  { (char *) "-prompt", glkunix_arg_ValueFollows, (char *) "-prompt" },
  { (char *) "--prompt", glkunix_arg_ValueFollows, (char *) "--prompt	Specify debugging prompt" },
  { (char *) "-path", glkunix_arg_ValueFollows, (char *) "-path" },
  { (char *) "--path", glkunix_arg_ValueFollows, (char *) "--path	Look for games in this directory" },
  { (char *) "-no-autoundo", glkunix_arg_NoValue, (char *) "-no-autoundo" },
  { (char *) "--no-autoundo", glkunix_arg_NoValue, (char *) "--no-autoundo" },
  { (char *) "-autoundo", glkunix_arg_NoValue, (char *) "-autoundo" },
  { (char *) "--autoundo", glkunix_arg_NoValue, (char *) "--autoundo	Ensure @code{@@save_undo} is called every turn" },
  { (char *) "-S", glkunix_arg_NumberValue, (char *) "-S" },
  { (char *) "-stacklimit", glkunix_arg_NumberValue, (char *) "-stacklimit" },
  { (char *) "--stacklimit", glkunix_arg_NumberValue, (char *) "--stacklimit	Exit when the stack is this deep" },
  { (char *) "-a", glkunix_arg_ValueFollows, (char *) "-a" },
  { (char *) "-alias", glkunix_arg_ValueFollows, (char *) "-alias" },
  { (char *) "--alias", glkunix_arg_ValueFollows, (char *) "--alias	Specify an alias" },
  { (char *) "-ralias", glkunix_arg_ValueFollows, (char *) "-ralias" },
  { (char *) "--ralias", glkunix_arg_ValueFollows, (char *) "--ralias	Specify an recursive alias" },
  { (char *) "-unalias", glkunix_arg_ValueFollows, (char *) "-unalias" },
  { (char *) "--unalias", glkunix_arg_ValueFollows, (char *) "--unalias	Remove an alias" },
  { (char *) "-r", glkunix_arg_NumberValue, (char *) "-r" },
  { (char *) "-random", glkunix_arg_NumberValue, (char *) "-random" },
  { (char *) "--random", glkunix_arg_NumberValue, (char *) "--random	Set random seed" },
  { (char *) "-mapsym", glkunix_arg_ValueFollows, (char *) "-mapsym" },
  { (char *) "--mapsym", glkunix_arg_ValueFollows, (char *) "--mapsym	Specify mapping glyphs" },
  { (char *) "-mapsize", glkunix_arg_NumberValue, (char *) "-mapsize" },
  { (char *) "--mapsize", glkunix_arg_NumberValue, (char *) "--mapsize	Specify map size" },
  { (char *) "-maploc", glkunix_arg_ValueFollows, (char *) "-maploc" },
  { (char *) "--maploc", glkunix_arg_ValueFollows, (char *) "--maploc	Specify map location" },
  { (char *) "-terpnum", glkunix_arg_NumberValue, (char *) "-terpnum" },
  { (char *) "--terpnum", glkunix_arg_NumberValue, (char *) "--terpnum	Specify interpreter number" },
  { (char *) "-terpver", glkunix_arg_ValueFollows, (char *) "-terpver" },
  { (char *) "--terpver", glkunix_arg_ValueFollows, (char *) "--terpver	Specify interpreter version" },
  { NULL, glkunix_arg_End, NULL }
};

static void code_ignore(int flag)
#line 6 "nitfol.opt"
{ ignore_errors = flag; }

static void code_fullname(int flag)
#line 9 "nitfol.opt"
{ fullname = flag; }

static void code_command(strid_t stream)
#line 12 "nitfol.opt"
{ if(stream) input_stream1 = stream; }

static void code_pirate(int flag)
#line 15 "nitfol.opt"
{ aye_matey = flag; }

static void code_quiet(int flag)
#line 18 "nitfol.opt"
{ quiet = flag; }

static void code_spell(int flag)
#line 21 "nitfol.opt"
{ do_spell_correct = flag; }

static void code_expand(int flag)
#line 24 "nitfol.opt"
{ do_expand = flag; }

static void code_symbols(strid_t stream)
#line 27 "nitfol.opt"
{ if(stream) init_infix(stream); }

static void code_tandy(int flag)
#line 30 "nitfol.opt"
{ do_tandy = flag; }

static void code_transcript(strid_t stream)
#line 33 "nitfol.opt"
{ if(stream) set_transcript(stream); }

static void code_debug(int flag)
#line 36 "nitfol.opt"
{ enter_debugger = flag; do_check_watches = flag; }

static void code_prompt(const char *string)
#line 39 "nitfol.opt"
{ n_free(db_prompt); db_prompt = n_strdup(string); }

static void code_path(const char *string)
#line 42 "nitfol.opt"
{ n_free(search_path); search_path = n_strdup(string); }

static void code_autoundo(int flag)
#line 45 "nitfol.opt"
{ auto_save_undo = flag; }

static void code_stacklimit(int number)
#line 52 "nitfol.opt"
{ stacklimit = number; }

static void code_alias(const char *string)
#line 55 "nitfol.opt"
{ if(string) parse_new_alias(string, FALSE); }

static void code_ralias(const char *string)
#line 58 "nitfol.opt"
{ if(string) parse_new_alias(string, TRUE); }

static void code_unalias(const char *string)
#line 61 "nitfol.opt"
{ if(string) remove_alias(string); }

static void code_random(int number)
#line 64 "nitfol.opt"
{ faked_random_seed = number; }

static void code_mapsym(const char *string)
#line 67 "nitfol.opt"
{ n_free(roomsymbol); roomsymbol = n_strdup(string); }

static void code_mapsize(int number)
#line 70 "nitfol.opt"
{ automap_size = number; }

static void code_maploc(const char *string)
#line 73 "nitfol.opt"
{ switch(glk_char_to_lower(*string)) { case 'a': case 't': case 'u': automap_split = winmethod_Above; break; case 'b': case 'd': automap_split = winmethod_Below; break; case 'l': automap_split = winmethod_Left; break; case 'r': automap_split = winmethod_Right; } }

static void code_terpnum(int number)
#line 76 "nitfol.opt"
{ interp_num = number; }

static void code_terpver(const char *string)
#line 79 "nitfol.opt"
{ if(string) { if(n_strlen(string) == 1) interp_ver = *string; else interp_ver = n_strtol(string, NULL, 10); } }

#line 760 "opt2glkc.pl"
typedef enum { option_flag, option_file, option_wfile, option_number, option_string } option_type;
typedef struct { const char *longname; char shortname; const char *description; option_type type; void (*int_func)(int); int defint; void (*str_func)(strid_t); strid_t defstream; void (*string_func)(const char *); const char *defstring; } option_option;

static option_option options[] = {
  { "ignore", 'i', "Ignore Z-machine strictness errors", option_flag, code_ignore, 1, NULL, NULL, NULL, NULL },
  { "fullname", 'f', "For running under Emacs or DDD", option_flag, code_fullname, 0, NULL, NULL, NULL, NULL },
  { "command", 'x', "Read commands from this file", option_file, NULL, 0, code_command, NULL, NULL, NULL },
  { "pirate", 'P', "Aye, matey", option_flag, code_pirate, 0, NULL, NULL, NULL, NULL },
  { "quiet", 'q', "Do not print introductory messages", option_flag, code_quiet, 1, NULL, NULL, NULL, NULL },
  { "spell", '-', "Perform spelling correction", option_flag, code_spell, 1, NULL, NULL, NULL, NULL },
  { "expand", '-', "Expand one letter abbreviations", option_flag, code_expand, 1, NULL, NULL, NULL, NULL },
  { "symbols", 's', "Specify symbol file for game", option_file, NULL, 0, code_symbols, NULL, NULL, NULL },
  { "tandy", 't', "Censors some Infocom games", option_flag, code_tandy, 0, NULL, NULL, NULL, NULL },
  { "transcript", 'T', "Write transcript to this file", option_wfile, NULL, 0, code_transcript, NULL, NULL, NULL },
  { "debug", 'd', "Enter debugger immediatly", option_flag, code_debug, 0, NULL, NULL, NULL, NULL },
  { "prompt", '-', "Specify debugging prompt", option_string, NULL, 0, NULL, NULL, code_prompt, "(nitfol) " },
  { "path", '-', "Look for games in this directory", option_string, NULL, 0, NULL, NULL, code_path, NULL },
  { "autoundo", '-', "Ensure '@save_undo' is called every turn", option_flag, code_autoundo, 1, NULL, NULL, NULL, NULL },
  { "stacklimit", 'S', "Exit when the stack is this deep", option_number, code_stacklimit, 0, NULL, NULL, NULL, NULL },
  { "alias", 'a', "Specify an alias", option_string, NULL, 0, NULL, NULL, code_alias, NULL },
  { "ralias", '-', "Specify an recursive alias", option_string, NULL, 0, NULL, NULL, code_ralias, NULL },
  { "unalias", '-', "Remove an alias", option_string, NULL, 0, NULL, NULL, code_unalias, NULL },
  { "random", 'r', "Set random seed", option_number, code_random, 0, NULL, NULL, NULL, NULL },
  { "mapsym", '-', "Specify mapping glyphs", option_string, NULL, 0, NULL, NULL, code_mapsym, "*udb@UDB+" },
  { "mapsize", '-', "Specify map size", option_number, code_mapsize, 12, NULL, NULL, NULL, NULL },
  { "maploc", '-', "Specify map location", option_string, NULL, 0, NULL, NULL, code_maploc, "above" },
  { "terpnum", '-', "Specify interpreter number", option_number, code_terpnum, 2, NULL, NULL, NULL, NULL },
  { "terpver", '-', "Specify interpreter version", option_string, NULL, 0, NULL, NULL, code_terpver, "N" }
};

#line 811 "opt2glkc.pl"
static void set_defaults(void)
{
  unsigned n;
  for(n = 0; n < sizeof(options) / sizeof(*options); n++) {
    if(options[n].int_func)
      options[n].int_func(options[n].defint);
    if(options[n].str_func)
      options[n].str_func(options[n].defstream);
    if(options[n].string_func)
      options[n].string_func(options[n].defstring);
  }
}

#line 829 "opt2glkc.pl"
static void read_textpref(strid_t pref, const char *progname)
{
  unsigned n;
  char buffer[1024];
  int prognamelen = n_strlen(progname);
  if(!pref)
    return;
  while(glk_get_line_stream(pref, buffer, sizeof(buffer))) {
    char *optname;
    char *optval;
    long int optnum;

    if(buffer[0] == '#')
      continue;
    while(buffer[0] == '[') {
      if(n_strncasecmp(buffer+1, progname, prognamelen) != 0
	 || buffer[1+prognamelen] != ']') {
	while(glk_get_line_stream(pref, buffer, sizeof(buffer)))
	  if(buffer[0] == '[')
	    break;
      } else {
	glk_get_line_stream(pref, buffer, sizeof(buffer));
      }
    }

    optname = buffer;
    while(isspace(*optname))
      optname++;
    if((optval = n_strchr(optname, '=')) != NULL) {
      char *p;
      *optval = 0;
      optval++;

      if((p = n_strchr(optname, ' ')) != NULL)
	*p = 0;
      
      while(isspace(*optval))
	optval++;

      while(isspace(optval[strlen(optval)-1]))
        optval[strlen(optval)-1] = 0;

      optnum = n_strtol(optval, NULL, 0);
      if(n_strcasecmp(optval, "false") == 0
	 || n_strcasecmp(optval, "f") == 0)
	optnum = FALSE;
      if(n_strcasecmp(optval, "true") == 0
	 || n_strcasecmp(optval, "t") == 0)
	optnum = TRUE;

      for(n = 0; n < sizeof(options) / sizeof(*options); n++) {
	if(n_strcmp(options[n].longname, optname) == 0) {
	  switch(options[n].type) {
	  case option_flag:
	  case option_number:
	    options[n].int_func(optnum);
	    break;
	  case option_file:
	    options[n].str_func(startup_open(optval));
	    break;
	  case option_wfile:
	    options[n].str_func(startup_wopen(optval));
	    break;
	  case option_string:
	    options[n].string_func(optval);
	    break;
	  }
          break;
	}
      }
    }
  }
  glk_stream_close(pref, NULL);
}

#line 910 "opt2glkc.pl"
static void show_help(void)
{
  unsigned n;
  printf("Usage: nitfol [OPTIONS] gamefile\n");
  for(n = 0; n < sizeof(options) / sizeof(*options); n++) {
    if(options[n].shortname != '-')
      printf(" -%c, ", options[n].shortname);
    else
      printf("     ");
    printf("-%-15s %s\n", options[n].longname, options[n].description);
  }
}

#line 928 "opt2glkc.pl"
static BOOL parse_commands(int argc, char **argv)
{
  int i;
  unsigned n;

  for(i = 1; i < argc; i++) {
    BOOL flag = TRUE;

    const char *p = argv[i];
    
    if(p[0] == '-') {
      BOOL found = FALSE;

      while(*p == '-')
	p++;
      if(n_strncmp(p, "no-", 3) == 0) {
	flag = FALSE;
	p+=3;
      }

      if(n_strcasecmp(p, "help") == 0) {
	show_help();
	exit(0);
      }
      if(n_strcasecmp(p, "version") == 0) {
	printf("nitfol version %d.%d\n", NITFOL_MAJOR, NITFOL_MINOR);
	exit(0);
      }
      
      for(n = 0; n < sizeof(options) / sizeof(*options); n++) {
	if((n_strlen(p) == 1 && *p == options[n].shortname) ||
	   n_strcmp(options[n].longname, p) == 0) {
	  found = TRUE;

	  switch(options[n].type) {
	  case option_flag:
	    options[n].int_func(flag);
	    break;

	  case option_file:
	    i++;
	    options[n].str_func(startup_open(argv[i]));
	    break;

	  case option_wfile:
	    i++;
	    options[n].str_func(startup_wopen(argv[i]));
	    break;

	  case option_number:
            i++;
            options[n].int_func(n_strtol(argv[i], NULL, 0));
	    break;

	  case option_string:
	    i++;
	    options[n].string_func(argv[i]);
	    break;
	  }
	}
      }

      if(!found)
	return FALSE;

    } else {
      strid_t s = startup_open(argv[i]);
      if(!s)
	return FALSE;
      if(!game_use_file(s))
	return FALSE;
    }
  }

  return TRUE;
}

#line 415 "opt2glkc.pl"

#ifdef DEBUGGING
static void sighandle(int unused);

static void sighandle(int unused)
{
/*  signal(SIGINT, sighandle); */ /* SysV resets default behaviour - foil it */
  enter_debugger = TRUE;
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
int glkunix_startup_code(glkunix_startup_t *data)
{
  set_defaults();

	garglk_set_program_name("Nitfol 0.5");
	garglk_set_program_info(
		"Nitfol 0.5 by Evin Robertson\n"
		"With countless patches by other people.\n");

  return parse_commands(data->argc, data->argv);
}
#ifdef __cplusplus
}
#endif
