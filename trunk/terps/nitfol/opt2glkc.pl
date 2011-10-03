#!/usr/local/bin/perl -w
use strict;

my $header = "nitfol.h";
my $appname = "nitfol";
my $appmajor = "NITFOL_MAJOR";
my $appminor = "NITFOL_MINOR";
my $texinfo = "nitfol.texi";
my $author = "Evin Robertson";
my $email = "nitfol\@my-deja.com";
my $search_path = "INFOCOM_PATH";
my @man_see_also = ( "frotz (6)", "txd (1)" );
my $mac_creator = "niTf";
my $mac_savefile = "IFZS";
my $mac_datafile = "ZipD";
my @mac_gamefile = ( "ZCOD", "IFRS", $mac_savefile );

# The following are modified by make_glk_* as appropriate
my $dirsep = "/";
my $configdir = "configdir = n_strdup(getenv(\"HOME\"));";
my $configname = "configname = \"${dirsep}.${appname}rc\";";


# opt2glkc.pl - Generates Glk startup code and some documentation
# Copyright (C) 1999  Evin Robertson
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
#  The author can be reached at nitfol@my-deja.com

my @LoL;
my $argtype;
my $systemtype;

my %optionlist = ( unix => [ "startunix.c",  \&make_glk_unix ],
		   dos  => [ "startdos.c",   \&make_glk_dos ],
		   win  => [ "startwin.c",   \&make_glk_win ],
		   mac  => [ "startmac.c",   \&make_glk_mac ],
		   info => [ "options.texi", \&make_info ],
		   man  => [ "$appname.6",   \&make_man ]
		   );

$_ = $ARGV[0];

if(/^-(.*)$/) {
    $systemtype = $1;
    shift;
} else {
    $systemtype = "unix";
}

read_in_optfile();

$optionlist{$systemtype} || die "Unknown switch '-$systemtype'\n";

open("MYOUTPUT", ">$optionlist{$systemtype}[0]") || die "cannot write to $optionlist{$systemtype}[0]\n";

select "MYOUTPUT";
&{ $optionlist{$systemtype}[1] }();

close "MYOUTPUT";




sub read_in_optfile
{
    my $longdesc;
    my $i = 1;
    while(<>) {
	if(/^\#/ || /^$/) {
	    ;
	}
	#             1          2      3         4         5       6                     9
	elsif(/^\s*\"(.*?)\"\s+(\S+)\s+(\S)\s+\"(.*?)\"\s+(\S+)\s+((\S+)|(\".*?\"))\s+(\{.*)$/) {
	    $longdesc = <>;
	    push @LoL, [ ( $1, $2, $3, $4, $5, $6, $9, $i, $ARGV, $longdesc ) ];
	    $i++;
	} else {
	    die "Error in file $ARGV line $i.";
	}
	$i++;
    }
}



sub make_info
{
    foreach my $soption (@LoL) {
	my @option = @{ $soption };
	if($option[4] eq "flag") {
	    print "\@item -$option[1]\n";
	    print "\@itemx -no-$option[1]\n";
	    if($option[2] ne "-") {
		print "\@itemx -$option[2]\n";
	    }
	} else {
	    print "\@item -$option[1] \@var{$option[4]}\n";
	    if($option[2] ne "-") {
		print "\@itemx -$option[2] \@var{$option[4]}\n";
	    }
	}
	print "$option[3].  $option[9]\n";
    }
}

