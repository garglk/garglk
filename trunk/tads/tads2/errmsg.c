#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/TADS2/ERRMSG.C,v 1.3 1999/07/11 00:46:29 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1992, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  errmsg.c - error message text
Function
  provides error message text for TADS2 error handlers
Notes
  none
Modified
  11/26/92 JEras         - send .MSG output to named file
  11/15/92 JEras         - portable .MSG format
  04/05/92 MJRoberts     - creation
*/

#include "os.h"
#include "err.h"
#include "mch.h"

#ifdef ERR_LINK_MESSAGES

/*
 *   Define some messages, if they're not otherwise defined.  On some
 *   systems, the os-dependent headers define non-standard default sizes
 *   for some of the memory settings.  Whenever an OS header does this, it
 *   should define the corresponding usage message string.  We'll define
 *   any that aren't already defined in the OS headers to provide default
 *   values here.  
 */
#ifndef TCD_HEAPSIZ_MSG
#define TCD_HEAPSIZ_MSG "  -mh size      heap size (default 1024 bytes)"
#endif
#ifndef TCD_POOLSIZ_MSG
#define TCD_POOLSIZ_MSG \
        "  -mp size      parse node pool size (default 6144 bytes)"
#endif
#ifndef TCD_LCLSIZ_MSG
#define TCD_LCLSIZ_MSG \
        "  -ml size      local symbol table size (default 4096 bytes)"
#endif
#ifndef TCD_STKSIZ_MSG
#define TCD_STKSIZ_MSG "  -ms size      stack size (default 50 elements)"
#endif
#ifndef TCD_LABSIZ_MSG
#define TCD_LABSIZ_MSG \
        "  -mg size      goto label table size (default 1024 bytes)"
#endif

#ifndef TRD_HEAPSIZ_MSG
#define TRD_HEAPSIZ_MSG "  -mh size      heap size (default 2048 bytes)"
#endif
#ifndef TRD_STKSIZ_MSG
#define TRD_STKSIZ_MSG  "  -ms size      stack size (default 200 elements)"
#endif
#ifndef TRD_UNDOSIZ_MSG
#define TRD_UNDOSIZ_MSG \
        "  -u size       set undo to size (0 to disable; default 16384)"
#endif

#ifndef TDD_HEAPSIZ_MSG
#define TDD_HEAPSIZ_MSG "  -mh size      heap size (default 2048 bytes)"
#endif
#ifndef TDD_STKSIZ_MSG
#define TDD_STKSIZ_MSG  "  -ms size      stack size (default 200 elements)"
#endif
#ifndef TDD_UNDOSIZ_MSG
#define TDD_UNDOSIZ_MSG \
        "  -u size       set undo to size (0 to disable; default 16384)"
#endif
#ifndef TDD_POOLSIZ_MSG
#define TDD_POOLSIZ_MSG "  -mp size      parse pool size (default 2048 bytes)"
#endif

/*
 *   The error list 
 */

