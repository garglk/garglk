/*--------------------------------------------------------------------*\
    spa.c				Date: 1995-04-13/reibert@home

    spa -- standard process of arguments (SoftLabs way)

    Author: Reibert Arbring.
    	    Copyright (c) 1989 - 1995 SoftLab ab

    Legal Notice: As in spa.h

    History:
    4.2(1) - 1994-05-13/reibert@home -- Errors more severe 
    4.2    - 1994-04-30/reibert@home -- SPA_COMMENT 
    4.1(1) - 1994-01-08/reibert@home -- spaExit 
    4.1    - 1993-11-01/reibert@home -- CMS/changed float interface 
    4.0(1) - 1992-03-12/reibert@home -- bits: * 
    4.0    - 1992-03-09/reibert@home -- SPA_ITEM outdated, "locale" messages,
    				    	mac adaption, files, alert
----
    3.1    - 1992-02-26/reibert@roo  -- SPA_PRINT_DEFAULT
    3.0(3) - 1993-02-16/thoni@rabbit -- SPA_Set with '--' requires no argument
    3.0(2) - 1992-02-26/reibert@roo  -- Ambiguous was misspelled, spaArgumentNo
    3.0(1) - 1991-09-23/reibert@roo  -- SPA_Set with '--' sets all on (good?)
    3.0    - 1991-08-28/reibert@roo  -- SPA_KeyWord, all SPA_funs as arguments
----
    2.4    - 1990-11-27/reibert@roo  -- SPA_Report, SPA_Toggle etc. removed
    2.3(2) - 1990-11-20/reibert@roo  -- ANSI-C && C++
    2.3(1) - 1990-04-24/reibert@roo  -- Some cleanup
    2.3    - 90-04-18/Reibert Olsson -- Multiple help lines, default settings
    2.2    - 90-02-13/Reibert Olsson -- SPA_Report
    2.1    - 89-12-03/Reibert Olsson -- SPA_Set, errfun is a SPA_FUNCTION,
   		 		        SPA_Toggle
    2.0(1) - 89-11-27/Reibert Olsson
    2.0    - 89-11-20/Reibert Olsson -- Adapted to SoftLab rules, SPA
----
    1.0    - 89-08-09/Reibert Olsson
\*--------------------------------------------------------------------*/
#include "spa.h"
#if _SPA_H_!=42
error "SPA header file version 4.2 required"
#endif

#include <stdio.h>
#include <string.h>
#ifdef __NEWC__
#include <stdlib.h>
#include <stdarg.h>
#else
#include <varargs.h>
#endif

typedef int boolean;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define PRIVATE static
#define PUBLIC

#ifndef SPA_IGNORE_CASE
#define SPA_IGNORE_CASE	TRUE	/* Unsensitive to the case of options */
#endif
#ifndef SPA_MATCH_PREFIX
#define SPA_MATCH_PREFIX TRUE	/* Unique prefix is matching options */
#endif
#ifndef SPA_PRINT_DEFAULT
#define SPA_PRINT_DEFAULT TRUE	/* Spa tries to report default in help */
#endif
#ifndef SPA_PRINT_ITEM_SZ
#define SPA_PRINT_ITEM_SZ 17	/* The item width in help printing */
#endif
#ifndef SPA_LANG
#define SPA_LANG 0		/* Language for messages (English) */
#endif

#if SPA_LANG==46		/* Swedish */
PRIVATE char *SpaFlagKeyWords[] = {
    "AV",    "PÅ",
    "NEJ",   "JA",
    "FALSK", "SANN",
    "NIL",   "T",
    NULL
};
PRIVATE char *SpaDefaultFormat = " (skönsvärde: %s)";
PRIVATE char *SpaStrArg = "Parametrar:";
PRIVATE char *SpaStrOpt = "Tillval:";
PRIVATE char *SpaStrUsg = "Användning:";
PRIVATE char *SpaStrNMK = "Nyckelord passar ej";
PRIVATE char *SpaStrAMK = "Tvetydigt nyckelord";
PRIVATE char *SpaStrNMO = "Tillval passar ej";
PRIVATE char *SpaStrAMO = "Tvetydigt tillval";
PRIVATE char *SpaStrILF = "Okänd funktionstyp";
PRIVATE char *SpaStrISC = "Otillåtet mängdtecken";
PRIVATE char *SpaStrVE  = "Förväntat värde saknas";
PRIVATE char *SpaStrIVE = "Heltal förväntat";
PRIVATE char *SpaStrRVE = "Flyttal förväntat";
PRIVATE char *SpaStrFRE = "Filen kunde ej öppnas för läsning";
PRIVATE char *SpaStrFWE = "Filen kunde ej öppnas för skrivning";
PRIVATE char *SpaStrTMA = "Överflödig parameter";
PRIVATE char *SpaStrAE  = "Parameterfel";

