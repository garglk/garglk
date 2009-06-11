#if     !__STDC__ || defined(_POSIX_)

/* Non-ANSI names for compatibility */

#define CLK_TCK  CLOCKS_PER_SEC

#ifdef  _NTSDK

/* Declarations and definitions compatible with the NT SDK */

#define daylight _daylight
/* timezone cannot be #defined because of <sys/timeb.h> */

#ifndef _POSIX_
#define tzname  _tzname
#define tzset   _tzset
#endif /* _POSIX_ */

#else   /* ndef _NTSDK */

#if     defined(_DLL) && defined(_M_IX86)

#define daylight   (*__p__daylight())
/* timezone cannot be #defined because of <sys/timeb.h>
   so CRT DLL for win32s will not have timezone */
_CRTIMP extern long timezone;
#define tzname     (__p__tzname())

#else   /* !(defined(_DLL) && defined(_M_IX86)) */

_CRTIMP extern int daylight;
_CRTIMP extern long timezone;
_CRTIMP extern char * tzname[2];

#endif  /* !(defined(_DLL) && defined(_M_IX86)) */