static errmdef errlist[] =
{
    { ERR_NOMEM, "fatal: out of memory" },
    { ERR_FSEEK, "fatal: error seeking in file" },
    { ERR_FREAD, "fatal: error reading from file" },
    { ERR_NOPAGE, "fatal: no more page slots" },
    { ERR_REALCK, "attempting to reallocate a locked object" },
    { ERR_SWAPBIG, "fatal: swapfile limit reached - out of virtual memory" },
    { ERR_FWRITE, "fatal: error writing file" },
    { ERR_SWAPPG, "fatal: exceeded swap page table limit" },
    { ERR_CLIUSE, "requested client object number already in use" },
    { ERR_CLIFULL, "fatal: client mapping table is full" },
    { ERR_NOMEM1, "fatal: no memory, even after swapping/garbage collection" },
    { ERR_NOMEM2, "fatal: no memory to resize (expand) an object" },
    { ERR_OPSWAP, "unable to open swap file" },
    { ERR_NOHDR, "can't get a new object header" },
    { ERR_NOLOAD, "mcm cannot find object to load (internal error)" },
    { ERR_LCKFRE, "attempting to free a locked object (internal error)" },
    { ERR_INVOBJ, "invalid object (object has been deleted)" },
    { ERR_BIGOBJ, "object too big - exceeds maximum object allocation size" },
    { ERR_INVTOK, "invalid token" },
    { ERR_STREOF, "end of file while scanning string" },
    { ERR_TRUNC, "warning: symbol too long - truncated to \"%s\"" },
    { ERR_NOLCLSY, "fatal: no space in local symbol table" },
    { ERR_PRPDIR, "invalid preprocessor (#) directive" },
    { ERR_INCNOFN, "no filename in #include directive" },
    { ERR_INCSYN, "invalid #include syntax" },
    { ERR_INCSEAR, "can't find included file \"%s\"" },
    { ERR_INCMTCH, "no matching delimiter in #include filename" },
    { ERR_MANYSYM, "fatal: out of space for symbol table expansion" },
    { ERR_LONGLIN, "input line is too long" },
    { ERR_INCRPT, "warning: file \"%s\" already included; #include ignored" },
    { ERR_PRAGMA, "unknown pragma (ignored)" },
    { ERR_BADPELSE, "unexpected #else" },
    { ERR_BADENDIF, "unexpected #endif" },
    { ERR_BADELIF, "unexpected #elif" },
    { ERR_MANYPIF, "#if nesting too deep" },
    { ERR_DEFREDEF, "#define symbol already defined -- redefinition ignored" },
    { ERR_PUNDEF, "warning:  symbol not defined in #undef" },
    { ERR_NOENDIF, "missing #endif" },
    { ERR_MACNEST, "macros nested too deeply" },
    { ERR_BADISDEF, "invalid argument for defined() preprocessor operator" },
    { ERR_PIF_NA, "#if is not implemented" },
    { ERR_PELIF_NA, "#elif is not implemented" },
    { ERR_P_ERROR, "Error directive: %s" },
    { ERR_LONG_FILE_MACRO,
    "internal error: __FILE__ expansion too long" },
    { ERR_LONG_LINE_MACRO,
    "internal error: __LINE__ expansion too long" },

    { ERR_UNDOVF, "operation is too big for undo log" },
    { ERR_NOUNDO, "no more undo information" },
    { ERR_ICUNDO, "incomplete undo (no previous savepoint)" },
    { ERR_REQTOK, "expected %s" },
    { ERR_REQSYM, "expected a symbol" },
    { ERR_REQPRP, "expected a property name" },
    { ERR_REQOPN, "expected an operand" },
    { ERR_REQARG, "expected a comma or closing paren (in arg list)" },
    { ERR_NONODE,
"fatal: no space for new parse node (use -mp option to increase the limit)" },
    { ERR_REQOBJ, "expected object name" },
    { ERR_REQEXT, "redefining symbol as external function" },
    { ERR_REQFCN, "redefining symbol as function" },
    { ERR_NOCLASS, "can't use \"class\" with function/external function" },
    { ERR_REQUNO, "unary operator required" },
    { ERR_REQBIN, "binary operator required" },
    { ERR_INVBIN, "invalid binary operator" },
    { ERR_INVASI, "invalid assignment" },
    { ERR_REQVAR, "variable name required" },
    { ERR_LCLSYN, "comma or semicolon required in local list" },
    { ERR_REQRBR, "right brace required (eof before end of group)" },
    { ERR_BADBRK, "'break' without 'while'" },
    { ERR_BADCNT, "'continue' without 'while'" },
    { ERR_BADELS, "'else' without 'if'" },
    { ERR_WEQASI, "warning: possible use of '=' where ':=' intended" },
    { ERR_EOF,    "unexpected end of file" },
    { ERR_SYNTAX, "general syntax error" },
    { ERR_INVOP, "invalid operand type" },
    { ERR_NOMEMLC, "fatal: can't expand local symbol table" },
    { ERR_NOMEMAR, "fatal: can't expand argument symbol table" },
    { ERR_FREDEF, "redefining a function which is already defined" },
    { ERR_NOSW, "'case' or 'default' not in switch block" },
    { ERR_SWRQCN, "constant required in switch case value" },
    { ERR_REQLBL, "label required for 'goto'" },
    { ERR_NOGOTO, "'goto' label never defined" },
    { ERR_MANYSC, "too many superclasses for object" },
    { ERR_OREDEF, "redefining symbol as object" },
    { ERR_PREDEF, "property being redefined in object" },
    { ERR_BADPVL, "invalid property value" },
    { ERR_BADVOC, "invalid vocabulary property value" },
    { ERR_BADTPL, "invalid template property value (need string)" },
    { ERR_LONGTPL, "template base property name too long" },
    { ERR_MANYTPL, "too many templates (internal compiler limit)" },
    { ERR_BADCMPD, "invalid value for compound word (string required)" },
    { ERR_BADFMT, "invalid value for format string (string required)" },
    { ERR_BADSYN, "invalid value for synonym (string required)"},
    { ERR_UNDFSYM, "undefined symbol" },
    { ERR_BADSPEC, "invalid value for specialWords list (string required)" },
    { ERR_NOSELF, "\"self\" is not valid in this context" },
    { ERR_STREND, "warning: possible unterminated string" },
    { ERR_MODRPLX, "'replace'/'modify' not allowed with external function" },
    { ERR_MODFCN, "'modify' not allowed with function; ignored" },
    { ERR_MODFWD, "'modify'/'replace' ignored with forward declaration" },
    { ERR_MODOBJ, "'modify' can only be used with a defined object" },
    { ERR_RPLSPEC, "warning:  replacing specialWords (please use 'replace')" },
    { ERR_SPECNIL, "nil allowed only with modify specialWords" },
    { ERR_BADLCL, "'local' is only allowed at the beginning of a block" },
    { ERR_IMPPROP,"implied verifier '%s' is not a property" },
    { ERR_BADTPLF,"invalid command template flag" },
    { ERR_NOTPLFLG,"flags are not allowed with old file formats" },
    { ERR_AMBIGBIN,"warning:  operator '%s' interpreted as unary in list" },
    { ERR_PIA,"warning: possibly incorrect assignment" },
    { ERR_BADSPECEXPR, "speculative evaluation failed" },

    { ERR_OBJOVF, "object cannot grow any bigger - code too big" },
    { ERR_NOLBL, "no more temporary labels/fixups (internal compiler limit)"},
    { ERR_LBNOSET, "(internal error) label never set" },
    { ERR_INVLSTE, "invalid datatype for list element" },
    { ERR_MANYDBG,
 "fatal: too many debugger source line records (internal limit)" },
    { ERR_VOCINUS, "vocabulary being redefined for object" },
    { ERR_VOCMNPG,
 "fatal: too many vocabulary word relations (internal limit)" },
    { ERR_VOCREVB, "warning:  same verb '%s' defined for two objects" },
    { ERR_VOCREVB2, "warning:  same verb '%s %s' defined for two objects" },
    { ERR_LOCNOBJ, "warning: location of object \"%s\" is not an object" },
    { ERR_CNTNLST, "warning: contents of object \"%s\" is not list" },
    { ERR_SUPOVF, "overflow trying to build contents list" },
    { ERR_RQOBJNF, "required object \"%s\" not found" },
    { ERR_WRNONF, "warning: object \"%s\" not found" },
    { ERR_MANYBIF, "too many built-in functions (internal error)" },
    { ERR_OPWGAM, "unable to open game for writing" },
    { ERR_WRTGAM, "error writing to game file" },
    { ERR_FIOMSC, "too many sc's for writing in fiowrt" },
    { ERR_UNDEFF, "undefined function \"%s\"" },
    { ERR_UNDEFO, "undefined object \"%s\"" },
    { ERR_UNDEF, "undefined symbols found" },
    { ERR_OPRGAM, "unable to open game for reading" },
    { ERR_RDGAM, "error reading game file" },
    { ERR_BADHDR, "file has invalid header - not a TADS game file" },
    { ERR_UNKRSC, "unknown resource type in .gam file" },
    { ERR_UNKOTYP, "unknown object type in OBJ resource" },
    { ERR_BADVSN, "file saved by different (incompatible) version" },
    { ERR_LDGAM, "error loading object on demand" },
    { ERR_LDBIG, "object too big for load region (internal error)" },
    { ERR_UNXEXT, "did not expect external function" },
    { ERR_WRTVSN, "compiler cannot write this file version" },
    { ERR_VNOCTAB, "this file version cannot be used with a -ctab setting" },
    { ERR_BADHDRRSC, "resource file \"%s\" has an invalid header" },
    { ERR_RDRSC,  "error reading resource file \"%s\"" },
    { ERR_CHRNOFILE,
    "character table file \"%s\" not found for internal character set \"%s\""},
    { ERR_USRINT,  "user requested cancel of current operation" },
    { ERR_STKOVF, "stack overflow" },
    { ERR_HPOVF, "heap overflow" },
    { ERR_REQNUM, "numeric value required" },
    { ERR_STKUND, "stack underflow" },
    { ERR_REQLOG, "logical value required" },
    { ERR_INVCMP, "invalid datatypes for magnitude comparison" },
    { ERR_REQSTR, "string value required" },
    { ERR_INVADD, "invalid datatypes for binary '+' operator" },
    { ERR_INVSUB, "invalid datatypes for binary '-' operator" },
    { ERR_REQVOB, "object value required" },
    { ERR_REQVFN, "function pointer required" },
    { ERR_REQVPR, "property pointer value required" },
    { ERR_RUNEXIT, "'exit' statement executed" },
    { ERR_RUNABRT, "'abort' statement executed" },
    { ERR_RUNASKD, "'askdo' statement executed" },
    { ERR_RUNASKI, "'askio' executed; preposition is object #%d" },
    { ERR_RUNQUIT, "'quit' executed" },
    { ERR_RUNRESTART, "'reset' executed" },
    { ERR_RUNEXITOBJ, "'exitobj' executed" },
    { ERR_REQVLS, "list value required" },
    { ERR_LOWINX, "index value too low (must be at least 1)" },
    { ERR_HIGHINX,"index value too high (must be at most length(list))" },
    { ERR_INVTBIF,"invalid type for built-in function" },
    { ERR_INVVBIF,"invalid value for built-in function \"%s\"" },
    { ERR_BIFARGC,"wrong number of arguments to built-in" },
    { ERR_ARGC, "wrong number of arguments to user function \"%s\"" },
    { ERR_FUSEVAL,"string/list not allowed for fuse/daemon arg" },
    { ERR_BADSETF,"internal error in setfuse/setdaemon/notify" },
    { ERR_MANYFUS,"too many fuses" },
    { ERR_MANYDMN,"too many daemons" },
    { ERR_MANYNFY,"too many notifiers" },
    { ERR_NOFUSE, "fuse not found in remfuse" },
    { ERR_NODMN, "daemon not found in remdaemon" },
    { ERR_NONFY, "notifier not found in unnotify" },
    { ERR_BADREMF,"internal error in remfuse/remdaemon/unnotify" },
    { ERR_DMDLOOP,"load-on-demand loop: property not being set (internal)" },
    { ERR_UNDFOBJ,"undefined object \"%s\" in class list"},
    { ERR_BIFCSTR,"c-string conversion overflows buffer (internal limit)" },
    { ERR_INVOPC, "invalid opcode (internal error)" },
    { ERR_RUNNOBJ,"property evaluated for non-existent object" },
    { ERR_EXTLOAD,"unable to load external function \"%s\"" },
    { ERR_EXTRUN,
    "external functions are obsolete (attempted to invoke \"%s\")" },
    { ERR_CIRCSYN,"circular synonym" },
    { ERR_DIVZERO,"divide by zero" },
    { ERR_BADDEL, "only objects allocated with 'new' can be deleted" },
    { ERR_BADNEWSC,"invalid superclass for 'new'" },
    { ERR_VOCSTK, "fatal: out of space in parser stack" },
    { ERR_BADFILE, "invalid file handle" },

    { ERR_PRS_SENT_UNK, "unrecognized sentence structure" },
    { ERR_PRS_VERDO_FAIL, "direct object validation failed" },
    { ERR_PRS_VERIO_FAIL, "indirect object validation failed" },
    { ERR_PRS_NO_VERDO, "no direct object validation method defined" },
    { ERR_PRS_NO_VERIO, "no indirect object validation method defined" },
    { ERR_PRS_VAL_DO_FAIL, "direct object validation failed" },
    { ERR_PRS_VAL_IO_FAIL, "indirect object validation failed" },

    { ERR_USAGE, "invalid command-line usage" },
    { ERR_OPNINP, "error opening input file" },
    { ERR_NODBG,
"game not compiled for debugging - recompile with -ds or -ds2 \
option (check your debugger instructions to determine which option to \
use with your version of the debugger)" },
    { ERR_ERRFIL, "unable to create error logging file" },
    { ERR_PRSCXSIZ,
 "parse pool + local sizes too large - total cannot exceed %u" },
    { ERR_STKSIZE, "stack size too large - cannot exceed %u" },
    { ERR_OPNSTRFIL, "error creating string capture file" },
    { ERR_INVCMAP, "character map file not found or invalid" },
        
    { ERR_BPSYM, "error setting breakpoint:  unknown symbol" },
    { ERR_BPPROP, "error setting breakpoint:  symbol is not a property" },
    { ERR_BPFUNC, "error setting breakpoint:  symbol is not a function" },
 { ERR_BPNOPRP, "error setting breakpoint:  property not defined in object" },
    { ERR_BPPRPNC, "error setting breakpoint:  property is not code" },
    { ERR_BPSET, "error:  breakpoint is already set at this location" },
    { ERR_BPNOTLN, "error setting breakpoint:  breakpoint not at line" },
    { ERR_MANYBP, "too many breakpoints" },
    { ERR_BPNSET, "breakpoint was not set" },
    { ERR_DBGMNSY, "too many symbols in eval expression (internal limit)" },
    { ERR_NOSOURC, "unable to find source file \"%s\"" },
    { ERR_WTCHLCL, "assignment to local is illegal in watch expression" },
    { ERR_INACTFR, "inactive frame (expression value not available)" },
    { ERR_MANYWX, "too many watch expressions" },
    { ERR_WXNSET, "watch expression not set" },
    { ERR_EXTRTXT, "extraneous text after end of command" },
    { ERR_BPOBJ, "error setting breakpoint:  symbol is not an object" },
    { ERR_DBGINACT, "debugger is inactive (program running)" },
    { ERR_BPINUSE, "breakpoint is already used" },
    { ERR_RTBADSPECEXPR, "speculative evaluation failed (runtime)" },
    { ERR_NEEDLIN2,
"New-style debugging records required.  Recompile game with -ds2 option." },

    /* compiler usage messages */
    { ERR_TCUS1, OS_TC_USAGE },
    { ERR_TCUS1 + 1, "options:" },
   { ERR_TCUS1 + 2, "  -C            (-)[toggle] use C-style =, == operators"},
    { ERR_TCUS1 + 3,
       "  -case         (+)[toggle] turn case sensitivity on(+) or off(-)" },
    { ERR_TCUS1 + 4, "  -ctab file    use character mapping table file" },
    { ERR_TCUS1 + 5, "  -D sym=val    define preprocessor symbol" },
    { ERR_TCUS1 + 6, 
       "  -ds           (-)[toggle] generate source debugging information" },
    { ERR_TCUS1 + 7,
       "  -ds2          (-)[toggle] generate new-style debug information" },
    { ERR_TCUS1 + 8, "  -e file       capture errors/warnings in file" },
    { ERR_TCUS1 + 9, "  -Fs file      capture strings in file" },
    { ERR_TCUS1 + 10, "  -fv a|b|c|*   .GAM file version to generate" },
    { ERR_TCUS1 + 11,"  -i path       add path to include file search list" },
    { ERR_TCUS1 + 12,"  -l file       load precompiled header file" },
    { ERR_TCUS1 + 13,"  -m            memory suboptions (use -m? to list)" },
    { ERR_TCUS1 + 14,
       "  -o file       write game to file (-o- for no output)" },
    { ERR_TCUS1 + 15,
       "  -p            (-)[toggle] pause for key after compilation" },
    { ERR_TCUS1 + 16,
       "  -s            (-)[toggle] show statistics after compilation" },
    { ERR_TCUS1 + 17,
       "  -tf file      use file for swapping (default: TADSSWAP.DAT)" },
    { ERR_TCUS1 + 18,
       "  -ts size      maximum swapfile size (default: unlimited)" },
    { ERR_TCUS1 + 19,
#if OS_DEFAULT_SWAP_ENABLED
       "  -t            (+)[toggle] swapping" },
#else
       "  -t            (-)[toggle] swapping" },