PRIVATE char *SpaAlertStr[] = {
    "Avlusning", "Information", "Varning", "Fel",
    "Allvarligt fel", "Internt fel", "Okänt fel",
    "Exekveringen avbryts"
};

#else				/* English */

PRIVATE char *SpaFlagKeyWords[] = {	/* The table for flag as argument */
    "OFF",   "ON",
    "FALSE", "TRUE",
    "NO",    "YES",
    "NIL",   "T",
    NULL
};

PRIVATE char *SpaDefaultFormat = " (default: %s)"; /* How to print default values */
PRIVATE char *SpaStrArg = "Arguments:";
PRIVATE char *SpaStrOpt = "Options:";
PRIVATE char *SpaStrUsg = "Usage:";
PRIVATE char *SpaStrURK = "Unrecognized keyword";
PRIVATE char *SpaStrAMK = "Ambiguous keyword";
PRIVATE char *SpaStrURO = "Unrecognized option";
PRIVATE char *SpaStrAMO = "Ambiguous option";
PRIVATE char *SpaStrILF = "Illegal function type";
PRIVATE char *SpaStrISC = "Illegal set character";
PRIVATE char *SpaStrVE  = "Value expected";
PRIVATE char *SpaStrIVE = "Integer value expected";
PRIVATE char *SpaStrRVE = "Real value expected";
PRIVATE char *SpaStrFRE = "File could not be opened for reading";
PRIVATE char *SpaStrFWE = "File could not be opened for writing";
PRIVATE char *SpaStrTMA = "Superfluous argument";
PRIVATE char *SpaStrAE  = "Argument error";

PRIVATE char *SpaAlertStr[] = {
    "Debug", "Information", "Warning", "Error",
    "Fatal error", "Internal error", "Unknown error",
    "Execution stopped"
};

#endif


/*----------------------------------------------------------------------
   These are to make the parameters to spaProcess accessible throughout
   this file 
*/
PRIVATE char **pArgV;
PRIVATE int pArgC;
PRIVATE int pArg;		/* Current arg, index in pArgV */
PRIVATE _SPA_ITEM *pArguments;
PRIVATE _SPA_ITEM *pOptions;

#ifdef __NEWC__

#define safeExecute(fun, item, raw, on) \
  if (fun) (*(void (*)(char *, char *, int))fun)(item->name, raw, on)
/*  if (fun) (*(SpaFun)fun)(item->name, raw, on) */

#define FUNCTION(N,A) N(
#define PROCEDURE(N,A) void N(
#define IN(T,N) T N
#define OUT(T,N) T* N
#define IS )
#define X ,

#else

#define safeExecute(fun, item, raw, on) \
  if (fun) (*(void (*)())fun)(item->name, raw, on)

#define FUNCTION(N,A) N A
#define PROCEDURE(N,A) void N A
#define IN(T,N) T N;
#define OUT(T,N) T* N;
#define X 
#define IS

#endif


PRIVATE SpaErrFun *pErrFun;	/* Points to errorfunction */

#define spaErr(m, a, s) (*pErrFun)(s, m, a)


PRIVATE struct {		/* Code reduction data structure */
    FILE *deffile;
    char *iomode;
} fileDefault[2] = {		/* We assume _SPA_OutFile-_SPA_InFile == 1 */
#ifdef STDIONONCONST
    { NULL, "r" },		/* Set file before use! */
    { NULL, "w" }
#else
    { stdin, "r" },
    { stdout, "w" }
#endif
};
#define mode(T) (fileDefault[(T)-_SPA_InFile].iomode)
#define file(T) (fileDefault[(T)-_SPA_InFile].deffile)


#if SPA_IGNORE_CASE
#include <ctype.h>
#define lwr(C) (isupper(C)? tolower(C): C)
#else
#define lwr(C) C
#endif

#define _SPA_EXACT (-TRUE)


