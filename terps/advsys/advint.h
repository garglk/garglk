/* advint.h - adventure interpreter definitions */
/*
	Copyright (c) 1986, by David Michael Betz
	All rights reserved
*/

#include <stdio.h>
#include <ctype.h>

/* useful definitions */
#define TRUE		1
#define FALSE		0
#define EOS		'\0'

/* program limits */
#define STKSIZE		500

/* code completion codes */
#define FINISH		1
#define CHAIN		2
#define ABORT		3