#endif
    { ERR_TCUS1 + 20, "  -U sym        undefine preprocessor symbol" },
    { ERR_TCUS1 + 21, "  -v arg        set warning level (-v? for details)" },
    { ERR_TCUS1 + 22, "  -w file       precompile headers to file" },
    { ERR_TCUS1 + 23,
       "  -Z            code generation options (use -Z? to list)" },
    { ERR_TCUSL,
       "  -1            v1 compatibility options (use -1? to list)" },

    { ERR_TCTGUS1, "" },
    { ERR_TCTGUS1 + 1,
       "toggle options: add + to enable, - to disable, nothing to toggle" },
    { ERR_TCTGUSL, "(default is shown in parentheses before [toggle] above)"},
   
    { ERR_TCZUS1, "-Z (code generation) suboptions:" },
    { ERR_TCZUSL,
    "  -Za           (+)[toggle] run-time user function argument checking" },

    { ERR_TC1US1, "-1 (TADS version 1 compatibility) suboptions:" },
    { ERR_TC1US1 + 1,
     "  -1a           (+)[toggle] run-time user function argument checking" },
    { ERR_TC1US1 + 2, "                (same as -Za)" },
    { ERR_TC1US1 + 3, "  -1k           (-)[toggle] disable new keywords" },
    { ERR_TC1US1 + 4,
     "  -1e           (-)[toggle] ignore ';' after 'if {...}'" },
    { ERR_TC1US1 + 5, "  -1d new       replace 'do' keyword with 'new'" },
    { ERR_TC1USL - 2, "" },
    { ERR_TC1USL - 1,
     "With no suboption, -1 acts like a toggle option for all" },
    { ERR_TC1USL, "toggle-type v1 compatibility options" },

    { ERR_TCMUS1, "-m (memory) suboptions:" },
    { ERR_TCMUS1 + 1,
       "  -m size       limit in-memory cache to size (in bytes)" },
    { ERR_TCMUS1 + 2, TCD_LABSIZ_MSG },
    { ERR_TCMUS1 + 3, TCD_HEAPSIZ_MSG },
    { ERR_TCMUS1 + 4, TCD_LCLSIZ_MSG },
    { ERR_TCMUS1 + 5, TCD_POOLSIZ_MSG },
    { ERR_TCMUSL,     TCD_STKSIZ_MSG },
    { ERR_TCVUS1, "-v (warning verbosity level) suboptions:" },
    { ERR_TCVUS1 + 1,
      "  -v level      set warning verbosity to level (default is 0)" },
    { ERR_TCVUSL,
"  -v-abin       disable ambiguous binary operator warnings (+abin enables)" },

    /* runtime usage */
    { ERR_TRUSPARM, "usage: %s [options] file" },
    { ERR_TRUS1, OS_TR_USAGE },
    { ERR_TRUS1 + 1, "options:" },
    { ERR_TRUS1 + 2, "  -ctab file    use character mapping table file" },
    { ERR_TRUS1 + 3, "  -ctab-        do not use any character mapping file" },
    { ERR_TRUS1 + 4, "  -i file       read commands from file" },
    { ERR_TRUS1 + 5, "  -o file       write commands to file" },
    { ERR_TRUS1 + 6, "  -l file       log all output to file" },
    { ERR_TRUS1 + 7, "  -m size       maximum cache size (in bytes)" },
    { ERR_TRUS1 + 8, TRD_HEAPSIZ_MSG },
    { ERR_TRUS1 + 9, TRD_STKSIZ_MSG },
    { ERR_TRUS1 + 10, "  -p            [toggle] pause after play" },
    { ERR_TRUS1 + 11,
       "  -r savefile   restore saved game position from savefile" },
    { ERR_TRUS1 + 12, "  -s level      set I/O safety level (-s? for help)" },
    { ERR_TRUS1 + 13,
       "  -tf file      use file for swapping (default: TADSSWAP.DAT)" },
    { ERR_TRUS1 + 14,
       "  -ts size      maximum swapfile size (default: unlimited)" },
    { ERR_TRUS1 + 15, "  -tp           (-)[toggle] preload all objects" },
  { ERR_TRUS1 + 16,
#if OS_DEFAULT_SWAP_ENABLED
                     "  -t-           disable swapping (enabled by default)" },