/*----------------------------------------------------------------------
    Return SPA_MATCH_PREFIX if a true prefix is found, and _SPA_EXACT
    if both strings are equal. The string is stripped of any comments
    (i.e blank and following).
*/
PRIVATE int FUNCTION(match, (p, s))
    IN(register char, *p) X	/* User supplied argv-item */
    IN(register char, *s)	/* SPA_ITEM string */
IS {
    while (*p) {
    	if (*s==' ' || lwr(*s)!=lwr(*p)) return FALSE;
    	s++; p++;
    }
    return (*s!=' '? SPA_MATCH_PREFIX: _SPA_EXACT);
}


/*----------------------------------------------------------------------
    Go thru a datastructure and match() to get any hits. Returns the
    number of hits, and sets first found index (or -1).
*/
PRIVATE int FUNCTION(find, (ai, kws, kwSz, kwO, found))
    IN(char, *ai) X		/* User supplied argv-item */
    IN(register char, kws[]) X	/* The matching words */
    IN(int, kwSz) X		/* Size of kws */
    IN(int, kwO) X		/* Offset to kws.name */
    OUT(int, found)		/* The found items index */
IS {
    register int i, o;
    int c, hits = 0;

    *found = -1;
    for (o=kwO, i=0; *(char **)(&kws[o]); i++, o += kwSz) {
	c= match(ai, *(char **)(&kws[o]));
	if (c!=FALSE) {
	    if (*found==-1) *found = i;
	    if (c==_SPA_EXACT) { hits = 1; break; }
	    else hits++;
	}
    }
    return hits;
}


/*----------------------------------------------------------------------
    Go thru the keywords and match() to get any hits. Returns the
    found items index or the default.
*/
PRIVATE int FUNCTION(findKeyWord, (thisWord, keyWords, def))
    IN(char, *thisWord) X	/* User supplied argv-item */
    IN(char, *keyWords[]) X	/* The keywords */
    IN(int, def)		/* The default index */
IS {
    int found;

    switch (find(thisWord, (char *)keyWords, sizeof(char *), 0, &found)) {
    case 0:
	spaErr(SpaStrURK, thisWord, 'F');
    	break;
    case 1: 
    	break;
    default:
	spaErr(SpaStrAMK, thisWord, 'F');
	break;
    }
    return found<0 ? def: found;
}


/*----------------------------------------------------------------------
    Print help line for one SPA_ITEM.
*/
PRIVATE PROCEDURE(printItem, (name, help, def, set, kws))
    IN(char *, name) X		/* Name of item */
    IN(register char *, help) X	/* Help string */
    IN(char *, def) X		/* Default value string */
    IN(register char *, set) X	/* Points to set string (or is NULL) */
    IN(char **,kws)		/* Points to keyword array (or is NULL) */
IS {
    boolean nl = FALSE;
    
    printf("  %-*s ", SPA_PRINT_ITEM_SZ, name);
    if (strlen(name)>SPA_PRINT_ITEM_SZ)
	printf("\n  %-*s ", SPA_PRINT_ITEM_SZ, "");
    if (help) {
	printf("-- ");
	for (;;) {
	    for (;*help; help++) {
		if (*help=='\n') { help++; nl = TRUE; break; }
		putchar(*help);
	    }
	    if (!*help) break;
	    printf("\n  %-*s    ", SPA_PRINT_ITEM_SZ, "");
	    if (set) printf("%c -- ", *set++);
	    if (kws && *kws) printf("%s -- ", *kws++);
	}
#if SPA_PRINT_DEFAULT
	if (*def) { 
	    if (nl) printf("\n  %-*s   ", SPA_PRINT_ITEM_SZ, "");
	    printf(SpaDefaultFormat, def);
	}
#endif
    }
    putchar('\n');
}


/*----------------------------------------------------------------------
    Print a SPA_ITEMs name as argument. Static area!
*/
PRIVATE char prName[128];	/* Used by p[AO]Name */

PRIVATE char* FUNCTION(pAName, (item))
    IN(_SPA_ITEM *, item)
IS {
    switch (item->type) {
    case _SPA_Flag:
#ifdef CMS
	sprintf(prName, "(%s!%s)", SpaFlagKeyWords[1], SpaFlagKeyWords[0]);
#else
	sprintf(prName, "[%s|%s]", SpaFlagKeyWords[1], SpaFlagKeyWords[0]);
#endif
	break;
    case _SPA_Bits:
	sprintf(prName, "{%s}", item->s);
	break;
    default:
	sprintf(prName, "<%s>", item->name);
	break;
    }
    return prName;
}


