/* $Header: d:/cvsroot/tads/TADS2/cmap.h,v 1.2 1999/05/17 02:52:14 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  cmap.h - character mapping definitions
Function
  
Notes
  
Modified
  05/31/98 MJRoberts  - Creation
*/

#ifndef CMAP_H
#define CMAP_H

struct errcxdef;

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the default character mappings.  If no mapping file is to
 *   be read, this function will establish identify mappings that leave
 *   characters untranslated. 
 */
void cmap_init_default(void);


/*
 *   Load a character map file.  Returns zero on success, non-zero on
 *   failure.  If filename is null, we'll use the default mapping.
 */
int cmap_load(char *filename);


/*
 *   Turn off character translation.  This overrides any game character
 *   set that we find and simply uses the default translation. 
 */
void cmap_override(void);


/*
 *   Set the game's internal character set.  This should be called when a
 *   game is loaded, and the game specifies an internal character set.  If
 *   there is no character map file explicitly loaded, we will attempt to
 *   load a character mapping file that maps this character set to the
 *   current native character set.  Signals an error on failure.  This
 *   routine will succeed (without doing anything) if a character set has
 *   already been explicitly loaded, since an explicitly-loaded character
 *   set overrides the automatic character set selection that we attempt
 *   when loading a game.
 *   
 *   argv0 must be provided so that we know where to look for our mapping
 *   file on systems where mapping files are stored in the same directory
 *   as the TADS executables.  
 */
void cmap_set_game_charset(struct errcxdef *errctx,
                           char *internal_id, char *internal_ldesc,
                           char *argv0);


/* ------------------------------------------------------------------------ */
/*
 *   Mapping macros 
 */

/* map a native character (read externally) into an internal character */
#define cmap_n2i(c) (G_cmap_input[(unsigned char)(c)])

/* map an internal character into a native character (for display) */
#define cmap_i2n(c) (G_cmap_output[(unsigned char)(c)])


/* ------------------------------------------------------------------------ */
/*
 *   Global character mapping tables.  The character map is established at
 *   start-up. 
 */

/* 
 *   input-mapping table - for native character 'n', cmap_input[n] yields
 *   the internal character code 
 */
extern unsigned char G_cmap_input[256];

/*
 *   output-mapping table - for internal character 'n', cmap_output[n]
 *   yields the output character code 
 */
extern unsigned char G_cmap_output[256];

/* the ID of the loaded character set */
extern char G_cmap_id[5];

/* the full name (for display purposes) of the loaded character set */
#define CMAP_LDESC_MAX_LEN  40
extern char G_cmap_ldesc[CMAP_LDESC_MAX_LEN + 1];

/*
 *   Maximum expansion for an HTML entity mapping 
 */
#define CMAP_MAX_ENTITY_EXPANSION  50


/* ------------------------------------------------------------------------ */
/* 
 *   Signatures for character map files.  The signature is stored at the
 *   beginning of the file.  
 */

/* single-byte character map version 1.0.0 */
#define CMAP_SIG_S100  "TADS2 charmap S100\n\r\01a"

#endif /* CMAP_H */