#else
                     "  -t+           enable swapping (disabled by default)" },
#endif
    { ERR_TRUS1 + 17, TRD_UNDOSIZ_MSG },
    { ERR_TRUSFT1, "" },
    { ERR_TRUSFT1 + 1,
       "toggle options: add + to enable, - to disable, nothing to toggle" },

    /* tr security suboptions help */
    { ERR_TRSUS1,     "-s (file I/O safety level) suboptions:" },
    { ERR_TRSUS1 + 1,
    "  -s0    minimum safety - read and write in any directory" },
    { ERR_TRSUS1 + 2,
    "  -s1    read in any directory, write in current directory only"
    },
    { ERR_TRSUS1 + 3,
    "  -s2    (default) read and write in current directory only" },
    { ERR_TRSUS1 + 4, "  -s3    read-only access in current directory only" },
    { ERR_TRSUS1 + 5, "  -s4    maximum safety - no file I/O allowed" },
    { ERR_TRSUS1 + 6,
    "(These options apply only to explicit file operations by the game,"},
    { ERR_TRSUSL,     "not to saving and restoring games.)" },

    /* debugger usage */
    { ERR_TDBUSPARM, "usage: %s [options] file" },
    { ERR_TDBUS1,  OS_TDB_USAGE },
    { ERR_TDBUS1 + 1, "options:"},
    { ERR_TDBUS1 + 2, "  -ctab file    use character mapping table file" },
    { ERR_TDBUS1 + 3, "  -ctab-        do not use any character table file" },
    { ERR_TDBUS1 + 4, "  -i path       add path to source file search list" },
    { ERR_TDBUS1 + 5, "  -m size       maximum cache size (in bytes)" },
    { ERR_TDBUS1 + 6, TDD_HEAPSIZ_MSG },
    { ERR_TDBUS1 + 7, TDD_POOLSIZ_MSG },
    { ERR_TDBUS1 + 8, TDD_STKSIZ_MSG },
    { ERR_TDBUS1 + 9,
        "  -r savefile   restore saved game position from savefile" },
    { ERR_TDBUS1 + 10,"  -s level      set I/O safety level (-s? for help)" },
    { ERR_TDBUS1 + 11,
        "  -tf file      use file for swapping (default: TADSSWAP.DAT)" },
    { ERR_TDBUS1 + 12,
        "  -ts size      maximum swapfile size (default: unlimited)" },
    { ERR_TDBUS1 + 13,
#if OS_DEFAULT_SWAP_ENABLED
        "  -t-           disable swapping (enabled by default)" },