/*----------------------------------------------------------------------
    Print a SPA_ITEMs name as option. Static area!
*/
PRIVATE char* FUNCTION(pOName, (item))
    IN(_SPA_ITEM *, item)
IS {
    static char *fmt[] = {
#ifdef CMS
	"-%s", "-(-)%s", "-(-)%s {%s}"
#else
	"-%s", "-[-]%s", "-[-]%s {%s}"
#endif
    };
    int i = 0;		 /* -> fmt[0] */

    switch (item->type) {
    case _SPA_Bits: i++; /* -> fmt[2] */
    case _SPA_Flag: i++; /* -> fmt[1] */
    default:
      break;
    }
    sprintf(prName, fmt[i], item->name, item->s);
    return prName;
}


/*----------------------------------------------------------------------
    Report one SPA_ITEM.
*/
PRIVATE PROCEDURE(reportItem, (item, name))
    IN(register _SPA_ITEM *, item) X
    IN(char *, name)
IS {
    char def[128];	/* Is this enough? Too much? No test ahead! */
    char *set = NULL;
    char **kws = NULL;

    switch (item->type) {
    case _SPA_None: return;
    case _SPA_Comment:
    	printf("%s\n", (item->help? item->help: ""));
    	return;
    default:
      break;
    }
    
    /* Set default values */
    *def = 0;
#if SPA_PRINT_DEFAULT
    switch (item->type) {
    case _SPA_Flag:
	strcpy(def, item->i? SpaFlagKeyWords[1]: SpaFlagKeyWords[0]);
	break;
    case _SPA_Integer:
	sprintf(def, "%d", item->i);
	break;
    case _SPA_Float:
	sprintf(def, "%g", item->f);
	break;
    case _SPA_String:
    case _SPA_InFile:
    case _SPA_OutFile:
	if (item->s) strcpy(def, item->s);
	break;
    case _SPA_KeyWord:
	kws = item->sp;
	strcpy(def, kws[item->i]);
	break;
    case _SPA_Bits: {
	register int i = 0, j = 1;
	def[0] = '{';
	set = item->s;	
	for (; set[i]; i++) {
	    if ((1<<i)&item->i) def[j++] = set[i];
	}
	def[j++] = '}'; def[j] = 0;
    } break;
    default:
	break;
    }
#endif
    printItem(name, item->help, def, set, kws);
}


/*----------------------------------------------------------------------
    Make a report list out of the arguments and options.
*/
PRIVATE PROCEDURE(report, (args, opts))
    IN(register _SPA_ITEM, args[]) X
    IN(register _SPA_ITEM, opts[])
IS {
    register int i;

    if (args[0].name && *args[0].name) printf("\n%s\n", SpaStrArg);
    for (i= 0; args[i].name && *args[i].name; i++)
	reportItem(&args[i], pAName(&args[i]));

    if (opts[0].name) printf("\n%s\n", SpaStrOpt);
    for (i= 0; opts[i].name; i++)
	reportItem(&opts[i], pOName(&opts[i]));

}


/*----------------------------------------------------------------------
    Assert that the file was opened.
*/
PRIVATE PROCEDURE(assertFile, (item))
    IN(register _SPA_ITEM, *item)
IS {
    if (!*item->FP) { /* open failure */
	spaErr((item->type==_SPA_InFile? SpaStrFRE: SpaStrFWE), pArgV[pArg], 'F');
	*item->FP = file(item->type);
	*item->sp = item->s;
    }
}

/*----------------------------------------------------------------------
    Set default values for one SPA_ITEM.
*/
PRIVATE PROCEDURE(setDefault, (item))
    IN(register _SPA_ITEM, *item)
IS {
    switch (item->type) {
    case _SPA_Flag:
    case _SPA_Integer:
    case _SPA_KeyWord:
    case _SPA_Bits:
	*item->ip = item->i;
	break;
    case _SPA_Float:
	*item->fp = item->f;
	break;
    case _SPA_String:
	*item->sp = item->s;
	break;
    case _SPA_InFile:
    case _SPA_OutFile:
	*item->sp = item->s;
	if (*item->sp && **item->sp)
	    *item->FP = fopen(*item->sp, mode(item->type));
	else *item->FP = file(item->type);
	assertFile(item);
	break;
    case _SPA_Function:
    case _SPA_Help:
    case _SPA_Comment:
    case _SPA_None:
	break;
    case _SPA_Private:
    default:
	spaErr(SpaStrILF, item->name, 'S');
	break;
    }
}


