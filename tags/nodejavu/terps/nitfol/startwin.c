#line 488 "opt2glkc.pl"
#include "nitfol.h"
#include "WinGlk.h"

#line 671 "opt2glkc.pl"
strid_t intd_filehandle_open(strid_t savefile, glui32 operating_id,
                             glui32 contents_id, glui32 interp_id,
                             glui32 length)
{
  return 0;
}

void intd_filehandle_make(strid_t savefile)
{
  ;
}

glui32 intd_get_size(void)
{
  return 0;
}
#line 694 "opt2glkc.pl"
strid_t startup_findfile(void)
{
  ;
}
#line 705 "opt2glkc.pl"
strid_t startup_open(const char *name)
{
  return n_file_name(fileusage_Data | fileusage_BinaryMode,
		     filemode_Read, name);
}
#line 717 "opt2glkc.pl"
static strid_t startup_wopen(const char *name)
{
  return n_file_name(fileusage_Data | fileusage_BinaryMode,
		     filemode_Write, name);
}
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
  { "ignore", 'i', "Ignore Z-machine strictness errors", option_flag, code_ignore, 0, NULL, NULL, NULL, NULL },
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

#line 504 "opt2glkc.pl"
void shift_string_left(char *str)
{
  int len = strlen(str);
  int i;
  for(i = 0; i < len; i++)
    str[i] = str[i+1];
}

int winglk_startup_code(void)
{
  BOOL status;
  char *commandline = strdup(GetCommandLine());
  char **argv = (char **) n_malloc(sizeof(char *) * strlen(commandline));
  int argc = 0;

  int i;

  while(*commandline) {
    while(*commandline && isspace(*commandline))
      commandline++;
    
    argv[argc++] =  commandline;
    
    while(*commandline && !isspace(*commandline)) {
      if(*commandline == '"') {
	shift_string_left(commandline);
	while(*commandline && *commandline != '"')
	  commandline++;
	shift_string_left(commandline);
      } else {
	commandline++;
      }
    }
    
    *commandline++ = 0;
  }

  argv[argc] = NULL;
  
  status = parse_commands(argc, argv);

  n_free(argv);
  n_free(commandline);

  winglk_app_set_name("nitfol");
  winglk_window_set_title("nitfol");
  set_defaults();

  return status;
}