#else
        "  -t+           enable swapping (disabled by default)" },
#endif
    { ERR_TDBUSL, TDD_UNDOSIZ_MSG },

    /* DOS 16-bit TR supplemental usage messages */
    { ERR_TRUS_DOS_1,
    "  -plain        use plain text console output (no BIOS calls)" },

    /* DOS 32-bit TR32 supplemental usage messages */
    { ERR_TRUS_DOS32_1,
    "  -plain        use plain text console output (no BIOS calls)" },
    { ERR_TRUS_DOS32_1 + 1,
    "  -kbfix95      use special Windows 95 keyboard handling (use only if" },
    { ERR_TRUS_DOS32_1 + 2,
    "                you have problems with keyboard layout switching)" },
    

    { ERR_GNOFIL, "warning: graphics resource file \"%s\" not found" },
    { ERR_GNORM, "can't find room \"%s\" (it's used in the resource file)" },
    { ERR_GNOOBJ,
        "can't find object \"%s\" (it's used in a hot spot resource)" },
    { ERR_GNOICN, "can't find object \"%s\" (it's used in an icon resource)"}
};

void errmsg(errcxdef *ctx, char *outbuf, uint outbufl, uint err)
{
    uint last;
    uint first;
    uint cur;

    VARUSED(outbufl);
    
    /* run a binary search for the indicated error */
    first = 0;
    last = (sizeof(errlist) / sizeof(errlist[0])) - 1;
    for (;;)
    {
        if (first > last)
        {
            strcpy(outbuf, "unknown error");
            return;
        }

        cur = first + (last - first)/2;
        if (errlist[cur].errmerr == err)
        {
            strcpy(outbuf, errlist[cur].errmtxt);
            return;
        }
        else if (errlist[cur].errmerr < err)
            first = (cur == first ? first + 1 : cur);
        else
            last = (cur == last ? last - 1 : cur);
    }
}