/*----------------------------------------------------------------------
    Execute an SPA_ITEM.
*/
PRIVATE PROCEDURE(execute, (item, option, on))
    IN(register _SPA_ITEM, *item) X
    IN(boolean, option) X	/* True if argv was an option */
    IN(register boolean, on)	/* True if argv was an on-option */
IS {
    if (item) {

	if (option)
	    switch (item->type) {
	    case _SPA_Flag:
	    case _SPA_Function:
	    case _SPA_Help:
    	    case _SPA_Comment:
	    case _SPA_None:
		break;
	    default:
	    /* An option using next argv-item goes here */
		if (++pArg>=pArgC) {
		    --pArg;	/* Too far, backup */
		    setDefault(item);
		    spaErr(SpaStrVE, pArgV[pArg], 'E');
		    goto postFun;
		}
	    }
	
	switch (item->type) {
	case _SPA_Flag:
	    if (option) *item->ip = on;
	    else {		/* Parse the argument */
		*item->ip = findKeyWord(pArgV[pArg], SpaFlagKeyWords, item->i)&1 ;
	    }
	    break;
	case _SPA_Bits: {
	    register char *arg, *bp;
	    boolean bon = on;

	    for (arg= pArgV[pArg]; *arg; arg++) { /* Go thru argument */
		if (*arg=='-') { bon = !bon; continue; }
		if (*arg=='*') { *item->ip = bon? -1: 0; continue; }
		for (bp= item->s; *bp; bp++) /* and descriptor*/
		    if (lwr(*arg)==lwr(*bp)) break;
		if (*bp) {
		    if (bon)
			*item->ip |= 1<<(bp-item->s);
		    else
			*item->ip &= ~(1<<(bp-item->s));
		} else {
		    static char tmp[128];
		    sprintf(tmp, "%s: %c", item->name, *arg);
		    spaErr(SpaStrISC, tmp, 'E');
		}
	    }
	} break;
	case _SPA_Integer:
	    if (sscanf(pArgV[pArg], "%i", item->ip)!=1) {
		/* Number not ok */
		setDefault(item);
		spaErr(SpaStrIVE, pArgV[pArg], 'E');
	    }
	    break;
	case _SPA_Float:
	    if (sscanf(pArgV[pArg], "%g", item->fp)!=1) {
		/* Number not ok */
		setDefault(item);
		spaErr(SpaStrRVE, pArgV[pArg], 'E');
	    }
	    break;
	case _SPA_String:
	    *item->sp= pArgV[pArg];
	    break;
	case _SPA_KeyWord:
	    *item->ip= findKeyWord(pArgV[pArg], item->sp, item->i);
	    break;
	case _SPA_InFile:
	case _SPA_OutFile:
#ifdef CMS
	    {	/* We do assemble CMS 3-part file names into one string */
		char *file, *extension, *disk;
	    
		file = pArgV[pArg];
		if (extension = spaArgument(1)) pArg++; else { extension = ""; } 
		if (disk = spaArgument(1)) pArg++; else { disk = ""; }
		*item->sp = (char*)malloc( 40 ); /* What is the maximum? */
	    		/* strlen(file) + strlen(extension) + strlen(disk) + 3 */
		sprintf(*item->sp,"%s %s %s", file, extension, disk);
		pArgV[pArg] = *item->sp; /* ! This could cause trouble if a hook reads previous items */
            }
#else
	    *item->sp = pArgV[pArg];
#endif
	    if (*item->sp && **item->sp) {
		if (*item->FP==file(item->type))
		    *item->FP = fopen(*item->sp, mode(item->type));
		else
		    *item->FP =
			freopen(*item->sp, mode(item->type), *item->FP);
	    } else *item->FP = file(item->type);
	    assertFile(item);
	    break;
	case _SPA_Help:
	    safeExecute(item->hFun, item, pArgV[pArg], on);
	    report(pArguments, pOptions);
	    break;
    	case _SPA_Comment:
	case _SPA_Function:
	case _SPA_None:
	case _SPA_Private:
	    break;
	default:
	    /* This means that someone has dribbled with the option tables */
	    spaErr(SpaStrILF, pArgV[pArg], 'S');
	    break;
	}
    postFun:
	safeExecute(item->postFun, item, pArgV[pArg], on);
    }
}