sub texi2roff
{
    $_ = $_[0];
    s/\@itemize \@minus\n//;
    s/\@end itemize/.PP/;
    s/\@item /.IP \\\(bu\n/;

    s/\@code\{(.*?)\}/\\fB$1\\fP/g;
    s/\@samp\{(.*?)\}/\`$1\'/g;
    s/\@var\{(.*?)\}/\\fI$1\\fP/g;
    s/\@kbd\{(.*?)\}/\`$1\'/g;
    s/\@file\{(.*?)\}/\`$1\'/g;
    s/\@cite\{(.*?)\}/\\fI$1\\fP/g;
    s/\@uref\{(.*?)(, (.*?))\}/$3 \<$1\>/g;
    s/\@uref\{(.*?)\}/\<$1\>/g;
    s/\@email\{(.*?)\}/\<$1\>/g;
    s/\@sc\{(.*?)\}/$1/g;
    s/\@\@/\@/g;

    return $_;
}

sub texi2txt
{
    $_ = $_[0];
    s/\@code\{(.*?)\}/\'$1\'/g;
    s/\@file\{(.*?)\}/\'$1\'/g;
    s/\@\@/\@/g;
    return $_;
}


sub make_man
{
    open("MYTEXINFO", "<$texinfo") || die "unable to read '$texinfo'\n";
    print ".TH ", uc $appname, " 6\n";
    print ".SH NAME\n";
    while(($_ = <MYTEXINFO>) !~ /^\* $appname: \($appname\). *.*/i) {
	;
    }
    /^\* $appname: \($appname\). *(.*)/i;
    print "$appname \\- $1\n";
    print ".SH SYNOPSIS\n";
    print ".B $appname\n";
    print ".I \"[options] file\"\n";
    print ".SH DESCRIPTION\n";
    print "This manpage was generated from bits of the info page.  See the info page for complete documentation.\n";
    while(<MYTEXINFO> !~ /^\@chapter introduction/i) {
	;
    }
    while(($_ = <MYTEXINFO>) !~ /^(\@node)|(\@bye)|(\@menu)/) {
	if($_ !~ /^\@.*/) {
	    print texi2roff($_);
	}
    }
    print ".SH OPTIONS\n";
    foreach my $soption (@LoL) {
	my @option = @{ $soption };
	print ".TP\n";
	print ".B";
	if($option[4] eq "flag") {
	    print " \\-$option[1]";
	    print ", \\-no\\-$option[1]";
	    if($option[2] ne "-") {
		print ", \\-$option[2]";
	    }
	} else {
	    print " \\-$option[1] \\fI$option[4]\\fB";
	    if($option[2] ne "-") {
		print ", \\-$option[2] \\fI$option[4]";
	    }
	}
	print "\n";
	print texi2roff($option[3]), ".  ", texi2roff($option[9]);
    }

    print ".SH BUGS\n";
    while(<MYTEXINFO> !~ /^\@chapter bugs/i) {
	;
    }
    while(($_ = <MYTEXINFO>) !~ /^(\@node)|(\@bye)/) {
	print texi2roff($_);
    }

    print ".SH \"SEE ALSO\"\n";
    print ".RB \"`\\|\" $appname \"\\|'\"\n";
    print "entry in\n";
    print ".B\n";
    print "info;\n";

    my $flag = 0;
    foreach my $also (@man_see_also) {
	if($flag) {
	    print ",\n";
	}
	$flag = 1;
	print ".BR $also";
    }
    print ".\n";

    print ".SH AUTHOR\n";
    print "$appname was written by $author, who can be reached at $email.\n";
    close("MYTEXINFO");
}

sub make_glk_unix
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
#include \"$header\"
#include \"glkstart.h\"

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
    if(!(path = n_strtok(pathstart, \":\"))) {
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
      if(!(path = n_strtok(NULL, \":\"))) {
	n_free(pathstart);
	pathstart = 0;
	return 0;
      }
    }
  } while(!dir);

  name = (char *) n_malloc(n_strlen(path) + n_strlen(d->d_name) + 2);
  n_strcpy(name, path);
  n_strcat(name, \"$dirsep\");
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
  if(operating_id != ", string_to_iff("UNIX"), ")
    return 0;
  if(contents_id != 0)
    return 0;
  if(interp_id != ", string_to_iff("    "), ")
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
  w_glk_put_string_stream(savefile, \"UNIX\");
  glk_put_char_stream(savefile, b00000010); /* Flags */
  glk_put_char_stream(savefile, 0);         /* Contents ID */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  w_glk_put_string_stream(savefile, \"    \"); /* Interpreter ID */
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

  str = glkunix_stream_open_pathname((char *) name, fileusage_Data | fileusage_BinaryMode, 0);
  if(str) {
    set_game_filename(name);
  } else {
    char *path = search_path;
    if(path) {
      char *p;
      char *newname = (char *) n_malloc(strlen(path) + strlen(name) + 2);
      path = n_strdup(path);
      for(p = n_strtok(path, \":\"); p; p = n_strtok(NULL, \":\")) {
	n_strcpy(newname, p);
	n_strcat(newname, \"$dirsep\");
	n_strcat(newname, name);
	str = glkunix_stream_open_pathname((char *) newname, fileusage_Data |
					   fileusage_BinaryMode, 0);
	if(str) {
	  set_game_filename(newname);
	  break;
        }
      }
      n_free(path);
    }
  }

  if(!str)
    fprintf(stderr, \"Cannot open '%s'\\n\", name);

  return str;
}

";

    make_generic_startup_wopen();
    make_useless_command_structure();
    make_functions();
    make_useful_command_structure();
    make_default_setter();
    make_textpref_reader();
    make_help_printer();
    make_command_parser();

    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "

#ifdef DEBUGGING
static void sighandle(int unused);

static void sighandle(int unused)
{
/*  signal(SIGINT, sighandle); */ /* SysV resets default behaviour - foil it */
  enter_debugger = TRUE;
}
#endif

#ifdef __cplusplus
extern \"C\" {
#endif
int glkunix_startup_code(glkunix_startup_t *data)
{
  strid_t pref;
  const char *configname;
  char *configdir, *prefname;
  char *execname;
  char *p;
  username = getenv(\"LOGNAME\");   /* SysV */
  if(!username)
    username = getenv(\"USER\");    /* BSD */

#ifdef DEBUGGING
/*  signal(SIGINT, sighandle); */
#endif

  execname = n_strrchr(data->argv[0], '$dirsep');

  if(execname)
    execname++;
  else
    execname = data->argv[0];
  
  set_defaults();
  $configdir
  $configname
  prefname = n_malloc(n_strlen(configdir) + n_strlen(configname) + 1);
  n_strcpy(prefname, configdir);
  n_strcat(prefname, configname);
  pref = glkunix_stream_open_pathname(prefname, fileusage_Data | fileusage_TextMode, 0);
  n_free(configdir);
  n_free(prefname);
  read_textpref(pref, execname);
  
  p = getenv(\"$search_path\");
  if(p) {
    free(search_path);
    search_path = n_strdup(p);
  }

  return parse_commands(data->argc, data->argv);
}
#ifdef __cplusplus
}
#endif
";
}

sub make_glk_dos
{
    $dirsep = "/";
    $configdir = "configdir = n_strdup(data->argv[0]); if(n_strrchr(configdir, '$dirsep')) *n_strrchr(configdir, '$dirsep') = 0;";
    $configname = "configname = \"${dirsep}${appname}.cfg\";";
    make_glk_unix();
}


sub make_glk_win
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
#include \"$header\"
#include \"WinGlk.h\"

";
    make_generic_intd();
    make_generic_findfile();
    make_generic_startup_open();
    make_generic_startup_wopen();
    make_functions();
    make_useful_command_structure();
    make_default_setter();
    make_help_printer();
    make_command_parser();


    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
      if(*commandline == '\"') {
	shift_string_left(commandline);
	while(*commandline && *commandline != '\"')
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

  winglk_app_set_name(\"$appname\");
  winglk_window_set_title(\"$appname\");
  set_defaults();

  return status;
}
";
}



sub string_to_iff
{
    my ($id) = @_;
    my $i;
    my $val;
    $val = 0;
    for($i=0; $i < length $id; $i++) {
	$val = $val * 0x100 + ord substr $id, $i, 1;
    }
    return sprintf("0x%x /* '$id' */", $val);
}



sub make_glk_mac
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
#include \"$header\"
#include \"macglk_startup.h\"

static strid_t mac_gamefile;

static BOOL hashandle = FALSE;
static AliasHandle gamehandle;
";
    make_generic_findfile();
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
strid_t intd_filehandle_open(strid_t savefile, glui32 operating_id,
                             glui32 contents_id, glui32 interp_id,
                             glui32 length)
{
  FSSpec file;
  Boolean wasChanged;
  if(operating_id != ", string_to_iff("MACS"), ")
    return 0;
  if(contents_id != 0)
    return 0;
  if(interp_id != ", string_to_iff("    "), ")
    return 0;

  gamehandle = NewHandle(length);
  glk_get_buffer_stream(savefile, *gamehandle, length);
  hashandle = TRUE;
  ResolveAlias(NULL, gamehandle, &file, &wasChanged);
  return macglk_stream_open_fsspec(&file, 0, 0);  
}

void intd_filehandle_make(strid_t savefile)
{
  if(!hashandle)
    return;
  glk_put_string_stream(savefile, \"MACS\");
  glk_put_char_stream(savefile, b00000010); /* Flags */
  glk_put_char_stream(savefile, 0);         /* Contents ID */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_char_stream(savefile, 0);         /* Reserved */
  glk_put_string_stream(savefile, \"    \");/* Interpreter ID */
  glk_put_buffer_stream(savefile, *gamehandle, *gamehandle->aliasSize);
}

glui32 intd_get_size(void)
{
  if(!hashandle)
    return 0;
  return *gamehandle->aliasSize + 12;
}

static Boolean mac_whenselected(FSSpec *file, OSType filetype)
{
  NewAlias(NULL, file, &gamehandle);
  hashandle = TRUE;
  return game_use_file(mac_gamefile);
}

static Boolean mac_whenbuiltin()
{
  return game_use_file(mac_gamefile);
}

Boolean macglk_startup_code(macglk_startup_t *data)
{
  OSType mac_gamefile_types[] = { ";

    my $flag = 0;
    foreach my $filetype (@mac_gamefile) {
	if($flag) {
	    print ", ";
        }
        $flag = 1;
        print string_to_iff($filetype);
    }

    print " };

  data->startup_model  = macglk_model_ChooseOrBuiltIn;
  data->app_creator    = ", string_to_iff($mac_creator), ";
  data->gamefile_types = mac_gamefile_types;
  data->num_gamefile_types = sizeof(mac_gamefile_types) / sizeof(*mac_gamefile_types);
  data->savefile_type  = ", string_to_iff($mac_savefile), ";
  data->datafile_type  = ", string_to_iff($mac_datafile), ";
  data->gamefile       = &mac_gamefile;
  data->when_selected  = mac_whenselected;
  data->when_builtin   = mac_whenbuiltin;

  return TRUE;
}
";
}

sub make_generic_intd
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
";
}


sub make_generic_findfile
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
strid_t startup_findfile(void)
{
  ;
}
";
}


sub make_generic_startup_open
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
strid_t startup_open(const char *name)
{
  return n_file_name(fileusage_Data | fileusage_BinaryMode,
		     filemode_Read, name);
}
";
}


sub make_generic_startup_wopen
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
static strid_t startup_wopen(const char *name)
{
  return n_file_name(fileusage_Data | fileusage_BinaryMode,
		     filemode_Write, name);
}
";
}



sub make_functions
{
    foreach my $soption (@LoL) {
	my @option = @{ $soption };
	if($option[4] eq "flag") {
	    $argtype = "int flag";
	}
	if($option[4] eq "file" || $option[4] eq "wfile") {
	    $argtype = "strid_t stream";
	}
	if($option[4] eq "number") {
	    $argtype = "int number";
	}
	if($option[4] eq "string") {
	    $argtype = "const char *string";
	}
	
	print "static void code_$option[1]($argtype)\n";
	print "#line $option[7] \"$option[8]\"\n";
	print "$option[6]\n\n";
    }
}



sub make_useful_command_structure
{
    #
    # Write structure so we can actually parse the options
    #
    my ( $int_func, $defint, $str_func, $defstr, $string_func, $defstring );

    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"';
    print "\ntypedef enum { option_flag, option_file, option_wfile, option_number, option_string } option_type;\n";
    print "typedef struct { const char *longname; char shortname; const char *description; option_type type; void (*int_func)(int); int defint; void (*str_func)(strid_t); strid_t defstream; void (*string_func)(const char *); const char *defstring; } option_option;\n\n";

    print "static option_option options[] = {\n";

    my $flag = 0;
    foreach my $soption (@LoL) {
	my @option = @{ $soption };
	if($flag) {
	    print ",\n";
	}
	$flag = 1;

	$int_func = "NULL";
	$defint = "0";

	$str_func = "NULL";
	$defstr = "NULL";

	$string_func = "NULL";
	$defstring = "NULL";
	
	if($option[4] eq "flag") {
	    $defint = $option[5];
	    $int_func = "code_" . $option[1];
	}
	if($option[4] eq "file" || $option[4] eq "wfile") {
	    $defstr = $option[5];
	    $str_func = "code_" . $option[1];
	}
	if($option[4] eq "number") {
	    $defint = $option[5];
	    $int_func = "code_" . $option[1];
	}
	if($option[4] eq "string") {
	    $defstring = $option[5];
	    $string_func = "code_" . $option[1];
	}
	
	print "  { \"$option[1]\", '$option[2]', \"", texi2txt($option[3]), "\", option_$option[4], $int_func, $defint, $str_func, $defstr, $string_func, $defstring }";
    }
    
    print "\n};\n\n";

}



sub make_default_setter
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
\n";
}

sub make_textpref_reader
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
      if(n_strcasecmp(optval, \"false\") == 0
	 || n_strcasecmp(optval, \"f\") == 0)
	optnum = FALSE;
      if(n_strcasecmp(optval, \"true\") == 0
	 || n_strcasecmp(optval, \"t\") == 0)
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
\n";
}


sub make_help_printer
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
static void show_help(void)
{
  unsigned n;
  printf(\"Usage: $appname [OPTIONS] gamefile\\n\");
  for(n = 0; n < sizeof(options) / sizeof(*options); n++) {
    if(options[n].shortname != '-')
      printf(\" -%c, \", options[n].shortname);
    else
      printf(\"     \");
    printf(\"-%-15s %s\\n\", options[n].longname, options[n].description);
  }
}
\n";
}

sub make_command_parser
{
    print "#line " . (__LINE__+1) . ' "' . __FILE__ . '"' . "
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
      if(n_strncmp(p, \"no-\", 3) == 0) {
	flag = FALSE;
	p+=3;
      }

      if(n_strcasecmp(p, \"help\") == 0) {
	show_help();
	exit(0);
      }
      if(n_strcasecmp(p, \"version\") == 0) {
	printf(\"$appname version %d.%d\\n\", $appmajor, $appminor);
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
\n";
}




sub make_useless_command_structure
{
    print "glkunix_argumentlist_t glkunix_arguments[] = {\n";
    
    print "  { (char *) \"\", glkunix_arg_ValueCanFollow, (char *) \"filename\tfile to load\" },\n";

    print "  { (char *) \"-help\", glkunix_arg_NoValue, (char *) \"list command-line options\" },\n";
    print "  { (char *) \"--help\", glkunix_arg_NoValue, (char *) \"list command-line options\" },\n";
    print "  { (char *) \"-version\", glkunix_arg_NoValue, (char *) \"get version number\" },\n";
    print "  { (char *) \"--version\", glkunix_arg_NoValue, (char *) \"get version number\" },\n";

    
    foreach my $soption (@LoL) {
	my @option = @{ $soption };
	if($option[4] eq "flag") {
	    $argtype = "glkunix_arg_NoValue";
	}
	if($option[4] eq "file" || $option[4] eq "wfile") {
	    $argtype = "glkunix_arg_ValueFollows";
	}
	if($option[4] eq "number") {
	    $argtype = "glkunix_arg_NumberValue";
	}
	if($option[4] eq "string") {
	   $argtype = "glkunix_arg_ValueFollows";
       }
	
	if($option[2] ne "-") {
	    print "  { (char *) \"-$option[2]\", $argtype, (char *) \"-$option[2]\" },\n";
	}
	
	if($option[4] eq "flag") {
	    print "  { (char *) \"-no-$option[1]\", $argtype, (char *) \"-no-$option[1]\" },\n";
	    print "  { (char *) \"--no-$option[1]\", $argtype, (char *) \"--no-$option[1]\" },\n";
	}
	
	print "  { (char *) \"-$option[1]\", $argtype, (char *) \"-$option[1]\" },\n";
	print "  { (char *) \"--$option[1]\", $argtype, (char *) \"--$option[1]\t$option[3]\" },\n";
    }

    print "  { NULL, glkunix_arg_End, NULL }\n";
    print "};\n\n";

}