void errini(errcxdef *ctx, osfildef *fp)
{
    VARUSED(ctx);
    VARUSED(fp);
}

#else /* ERR_LINK_MESSAGES */

void errmsg(errcxdef *ctx, char *outbuf, uint outbufl, uint err)
{
    uint      last;
    uint      first;
    uint      cur;
    errmfdef *errlist = ctx->errcxseek;
    int       l;
    
    if (!errlist)
    {
        strcpy(outbuf, "unknown: no error message file");
        return;
    }

    /* run a binary search for the indicated error */
    first = 0;
    last = ctx->errcxsksz - 1;
    for (;;)
    {
        if (first > last)
        {
            strcpy(outbuf, "error not found");
            return;
        }

        cur = first + (last - first)/2;
        if (errlist[cur].errmfnum == err)
        {
            osfseek(ctx->errcxfp, errlist[cur].errmfseek + ctx->errcxbase,
                    OSFSK_SET);
            osfgets(outbuf, outbufl, ctx->errcxfp);
            l = strlen(outbuf);
            while (l && (outbuf[l-1] == '\n' || outbuf[l-1] == '\r')) --l;
            outbuf[l] = '\0';
            return;
        }
        else if (errlist[cur].errmfnum < err)
            first = (cur == first ? first + 1 : cur);
        else
            last = (cur == last ? last - 1 : cur);
    }
}