/*----------------------------------------------------------------------
    Detect if current argv-item is an option, if so do as specified.
*/
PRIVATE boolean FUNCTION(option, (options))
    IN(_SPA_ITEM, options[])		/* Possible options */
IS {
    int found;
    register char *argvItem = pArgV[pArg];
    register int start;
    
    if (argvItem[0]=='-') {
    	start = (argvItem[1]=='-'? 2: 1);
    	if (argvItem[start]) {
   	    switch (find(&argvItem[start],
			 (char *)options,
			 sizeof(_SPA_ITEM),
			 (int)((unsigned long)&options[0].name -
			       (unsigned long)&options[0]),
			 &found)) {
    	    case 0:
  	        spaErr(SpaStrURO, argvItem, 'E');
	        break;
    	    case 1:
	        execute(&options[found], TRUE, start==1);
	        break;
    	    default:
  	        spaErr(SpaStrAMO, argvItem, 'E');
	        break;
    	    }
    	    return TRUE;
    	}
    }
    return FALSE;
}


/***********************************************************************
    Builtin functions.
*/

/*----------------------------------------------------------------------
    Error function, uses spaAlert.
*/
PRIVATE SPA_ERRFUN(biErrFun) {
    spaAlert(sev, "%s: %s: %s", SpaStrAE, msg, add);
}

PRIVATE SPA_FUN(biExit) { exit(1); }

PRIVATE SPA_FUN(biArgTooMany) {
    spaErr(SpaStrTMA, rawName, 'E');
}

PRIVATE SPA_DECLARE(biArguments)
  SPA_FUNCTION("", NULL, biArgTooMany)
SPA_END

#ifdef __NEWC__
PRIVATE SPA_FUN(biUsage);	/* Forward */
#else
PRIVATE void biUsage();
#endif

PRIVATE SPA_DECLARE(biOptions)
#if SPA_LANG==46
  SPA_HELP("hjälp", "ger denna utskrift", biUsage, biExit)
#else
  SPA_HELP("help", "this help", biUsage, biExit)
#endif
SPA_END

#ifdef CMS
# define oSTART	" ("
# define oALT	"!"
# define oSTOP	")..."
#else
# define oSTART	" ["
# define oALT	"|"
# define oSTOP	"]..."
#endif

PRIVATE SPA_FUN(biUsage) {
    register int i, j;

    printf("%s %s", SpaStrUsg, SpaAlertName);
    if (pOptions!=biOptions) {
    	printf(oSTART);
	for (i=j=0; pOptions[i].name; i++)
	    if (pOptions[i].type && *pOptions[i].name) {
		if (j++>0) printf(oALT);
		printf(pOName(&pOptions[i]));
	    }
    	printf(oSTOP);
    }
    if (pArguments!=biArguments) {
	for (i=0; pArguments[i].name; i++)
	    if (pArguments[i].type && *pArguments[i].name) 
		printf(" %s", pAName(&pArguments[i]));
    }
    printf("\n");
}


/***********************************************************************
    Public data & functions.
*/

/*======================================================================
    Program name for Alerts (tail of argv[0])
*/
PUBLIC char *SpaAlertName = NULL;

/*======================================================================
    Alert at this level and higher
*/
PUBLIC char SpaAlertLevel = 'I';

/*======================================================================
    File for alert messages (default stderr)
*/
PUBLIC FILE *SpaAlertFile
#ifdef STDIONONCONST
	= NULL;	/* In this env stderr isn't a constant */
#else
	= stderr;
#endif

/*----------------------------------------------------------------------
    Returns error severity as a number [0..6].
*/
PRIVATE int FUNCTION(level, (sev))
    IN(char, sev)		/* Index in SpaAlertStr */
IS {
    static char *sevstr = "DIWEFS";
    register char *s;
	
    s = strchr(sevstr, sev);
    return s? s-sevstr: 6;
}

