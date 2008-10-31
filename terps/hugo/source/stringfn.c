/*
	STRINGFN.C

	String manipulation functions

	Copyright (c) 1995-2006 by Kent Tessman
	
	Only if ALLOW_NESTING is #defined may these calls be nested, and then
	only two at a time (noting that it takes another 1025 bytes of global
	storage to do so).
*/


#include <stdio.h>
#include <string.h>
#include <ctype.h>

char *Left(char *a, int l);
char *Ltrim(char *a);
char *Mid(char *a, int pos, int n);
char *Right(char *a, int l);
char *Rtrim(char *a);

#ifdef QUICKC
#define OMIT_EXTRA_STRING_FUNCTIONS
#endif

#ifndef OMIT_EXTRA_STRING_FUNCTIONS
#define EXTRA_STRING_FUNCTIONS
#endif

#if defined (EXTRA_STRING_FUNCTIONS)
char *itoa(int a, char *buf, int base);
char *strlwr(char *s);
char *strnset(char *s, int c, size_t l);
char *strupr(char *s);
#endif


/* GETTEMPSTRING

	Prevents having to make sure each string-returning function has its
	own static char array to copy into.  In other words, for five
	functions, we don't need 5 * 1K each of static char array.

	NOTE:  Assumes that no more than NUM_TEMPSTRINGS nested
	string-manipulations will be done at once.
*/

#ifndef ALLOW_NESTING
static char tempstring[1025];
#else
#define NUM_TEMPSTRINGS 2
static char tempstring[NUM_TEMPSTRINGS][1025];
static char tempstring_count = 0;

static char *GetTempString(void)
{
	static char *r;

	r = &tempstring[(int)tempstring_count][0];
	if (++tempstring_count >= NUM_TEMPSTRINGS) tempstring_count = 0;

	return r;
}
#endif


/* The following string-manipulation functions closely mimic BASIC-language
   string functionality.   They do not alter the provided string; instead,
   they return a pointer to a static (modified) copy.
*/


/* LEFT */

char *Left(char a[], int l)
{
	static char *temp;
	int i;

#ifdef ALLOW_NESTING
	temp = GetTempString();
#else
	temp = &tempstring[0];
#endif
	if (l > (int)strlen(a))
		l = strlen(a);
	for (i = 0; i<l; i++)
		temp[i] = a[i];
	temp[i] = '\0';
	return temp;
}


/* LTRIM */

char *Ltrim(char a[])
{
	static char *temp;

#ifdef ALLOW_NESTING
	temp = GetTempString();
#else
	temp = &tempstring[0];
#endif
	strcpy(temp, a);
	while (temp[0]==' ' || temp[0]=='\t')
		strcpy(temp, temp+1);
	return temp;
}


/* MID */

char *Mid(char a[], int pos, int n)
{
	static char *temp;
	int i;

#ifdef ALLOW_NESTING
	temp = GetTempString();
#else
	temp = &tempstring[0];
#endif
	pos--;
	if (pos+n > (int)strlen(a))
		n = strlen(a)-pos;
	for (i = 0; i<n; i++)
		temp[i] = a[pos+i];
	temp[i] = '\0';
	return temp;
}


/* RIGHT */

char *Right(char a[], int l)
{
	static char *temp;
	int i;

#ifdef ALLOW_NESTING
	temp = GetTempString();
#else
	temp = &tempstring[0];
#endif
	if (l > (int)strlen(a))
		l = strlen(a);
	for (i = 0; i<l; i++)
		temp[i] = a[strlen(a)-l+i];
	temp[i] = '\0';
	return temp;
}


/* RTRIM */

char *Rtrim(char a[])
{
	static char *temp;
	int len;

#ifdef ALLOW_NESTING
	temp = GetTempString();
#else
	temp = &tempstring[0];
#endif
	strcpy(temp, a);
	while (((len = strlen(temp))) && (temp[len-1]==' ' || temp[len-1]=='\t'))
		strcpy(temp, Left(temp, len-1));
	return temp;
}


#if defined (EXTRA_STRING_FUNCTIONS)

char *itoa(int a, char *buf, int base)
{
	/* This only works if base is 10 (which it will be) */
	sprintf(buf, "%d" ,a);
	return buf;
}

char *strlwr(char *s)
{
	int i;

	i = 0;
	while (*(s+i)!='\0')
	{
		*(s+i) = tolower(*(s+i));
		i++;
	}
	return s;
}

char *strnset(char *s, int c, size_t l)
{
	int i;

	for (i=0; i<(int)l; i++) *(s+i) = (char)c;
	return s;
}

char *strupr(char *s)
{
	int i;

	i = 0;
	while (*(s+i)!='\0')
	{
		*(s+i) = toupper(*(s+i));
		i++;
	}
	return s;
}

#endif