void errini(errcxdef *ctx, osfildef *fp)
{
    uint  siz;
    uint  i;
    uchar buf[6];
    
    ctx->errcxseek = (errmfdef *)0;
    ctx->errcxsksz = 0;
    ctx->errcxbase = 0;
    if (!fp || ctx->errcxfp) return;
    
    /* read number of messages, and allocate space for descriptors */
    (void)osfrb(fp, buf, 2);
    ctx->errcxsksz = osrp2(buf);
    siz = ctx->errcxsksz * sizeof(errmdef);
    ctx->errcxseek = (errmfdef *)mchalo(ctx, siz, "error messages");
    for (i = 0; i < ctx->errcxsksz; ++i)
    {
        (void)osfrb(fp, buf, 6);
        ctx->errcxseek[i].errmfnum = osrp2(buf);
        ctx->errcxseek[i].errmfseek = osrp4(buf+2);
    }

    ctx->errcxfp = fp;
}

#endif /* ERR_LINK_MESSAGES */

#ifdef ERR_BUILD_FILE

/* build error message file to named file or TADSERR.MSG in current dir */

int main(int argc, char *argv[])
{
    osfildef *fp;
    uint      i;
    uint      siz;
    ulong     pos;
    char     *nameout;
    uchar     buf[6];
    
    if (argc > 2)
    {
        printf("usage: tadserr [msg-file]\n");
        os_term(1);
    }
    nameout = (argc == 2) ? argv[1] : "tadserr.msg";

    fp = osfopwb(nameout, OSFTERRS);
    if (!fp)
    {
        printf("error: can't open %s\n", nameout);
        os_term(OSEXFAIL);
    }
    
    /* write number of messages */
    siz = sizeof(errlist) / sizeof(errlist[0]);
    oswp2(buf, siz);
    (void)osfwb(fp, buf, 2);

    /* write message headers */
    for (i = 0, pos = 2 + siz * 6 ; i < siz ; ++i)
    {
        oswp2(buf, errlist[i].errmerr);
        oswp4(buf+2, pos);
        (void)osfwb(fp, buf, 6);

        pos += strlen(errlist[i].errmtxt) + 1;
    }
    
    /* write messages */
    for (i = 0 ; i < siz ; ++i)
    {
        uint msglen;

        /* only write the message if its length is non-zero */
        msglen = strlen(errlist[i].errmtxt);
        if (msglen) (void)osfwb(fp, errlist[i].errmtxt, msglen);
        (void)osfwb(fp, "\n", 1);
    }
    
    osfcls(fp);
    return 0;
}

#endif /* ERR_BUILD_FILE */

#ifdef ERR_BUILD_DOC

#include <ctype.h>

/* build error message file in current directory */