/*======================================================================
    Error notification (name: sev! <fmt ...>) to user.
    Exits on severe errors.
*/
#ifdef __NEWC__
PUBLIC void spaAlert(		/* Error notification; Exits on severe errors */
    char sev,			/* IN - [DIWEFS] */
    char * fmt,			/* IN - printf-format for additional things */
    ...				/* IN - additional things */
){
#else
PUBLIC void spaAlert(va_alist)
    va_dcl
{
    char sev;
    char *fmt;
#endif
    va_list ap;
    register int lev;

#ifdef __NEWC__
    va_start(ap, fmt);
#else
    va_start(ap);
    sev = va_arg(ap, char);
    fmt = va_arg(ap, char *);
#endif
    lev = level(sev);
    if (lev>=level(SpaAlertLevel)) {
	if (SpaAlertName) fprintf(SpaAlertFile, "%s: ", SpaAlertName);
	fprintf(SpaAlertFile, "%s! ", SpaAlertStr[lev]);
	vfprintf(SpaAlertFile, fmt, ap);
	fprintf(SpaAlertFile, "\n");
    }
    va_end(ap);

    if (lev>3 /*level('E')*/) { spaAlert('I', SpaAlertStr[7]); exit(1); }
}


/*======================================================================
    Return Nth argument in argv, NULL if outside argv.
*/
PUBLIC char * FUNCTION(spaArgumentNo, (n))
    IN(register int, n)
IS {
    return ( (n>=pArgC || n<0)? NULL: pArgV[n] );
}


/*======================================================================
    Return Nth relative argument, NULL if outside argv.
*/
PUBLIC char * FUNCTION(spaArgument, (n))
    IN(int, n)
IS {
    return spaArgumentNo(pArg+n);
}


/*======================================================================
    Skip arguments N steps forward (or backward), stays inside argv.
*/
PUBLIC PROCEDURE(spaSkip, (n))
    IN(int, n)
IS {
    register int t= pArg+n;

    pArg= ( (t>=pArgC)? pArgC: (t<0? 0: t) );
}


/*======================================================================
    Will walk through the arguments and try to act according to spec's.
    Must not be called recursivly.
*/
PUBLIC int FUNCTION(_spaProcess, (argc, argv, arguments, options, errfun))
    IN(int, argc) X
    IN(char, *argv[]) X
    IN(_SPA_ITEM, arguments[]) X
    IN(_SPA_ITEM, options[]) X
    IN(SpaErrFun, *errfun)
IS {
    register int a, n;
    register char *s;

#ifdef STDIONONCONST
    fileDefault[0].deffile = stdin;
    fileDefault[1].deffile = stdout;
    if (!SpaAlertFile) SpaAlertFile = stderr;
#endif

    pArgC= argc;
    pArgV= (char **)argv;
    pOptions= (options? options: biOptions);
    pArguments= (arguments? arguments: biArguments);
    pErrFun= (errfun? errfun: biErrFun);

    if (!SpaAlertName) {	/* If no name given, get it from argv[0] */
    	s = strrchr(argv[0], '/');
    	SpaAlertName = s? s+1: argv[0];
    }

    for (n= 0; pArguments[n].name; n++) setDefault(&pArguments[n]);
    for (a= -1, n= 0; pOptions[n].name; n++) {
	setDefault(&pOptions[n]);
    	if (pOptions[n].type==_SPA_Help) a = n;
    }
    if (a<0) {				/* No help declared */
	pOptions[n] = biOptions[0];	/* Insert builtin d:o */
    }

    for (a= n= 0, pArg= 1; pArg<pArgC; pArg++)
	if (!option(pOptions)) {
	    n++;
	    if (pArguments[a].name) {
		execute(&pArguments[a], FALSE, TRUE);
		if (pArguments[a+1].name) a++;
	    }
	}
    return n;
}

/*======================================================================
    (Clean up and then) exit.
*/
PUBLIC PROCEDURE(spaExit, (exitCode))
    IN(int, exitCode) 
IS {
    exit(exitCode);
}

/*======================================================================
    Special wrapper for THINK-C/MacOs emulated argv handling.
*/
#ifdef __mac__
#include <console.h>
PUBLIC int FUNCTION(_spaPreProcess, (argc, argv, arguments, options, errfun))
    IN(int, *argc) X
    IN(char, **argv[]) X
    IN(_SPA_ITEM, arguments[]) X
    IN(_SPA_ITEM, options[]) X
    IN(SpaErrFun, *errfun)
IS {
#ifdef THINK_C
   console_options.title = CurApName;
#endif
   *argc = ccommand(argv);
   return _spaProcess(*argc, *argv, arguments, options, errfun);
}
#endif


/* == EoF =========================================================== */
