#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines some macros for the standard tokenizer class.  
 */

/* 
 *   token rule flags 
 */

/*
 *   The tokenizer's return value is a list of tokens.  Each token
 *   represented by a sublist describing the token.  These macros extract
 *   information from the token sublist.  
 */

/* 
 *   Get the token value.  This is the parsed representation of the token;
 *   in most cases, this is simply the text of the original token, although
 *   it might be converted in some way (words are usually converted to
 *   lower-case, for example).  
 */
#define getTokVal(tok) ((tok)[1])

/*
 *   Get the token type.  This is a token enum value describing the type of
 *   the token. 
 */
#define getTokType(tok) ((tok)[2])

/*
 *   Get the token's original text.  This is the original text matched from
 *   the tokenized string. 
 */
#define getTokOrig(tok) ((tok)[3])