int main(int argc, char *argv[])
{
    osfildef *fp;
    uint      i;
    uint      siz;
    osfildef *fpin;
    char     *namein;
    char      buf[128];
    char      buf2[80];
    unsigned  inmsg;
    char     *src;
    char     *dst;
    int       found_end = 0;
    long      prvpos;
    int       inquote;
    
    if (argc != 2)
    {
        printf("usage: tadsmdoc tex-file\n");
        os_term(OSEXFAIL);
    }
    
    namein = argv[1];
    rename(namein, "$DOC$.TMP");
    if (!(fpin = osfoprt("$DOC$.TMP", OSFTTEXT)))
        printf("warning: old doc file not found - creating %s\n", namein);
    
    fp = osfopwt(namein, OSFTTEXT);
    if (!fp)
    {
        printf("error: can't create %s\n", namein);
        rename("$DOC$.TMP", namein);
        os_term(OSEXFAIL);
    }
    
    /* seek first message, copying original input file up to that point */
    if (fpin != 0)
    {
        for (;;)
        {
            if (!fgets(buf, sizeof(buf), fpin))
            {
                fpin = 0;
                break;
            }
            
            if (!memcmp(buf, "% message #", 11))
            {
                inmsg = atoi(buf + 11);
                break;
            }
            else if (!memcmp(buf, "% end messages", 14))
            {
                found_end = 1;
                break;
            }
            else
                fputs(buf, fp);
        }
    }
    
    /* write messages */
    siz = sizeof(errlist) / sizeof(errlist[0]);
    for (i = 0 ; i < siz ; ++i)
    {
        if (errlist[i].errmerr == ERR_TCUS1) break;
        
        /* wait until we're at least at current message in input */
        while (fpin != 0 && !found_end && inmsg < errlist[i].errmerr)
        {
            prvpos = ftell(fpin);
            if (!fgets(buf, sizeof(buf), fpin))
                fpin = 0;
            else if (!memcmp(buf, "% message #", 11))
            {
                inmsg = atoi(buf + 11);
                if (inmsg < errlist[i].errmerr)
                    fputs(buf, fp);
            }
            else if (!memcmp(buf, "% end messages", 14))
                found_end = 1;
            else
                fputs(buf, fp);
        }
        
        /* discard next line of input if it's the current message */
        if (fpin != 0 && inmsg == errlist[i].errmerr)
            fgets(buf, sizeof(buf), fpin);
        
        /* add the current message to output file */
        sprintf(buf2, "%% message #%d\n", errlist[i].errmerr);
        fputs(buf2, fp);
        
        sprintf(buf2, "\\tadsmessage{%d}{", errlist[i].errmerr);
        dst = buf2 + strlen(buf2);
        inquote = 0;
        for (src = errlist[i].errmtxt ; *src ; ++src)
        {
            if (*src == '%')
            {
                switch(*++src)
                {
                case 'd':
                    memcpy(dst, "{\\it n}", 8);
                    dst += 7;
                    break;
                case 's':
                    memcpy(dst, "{\\it xxx}", 10);
                    dst += 9;
                    break;
                case 't':
                    memcpy(dst, "{\\it token}", 12);
                    dst += 11;
                    break;
                }
            }
            else if (*src == '"')
            {
                if (inquote)
                {
                    *dst++ = '\'';
                    *dst++ = '\'';
                }
                else
                {
                    *dst++ = '`';
                    *dst++ = '`';
                }
                inquote = !inquote;
            }
            else if (*src == '\'')
            {
                if (inquote
                    || (src > errlist[i].errmtxt && isalpha(*(src-1))
                        && isalpha(*(src+1))))
                {
                    *dst++ = *src;
                    inquote = 0;
                }
                else
                {
                    *dst++ = '`';
                    inquote = 1;
                }
            }
            else if (*src == '#')
            {
                *dst++ = '\\';
                *dst++ = *src;
            }
            else
                *dst++ = *src;
        }
        *dst++ = '}';
        *dst++ = '\n';
        *dst++ = '\0';
        fputs(buf2, fp);
        
        /* go back to previous position if appropriate */
        if (fpin != 0 && errlist[i].errmerr != inmsg)
            fseek(fpin, prvpos, 0);
    }
    
    /* copy the remainder up to the end of file marker */
    while (fpin != 0 && !found_end)
    {
        if (!fgets(buf, sizeof(buf), fpin))
            fpin = 0;
        else if (!memcmp(buf, "% end messages", 14))
            found_end = 1;
        else
            fputs(buf, fp);
    }
    
    /* write out end of file marker, and copy remainder of input file */
    fputs("% end messages\n", fp);
    while (fpin != 0)
    {
        if (!fgets(buf, sizeof(buf), fpin))
            fpin = 0;
        else
            fputs(buf, fp);
    }
    
    osfcls(fp);
    if (fpin != 0)
        fclose(fpin);
    remove("$DOC$.TMP");

    return 0;
}

#endif /* ERR_BUILD_DOC */

