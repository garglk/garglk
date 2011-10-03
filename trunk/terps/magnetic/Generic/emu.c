/****************************************************************************\
*
* Magnetic - Magnetic Scrolls Interpreter.
*
* Written by Niclas Karlsson <nkarlsso@abo.fi>,
*            David Kinder <davidk.kinder@virgin.net>,
*            Stefan Meier <Stefan.Meier@if-legends.org> and
*            Paul David Doherty <pdd@if-legends.org>
*
* Copyright (C) 1997-2008  Niclas Karlsson
*
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; either version 2 of the License, or
*     (at your option) any later version.
*
*     This program is distributed in the hope that it will be useful,
*     but WITHOUT ANY WARRANTY; without even the implied warranty of
*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*     GNU General Public License for more details.
*
*     You should have received a copy of the GNU General Public License
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* History:
*
* 0.1  123096 : Making the 68000 emulation work. Converting C64 6510 code to
*     -010297   C isn't all that much fun. Don't mention the word "BUG".
*
* 0.2  010397 : Debugging, debugging, debugging... Probably quite a lot left
*               as well. Haven't tried it out in practice yet.
*
* 0.3  010497 : String decoding now works. Trying out on the real thing - more
*               bugs, modest success. Implementing some very specialized
*               instructions. 0xA0FF gives the word nightmare a whole new
*               meaning.
*
* 0.4  010597 : Getting better... More specialized instructions coming up. The
*               output routines are up and running as well. Occasional
*               emulation bugs still show up due to my MS-style program
*               developing techniques ;-) First playable version [The Pawn].
*
* 0.5  010697 : Trying out with Jinxter. Sigh... Back to reverse engineering
*               (10+ new instructions, separate dictionary and two string
*               banks - these guys sure had some space problems).
*
* 0.6  010997 : Guild of Thieves seems to work now as well. Jinxter still has
*               a small problem with the parsing (I know of _one_ different
*               reaction) - probably due to the not-so-nice dict_lookup2().
*               Load/Save implemented.
*
* 0.7  011097 : The last (major) bug is now fixed (an 8-bit variable instead
*               of a 16-bit one messed up the adjectives). Small cleanup.
*
* 0.8  011397 : Now also runs Fish and Corruption. Small status-bar fix.
*
* 0.9  011497 : New single-file format. Improved user friendliness.
*
* 0.91 011597 : More cleaning. Random number range fixed. Other minor bugfixes.
*
* 0.92 011897 : Major dict_lookup() overhaul. Minor cleanups. <= 64K memory
*               blocks. Seems like the file format wasn't that great after all
*               (most suitable for one big block).
*
* 0.93 012097 : New file format. Fast restarts (no loading). UNDO implemented.
*               Subtle but fatal bug in Write_string() removed [Corruption].
*
* 0.94 012397 : Emulation bug (rotations) fixed. Prng slightly improved, or
*               at least changed. :-) Admode $xx(PC,[A|D]x) implemented.
*
* 0.95 012597 : Another one bites the dust - bit operations with register
*               source were broken. __MSDOS__ ifdef inserted (changes by
*               David Kinder). Error reporting improved (initiative by
*               Paul David Doherty).
*
* 0.96 012697 : No flag changes after MOVE xx,Ax (version>1) - Duhh...
*
* 0.97 020497 : Small output handling bug (0x7E-handling) for version>=3
*
* 0.98 040497 : Pain, agony... Another bit operation bug spotted and removed.
*               A difference between version 0 and the rest (MOVEM.W) caused
*               problems with Myth. Also, findproperties behaved badly with
*               bit 14 set (version 3 only?). dict_lookup() didn't recognise
*               composite words properly.
*
* 0.99 041697 : ADDQ/SUBQ xx,Ax doesn't set flags for (version>=3) [corruption]
*               Small dict_lookup() fix (must be checked thoroughly once more).
*               Difference between versions in findproperties caused problems
*               with Jinxter. [light match/unicorn]. Integrated gfx-handling.
*
* 0.9A 050397 : = instead of == caused problems. Sign error in do_findprop()
*               Stupid A0F1 quirk removed. SAVEMEM flag added.
*
* 0.9B 050997 : Small ms_showpic() modification.
*
* 0.9C 051297 : Fixes for running Magnetic Windows games.
*
* 0.9D 080297 : Minor improvements to save some memory. Bug fix: Last picture
*               couldn't be extracted when SAVEMEM was set. Use time() to get
*               initial random seed.
*
* 1.0  082897 : First public release.
*
* 2.0  081599 : [DK] Fixed Wonderland bug: ADDA and SUBA don't change
*               the condition codes.
*               [DK] Changed the A0EF line A opcode to do nothing, rather
*               than exit the interpreter. This opcode is used by the
*               PRINTER command available in some games.
*
* 2.0  082799 : [DK] Merged in Stefan Jokisch's MS-DOS changes.
*             : [DK] Changed MS-DOS ifdefs to also depend on __BORLANDC__.
*
* 2.0  092699 : [DK] ADDA and SUBA always operate on the entire address
*             : register, rather than using the opcode size.
*
* 2.0  101699 : [DK] Changed handling of string buffer to cope with
*             : MS-DOS limitations.
*
* 2.0  102399 : [DK] Fixed spurious capitalization and leading white
*               space problems.
*
* 2.0  102499 : [DK] Hopefully fixed problem in dict_lookup() with
*               dictionary words ending with a space character.
*
* 2.0  102699 : [SM] Added code to line_a DF for displaying pictures
*               in version 4 games. Also modified line_a E1 (readline)
*               for additional security check to avoid false DF actions
*
* 2.0  102799 : [SM] Changed line_a DF for game locations where the
*               picture request is not the first DF after keyboard input.
*
* 2.0  102999 : [SM] Another slight modification of DF for Collection games
*               compatibility
*
* 2.0  103099 : [DK] Fixed "#undo" for version 4 games.
*
* 2.0  110299 : [SM] One more fix to DF code for image indexing
*
* 2.0  110799 : [DK] Disabled calls to ms_statuschar() for version 4 games.
*               Disabled printing of spurious '@' characters to the status
*               bar (Spectrum Pawn 2.3)
*
* 2.0  110999 : [DK] Added an extra allowed return code to ms_getchar()
*               of 1, which means that the calling opcode should be
*               terminated immediately. This is needed to prevent possible
*               corruption of the game's memory in interpreters which
*               allow a new game to be loaded without restarting
*               (e.g. the Windows version).
*
* 2.0  112099 : [DK] Allowed all games to have a code segment greater than
*               65536 bytes. This is needed for Acorn Jinxter 1.3 to run.
*
* 2.0  112199 : [NK] Fixed dictionary code for Guild of Thieves 1.3.
*
* 2.0  112199 : [PDD] Made whether or not a game has a separate dictionary
*               independant of the version number.
*
* 2.0  121399 : [DK] Changed fix for spurious capitalization to apply to
*               all game versions.
*
* 2.0  011500 : [DK] Added support for a new graphics format for the
*               Magnetic Windows games.
*
* 2.0  012900 : [DK] Fixed a bug in ms_extract2 and tidied up ms_showpic
*               and ms_extract.
*
* 2.0  020500 : [DK] For v4, ms_showpic() is only called if pic_index > 0.
*
* 2.0  021100 : [DK] Updated #define for MSDOS to include lfopen().
*
* 2.0  022700 : [DK] A0E3 appears to be used to turn windows on and off.
*               If it is called with d1 == 0 then graphics are turned off.
*               [DK] Fixed a problem with v4 graphics not showing after
*               loading a new .mag file into the interpreter.
*
* 2.0  040900 : [DK] Completely changed the graphics format for v4 games.
*
* 2.0  041500 : [DK] ms_get_anim_frame() now returns a masking flag.
*
* 2.0  041700 : [SM] ms_get_anim_frame() now returns the frame mask. One bug
*               removed that messed up the frame sequence.
*
* 2.0  042200 : [DK] More animation work.
*
* 2.0  050400 : [SM] Even more animation work.
*
* 2.0  050500 : [SM] Oh no, more animation work.
*
* 2.0  050700 : [DK] A little tidying up.
*
* 2.0  060300 : [DK] More tidying and adding type casts to prevent
*               compiler warnings.
*
* 2.0  061800 : [DK] Merged in Stefan's latest animation changes.
*
* 2.0  070900 : [DK] Figured out what the final argument to the animation
*               command 0x01 means, with the result that nearly all the
*               animations in Wonderland play correctly.
*
* 2.0  072300 : [DK] Added an extra check to A0E3 to stop graphics being
*               accidentally turned off in the Collection Vol. 1 games.
*               Added animation command 0x00 (stop) for Corruption.
*
* 2.0  080600 : [SM] Minor change to A0DF code for preventing mysterious
*               crash in MW-Fish!
*
* 2.0  081700 : [SM] Adjusted lower code offsets for A0DF, now the missing 
*               Wonderland image comes up correctly.
*
* 2.0  081700 : [SM] Changed 03 animation command to act as "repeat comannd"
*               with Collection games and "Stop" for Wonderland.
*
* 2.0  081700 : [SM] Changes to the 0x05 animation command for "outpal"
*               picture.
*
* 2.0  082800 : [DK] Added ms_anim_is_repeating(), plus a few more minor
*               corrections.
*
* 2.0  092300 : [DK] Added a workaround for problems with Wonderland's
*               "catter" animation.
*
* 2.1  032002 : [SM] Added support for Magnetic Windows' online hints.
*
* 2.1  040202 : [SM] Added small A0DF workaround for non-supported functions.
*
* 2.1  042002 : [DK] Reset hint variables in ms_freemem().
*
* 2.2  020303 : [DK] Cleaned up hint code.
*
* 2.2  022303 : [DK] Added fallback hint support if ms_showhints() returns 0.
*               [DK] Added argument to ms_getchar() to indicate if #undo
*               should be translated.
*
* 2.2  022503 : [DK] Memory for hints is only allocated when needed.
*
* 2.2  031203 : [DK] Reduced the memory used for hints to about 45k, which
*               seems to be small enough for MS-DOS.
*
* 2.3  080722 : [SM] Added support for Wonderland music scores. The scores 
*               are identified by the accompanying picture name.
*
* 2.3  080723 : [SM] Added #sound control command to toggle sound support 
*               on interpreter level
*
* 2.3  080725 : [SM] Added isSoundEnabled to check sound status on game level
*
* 2.3  080804 : [SM] line_A DF code completely rewritten. SoundEnabled,
*               basetable and gfx_table removed 
*
* 2.3  080811 : [DK] Changed prototype for ms_sndextract and removed
*               memory leaks.
*
* 2.3  080811 : [SM] line_A DF rewrite broke hints support. Fixed.
*
* 2.3  080812 : [DK] Changed prototype for ms_playmusic and removed the
*               need for ms_sndextract as an externally visible function.
*
\****************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "defs.h"

#if defined(__MSDOS__) && defined(__BORLANDC__)

#include <alloc.h>

#define fread(p,s,n,f) lfread(p,s,n,f)
#define fopen(f,m)     lfopen(f,m)
#define malloc(s)      farmalloc(s)
#define free(p)        farfree(p)

extern long lfread(void far *, long, long, FILE far *);
extern FILE far * lfopen(const char far *, const char far *);

#endif

type32 dreg[8], areg[8], i_count, string_size, rseed = 0, pc, arg1i, mem_size;
type16 properties, fl_sub, fl_tab, fl_size, fp_tab, fp_size;
type8 zflag, nflag, cflag, vflag, byte1, byte2, regnr, admode, opsize;
type8 *arg1, *arg2, is_reversible, running = 0, tmparg[4] = {0, 0, 0, 0};
type8 lastchar = 0, version = 0, sd = 0;
type8 *decode_table, *restart = 0, *code = 0, *string = 0, *string2 = 0;
type8 *string3 = 0, *dict = 0;
type8 quick_flag = 0, gfx_ver = 0, *gfx_buf = 0, *gfx_data = 0;
type8 *gfx2_hdr = 0, *gfx2_buf = 0;
type8s *gfx2_name = 0;
type16 gfx2_hsize = 0;
FILE *gfx_fp = 0;
type8 *snd_buf = 0, *snd_hdr = 0;
type16 snd_hsize = 0;
FILE *snd_fp = 0;

const type8s undo_ok[] = "\n[Previous turn undone.]";
const type8s undo_fail[] = "\n[You can't \"undo\" what hasn't been done!]";
type32 undo_regs[2][18], undo_pc, undo_size;
type8 *undo[2] = {0, 0}, undo_stat[2] = {0, 0};
type16 gfxtable = 0, table_dist = 0;
type16 v4_id = 0, next_table = 1;

struct picture
{
	type8 * data;
	type32 data_size;
	type16 width;
	type16 height;
	type16 wbytes;
	type16 plane_step;
	type8 * mask;
};

#ifndef NO_ANIMATION

struct lookup
{
	type16s flag;
	type16s count;
};

#define MAX_POSITIONS 20
#define MAX_ANIMS 200
#define MAX_FRAMES 20

struct picture anim_frame_table[MAX_ANIMS];
type16 pos_table_size = 0;
type16 pos_table_count[MAX_POSITIONS];
struct ms_position pos_table[MAX_POSITIONS][MAX_ANIMS];
type8 * command_table = 0;
type16s command_index = -1;
struct lookup anim_table[MAX_POSITIONS];
type16s pos_table_index = -1;
type16s pos_table_max = -1;
struct ms_position pos_array[MAX_FRAMES];
type8 anim_repeat = 0;

#endif

/* Hint support */
#define MAX_HINTS 260
#define MAX_HCONTENTS 30000
struct ms_hint* hints = 0;
type8* hint_contents = 0;
const type8s no_hints[] = "[Hints are not available.]\n";
const type8s not_supported[] = "[This function is not supported.]\n";

#if defined(LOGEMU) || defined(LOGGFX) || defined(LOGHNT)
FILE *dbg_log;
#ifndef LOG_FILE
#error LOG_FILE must be defined to be the name of the log file.
#endif
#endif

/* prototypes */
type32 read_reg(int, int);
void write_reg(int, int, type32);

#define MAX_STRING_SIZE  0xFF00
#define MAX_PICTURE_SIZE 0xC800
#define MAX_MUSIC_SIZE   0x4E20

#ifdef LOGEMU
void out(char *format,...)
{
	va_list a;

	va_start(a, format);
	vfprintf(dbg_log, format, a);
	va_end(a);
}
#endif
#if defined(LOGGFX) || defined(LOGHNT)
void out2(char *format,...)
{
	va_list a;

	va_start(a, format);
	vfprintf(dbg_log, format, a);
	va_end(a);
	fflush(dbg_log);
}
#endif

/* Convert virtual pointer to effective pointer */
type8 *effective(type32 ptr)
{
	if ((version < 4) && (mem_size == 0x10000))
		return &(code[ptr & 0xffff]);
	if (ptr >= mem_size)
	{
		ms_fatal("Outside memory experience");
		return code;
	}
#if defined(__MSDOS__) && defined(__BORLANDC__)
	return (type8 *) (((type8 huge *) code) + ptr);
#else
	return &(code[ptr]);
#endif
}

type32 read_l(type8 * ptr)
{
	return (type32) ((type32) ptr[0] << 24 | (type32) ptr[1] << 16 | (type32) ptr[2] << 8 | (type32) ptr[3]);
}

type16 read_w(type8 * ptr)
{
	return (type16) (ptr[0] << 8 | ptr[1]);
}

void write_l(type8 * ptr, type32 val)
{
	ptr[3] = (type8) val;
	val >>= 8;
	ptr[2] = (type8) val;
	val >>= 8;
	ptr[1] = (type8) val;
	val >>= 8;
	ptr[0] = (type8) val;
}

void write_w(type8 * ptr, type16 val)
{
	ptr[1] = (type8) val;
	val >>= 8;
	ptr[0] = (type8) val;
}

type32 read_l2(type8 * ptr)
{
	return ((type32) ptr[1] << 24 | (type32) ptr[0] << 16 | (type32) ptr[3] << 8 | (type32) ptr[2]);
}

type16 read_w2(type8 * ptr)
{
	return (type16) (ptr[1] << 8 | ptr[0]);
}

/* Standard rand - for equal cross-platform behaviour */

void ms_seed(type32 seed)
{
	rseed = seed;
}

type32 rand_emu(void)
{
	rseed = 1103515245L * rseed + 12345L;
	return rseed & 0x7fffffffL;
}

void ms_freemem(void)
{
	if (code)
		free(code);
	if (string)
		free(string);
	if (string2)
		free(string2);
	if (string3)
		free(string3);
	if (dict)
		free(dict);
	if (undo[0])
		free(undo[0]);
	if (undo[1])
		free(undo[1]);
	if (restart)
		free(restart);
	code = string = string2 = string3 = dict = undo[0] = undo[1] = restart = 0;
	if (gfx_data)
		free(gfx_data);
	if (gfx_buf)
		free(gfx_buf);
	if (gfx2_hdr)
		free(gfx2_hdr);
	if (gfx2_buf)
		free(gfx2_buf);
	if (gfx_fp)
		fclose(gfx_fp);
	gfx_data = gfx_buf = gfx2_hdr = gfx2_buf = 0;
	gfx2_name = 0;
	gfx_fp = 0;
	gfx_ver = 0;
	gfxtable = table_dist = 0;
#ifndef NO_ANIMATION
	pos_table_size = 0;
	command_index = 0;
	anim_repeat = 0;
	pos_table_index = -1;
	pos_table_max = -1;
#endif
	lastchar = 0;
	if (hints)
		free(hints);
	if (hint_contents)
		free(hint_contents);
	hints = 0;
	hint_contents = 0;
	if (snd_hdr)
		free(snd_hdr);
	if (snd_buf)
		free(snd_buf);
	snd_hdr = 0;
	snd_hsize = 0;
	snd_buf = 0;
}

type8 ms_is_running(void)
{
	return running;
}

type8 ms_is_magwin(void)
{
	return (version == 4) ? 1 : 0;
}

void ms_stop(void)
{
	running = 0;
}

type8 init_gfx1(type8* header)
{
#ifdef SAVEMEM
	type32 i;
#endif

	if (!(gfx_buf = malloc(MAX_PICTURE_SIZE)))
	{
		fclose(gfx_fp);
		gfx_fp = 0;
		return 1;
	}
#ifdef SAVEMEM
	if (!(gfx_data = malloc(128)))
	{
#else
	if (!(gfx_data = malloc(read_l(header + 4) - 8)))
	{
#endif
		free(gfx_buf);
		fclose(gfx_fp);
		gfx_buf = 0;
		gfx_fp = 0;
		return 1;
	}
#ifdef SAVEMEM
	if (!fread(gfx_data, 128, 1, gfx_fp))
	{
#else
	if (!fread(gfx_data, read_l(header + 4) - 8, 1, gfx_fp))
	{
#endif
		free(gfx_data);
		free(gfx_buf);
		fclose(gfx_fp);
		gfx_data = gfx_buf = 0;
		gfx_fp = 0;
		return 1;
	}

#ifdef SAVEMEM
	for (i = 0; i < 128; i += 4)
		if (!read_l(gfx_data + i))
			write_l(gfx_data + i, read_l(header + 4));
#else
	fclose(gfx_fp);
	gfx_fp = 0;
#endif

	gfx_ver = 1;
	return 2;
}

type8 init_gfx2(type8* header)
{
	if (!(gfx_buf = malloc(MAX_PICTURE_SIZE)))
	{
		fclose(gfx_fp);
		gfx_fp = 0;
		return 1;
	}

	gfx2_hsize = read_w(header + 4);
	if (!(gfx2_hdr = malloc(gfx2_hsize)))
	{
		free(gfx_buf);
		fclose(gfx_fp);
		gfx_buf = 0;
		gfx_fp = 0;
		return 1;
	}

	fseek(gfx_fp, 6, SEEK_SET);
	if (!fread(gfx2_hdr, gfx2_hsize, 1, gfx_fp))
	{
		free(gfx_buf);
		free(gfx2_hdr);
		fclose(gfx_fp);
		gfx_buf = 0;
		gfx2_hdr = 0;
		gfx_fp = 0;
		return 1;
	}

	gfx_ver = 2;
	return 2;
}

type8 init_snd(type8* header)
{
	if (!(snd_buf = malloc(MAX_MUSIC_SIZE)))
	{
		fclose(snd_fp);
		snd_fp = 0;
		return 1;
	}

	snd_hsize = read_w(header + 4);
	if (!(snd_hdr = malloc(snd_hsize)))
	{
		free(snd_buf);
		fclose(snd_fp);
		snd_buf = 0;
		snd_fp = 0;
		return 1;
	}

	fseek(snd_fp, 6, SEEK_SET);
	if (!fread(snd_hdr, snd_hsize, 1, snd_fp))
	{
		free(snd_buf);
		free(snd_hdr);
		fclose(snd_fp);
		snd_buf = 0;
		snd_hdr = 0;
		snd_fp = 0;
		return 1;
	}

	return 2;
}

/* zero all registers and flags and load the game */

type8 ms_init(type8s * name, type8s * gfxname, type8s * hntname, type8s * sndname)
{
	FILE *fp;
	type8 header[42], header2[8],header3[4];
	type32 i, dict_size, string2_size, code_size, dec;

#if defined(LOGEMU) || defined(LOGGFX) || defined(LOGHNT)
	dbg_log = fopen(LOG_FILE, "wt");
#endif
	ms_stop();
	if (!name)
	{
		if (!restart)
			return 0;
		else
		{
			memcpy(code, restart, undo_size);
			undo_stat[0] = undo_stat[1] = 0;
			ms_showpic(0, 0);
		}
	}
	else
	{
		undo_stat[0] = undo_stat[1] = 0;
		ms_seed(time(0));
		if (!(fp = fopen(name, "rb")))
			return 0;
		if ((fread(header, 1, 42, fp) != 42) || (read_l(header) != 0x4d615363))
		{
			fclose(fp);
			return 0;
		}
		if (read_l(header + 8) != 42)
		{
			fclose(fp);
			return 0;
		}
		ms_freemem();
		version = header[13];
		code_size = read_l(header + 14);
		string_size = read_l(header + 18);
		string2_size = read_l(header + 22);
		dict_size = read_l(header + 26);
		undo_size = read_l(header + 34);
		undo_pc = read_l(header + 38);

		if ((version < 4) && (code_size < 65536))
			mem_size = 65536;
		else
			mem_size = code_size;

		/* Some C libraries don't like malloc(0), so make
		   sure that undo_size is always positive. */
		if (undo_size == 0)
			undo_size = 8;

		sd = (type8)((dict_size != 0L) ? 1 : 0); /* if (sd) => separate dict */ 

		if (!(code = malloc(mem_size)) || !(string2 = malloc(string2_size)) ||
				!(restart = malloc(undo_size)) || (sd && 
				!(dict = malloc(dict_size)))) 
		{
			ms_freemem();
			fclose(fp);
			return 0;
		}
		if (string_size > MAX_STRING_SIZE)
		{
			if (!(string = malloc(MAX_STRING_SIZE)) ||
			    !(string3 = malloc(string_size - MAX_STRING_SIZE)))
			{
				ms_freemem();
				fclose(fp);
				return 0;
			}
		}
		else
		{
			if (!(string = malloc(string_size)))
			{
				ms_freemem();
				fclose(fp);
				return 0;
			}
		}
		if (!(undo[0] = malloc(undo_size)) || !(undo[1] = malloc(undo_size)))
		{
			ms_freemem();
			fclose(fp);
			return 0;
		}
		if (fread(code, 1, code_size, fp) != code_size)
		{
			ms_freemem();
			fclose(fp);
			return 0;
		}
		memcpy(restart, code, undo_size);	/* fast restarts */
		if (string_size > MAX_STRING_SIZE)
		{
			if (fread(string, 1, MAX_STRING_SIZE, fp) != MAX_STRING_SIZE)
			{
				ms_freemem();
				fclose(fp);
				return 0;
			}
			if (fread(string3, 1, string_size - MAX_STRING_SIZE, fp) != string_size - MAX_STRING_SIZE)
			{
				ms_freemem();
				fclose(fp);
				return 0;
			}
		}
		else
		{
			if (fread(string, 1, string_size, fp) != string_size)
			{
				ms_freemem();
				fclose(fp);
				return 0;
			}
		}
		if (fread(string2, 1, string2_size, fp) != string2_size)
		{
			ms_freemem();
			fclose(fp);
			return 0;
		}
		if (sd && fread(dict, 1, dict_size, fp) != dict_size) 
		{
			ms_freemem();
			fclose(fp);
			return 0;
		}
		dec = read_l(header + 30);
		if (dec >= string_size)
			decode_table = string2 + dec - string_size;
		else
		{
			if (dec >= MAX_STRING_SIZE)
				decode_table = string3 + dec - MAX_STRING_SIZE;
			else
				decode_table = string + dec;
		}
		fclose(fp);
	}

	for (i = 0; i < 8; i++)
		dreg[i] = areg[i] = 0;
	write_reg(8 + 7, 2, 0xfffe);	/* Stack-pointer, -2 due to MS-DOS segments */
	pc = 0;
	zflag = nflag = cflag = vflag = 0;
	i_count = 0;
	running = 1;

	if (!name)
		return (type8) (gfx_buf ? 2 : 1);	/* Restarted */

	if (version == 4)
	{
		/* Try loading a hint file */
		FILE *hnt_fp;
		if (hntname && (hnt_fp = fopen(hntname, "rb")))
		{
			if ((fread(&header3, 1, 4, hnt_fp) == 4) && (read_l(header3) == 0x4D614874))
			{
				type8 buf[8];
				type16 i,j,blkcnt,elcnt,ntype,elsize,conidx;

				/* Allocate memory for hints */
				hints = malloc(MAX_HINTS * sizeof(struct ms_hint));
				hint_contents = malloc(MAX_HCONTENTS);
				if ((hints != 0) && (hint_contents != 0))
				{
					/* Read number of blocks */
					if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
					blkcnt = read_w2(buf);
#ifdef LOGHNT
					out2("Blocks: %d\n",blkcnt);
#endif
					conidx = 0;
					for (i = 0; i < blkcnt; i++)
					{
#ifdef LOGHNT
						out2("\nBlock No. %d\n",i);
#endif
						/* Read number of elements */
						if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
						elcnt = read_w2(buf);
#ifdef LOGHNT
						out2("Elements: %d\n",elcnt);
#endif
						hints[i].elcount = elcnt;

						/* Read node type */
						if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
						ntype = read_w2(buf);
#ifdef LOGHNT
						if (ntype == 1)
							out2("Type: Node\n");
						else
							out2("Type: Leaf\n");
#endif
						hints[i].nodetype = ntype;
						hints[i].content = hint_contents+conidx;
#ifdef LOGHNT
						out2("Elements:\n");
#endif
						for (j = 0; j < elcnt; j++)
						{
							if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
							elsize = read_w2(buf);
							if (fread(hint_contents+conidx, 1, elsize, hnt_fp) != elsize && !feof(hnt_fp)) return 0;
							hint_contents[conidx+elsize-1] = '\0';
#ifdef LOGHNT
							out2("%s\n",hint_contents+conidx);
#endif
							conidx += elsize;
						}

						/* Do we need a jump table? */
						if (ntype == 1)
						{
#ifdef LOGHNT
							out2("Jump to block:\n");
#endif
							for (j = 0; j < elcnt; j++)
							{
								if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
								hints[i].links[j] = read_w2(buf);
#ifdef LOGHNT
								out2("%d\n",hints[i].links[j]);
#endif
							}
						}

						/* Read the parent block */
						if (fread(&buf, 1, 2, hnt_fp) != 2 && !feof(hnt_fp)) return 0;
						hints[i].parent = read_w2(buf);
#ifdef LOGHNT
						out2("Parent: %d\n",hints[i].parent);
#endif
					}
				}
				else
				{
					if (hints)
						free(hints);
					if (hint_contents)
						free(hint_contents);
					hints = 0;
					hint_contents = 0;
				}
			}
			fclose(hnt_fp);
		}

		/* Try loading a music file */
		if (sndname && (snd_fp = fopen(sndname, "rb")))
		{
			if (fread(&header2, 1, 8, snd_fp) != 8)
			{
				fclose(snd_fp);
				snd_fp = 0;
			}
			else
			{
				if (read_l(header2) == 0x4D615364) /* MaSd */
				{
					init_snd(header2);
#ifdef LOGSND
					out2("Sound file loaded.\n");
#endif
				}
			}
		}
	}

	if (!gfxname || !(gfx_fp = fopen(gfxname, "rb")))
		return 1;
	if (fread(&header2, 1, 8, gfx_fp) != 8)
	{
		fclose(gfx_fp);
		gfx_fp = 0;
		return 1;
	}

	if (version < 4 && read_l(header2) == 0x4D615069) /* MaPi */
		return init_gfx1(header2);
	else if (version == 4 && read_l(header2) == 0x4D615032) /* MaP2 */
		return init_gfx2(header2);
	fclose(gfx_fp);
	gfx_fp = 0;
	return 1;
}

type8 is_blank(type16 line, type16 width)
{
	type32s i;

	for (i = line * width; i < (line + 1) * width; i++)
		if (gfx_buf[i])
			return 0;
	return 1;
}

type8 *ms_extract1(type8 pic, type16 * w, type16 * h, type16 * pal)
{
	type8 *decode_table, *data, bit, val, *buffer;
	type16 tablesize, count;
	type32 i, j, datasize, upsize, offset;

	offset = read_l(gfx_data + 4 * pic);
#ifdef SAVEMEM
	if (fseek(gfx_fp, offset, SEEK_SET) < 0)
		return 0;
	datasize = read_l(gfx_data + 4 * (pic + 1)) - offset;
	if (!(buffer = malloc(datasize)))
		return 0;
	if (fread(buffer, 1, datasize, gfx_fp) != datasize)
		return 0;
#else
	buffer = gfx_data + offset - 8;
#endif

	for (i = 0; i < 16; i++)
		pal[i] = read_w(buffer + 0x1c + 2 * i);
	w[0] = (type16)(read_w(buffer + 4) - read_w(buffer + 2));
	h[0] = read_w(buffer + 6);

	tablesize = read_w(buffer + 0x3c);
	datasize = read_l(buffer + 0x3e);
	decode_table = buffer + 0x42;
	data = decode_table + tablesize * 2 + 2;
	upsize = h[0] * w[0];

	for (i = 0, j = 0, count = 0, val = 0, bit = 7; i < upsize; i++, count--)
	{
		if (!count)
		{
			count = tablesize;
			while (count < 0x80)
			{
				if (data[j] & (1 << bit))
					count = decode_table[2 * count];
				else
					count = decode_table[2 * count + 1];
				if (!bit)
					j++;
				bit = (type8)(bit ? bit - 1 : 7);
			}
			count &= 0x7f;
			if (count >= 0x10)
				count -= 0x10;
			else
			{
				val = (type8)count;
				count = 1;
			}
		}
		gfx_buf[i] = val;
	}
	for (j = w[0]; j < upsize; j++)
		gfx_buf[j] ^= gfx_buf[j - w[0]];

#ifdef SAVEMEM
	free(buffer);
#endif
	for (; h[0] > 0 && is_blank((type16)(h[0] - 1), w[0]); h[0]--);
	for (i = 0; h[0] > 0 && is_blank((type16)i, w[0]); h[0]--, i++);
	return gfx_buf + i * w[0];
}

type16s find_name_in_header(type8s * name, type8 upper)
{
	type16s header_pos = 0;
	type8s pic_name[8];
	type8 i;

	for (i = 0; i < 8; i++)
		pic_name[i] = 0;
	strncpy(pic_name,name,6);
	if (upper)
	{
		for (i = 0; i < 8; i++)
			pic_name[i] = (type8s)toupper(pic_name[i]);
	}

	while (header_pos < gfx2_hsize)
	{
		type8s* hname = (type8s*)(gfx2_hdr + header_pos);
		if (strncmp(hname,pic_name,6) == 0)
			return header_pos;
		header_pos += 16;
	}
	return -1;
}

void extract_frame(struct picture * pic)
{
	type32 i, x, y, bit_x, mask, ywb, yw, value, values[4];

	if (pic->width * pic->height > MAX_PICTURE_SIZE)
	{
		ms_fatal("picture too large");
		return;
	}

	for (y = 0; y < pic->height; y++)
	{
		ywb = y * pic->wbytes;
		yw = y * pic->width;

		for (x = 0; x < pic->width; x++)
		{
			if ((x % 8) == 0)
			{
				for (i = 0; i < 4; i++)
					values[i] = pic->data[ywb + (x / 8) + (pic->plane_step * i)];
			}

			bit_x = 7 - (x & 7);
			mask = 1 << bit_x;
			value = ((values[0] & mask) >> bit_x) << 0|
			        ((values[1] & mask) >> bit_x) << 1|
			        ((values[2] & mask) >> bit_x) << 2|
			        ((values[3] & mask) >> bit_x) << 3;
			value &= 15;

			gfx_buf[yw + x] = (type8)value;
		}
	}
}

type8 *ms_extract2(type8s * name, type16 * w, type16 * h, type16 * pal, type8 * is_anim)
{
	struct picture main_pic;
	type32 offset = 0, length = 0, i;
	type16s header_pos = -1;
#ifndef NO_ANIMATION
	type8* anim_data;
	type32 j;
#endif

	if (is_anim != 0)
		*is_anim = 0;
	gfx2_name = name;

#ifndef NO_ANIMATION
	pos_table_size = 0;
#endif

#ifdef NO_ANIMATION
	/* Find the uppercase (no animation) version of the picture first. */
	header_pos = find_name_in_header(name,1);
#endif
	if (header_pos < 0)
		header_pos = find_name_in_header(name,0);
	if (header_pos < 0)
		return 0;

	offset = read_l(gfx2_hdr + header_pos + 8);
	length = read_l(gfx2_hdr + header_pos + 12);

	if (offset != 0)
	{
		if (gfx2_buf)
		{
			free(gfx2_buf);
			gfx2_buf = 0;
		}

		gfx2_buf = malloc(length);
		if (!gfx2_buf)
			return 0;

		if (fseek(gfx_fp, offset, SEEK_SET) < 0)
		{
			free(gfx2_buf);
			gfx2_buf = 0;
			return 0;
		}

		if (!fread(gfx2_buf, length, 1, gfx_fp))
		{
			free(gfx2_buf);
			gfx2_buf = 0;
			return 0;
		}

		for (i = 0; i < 16; i++)
			pal[i] = read_w2(gfx2_buf + 4 + (2 * i));

		main_pic.data = gfx2_buf + 48;
		main_pic.data_size = read_l2(gfx2_buf + 38);
		main_pic.width = read_w2(gfx2_buf + 42);
		main_pic.height = read_w2(gfx2_buf + 44);
		main_pic.wbytes = (type16)(main_pic.data_size / main_pic.height);
		main_pic.plane_step = (type16)(main_pic.wbytes / 4);
		main_pic.mask = (type8*)0;
		extract_frame(&main_pic);

		*w = main_pic.width;
		*h = main_pic.height;

#ifndef NO_ANIMATION
		/* Check for an animation */
		anim_data = gfx2_buf + 48 + main_pic.data_size;
		if ((anim_data[0] != 0xD0) || (anim_data[1] != 0x5E))
		{
			type8 *current;
			type16 frame_count, command_count;
			type16 value1, value2;

			if (is_anim != 0)
				*is_anim = 1;

			current = anim_data + 6;
			frame_count = read_w2(anim_data + 2);
			if (frame_count > MAX_ANIMS)
			{
				ms_fatal("animation frame array too short");
				return 0;
			}

			/* Loop through each animation frame */
			for (i = 0; i < frame_count; i++)
			{
				anim_frame_table[i].data = current + 10;
				anim_frame_table[i].data_size = read_l2(current);
				anim_frame_table[i].width = read_w2(current + 4);
				anim_frame_table[i].height = read_w2(current + 6);
				anim_frame_table[i].wbytes = (type16)(anim_frame_table[i].data_size / anim_frame_table[i].height);
				anim_frame_table[i].plane_step = (type16)(anim_frame_table[i].wbytes / 4);
				anim_frame_table[i].mask = (type8*)0;

				current += anim_frame_table[i].data_size + 12;
				value1 = read_w2(current - 2);
				value2 = read_w2(current);

				/* Get the mask */
				if ((value1 == anim_frame_table[i].width) && (value2 == anim_frame_table[i].height))
				{
					type16 skip;

					anim_frame_table[i].mask = (type8*)(current + 4);
					skip = read_w2(current + 2);
					current += skip + 6;
				}
			}

			/* Get the positioning tables */
			pos_table_size = read_w2(current - 2);
			if (pos_table_size > MAX_POSITIONS)
			{
				ms_fatal("animation position array too short");
				return 0;
			}

#ifdef LOGGFX_EXT
			out2("POSITION TABLE DUMP\n");
#endif
			for (i = 0; i < pos_table_size; i++)
			{
				pos_table_count[i] = read_w2(current + 2);
				current += 4;

				if (pos_table_count[i] > MAX_ANIMS)
				{
					ms_fatal("animation position array too short");
					return 0;
				}

				for (j = 0; j < pos_table_count[i]; j++)
				{
					pos_table[i][j].x = read_w2(current);
					pos_table[i][j].y = read_w2(current + 2);
					pos_table[i][j].number = read_w2(current + 4) - 1;
					current += 8;
#ifdef LOGGFX_EXT
					out2("Position entry: Table: %d  Entry: %d  X: %d Y: %d Frame: %d\n",
						i,j,pos_table[i][j].x,pos_table[i][j].y,pos_table[i][j].number);
#endif
				}
			}

			/* Get the command sequence table */
			command_count = read_w2(current);
			command_table = current + 2;

			for (i = 0; i < MAX_POSITIONS; i++)
			{
				anim_table[i].flag = -1;
				anim_table[i].count = -1;
			}
			command_index = 0;
			anim_repeat = 0;
			pos_table_index = -1;
			pos_table_max = -1;
		}
#endif
		return gfx_buf;
	}
	return 0;
}

type8 *ms_extract(type32 pic, type16 * w, type16 * h, type16 * pal, type8 * is_anim)
{
	if (is_anim)
		*is_anim = 0;

	if (gfx_buf)
	{
		switch (gfx_ver)
		{
		case 1:
			return ms_extract1((type8)pic,w,h,pal);
		case 2:
			return ms_extract2((type8s*)(code + pic),w,h,pal,is_anim);
		}
	}
	return 0;
}

type8 ms_animate(struct ms_position ** positions, type16 * count)
{
#ifndef NO_ANIMATION
	type8 got_anim = 0;
	type16 i, j, ttable;

	if ((gfx_buf == 0) || (gfx2_buf == 0) || (gfx_ver != 2))
		return 0;
	if ((pos_table_size == 0) || (command_index < 0))
		return 0;

	*count = 0;
	*positions = (struct ms_position*)0;

	while (got_anim == 0)
	{
		if (pos_table_max >= 0)
		{
			if (pos_table_index < pos_table_max)
			{
				for (i = 0; i < pos_table_size; i++)
				{
					if (anim_table[i].flag > -1)
					{
						if (*count >= MAX_FRAMES)
						{
							ms_fatal("returned animation array too short");
							return 0;
						}

						pos_array[*count] = pos_table[i][anim_table[i].flag];
#ifdef LOGGFX_EXT
						out2("Adding frame %d to buffer\n",pos_array[*count].number);
#endif
						(*count)++;

						if (anim_table[i].flag < (pos_table_count[i]-1))
							anim_table[i].flag++;
						if (anim_table[i].count > 0)
							anim_table[i].count--;
						else
							anim_table[i].flag = -1;
					}
				}
				if (*count > 0)
				{
					*positions = pos_array;
					got_anim = 1;
				}
				pos_table_index++;
			}
		}

		if (got_anim == 0)
		{
			type8 command = command_table[command_index];
			command_index++;

			pos_table_max = -1;
			pos_table_index = -1;

			switch (command)
			{
			case 0x00:
				command_index = -1;
				return 0;
			case 0x01:
#ifdef LOGGFX_EXT
				out2("Load Frame Table: %d  Start at: %d  Count: %d\n",
					command_table[command_index],command_table[command_index+1],
					command_table[command_index+2]);
#endif
				ttable = command_table[command_index];
				command_index++;

				if (ttable-1 >= MAX_POSITIONS)
				{
					ms_fatal("animation table too short");
					return 0;
				}

				anim_table[ttable-1].flag = (type16s)(command_table[command_index]-1);
				command_index++;
				anim_table[ttable-1].count = (type16s)(command_table[command_index]-1);
				command_index++;

				/* Workaround for Wonderland "catter" animation */
				if (v4_id == 0)
				{
					if (strcmp(gfx2_name,"catter") == 0)
					{
						if (command_index == 96)
							anim_table[ttable-1].count = 9;
						if (command_index == 108)
							anim_table[ttable-1].flag = -1;
						if (command_index == 156)
							anim_table[ttable-1].flag = -1;
					}
				}
				break;
			case 0x02:
#ifdef LOGGFX_EXT
				out2("Animate: %d\n", command_table[command_index]);
#endif
				pos_table_max = command_table[command_index];
				pos_table_index = 0;
				command_index++;
				break;
			case 0x03:
#ifdef LOGGFX_EXT
				out2("Stop/Repeat Param: %d\n", command_table[command_index]);
				command_index = -1;
				return 0;
#else
				if (v4_id == 0)
				{
					command_index = -1;
					return 0;
				}
				else
				{
					command_index = 0;
					anim_repeat = 1;
					pos_table_index = -1;
					pos_table_max = -1;
					for (j = 0; j < MAX_POSITIONS; j++)
					{
						anim_table[j].flag = -1;
						anim_table[j].count = -1;
					}
				}
				break;
#endif
			case 0x04:
#ifdef LOGGFX_EXT
				out2("Unknown Command: %d Prop1: %d  Prop2: %d WARNING:not parsed\n", command,
					command_table[command_index],command_table[command_index+1]);
#endif
				command_index += 3;
				return 0;
			case 0x05:
				ttable = next_table;
				command_index++;

				anim_table[ttable-1].flag = 0;
				anim_table[ttable-1].count = command_table[command_index];

				pos_table_max = command_table[command_index];
				pos_table_index = 0;
				command_index++;
				command_index++;
				next_table++;
				break;
			default:
				ms_fatal("unknown animation command");
				command_index = -1;
				return 0;
			}
		}
	}
#ifdef LOGGFX_EXT
	out2("ms_animate() returning %d frames\n",*count);
#endif
	return got_anim;
#else
	return 0;
#endif
}

type8 * ms_get_anim_frame(type16s number, type16 * width, type16 * height, type8 ** mask)
{
#ifndef NO_ANIMATION
	if (number >= 0)
	{
		extract_frame(anim_frame_table + number);
		*width = anim_frame_table[number].width;
		*height = anim_frame_table[number].height;
		*mask = anim_frame_table[number].mask;
		return gfx_buf;
	}
#endif
	return 0;
}

type8 ms_anim_is_repeating(void)
{
#ifndef NO_ANIMATION
	return anim_repeat;
#else
	return 0;
#endif
}

type16s find_name_in_sndheader(type8s * name)
{
	type16s header_pos = 0;

	while (header_pos < snd_hsize)
	{
		type8s* hname = (type8s*)(snd_hdr + header_pos);
		if (strcmp(hname,name) == 0)
			return header_pos;
		header_pos += 18;
	}
	return -1;
}

type8 *sound_extract(type8s * name, type32 * length, type16 * tempo)
{
	type32 offset = 0;
	type16s header_pos = -1;

	if (header_pos < 0)
		header_pos = find_name_in_sndheader(name);
	if (header_pos < 0)
		return 0;

	*tempo = read_w(snd_hdr + header_pos + 8);
	offset = read_l(snd_hdr + header_pos + 10);
	*length = read_l(snd_hdr + header_pos + 14);

	if (offset != 0)
	{
		if (!snd_buf)
			return 0;
		if (fseek(snd_fp, offset, SEEK_SET) < 0)
			return 0;
		if (!fread(snd_buf, (int)(*length), 1, snd_fp))
			return 0;
		return snd_buf;
	}
	return 0;
}

void save_undo(void)
{
	type8 *tmp, i;
	type32 tmp32;

	tmp = undo[0];	/* swap buffers */
	undo[0] = undo[1];
	undo[1] = tmp;

	for (i = 0; i < 18; i++)
	{
		tmp32 = undo_regs[0][i];
		undo_regs[0][i] = undo_regs[1][i];
		undo_regs[1][i] = tmp32;
	}

	memcpy(undo[1], code, undo_size);
	for (i = 0; i < 8; i++)
	{
		undo_regs[1][i] = dreg[i];
		undo_regs[1][8 + i] = areg[i];
	}
	undo_regs[1][16] = i_count;
	undo_regs[1][17] = pc;	/* status flags intentionally omitted */

	undo_stat[0] = undo_stat[1];
	undo_stat[1] = 1;
}

type8 ms_undo(void)
{
	type8 i;

	ms_flush();
	if (!undo_stat[0])
		return 0;

	undo_stat[0] = undo_stat[1] = 0;
	memcpy(code, undo[0], undo_size);
	for (i = 0; i < 8; i++)
	{
		dreg[i] = undo_regs[0][i];
		areg[i] = undo_regs[0][8 + i];
	}
	i_count = undo_regs[0][16];
	pc = undo_regs[0][17];	/* status flags intentionally omitted */
	return 1;
}

#ifdef LOGEMU
void log_status(void)
{
	int j;

	fprintf(dbg_log, "\nD0:");
	for (j = 0; j < 8; j++)
		fprintf(dbg_log, " %8.8x", read_reg(j, 3));
	fprintf(dbg_log, "\nA0:");
	for (j = 0; j < 8; j++)
		fprintf(dbg_log, " %8.8x", read_reg(8 + j, 3));
	fprintf(dbg_log, "\nPC=%5.5x (%8.8x) ZCNV=%d%d%d%d - %d instructions\n\n",
		pc, code, zflag & 1, cflag & 1, nflag & 1, vflag & 1, i_count);
}
#endif

void ms_status(void)
{
	int j;

	fprintf(stderr, "D0:");
	for (j = 0; j < 8; j++)
		fprintf(stderr, " %8.8lx", (long) read_reg(j, 3));
	fprintf(stderr, "\nA0:");
	for (j = 0; j < 8; j++)
		fprintf(stderr, " %8.8lx", (long) read_reg(8 + j, 3));
	fprintf(stderr, "\nPC=%5.5lx ZCNV=%d%d%d%d - %ld instructions\n",
		(long) pc, zflag & 1, cflag & 1, nflag & 1, vflag & 1, (long) i_count);
}

type32 ms_count(void)
{
	return i_count;
}

/* align register pointer for word/byte accesses */

type8 *reg_align(type8 * ptr, type8 size)
{
	if (size == 1)
		ptr += 2;
	if (size == 0)
		ptr += 3;
	return ptr;
}

type32 read_reg(int i, int s)
{
	type8 *ptr;

	if (i > 15)
	{
		ms_fatal("invalid register in read_reg");
		return 0;
	}
	if (i < 8)
		ptr = (type8 *) & dreg[i];
	else
		ptr = (type8 *) & areg[i - 8];

	switch (s)
	{
	case 0:
		return reg_align(ptr, 0)[0];
	case 1:
		return read_w(reg_align(ptr, 1));
	default:
		return read_l(ptr);
	}
}

void write_reg(int i, int s, type32 val)
{
	type8 *ptr;

	if (i > 15)
	{
		ms_fatal("invalid register in write_reg");
		return;
	}
	if (i < 8)
		ptr = (type8 *) & dreg[i];
	else
		ptr = (type8 *) & areg[i - 8];

	switch (s)
	{
	case 0:
		reg_align(ptr, 0)[0] = (type8)val;
		break;
	case 1:
		write_w(reg_align(ptr, 1), (type16)val);
		break;
	default:
		write_l(ptr, val);
		break;
	}
}

/* [35c4] */

void char_out(type8 c)
{
	static type8 big = 0, period = 0, pipe = 0;

	if (c == 0xff)
	{
		big = 1;
		return;
	}
	c &= 0x7f;
	if (read_reg(3, 0))
	{
		if (c == 0x5f || c == 0x40)
			c = 0x20;
		if ((c >= 'a') && (c <= 'z'))
			c &= 0xdf;
		if (version < 4)
			ms_statuschar(c);
		return;
	}
	if (c == 0x5e)
		c = 0x0a;
	if (c == 0x40)
	{
		if (read_reg(2, 0))
			return;
		else
			c = 0x73;
	}
	if (version < 3 && c == 0x7e)
	{
		lastchar = 0x7e;
		c = 0x0a;
	}
	if (((c > 0x40) && (c < 0x5b)) || ((c > 0x60) && (c < 0x7b)))
	{
		if (big)
		{
			c &= 0xdf;
			big = 0;
		}
		if (period)
			char_out(0x20);
	}
	period = 0;
	if (version < 4)
	{
		if ((c == 0x2e) || (c == 0x3f) || (c == 0x21) || (c == 0x0a))
			big = 1;
		else if (c == 0x22)
			big = 0;
	}
	else
	{
		if ((c == 0x20) && (lastchar == 0x0a))
			return;
		if ((c == 0x2e) || (c == 0x3f) || (c == 0x21) || (c == 0x0a))
			big = 1;
		else if (c == 0x22)
			big = 0;
	}
	if (((c == 0x20) || (c == 0x0a)) && (c == lastchar))
		return;
	if (version < 3)
	{
		if (pipe)
		{
			pipe = 0;
			return;
		}
		if (c == 0x7c)
		{
			pipe = 1;
			return;
		}
	}
	else
	{
		if (c == 0x7e)
		{
			c = 0x0a;
			if (lastchar != 0x0a)
				char_out(0x0a);
		}
	}
	lastchar = c;
	if (c == 0x5f)
		c = 0x20;
	if ((c == 0x2e) || (c == 0x2c) || (c == 0x3b) || (c == 0x3a) || (c == 0x21) || (c == 0x3f))
		period = 1;
	ms_putchar(c);
}


/* extract addressing mode information [1c6f] */

void set_info(type8 b)
{
	regnr = (type8)(b & 0x07);
	admode = (type8)((b >> 3) & 0x07);
	opsize = (type8)(b >> 6);
}

/* read a word and increase pc */

void read_word(void)
{
	type8 *epc;

	epc = effective(pc);
	byte1 = epc[0];
	byte2 = epc[1];
	pc += 2;
}

/* get addressing mode and set arg1 [1c84] */

void set_arg1(void)
{
	type8 tmp[2], l1c;

	is_reversible = 1;
	switch (admode)
	{
	case 0:
		arg1 = reg_align((type8 *) & dreg[regnr], opsize);	/* Dx */
		is_reversible = 0;
#ifdef LOGEMU
		out(" d%.1d", regnr);
#endif
		break;
	case 1:
		arg1 = reg_align((type8 *) & areg[regnr], opsize);	/* Ax */
		is_reversible = 0;
#ifdef LOGEMU
		out(" a%.1d", regnr);
#endif
		break;
	case 2:
		arg1i = read_reg(8 + regnr, 2);		/* (Ax) */
#ifdef LOGEMU
		out(" (a%.1d)", regnr);
#endif
		break;
	case 3:
		arg1i = read_reg(8 + regnr, 2);		/* (Ax)+ */
		write_reg(8 + regnr, 2, read_reg(8 + regnr, 2) + (1 << opsize));
#ifdef LOGEMU
		out(" (a%.1d)+", regnr);
#endif
		break;
	case 4:
		write_reg(8 + regnr, 2, read_reg(8 + regnr, 2) - (1 << opsize));
		arg1i = read_reg(8 + regnr, 2);		/* -(Ax) */
#ifdef LOGEMU
		out(" -(a%.1d)", regnr);
#endif
		break;
	case 5:
		{
			type16s i = (type16s) read_w(effective(pc));
			arg1i = read_reg(8 + regnr, 2) + i;
			pc += 2;	/* offset.w(Ax) */
#ifdef LOGEMU
			out(" %X(a%.1d)", i, regnr);
#endif
		}
		break;
	case 6:
		tmp[0] = byte1;
		tmp[1] = byte2;
		read_word();	/* offset.b(Ax, Dx/Ax) [1d1c] */
#ifdef LOGEMU
		out(" %.2X(a%.1d,", (int) byte2, regnr);
#endif
		arg1i = read_reg(regnr + 8, 2) + (type8s) byte2;
#ifdef LOGEMU
		if ((byte1 >> 4) > 8)
			out("a%.1d", (byte1 >> 4) - 8);
		else
			out("d%.1d", byte1 >> 4);
#endif
		if (byte1 & 0x08)
		{
#ifdef LOGEMU
			out(".l)");
#endif
			arg1i += (type32s) read_reg((byte1 >> 4), 2);
		}
		else
		{
#ifdef LOGEMU
			out(".w)");
#endif
			arg1i += (type16s) read_reg((byte1 >> 4), 1);
		}
		byte1 = tmp[0];
		byte2 = tmp[1];
		break;
	case 7:		/* specials */
		switch (regnr)
		{
		case 0:
			arg1i = read_w(effective(pc));	/* $xxxx.W */
			pc += 2;
#ifdef LOGEMU
			out(" %.4X.w", arg1i);
#endif
			break;
		case 1:
			arg1i = read_l(effective(pc));	/* $xxxx */
			pc += 4;
#ifdef LOGEMU
			out(" %.4X", arg1i);
#endif
			break;
		case 2:
			arg1i = (type16s) read_w(effective(pc)) + pc;	/* $xxxx(PC) */
			pc += 2;
#ifdef LOGEMU
			out(" %.4X(pc)", arg1i);
#endif
			break;
		case 3:
			l1c = effective(pc)[0];		/* $xx(PC,A/Dx) */
#ifdef LOGEMU
			out(" ???2", arg1i);
#endif
			if (l1c & 0x08)
				arg1i = pc + (type32s) read_reg((l1c >> 4), 2);
			else
				arg1i = pc + (type16s) read_reg((l1c >> 4), 1);
			l1c = effective(pc)[1];
			pc += 2;
			arg1i += (type8s) l1c;
			break;
		case 4:
			arg1i = pc;	/* #$xxxx */
			if (opsize == 0)
				arg1i += 1;
			pc += 2;
			if (opsize == 2)
				pc += 2;
#ifdef LOGEMU
			out(" #%.4X", arg1i);
#endif
			break;
		}
		break;
	}
	if (is_reversible)
		arg1 = effective(arg1i);
}

/* get addressing mode and set arg2 [1bc5] */

void set_arg2_nosize(int use_dx, type8 b)
{
	if (use_dx)
		arg2 = (type8 *) dreg;
	else
		arg2 = (type8 *) areg;
	arg2 += (b & 0x0e) << 1;
}

void set_arg2(int use_dx, type8 b)
{
	set_arg2_nosize(use_dx, b);
	arg2 = reg_align(arg2, opsize);
}

/* [1b9e] */

void swap_args(void)
{
	type8 *tmp;

	tmp = arg1;
	arg1 = arg2;
	arg2 = tmp;
}

/* [1cdc] */

void push(type32 c)
{
	write_reg(15, 2, read_reg(15, 2) - 4);
	write_l(effective(read_reg(15, 2)), c);
}

/* [1cd1] */

type32 pop(void)
{
	type32 c;

	c = read_l(effective(read_reg(15, 2)));
	write_reg(15, 2, read_reg(15, 2) + 4);
	return c;
}

/* check addressing mode and get argument [2e85] */

void get_arg(void)
{
#ifdef LOGEMU
	out(" %.4X", pc);
#endif
	set_info(byte2);
	arg2 = effective(pc);
	pc += 2;
	if (opsize == 2)
		pc += 2;
	if (opsize == 0)
		arg2 += 1;
	set_arg1();
}

void set_flags(void)
{
	type16 i;
	type32 j;

	zflag = nflag = 0;
	switch (opsize)
	{
	case 0:
		if (arg1[0] > 127)
			nflag = 0xff;
		if (arg1[0] == 0)
			zflag = 0xff;
		break;
	case 1:
		i = read_w(arg1);
		if (i == 0)
			zflag = 0xff;
		if ((i >> 15) > 0)
			nflag = 0xff;
		break;
	case 2:
		j = read_l(arg1);
		if (j == 0)
			zflag = 0xff;
		if ((j >> 31) > 0)
			nflag = 0xff;
		break;
	}
}

/* [263a] */

int condition(type8 b)
{
	switch (b & 0x0f)
	{
	case 0:
		return 0xff;
	case 1:
		return 0x00;
	case 2:
		return (zflag | cflag) ^ 0xff;
	case 3:
		return (zflag | cflag);
	case 4:
		return cflag ^ 0xff;
	case 5:
		return cflag;
	case 6:
		return zflag ^ 0xff;
	case 7:
		return zflag;
	case 8:
		return vflag ^ 0xff;
	case 9:
		return vflag;
	case 10:
	case 12:
		return nflag ^ 0xff;
	case 11:
	case 13:
		return nflag;
	case 14:
		return (zflag | nflag) ^ 0xff;
	case 15:
		return (zflag | nflag);
	}
	return 0x00;
}

/* [26dc] */

void branch(type8 b)
{
	if (b == 0)
		pc += (type16s) read_w(effective(pc));
	else
		pc += (type8s) b;
#ifdef LOGEMU
	out(" %.4X", pc);
#endif
}

/* [2869] */

void do_add(type8 adda)
{
	if (adda)
	{
		if (opsize == 0)
			write_l(arg1, read_l(arg1) + (type8s) arg2[0]);
		if (opsize == 1)
			write_l(arg1, read_l(arg1) + (type16s) read_w(arg2));
		if (opsize == 2)
			write_l(arg1, read_l(arg1) + (type32s) read_l(arg2));
	}
	else
	{
		cflag = 0;
		if (opsize == 0)
		{
			arg1[0] += arg2[0];
			if (arg2[0] > arg1[0])
				cflag = 0xff;
		}
		if (opsize == 1)
		{
			write_w(arg1, (type16)(read_w(arg1) + read_w(arg2)));
			if (read_w(arg2) > read_w(arg1))
				cflag = 0xff;
		}
		if (opsize == 2)
		{
			write_l(arg1, read_l(arg1) + read_l(arg2));
			if (read_l(arg2) > read_l(arg1))
				cflag = 0xff;
		}
		if (version < 3 || !quick_flag)
		{
			/* Corruption onwards */
			vflag = 0;
			set_flags();
		}
	}
}

/* [2923] */

void do_sub(type8 suba)
{
	if (suba)
	{
		if (opsize == 0)
			write_l(arg1, read_l(arg1) - (type8s) arg2[0]);
		if (opsize == 1)
			write_l(arg1, read_l(arg1) - (type16s) read_w(arg2));
		if (opsize == 2)
			write_l(arg1, read_l(arg1) - (type32s) read_l(arg2));
	}
	else
	{
		cflag = 0;
		if (opsize == 0)
		{
			if (arg2[0] > arg1[0])
				cflag = 0xff;
			arg1[0] -= arg2[0];
		}
		if (opsize == 1)
		{
			if (read_w(arg2) > read_w(arg1))
				cflag = 0xff;
			write_w(arg1, (type16)(read_w(arg1) - read_w(arg2)));
		}
		if (opsize == 2)
		{
			if (read_l(arg2) > read_l(arg1))
				cflag = 0xff;
			write_l(arg1, read_l(arg1) - read_l(arg2));
		}
		if (version < 3 || !quick_flag)
		{
			/* Corruption onwards */
			vflag = 0;
			set_flags();
		}
	}
}

/* [283b] */

void do_eor(void)
{
	if (opsize == 0)
		arg1[0] ^= arg2[0];
	if (opsize == 1)
		write_w(arg1, (type16)(read_w(arg1) ^ read_w(arg2)));
	if (opsize == 2)
		write_l(arg1, read_l(arg1) ^ read_l(arg2));
	cflag = vflag = 0;
	set_flags();
}

/* [280d] */

void do_and(void)
{
	if (opsize == 0)
		arg1[0] &= arg2[0];
	if (opsize == 1)
		write_w(arg1, (type16)(read_w(arg1) & read_w(arg2)));
	if (opsize == 2)
		write_l(arg1, read_l(arg1) & read_l(arg2));
	cflag = vflag = 0;
	set_flags();
}

/* [27df] */

void do_or(void)
{
	if (opsize == 0)
		arg1[0] |= arg2[0];
	if (opsize == 1)
		write_w(arg1, (type16)(read_w(arg1) | read_w(arg2)));
	if (opsize == 2)
		write_l(arg1, read_l(arg1) | read_l(arg2));
	cflag = vflag = 0;
	set_flags();	/* [1c2b] */
}

/* [289f] */

void do_cmp(void)
{
	type8 *tmp;

	tmp = arg1;
	tmparg[0] = arg1[0];
	tmparg[1] = arg1[1];
	tmparg[2] = arg1[2];
	tmparg[3] = arg1[3];
	arg1 = tmparg;
	quick_flag = 0;
	do_sub(0);
	arg1 = tmp;
}

/* [2973] */

void do_move(void)
{

	if (opsize == 0)
		arg1[0] = arg2[0];
	if (opsize == 1)
		write_w(arg1, read_w(arg2));
	if (opsize == 2)
		write_l(arg1, read_l(arg2));
	if (version < 2 || admode != 1)
	{
		/* Jinxter: no flags if destination Ax */
		cflag = vflag = 0;
		set_flags();
	}
}

type8 do_btst(type8 a)
{
	a &= admode ? 0x7 : 0x1f;
	while (admode == 0 && a >= 8)
	{
		a -= 8;
		arg1 -= 1;
	}
	zflag = 0;
	if ((arg1[0] & (1 << a)) == 0)
		zflag = 0xff;
	return a;
}

/* bit operation entry point [307c] */

void do_bop(type8 b, type8 a)
{
#ifdef LOGEMU
	out("bop (%.2x,%.2x) ", (int) b, (int) a);
#endif
	b = b & 0xc0;
	a = do_btst(a);
#ifdef LOGEMU
	if (b == 0x00)
		out("no bop???");
#endif
	if (b == 0x40)
	{
		arg1[0] ^= (1 << a);	/* bchg */
#ifdef LOGEMU
		out("bchg");
#endif
	}
	if (b == 0x80)
	{
		arg1[0] &= ((1 << a) ^ 0xff);	/* bclr */
#ifdef LOGEMU
		out("bclr");
#endif
	}
	if (b == 0xc0)
	{
		arg1[0] |= (1 << a);	/* bset */
#ifdef LOGEMU
		out("bset");
#endif
	}
}

void check_btst(void)
{
#ifdef LOGEMU
	out("btst");
#endif
	set_info((type8)(byte2 & 0x3f));
	set_arg1();
	set_arg2(1, byte1);
	do_bop(byte2, arg2[0]);
}

void check_lea(void)
{
#ifdef LOGEMU
	out("lea");
#endif
	if ((byte2 & 0xc0) == 0xc0)
	{
		set_info(byte2);
		opsize = 2;
		set_arg1();
		set_arg2(0, byte1);
		write_w(arg2, 0);
		if (is_reversible)
			write_l(arg2, arg1i);
		else
			ms_fatal("illegal addressing mode for LEA");
	}
	else
	{
		ms_fatal("unimplemented instruction CHK");
	}
}

/* [33cc] */

void check_movem(void)
{
	type8 l1c;

#ifdef LOGEMU
	out("movem");
#endif
	set_info((type8)(byte2 - 0x40));
	read_word();
	for (l1c = 0; l1c < 8; l1c++)
	{
		if (byte2 & 1 << l1c)
		{
			set_arg1();
			if (opsize == 2)
				write_l(arg1, read_reg(15 - l1c, 2));
			if (opsize == 1)
				write_w(arg1, (type16)read_reg(15 - l1c, 1));
		}
	}
	for (l1c = 0; l1c < 8; l1c++)
	{
		if (byte1 & 1 << l1c)
		{
			set_arg1();
			if (opsize == 2)
				write_l(arg1, read_reg(7 - l1c, 2));
			if (opsize == 1)
				write_w(arg1, (type16)read_reg(7 - l1c, 1));
		}
	}
}

/* [3357] */

void check_movem2(void)
{
	type8 l1c;

#ifdef LOGEMU
	out("movem (2)");
#endif
	set_info((type8)(byte2 - 0x40));
	read_word();
	for (l1c = 0; l1c < 8; l1c++)
	{
		if (byte2 & 1 << l1c)
		{
			set_arg1();
			if (opsize == 2)
				write_reg(l1c, 2, read_l(arg1));
			if (opsize == 1)
				write_reg(l1c, 1, read_w(arg1));
		}
	}
	for (l1c = 0; l1c < 8; l1c++)
	{
		if (byte1 & 1 << l1c)
		{
			set_arg1();
			if (opsize == 2)
				write_reg(8 + l1c, 2, read_l(arg1));
			if (opsize == 1)
				write_reg(8 + l1c, 1, read_w(arg1));
		}
	}
}

/* [30e4] in Jinxter, ~540 lines of 6510 spaghetti-code */
/* The mother of all bugs, but hey - no gotos used :-) */

void dict_lookup(void)
{
	type16 dtab, doff, output, output_bak, bank, word, output2;
	type16 tmp16, i, obj_adj, adjlist, adjlist_bak;
	type8 c, c2, c3, flag, matchlen, longest, flag2;
	type8 restart = 0, accept = 0;

/*
   dtab=A5.W                    ;dict_table offset <L22>
   output=output_bak=A2.W       ;output <L24>
   A5.W=A6.W                    ;input word
   doff=A3.W                    ;lookup offset (doff) <L1C>
   adjlist=A0.W ;adjlist <L1E>
 */

	dtab = (type16)read_reg(8 + 5, 1);	/* used by version>0 */
	output = (type16)read_reg(8 + 2, 1);
	write_reg(8 + 5, 1, read_reg(8 + 6, 1));
	doff = (type16)read_reg(8 + 3, 1);
	adjlist = (type16)read_reg(8 + 0, 1);

	bank = (type16)read_reg(6, 0);	/* l2d */
	flag = 0;		/* l2c */
	word = 0;		/* l26 */
	matchlen = 0;		/* l2e */
	longest = 0;		/* 30e2 */
	write_reg(0, 1, 0);	/* apostroph */

	while ((c = sd ? dict[doff] : effective(doff)[0]) != 0x81)
	{
		if (c >= 0x80)
		{
			if (c == 0x82)
			{
				flag = matchlen = 0;
				word = 0;
				write_reg(8 + 6, 1, read_reg(8 + 5, 1));
				bank++;
				doff++;
				continue;
			}
			c3 = c;
			c &= 0x5f;
			c2 = effective(read_reg(8 + 6, 1))[0] & 0x5f;
			if (c2 == c)
			{
				write_reg(8 + 6, 1, read_reg(8 + 6, 1) + 1);
				c = effective(read_reg(8 + 6, 1))[0];
				if ((!c) || (c == 0x20) || (c == 0x27) || (!version && (matchlen > 6)))
				{
					if (c == 0x27)
					{
						write_reg(8 + 6, 1, read_reg(8 + 6, 1) + 1);
						write_reg(0, 1, 0x200 + effective(read_reg(8 + 6, 1))[0]);
					}
					if ((version < 4) || (c3 != 0xa0))
						accept = 1;
				}
				else
					restart = 1;
			}
			else if (!version && matchlen > 6 && !c2)
				accept = 1;
			else
				restart = 1;
		}
		else
		{
			c &= 0x5f;
			c2 = effective(read_reg(8 + 6, 1))[0] & 0x5f;
			if ((c2 == c && c) || (version && !c2 && (c == 0x5f)))
			{
				if (version && !c2 && (c == 0x5f))
					flag = 0x80;
				matchlen++;
				write_reg(8 + 6, 1, read_reg(8 + 6, 1) + 1);
				doff++;
			}
			else if (!version && matchlen > 6 && !c2)
				accept = 1;
			else
				restart = 1;
		}
		if (accept)
		{
			effective(read_reg(8 + 2, 1))[0] = (version) ? flag : 0;
			effective(read_reg(8 + 2, 1))[1] = (type8)bank;
			write_w(effective(read_reg(8 + 2, 1) + 2), word);
			write_reg(8 + 2, 1, read_reg(8 + 2, 1) + 4);
			if (matchlen >= longest)
				longest = matchlen;
			restart = 1;
			accept = 0;
		}
		if (restart)
		{
			write_reg(8 + 6, 1, read_reg(8 + 5, 1));
			flag = matchlen = 0;
			word++;
			if (sd)
				while (dict[doff++] < 0x80);
			else
				while (effective(doff++)[0] < 0x80);
			restart = 0;
		}
	}
	write_w(effective(read_reg(8 + 2, 1)), 0xffff);

	if (version)
	{
		/* version > 0 */
		output_bak = output;	/* check synonyms */
		while ((c = effective(output)[1]) != 0xff)
		{
			if (c == 0x0b)
			{
				if (sd)
					tmp16 = read_w(&dict[dtab + read_w(effective(output + 2)) * 2]);
				else
					tmp16 = read_w(effective(dtab + read_w(effective(output + 2)) * 2));
				effective(output)[1] = tmp16 & 0x1f;
				write_w(effective(output + 2), (type16)(tmp16 >> 5));
			}
			output += 4;
		}
		output = output_bak;
	}

/* l22 = output2,     l1e = adjlist, l20 = obj_adj, l26 = word, l2f = c2 */
/* l1c = adjlist_bak, 333C = i,      l2d = bank,    l2c = flag, l30e3 = flag2 */

	write_reg(1, 1, 0);	/* D1.W=0  [32B5] */
	flag2 = 0;
	output_bak = output;
	output2 = output;
	while ((bank = effective(output2)[1]) != 0xff)
	{
		obj_adj = (type16)read_reg(8 + 1, 1);	/* A1.W - obj_adj, ie. adjs for this word */
		write_reg(1, 0, 0);	/* D1.B=0 */
		flag = effective(output2)[0];	/* flag */
		word = read_w(effective(output2 + 2));	/* wordnumber */
		output2 += 4;	/* next match */
		if ((read_w(effective(obj_adj))) && (bank == 6))
		{
			/* Any adjectives? */
			if ((i = word) != 0)
			{
				/* Find list of valid adjs */
				do
				{
					while (effective(adjlist++)[0]);
				}
				while (--i > 0);
			}
			adjlist_bak = adjlist;
			do
			{
				adjlist = adjlist_bak;
				c2 = effective(obj_adj)[1];	/* given adjective */
				if ((tmp16 = read_w(effective(obj_adj))) != 0)
				{
					obj_adj += 2;
					while ((c = effective(adjlist++)[0]) && (c - 3 != c2));
					if (c - 3 != c2)
						write_reg(1, 0, 1);	/* invalid adjective */
				}
			}
			while (tmp16 && !read_reg(1, 0));
			adjlist = (type16)read_reg(8 + 0, 1);
		}
		if (!read_reg(1, 0))
		{
			/* invalid_flag */
			flag2 |= flag;
			effective(output)[0] = flag2;
			effective(output)[1] = (type8)bank;
			write_w(effective(output + 2), word);
			output += 4;
		}
	}
	write_reg(8 + 2, 1, output);
	output = output_bak;

	if (flag2 & 0x80)
	{
		tmp16 = output;
		output -= 4;
		do
		{
			output += 4;
			c = effective(output)[0];
		}
		while (!(c & 0x80));
		write_l(effective(tmp16), read_l(effective(output)) & 0x7fffffff);
		write_reg(8 + 2, 2, tmp16 + 4);
		if (longest > 1)
		{
			write_reg(8 + 5, 1, read_reg(8 + 5, 1) + longest - 2);
		}
	}
	write_reg(8 + 6, 1, read_reg(8 + 5, 1) + 1);
}

/* A0=findproperties(D0) [2b86], properties_ptr=[2b78] A0FE */

void do_findprop(void)
{
	type16 tmp;

	if ((version > 2) && ((read_reg(0, 1) & 0x3fff) > fp_size))
	{
		tmp = (type16)(((fp_size - (read_reg(0, 1) & 0x3fff)) ^ 0xffff) << 1);
		tmp += fp_tab;
		tmp = read_w(effective(tmp));
	}
	else
	{
		if (version < 2)
			write_reg(0, 2, read_reg(0, 2) & 0x7fff);
		else
			write_reg(0, 1, read_reg(0, 1) & 0x7fff);
		tmp = (type16)read_reg(0, 1);
	}
	tmp &= 0x3fff;
	write_reg(8 + 0, 2, tmp * 14 + properties);
}

void write_string(void)
{
	static type32 offset_bak;
	static type8 mask_bak;
	type8 c, b, mask;
	type16 ptr;
	type32 offset;

	if (!cflag)
	{
		/* new string */
		ptr = (type16)read_reg(0, 1);
		if (!ptr)
			offset = 0;
		else
		{
			offset = read_w(&decode_table[0x100 + 2 * ptr]);
			if (read_w(&decode_table[0x100]))
			{
				if (ptr >= read_w(&decode_table[0x100]))
					offset += string_size;
			}
		}
		mask = 1;
	}
	else
	{
		offset = offset_bak;
		mask = mask_bak;
	}
	do
	{
		c = 0;
		while (c < 0x80)
		{
			if (offset >= string_size)
				b = string2[offset - string_size];
			else
			{
				if (offset >= MAX_STRING_SIZE)
					b = string3[offset - MAX_STRING_SIZE];
				else
					b = string[offset];
			}
			if (b & mask)
				c = decode_table[0x80 + c];
			else
				c = decode_table[c];
			mask <<= 1;
			if (!mask)
			{
				mask = 1;
				offset++;
			}
		}
		c &= 0x7f;
		if (c && ((c != 0x40) || (lastchar != 0x20)))
			char_out(c);
	}
	while (c && ((c != 0x40) || (lastchar != 0x20)));
	cflag = c ? 0xff : 0;
	if (c)
	{
		offset_bak = offset;
		mask_bak = mask;
	}
}

void output_number(type16 number)
{
	type16 tens = number / 10;

	if (tens > 0)
		ms_putchar('0'+tens);
	number -= (tens*10);
	ms_putchar('0'+number);
}

type16 output_text(const type8* text)
{
	type16 i;

	for (i = 0; text[i] != 0; i++)
		ms_putchar(text[i]);
	return i;
}

type16s hint_input(void)
{
	type8 c1, c2, c3;

	output_text(">>");
	ms_flush();

	do
	{
		c1 = ms_getchar(0);
	}
	while (c1 == '\n');
	if (c1 == 1)
		return -1; /* New game loaded, abort hints */

	c2 = ms_getchar(0);
	if (c2 == 1)
		return -1;

	/* Get and discard any remaining characters */
	c3 = c2;
	while (c3 != '\n')
	{
		c3 = ms_getchar(0);
		if (c3 == 1)
			return -1;
	}
	ms_putchar('\n');

	if ((c1 >= '0') && (c1 <= '9'))
	{
		type16 number = c1 - '0';
		if ((c2 >= '0') && (c2 <= '9'))
		{
			number *= 10;
			number += c2 - '0';
		}
		return number;
	}

	if ((c1 >= 'A') && (c1 <= 'Z'))
		c1 = 'a' + (c1 - 'A');
	if ((c1 >= 'a') && (c1 <= 'z'))
	{
		switch (c1)
		{
		case 'e':
			return -2; /* End hints */
		case 'n':
			return -3; /* Next hint */
		case 'p':
			return -4; /* Show parent hint list */
		}
	}
	return 0;
}

type16 show_hints_text(struct ms_hint* hints, type16 index)
{
	type16 i = 0, j = 0;
	type16s input;
	struct ms_hint* hint = hints+index;

	while (1)
	{
		switch (hint->nodetype)
		{

		case 1: /* folders */
			output_text("Hint categories:\n");
			for (i = 0, j = 0; i < hint->elcount; i++)
			{
				output_number(i+1);
				output_text(". ");
				j += output_text(hint->content+j)+1;
				ms_putchar('\n');
			}
			output_text("Enter hint category number, ");
			if (hint->parent != 0xffff)
				output_text("P for the parent hint menu, ");
			output_text("or E to end hints.\n");

			input = hint_input();
			switch (input)
			{
			case -1: /* A new game is being loaded */
				return 1;
			case -2: /* End hints */
				return 1;
			case -4: /* Show parent hint list */
				if (hint->parent != 0xffff)
					return 0;
			default:
				if ((input > 0) && (input <= hint->elcount))
				{
					if (show_hints_text(hints,hint->links[input-1]) == 1)
						return 1;
				}
				break;
			}
			break;

		case 2: /* hints */
			if (i < hint->elcount)
			{
				output_number(i+1);
				output_text(". ");
				j += output_text(hint->content+j)+1;

				if (i == hint->elcount-1)
				{
					output_text("\nNo more hints.\n");
					return 0; /* Last hint */
				}
				else
				{
					output_text("\nEnter N for the next hint, ");
					output_text("P for the parent hint menu, ");
					output_text("or E to end hints.\n");
				}

				input = hint_input();
				switch (input)
				{
				case -1: /* A new game is being loaded */
					return 1;
				case -2: /* End hints */
					return 1;
				case -3:
					i++;
					break;
				case -4: /* Show parent hint list */
					return 0;
				}
			}
			else
				return 0;
			break;
		}
	}
	return 0;
}

void do_line_a(void)
{
	type8 l1c;
	type8s *str;
	type16 ptr, ptr2, tmp16, dtype;
	type32 a1reg, tmp32;

#ifdef LOGGFX
/*
	if (((byte2-0xdd) == 4) || ((byte2-0xdd) == 13))
		out2("--> %d\n",byte2-0xdd);
	else
		out2("LINE A %d\n",byte2-0xdd);
 */
#endif
	if ((byte2 < 0xdd) || (version < 4 && byte2 < 0xe4) || (version < 2 && byte2 < 0xed))
	{
		ms_flush();	/* flush output-buffer */
		rand_emu();	/* Increase game randomness */
		l1c = ms_getchar(1);	/* 0 means UNDO */
		if (l1c == 1)
			return;
		if (l1c)
			write_reg(1, 2, l1c);	/* d1=getkey() */
		else
		{
			if ((l1c = ms_undo()) != 0)
				output_text(undo_ok);
			else
				output_text(undo_fail);
			if (!l1c)
				write_reg(1, 2, '\n');
		}
	}
	else
		switch (byte2 - 0xdd)
		{

		case 0:	/* A0DD - Won't probably be needed at all */
			break;

		case 1:	/* A0DE */
			write_reg(1, 0, 1);	/* Should remove the manual check */
			break;

		case 2:	/* A0DF */
			a1reg = (type32)read_reg(9,2);
			dtype = (code+a1reg+2)[0];

			switch (dtype)
			{
			case 7: /* Picture */
#ifdef LOGGFX
				out2("PICTURE IS %s\n", code + a1reg + 3);
#endif
				/* gfx mode = normal, df is not called if graphics are off */
				ms_showpic(a1reg + 3, 2);
				break;

			case 10: /* Open window commands */
				switch ((code+a1reg+3)[0])
				{
				case 4: /* Help/Hints */
					if (hints != 0)
					{
						if (ms_showhints(hints) == 0)
							show_hints_text(hints,0);
					}
					else
						output_text(no_hints);
					break;
				case 0: /* Carried items */
				case 1: /* Room items */
				case 2: /* Map */
				case 3: /* Compass */
					output_text(not_supported);
					break;
				}
				break;

			case 13: /* Music */
				switch ((code+a1reg+3)[0])
				{
				case 0: /* stop music */
					ms_playmusic(0,0,0);
					break;
				default: /* play music */
#ifdef LOGSND
					out2("MUSIC IS %s\n", code + a1reg + 3);
#endif
					{
						type32 length = 0;
						type16 tempo = 0;
						type8* midi = sound_extract(code + a1reg + 3,&length,&tempo);
						if (midi != NULL)
							ms_playmusic(midi,length,tempo);
					}
					break;
				}
				break;
			}
			break;

		case 3:	/* A0E0 */
			/* printf("A0E0 stubbed\n"); */
			break;

		case 4:	/* A0E1 Read from keyboard to (A1), status in D1 (0 for ok) */
			ms_flush();
			rand_emu();
			tmp32 = read_reg(8 + 1, 2);
			str = (type8s*)effective(tmp32);
			tmp16 = 0;
			do
			{
				if (!(l1c = ms_getchar(1)))
				{
					if ((l1c = ms_undo()) != 0)
						output_text(undo_ok);
					else
						output_text(undo_fail);
					if (!l1c)
					{
						tmp16 = 0;
						str[tmp16++] = '\n';
						l1c = '\n';
						output_text("\n>");
					}
					else
					{
						ms_putchar('\n');
						return;
					}
				}
				else
				{
					if (l1c == 1)
						return;
					str[tmp16++] = l1c;
#ifdef LOGGFX
					out2("%c",l1c);
#endif
				}
			}
			while (l1c != '\n' && tmp16 < 256);
			write_reg(8 + 1, 2, tmp32 + tmp16 - 1);
			if (tmp16 != 256 && tmp16 != 1)
				write_reg(1, 1, 0);
			else
				write_reg(1, 1, 1);
			break;

		case 5:	/* A0E2 */
			/* printf("A0E2 stubbed\n"); */
			break;

		case 6:	/* A0E3 */
			if (read_reg(1, 2) == 0)
			{
				if ((version < 4) || (read_reg(6, 2) == 0))
					ms_showpic(0, 0);
			}
			/* printf("\nMoves: %u\n",read_reg(0,1)); */
			break;

		case 7:	/* A0E4 sp+=4, RTS */
			write_reg(8 + 7, 1, read_reg(8 + 7, 1) + 4);
			pc = pop();
			break;

		case 8:	/* A0E5 set z, RTS */
		case 9:	/* A0E6 clear z, RTS */
			pc = pop();
			zflag = (byte2 == 0xe5) ? 0xff : 0;
			break;

		case 10: /* A0E7 set z */
			zflag = 0xff;
			break;

		case 11: /* A0E8 clear z */
			zflag = 0;
			break;

		case 12: /* A0E9 [3083 - j] */
			ptr = (type16)read_reg(8 + 0, 1);
			ptr2 = (type16)read_reg(8 + 1, 1);
			do
			{
				l1c = dict[ptr2++];
				effective(ptr++)[0] = l1c;
			}
			while ((l1c & 0x80) == 0);
			write_reg(8 + 0, 1, ptr);
			write_reg(8 + 1, 1, ptr2);
			break;

		case 13: /* A0EA A1=write_dictword(A1,D1=output_mode) */
			ptr = (type16)read_reg(8 + 1, 1);
			tmp32 = read_reg(3, 0);
			write_reg(3, 0, read_reg(1, 0));
			do
			{
				l1c = dict[ptr++];
				char_out(l1c);
			}
			while (l1c < 0x80);
			write_reg(8 + 1, 1, ptr);
			write_reg(3, 0, tmp32);
			break;

		case 14: /* A0EB [3037 - j] */
			dict[read_reg(8 + 1, 1)] = (type8)read_reg(1, 0);
			break;

		case 15: /* A0EC */
			write_reg(1, 0, dict[read_reg(8 + 1, 1)]);
			break;

		case 16:
			ms_stop();	/* infinite loop A0ED */
			break;
		case 17:
			if (!ms_init(0, 0, 0, 0))
				ms_stop();	/* restart game ie. pc, sp etc. A0EE */
			break;
		case 18:	/* printer A0EF */
			break;
		case 19:
			ms_showpic(read_reg(0, 0), (type8)read_reg(1, 0));	/* Do_picture(D0) A0F0 */
			break;
		case 20:
			ptr = (type16)read_reg(8 + 1, 1);	/* A1=nth_string(A1,D0) A0F1 */
			tmp32 = read_reg(0, 1);
			while (tmp32-- > 0)
			{
				while (effective(ptr++)[0]);
			}
			write_reg(8 + 1, 1, ptr);
			break;

		case 21: /* [2a43] A0F2 */
			cflag = 0;
			write_reg(0, 1, read_reg(2, 1));
			do_findprop();
			ptr = (type16)read_reg(8 + 0, 1);
			while (read_reg(2, 1) > 0)
			{
				if (read_w(effective(ptr + 12)) & 0x3fff)
				{
					cflag = 0xff;
					break;
				}
				if (read_reg(2, 1) == (read_reg(4, 1) & 0x7fff))
				{
					cflag = 0xff;
					break;
				}
				ptr -= 0x0e;
				write_reg(2, 1, read_reg(2, 1) - 1);
			}
			break;

		case 22:
			char_out((type8)read_reg(1, 0));	/* A0F3 */
			break;

		case 23: /* D7=Save_(filename A0) D1 bytes starting from A1  A0F4 */
			str = (version < 4) ? (type8s*)effective(read_reg(8 + 0, 1)) : 0;
			write_reg(7, 0, ms_save_file(str, effective(read_reg(8 + 1, 1)),
				(type16)read_reg(1, 1)));
			break;

		case 24: /* D7=Load_(filename A0) D1 bytes starting from A1  A0F5 */
			str = (version < 4) ? (type8s*)effective(read_reg(8 + 0, 1)) : 0;
			write_reg(7, 0, ms_load_file(str, effective(read_reg(8 + 1, 1)),
				(type16)read_reg(1, 1)));
			break;

		case 25: /* D1=Random(0..D1-1) [3748] A0F6 */
			l1c = (type8)read_reg(1, 0);
			write_reg(1, 1, rand_emu() % (l1c ? l1c : 1));
			break;

		case 26: /* D0=Random(0..255) [3742] A0F7 */
			tmp16 = (type16)rand_emu();
			write_reg(0, 0, tmp16 + (tmp16 >> 8));
			break;

		case 27: /* write string [D0] [2999] A0F8 */
			write_string();
			break;

		case 28: /* Z,D0=Get_inventory_item(D0) [2a9e] A0F9 */
			zflag = 0;
			ptr = (type16)read_reg(0, 1);
			do
			{
				write_reg(0, 1, ptr);
				do
				{
					do_findprop();
					ptr2 = (type16)read_reg(8 + 0, 1);	/* object properties */
					if ((effective(ptr2)[5]) & 1)
						break;	/* is_described or so */
					l1c = effective(ptr2)[6];	/* some_flags */
					tmp16 = read_w(effective(ptr2 + 8));	/* parent_object */
					if (!l1c)
					{
						/* ordinary object? */
						if (!tmp16)
							zflag = 0xff;	/* return if parent()=player */
						break;	/* otherwise try next */
					}
					if (l1c & 0xcc)
						break;	/* skip worn, bodypart, room, hidden */
					if (tmp16 == 0)
					{
						/* return if parent()=player? */
						zflag = 0xff;
						break;
					}
					write_reg(0, 1, tmp16);	/* else look at parent() */
				}
				while (1);
				ptr--;
			}
			while ((!zflag) && ptr);
			write_reg(0, 1, ptr + 1);
			break;

		case 29: /* [2b18] A0FA */
			ptr = (type16)read_reg(8, 1);
			do
			{
				if (read_reg(5, 0))
				{
					l1c = ((type32)((read_w(effective(ptr)) & 0x3fff)) == read_reg(2, 1));
				}
				else
				{
					l1c = (effective(ptr)[0] == read_reg(2, 0));
				}
				if (read_reg(3, 1) == read_reg(4, 1))
				{
					cflag = 0;
					write_reg(8, 1, ptr);
				}
				else
				{
					write_reg(3, 1, read_reg(3, 1) + 1);
					ptr += 0x0e;
					if (l1c)
					{
						cflag = 0xff;
						write_reg(8, 1, ptr);
					}
				}
			}
			while ((!l1c) && (read_reg(3, 1) != read_reg(4, 1)));
			break;

		case 30: /* [2bd1] A0FB */
			ptr = (type16)read_reg(8 + 1, 1);
			do
			{
				if (dict)
					while (dict[ptr++] < 0x80);
				else
					while (effective(ptr++)[0] < 0x80);
				write_reg(2, 1, read_reg(2, 1) - 1);
			}
			while (read_reg(2, 1));
			write_reg(8 + 1, 1, ptr);
			break;

		case 31: /* [2c3b] A0FC */
			ptr = (type16)read_reg(8 + 0, 1);
			ptr2 = (type16)read_reg(8 + 1, 1);
			do
			{
				if (dict)
					while (dict[ptr++] < 0x80);
				else
					while (effective(ptr++)[0] < 0x80);
				while (effective(ptr2++)[0]);
				write_reg(0, 1, read_reg(0, 1) - 1);
			}
			while (read_reg(0, 1));
			write_reg(8 + 0, 1, ptr);
			write_reg(8 + 1, 1, ptr2);
			break;

		case 32: /* Set properties pointer from A0 [2b7b] A0FD */
			properties = (type16)read_reg(8 + 0, 1);
			if (version > 0)
				fl_sub = (type16)read_reg(8 + 3, 1);
			if (version > 1)
			{
				fl_tab = (type16)read_reg(8 + 5, 1);
				fl_size = (type16)read_reg(7, 1) + 1;
				/* A3 [routine], A5 [table] and D7 [table-size] */
			}
			if (version > 2)
			{
				fp_tab = (type16)read_reg(8 + 6, 1);
				fp_size = (type16)read_reg(6, 1);
			}
			break;

		case 33: /* A0FE */
			do_findprop();
			break;

		case 34: /* Dictionary_lookup A0FF */
			dict_lookup();
			break;
		}
}

/* emulate an instruction [1b7e] */

type8 ms_rungame(void)
{
	type8 l1c;
	type16 ptr;
	type32 tmp32;
#ifdef LOGEMU
	static int stat = 0;
#endif

	if (!running)
		return running;
	if (pc == undo_pc)
		save_undo();

#ifdef LOGEMU
	if (pc == 0x0000)
		stat = 0;
	if (stat)
	{
		log_status();
		fflush(dbg_log);
	}

	fprintf(dbg_log, "%.8X: ", pc);
#endif
	i_count++;
	read_word();
	switch (byte1 >> 1)
	{

/* 00-0F */
	case 0x00:
		if (byte1 == 0x00)
		{
			if (byte2 == 0x3c || byte2 == 0x7c)
			{
				/* OR immediate to CCR (30D9) */
				read_word();
#ifdef LOGEMU
				out("or_ccr #%.2x", byte2);
#endif
				if (byte2 & 0x01)
					cflag = 0xff;
				if (byte2 & 0x02)
					vflag = 0xff;
				if (byte2 & 0x04)
					zflag = 0xff;
				if (byte2 & 0x08)
					nflag = 0xff;
			}
			else
			{
				/* OR [27df] */
#ifdef LOGEMU
				out("or");
#endif
				get_arg();
				do_or();
			}
		}
		else
			check_btst();
		break;

	case 0x01:
		if (byte1 == 0x02)
		{
			if (byte2 == 0x3c || byte2 == 0x7c)
			{
				/* AND immediate to CCR */
				read_word();
#ifdef LOGEMU
				out("and_ccr #%.2x", byte2);
#endif
				if (!(byte2 & 0x01))
					cflag = 0;
				if (!(byte2 & 0x02))
					vflag = 0;
				if (!(byte2 & 0x04))
					zflag = 0;
				if (!(byte2 & 0x08))
					nflag = 0;
			}
			else
			{
				/* AND */
#ifdef LOGEMU
				out("and");
#endif
				get_arg();
				do_and();
			}
		}
		else
			check_btst();
		break;

	case 0x02:
		if (byte1 == 0x04)
		{
			/* SUB */
#ifdef LOGEMU
			out("sub");
#endif
			get_arg();
			do_sub(0);
		}
		else
			check_btst();
		break;

	case 0x03:
		if (byte1 == 0x06)
		{
			/* ADD */
#ifdef LOGEMU
			out("addi");
#endif
			get_arg();
			do_add(0);
		}
		else
			check_btst();
		break;

	case 0x04:
		if (byte1 == 0x08)
		{
			/* bit operations (immediate) */
			set_info((type8)(byte2 & 0x3f));
			l1c = (effective(pc))[1];
			pc += 2;
			set_arg1();
			do_bop(byte2, l1c);
		}
		else
			check_btst();
		break;

	case 0x05:
		if (byte1 == 0x0a)
		{
			if (byte2 == 0x3c || byte2 == 0x7c)
			{
				/* EOR immediate to CCR */
				read_word();
#ifdef LOGEMU
				out("eor_ccr #%.2X", byte2);
#endif
				if (byte2 & 0x01)
					cflag ^= 0xff;
				if (byte2 & 0x02)
					vflag ^= 0xff;
				if (byte2 & 0x04)
					zflag ^= 0xff;
				if (byte2 & 0x08)
					nflag ^= 0xff;
			}
			else
			{
				/* EOR */
#ifdef LOGEMU
				out("eor");
#endif
				get_arg();
				do_eor();
			}
		}
		else
			check_btst();
		break;

	case 0x06:
		if (byte1 == 0x0c)
		{
			/* CMP */
#ifdef LOGEMU
			out("cmp");
#endif
			get_arg();
			do_cmp();
		}
		else
			check_btst();
		break;

	case 0x07:
		check_btst();
		break;

/* 10-1F [3327] MOVE.B */
	case 0x08: case 0x09: case 0x0a: case 0x0b:
	case 0x0c: case 0x0d: case 0x0e: case 0x0f:

#ifdef LOGEMU
		out("move.b");
#endif
		set_info((type8)(byte2 & 0x3f));
		set_arg1();
		swap_args();
		l1c = (byte1 >> 1 & 0x07) | (byte2 >> 3 & 0x18) | (byte1 << 5 & 0x20);
		set_info(l1c);
		set_arg1();
		do_move();
		break;

/* 20-2F [32d1] MOVE.L */
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:

#ifdef LOGEMU
		out("move.l");
#endif
		set_info((type8)((byte2 & 0x3f) | 0x80));
		set_arg1();
		swap_args();
		l1c = (byte1 >> 1 & 0x07) | (byte2 >> 3 & 0x18) | (byte1 << 5 & 0x20);
		set_info((type8)(l1c | 0x80));
		set_arg1();
		do_move();
		break;

/* 30-3F [3327] MOVE.W */
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f:

#ifdef LOGEMU
		out("move.w");
#endif
		set_info((type8)((byte2 & 0x3f) | 0x40));
		set_arg1();
		swap_args();
		l1c = (byte1 >> 1 & 0x07) | (byte2 >> 3 & 0x18) | (byte1 << 5 & 0x20);
		set_info((type8)(l1c | 0x40));
		set_arg1();
		do_move();
		break;

/* 40-4F various commands */

	case 0x20:
		if (byte1 == 0x40)
		{
			/* [31d5] */
			ms_fatal("unimplemented instructions NEGX and MOVE SR,xx");
		}
		else
			check_lea();
		break;

	case 0x21:
		if (byte1 == 0x42)
		{
			/* [3188] */
			if ((byte2 & 0xc0) == 0xc0)
			{
				ms_fatal("unimplemented instruction MOVE CCR,xx");
			}
			else
			{
				/* CLR */
#ifdef LOGEMU
				out("clr");
#endif
				set_info(byte2);
				set_arg1();
				if (opsize == 0)
					arg1[0] = 0;
				if (opsize == 1)
					write_w(arg1, 0);
				if (opsize == 2)
					write_l(arg1, 0);
				nflag = cflag = 0;
				zflag = 0xff;
			}
		}
		else
			check_lea();
		break;

	case 0x22:
		if (byte1 == 0x44)
		{
			/* [31a0] */
			if ((byte2 & 0xc0) == 0xc0)
			{
				/* MOVE to CCR */
#ifdef LOGEMU
				out("move_ccr");
#endif
				zflag = nflag = cflag = vflag = 0;
				set_info((type8)(byte2 & 0x7f));
				set_arg1();
				byte2 = arg1[1];
				if (byte2 & 0x01)
					cflag = 0xff;
				if (byte2 & 0x02)
					vflag = 0xff;
				if (byte2 & 0x04)
					zflag = 0xff;
				if (byte2 & 0x08)
					nflag = 0xff;
			}
			else
			{
#ifdef LOGEMU
				out("neg");
#endif
				set_info(byte2);	/* NEG */
				set_arg1();
				cflag = 0xff;
				if (opsize == 0)
				{
					arg1[0] = (-arg1[0]);
					cflag = arg1[0] ? 0xff : 0;
				}
				if (opsize == 1)
				{
					write_w(arg1, (type16)(-1 * read_w(arg1)));
					cflag = read_w(arg1) ? 0xff : 0;
				}
				if (opsize == 2)
				{
					write_l(arg1, -1 * read_l(arg1));
					cflag = read_l(arg1) ? 0xff : 0;
				}
				vflag = 0;
				set_flags();
			}
		}
		else
			check_lea();
		break;

	case 0x23:
		if (byte1 == 0x46)
		{
			if ((byte2 & 0xc0) == 0xc0)
			{
				ms_fatal("unimplemented instruction MOVE xx,SR");
			}
			else
			{
#ifdef LOGEMU
				out("not");
#endif
				set_info(byte2);	/* NOT */
				set_arg1();
				tmparg[0] = tmparg[1] = tmparg[2] = tmparg[3] = 0xff;
				arg2 = tmparg;
				do_eor();
			}
		}
		else
			check_lea();
		break;

	case 0x24:
		if (byte1 == 0x48)
		{
			if ((byte2 & 0xf8) == 0x40)
			{
#ifdef LOGEMU
				out("swap");
#endif
				opsize = 2;	/* SWAP */
				admode = 0;
				regnr = byte2 & 0x07;
				set_arg1();
				tmp32 = read_w(arg1);
				write_w(arg1, read_w(arg1 + 2));
				write_w(arg1 + 2, (type16)tmp32);
				set_flags();
			}
			else if ((byte2 & 0xf8) == 0x80)
			{
#ifdef LOGEMU
				out("ext.w");
#endif
				opsize = 1;	/* EXT.W */
				admode = 0;
				regnr = byte2 & 0x07;
				set_arg1();
				if (arg1[1] > 0x7f)
					arg1[0] = 0xff;
				else
					arg1[0] = 0;
				set_flags();
			}
			else if ((byte2 & 0xf8) == 0xc0)
			{
#ifdef LOGEMU
				out("ext.l");
#endif
				opsize = 2;	/* EXT.L */
				admode = 0;
				regnr = byte2 & 0x07;
				set_arg1();
				if (read_w(arg1 + 2) > 0x7fff)
					write_w(arg1, 0xffff);
				else
					write_w(arg1, 0);
				set_flags();
			}
			else if ((byte2 & 0xc0) == 0x40)
			{
#ifdef LOGEMU
				out("pea");
#endif
				set_info((type8)((byte2 & 0x3f) | 0x80));	/* PEA */
				set_arg1();
				if (is_reversible)
					push(arg1i);
				else
					ms_fatal("illegal addressing mode for PEA");
			}
			else
			{
				check_movem();	/* MOVEM */
			}
		}
		else
			check_lea();
		break;

	case 0x25:
		if (byte1 == 0x4a)
		{
			/* [3219] TST */
			if ((byte2 & 0xc0) == 0xc0)
			{
				ms_fatal("unimplemented instruction TAS");
			}
			else
			{
#ifdef LOGEMU
				out("tst");
#endif
				set_info(byte2);
				set_arg1();
				cflag = vflag = 0;
				set_flags();
			}
		}
		else
			check_lea();
		break;

	case 0x26:
		if (byte1 == 0x4c)
			check_movem2();		/* [3350] MOVEM.L (Ax)+,A/Dx */
		else
			check_lea();	/* LEA */
		break;

	case 0x27:
		if (byte1 == 0x4e)
		{
			/* [3290] */
			if (byte2 == 0x75)
			{
				/* RTS */
#ifdef LOGEMU
				out("rts\n");
#endif
				pc = pop();
			}
			else if (byte2 == 0x71)
			{
				/* NOP */
#ifdef LOGEMU
				out("nop");
#endif
			}
			else if ((byte2 & 0xc0) == 0xc0)
			{
				/* indir JMP */
#ifdef LOGEMU
				out("jmp");
#endif
				set_info((type8)(byte2 | 0xc0));
				set_arg1();
				if (is_reversible)
					pc = arg1i;
				else
					ms_fatal("illegal addressing mode for JMP");
			}
			else if ((byte2 & 0xc0) == 0x80)
			{
#ifdef LOGEMU
				out("jsr");
#endif
				set_info((type8)(byte2 | 0xc0));		/* indir JSR */
				set_arg1();
				push(pc);
				if (is_reversible)
					pc = arg1i;
				else
					ms_fatal("illegal addressing mode for JSR");
			}
			else
			{
				ms_fatal("unimplemented instructions 0x4EXX");
			}
		}
		else
			check_lea();	/* LEA */
		break;

/* 50-5F [2ed5] ADDQ/SUBQ/Scc/DBcc */
	case 0x28: case 0x29: case 0x2a: case 0x2b:
	case 0x2c: case 0x2d: case 0x2e: case 0x2f:

		if ((byte2 & 0xc0) == 0xc0)
		{
			set_info((type8)(byte2 & 0x3f));
			set_arg1();
			if (admode == 1)
			{
				/* DBcc */
#ifdef LOGEMU
				out("dbcc");
#endif
				if (condition(byte1) == 0)
				{
					arg1 = (arg1 - (type8 *) areg) + (type8 *) dreg - 1;
					write_w(arg1, (type16)(read_w(arg1) - 1));
					if (read_w(arg1) != 0xffff)
						branch(0);
					else
						pc += 2;
				}
				else
					pc += 2;
			}
			else
			{
				/* Scc */
#ifdef LOGEMU
				out("scc");
#endif
				arg1[0] = condition(byte1) ? 0xff : 0;
			}
		}
		else
		{
			set_info(byte2);
			set_arg1();
			quick_flag = (admode == 1) ? 0xff : 0;
			l1c = byte1 >> 1 & 0x07;
			tmparg[0] = tmparg[1] = tmparg[2] = 0;
			tmparg[3] = l1c ? l1c : 8;
			arg2 = reg_align(tmparg, opsize);
			{
#ifdef LOGEMU
				type32s outnum = 0;
				switch (opsize)
				{
				case 0:
					outnum = (type8s) arg2[0];
					break;
				case 1:
					outnum = (type16s) read_w(arg2);
					break;
				case 2:
					outnum = (type32s) read_l(arg2);
					break;
				}
#endif
				if ((byte1 & 0x01) == 1)
				{
#ifdef LOGEMU
					out("subq #%.8X", outnum);
#endif
					do_sub(0);	/* SUBQ */
				}
				else
				{
#ifdef LOGEMU
					out("addq #%.8X", outnum);
#endif
					do_add(0);	/* ADDQ */
				}
			}
		}
		break;

/* 60-6F [26ba] Bcc */

	case 0x30:
		if (byte1 == 0x61)
		{
			/* BRA, BSR */
#ifdef LOGEMU
			out("bsr");
#endif
			if (byte2 == 0)
				push(pc + 2);
			else
				push(pc);
		}
#ifdef LOGEMU
		else
			out("bra");
#endif
		if ((byte1 == 0x60) && (byte2 == 0xfe))
		{
			ms_flush();	/* flush stdout */
			ms_stop();	/* infinite loop - just exit */
		}
		branch(byte2);
		break;

	case 0x31: case 0x32: case 0x33: case 0x34:
	case 0x35: case 0x36: case 0x37:

		if (condition(byte1) == 0)
		{
#ifdef LOGEMU
			out("beq.s");
#endif
			if (byte2 == 0)
				pc += 2;
		}
		else
		{
#ifdef LOGEMU
			out("bra");
#endif
			branch(byte2);
		}
		break;

/* 70-7F [260a] MOVEQ */
	case 0x38: case 0x39: case 0x3a: case 0x3b:
	case 0x3c: case 0x3d: case 0x3e: case 0x3f:

#ifdef LOGEMU
		out("moveq");
#endif
		arg1 = (type8 *) & dreg[byte1 >> 1 & 0x07];
		if (byte2 > 127)
			nflag = arg1[0] = arg1[1] = arg1[2] = 0xff;
		else
			nflag = arg1[0] = arg1[1] = arg1[2] = 0;
		arg1[3] = byte2;
		zflag = byte2 ? 0 : 0xff;
		break;

/* 80-8F [2f36] */
	case 0x40: case 0x41: case 0x42: case 0x43:
	case 0x44: case 0x45: case 0x46: case 0x47:

		if ((byte2 & 0xc0) == 0xc0)
		{
			ms_fatal("unimplemented instructions DIVS and DIVU");
		}
		else if (((byte2 & 0xf0) == 0) && ((byte1 & 0x01) != 0))
		{
			ms_fatal("unimplemented instruction SBCD");
		}
		else
		{
#ifdef LOGEMU
			out("or?");
#endif
			set_info(byte2);
			set_arg1();
			set_arg2(1, byte1);
			if ((byte1 & 0x01) == 0)
				swap_args();
			do_or();
		}
		break;

/* 90-9F [3005] SUB */
	case 0x48: case 0x49: case 0x4a: case 0x4b:
	case 0x4c: case 0x4d: case 0x4e: case 0x4f:

#ifdef LOGEMU
		out("sub");
#endif
		quick_flag = 0;
		if ((byte2 & 0xc0) == 0xc0)
		{
			if ((byte1 & 0x01) == 1)
				set_info((type8)(byte2 & 0xbf));
			else
				set_info((type8)(byte2 & 0x7f));
			set_arg1();
			set_arg2_nosize(0, byte1);
			swap_args();
			do_sub(1);
		}
		else
		{
			set_info(byte2);
			set_arg1();
			set_arg2(1, byte1);
			if ((byte1 & 0x01) == 0)
				swap_args();
			do_sub(0);
		}
		break;

/* A0-AF various special commands [LINE_A] */

	case 0x50: case 0x56: case 0x57:		/* [2521] */
		do_line_a();
#ifdef LOGEMU
		out("LINE_A A0%.2X", byte2);
#endif
		break;

	case 0x51:
#ifdef LOGEMU
		out("rts\n");
#endif
		pc = pop();	/* RTS */
		break;

	case 0x52:
#ifdef LOGEMU
		out("bsr");
#endif
		if (byte2 == 0)
			push(pc + 2);	/* BSR */
		else
			push(pc);
		branch(byte2);
		break;

	case 0x53:
		if ((byte2 & 0xc0) == 0xc0)
		{
			/* TST [321d] */
			ms_fatal("unimplemented instructions LINE_A #$6C0-#$6FF");
		}
		else
		{
#ifdef LOGEMU
			out("tst");
#endif
			set_info(byte2);
			set_arg1();
			cflag = vflag = 0;
			set_flags();
		}
		break;

	case 0x54:
		check_movem();
		break;

	case 0x55:
		check_movem2();
		break;

/* B0-BF [2fe4] */
	case 0x58: case 0x59: case 0x5a: case 0x5b:
	case 0x5c: case 0x5d: case 0x5e: case 0x5f:

		if ((byte2 & 0xc0) == 0xc0)
		{
#ifdef LOGEMU
			out("cmp");
#endif
			if ((byte1 & 0x01) == 1)
				set_info((type8)(byte2 & 0xbf));
			else
				set_info((type8)(byte2 & 0x7f));
			set_arg1();
			set_arg2(0, byte1);
			swap_args();
			do_cmp();	/* CMP */
		}
		else
		{
			if ((byte1 & 0x01) == 0)
			{
#ifdef LOGEMU
				out("cmp");
#endif
				set_info(byte2);
				set_arg1();
				set_arg2(1, byte1);
				swap_args();
				do_cmp();	/* CMP */
			}
			else
			{
#ifdef LOGEMU
				out("eor");
#endif
				set_info(byte2);
				set_arg1();
				set_arg2(1, byte1);
				do_eor();	/* EOR */
			}
		}
		break;

/* C0-CF [2f52] EXG, AND */
	case 0x60: case 0x61: case 0x62: case 0x63:
	case 0x64: case 0x65: case 0x66: case 0x67:

		if ((byte1 & 0x01) == 0)
		{
			if ((byte2 & 0xc0) == 0xc0)
			{
				ms_fatal("unimplemented instruction MULU");
			}
			else
			{
				/* AND */
#ifdef LOGEMU
				out("and");
#endif
				set_info(byte2);
				set_arg1();
				set_arg2(1, byte1);
				if ((byte1 & 0x01) == 0)
					swap_args();
				do_and();
			}
		}
		else
		{
			if ((byte2 & 0xf8) == 0x40)
			{
#ifdef LOGEMU
				out("exg (dx)");
#endif
				opsize = 2;	/* EXG Dx,Dx */
				set_arg2(1, (type8)(byte2 << 1));
				swap_args();
				set_arg2(1, byte1);
				tmp32 = read_l(arg1);
				write_l(arg1, read_l(arg2));
				write_l(arg2, tmp32);
			}
			else if ((byte2 & 0xf8) == 0x48)
			{
				opsize = 2;	/* EXG Ax,Ax */
#ifdef LOGEMU
				out("exg (ax)");
#endif
				set_arg2(0, (type8)(byte2 << 1));
				swap_args();
				set_arg2(0, byte1);
				tmp32 = read_l(arg1);
				write_l(arg1, read_l(arg2));
				write_l(arg2, tmp32);
			}
			else if ((byte2 & 0xf8) == 0x88)
			{
				opsize = 2;	/* EXG Dx,Ax */
#ifdef LOGEMU
				out("exg (dx,ax)");
#endif
				set_arg2(0, (type8)(byte2 << 1));
				swap_args();
				set_arg2(1, byte1);
				tmp32 = read_l(arg1);
				write_l(arg1, read_l(arg2));
				write_l(arg2, tmp32);
			}
			else
			{
				if ((byte2 & 0xc0) == 0xc0)
				{
					ms_fatal("unimplemented instruction MULS");
				}
				else
				{
					set_info(byte2);
					set_arg1();
					set_arg2(1, byte1);
					if ((byte1 & 0x01) == 0)
						swap_args();
					do_and();
				}
			}
		}
		break;

/* D0-DF [2fc8] ADD */
	case 0x68: case 0x69: case 0x6a: case 0x6b:
	case 0x6c: case 0x6d: case 0x6e: case 0x6f:

#ifdef LOGEMU
		out("add");
#endif
		quick_flag = 0;
		if ((byte2 & 0xc0) == 0xc0)
		{
			if ((byte1 & 0x01) == 1)
				set_info((type8)(byte2 & 0xbf));
			else
				set_info((type8)(byte2 & 0x7f));
			set_arg1();
			set_arg2_nosize(0, byte1);
			swap_args();
			do_add(1);
		}
		else
		{
			set_info(byte2);
			set_arg1();
			set_arg2(1, byte1);
			if ((byte1 & 0x01) == 0)
				swap_args();
			do_add(0);
		}
		break;

/* E0-EF [3479] LSR ASL ROR ROL */
	case 0x70: case 0x71: case 0x72: case 0x73:
	case 0x74: case 0x75: case 0x76: case 0x77:

#ifdef LOGEMU
		out("lsr,asl,ror,rol");
#endif
		if ((byte2 & 0xc0) == 0xc0)
		{
			set_info((type8)(byte2 & 0xbf));		/* OP Dx */
			set_arg1();
			l1c = 1;	/* steps=1 */
			byte2 = (byte1 >> 1) & 0x03;
		}
		else
		{
			set_info((type8)(byte2 & 0xc7));
			set_arg1();
			if ((byte2 & 0x20) == 0)
			{
				/* immediate */
				l1c = (byte1 >> 1) & 0x07;
				if (l1c == 0)
					l1c = 8;
			}
			else
			{
				l1c = (type8)read_reg(byte1 >> 1 & 0x07, 0);
			}
			byte2 = (byte2 >> 3) & 0x03;
		}
		if ((byte1 & 0x01) == 0)
		{
			/* right */
			while (l1c-- > 0)
			{
				if (opsize == 0)
				{
					cflag = arg1[0] & 0x01 ? 0xff : 0;
					arg1[0] >>= 1;
					if (cflag && (byte2 == 3))
						arg1[0] |= 0x80;
				}
				if (opsize == 1)
				{
					cflag = read_w(arg1) & 0x01 ? 0xff : 0;
					write_w(arg1, (type16)(read_w(arg1) >> 1));
					if (cflag && (byte2 == 3))
						write_w(arg1, (type16)(read_w(arg1) | ((type16) 1 << 15)));
				}
				if (opsize == 2)
				{
					cflag = read_l(arg1) & 0x01 ? 0xff : 0;
					write_l(arg1, read_l(arg1) >> 1);
					if (cflag && (byte2 == 3))
						write_l(arg1, read_l(arg1) | ((type32) 1 << 31));
				}
			}
		}
		else
		{
			/* left */
			while (l1c-- > 0)
			{
				if (opsize == 0)
				{
					cflag = arg1[0] & 0x80 ? 0xff : 0;	/* [3527] */
					arg1[0] <<= 1;
					if (cflag && (byte2 == 3))
						arg1[0] |= 0x01;
				}
				if (opsize == 1)
				{
					cflag = read_w(arg1) & ((type16) 1 << 15) ? 0xff : 0;
					write_w(arg1, (type16)(read_w(arg1) << 1));
					if (cflag && (byte2 == 3))
						write_w(arg1, (type16)(read_w(arg1) | 0x01));
				}
				if (opsize == 2)
				{
					cflag = read_l(arg1) & ((type32) 1 << 31) ? 0xff : 0;
					write_l(arg1, read_l(arg1) << 1);
					if (cflag && (byte2 == 3))
						write_l(arg1, read_l(arg1) | 0x01);
				}
			}
		}
		set_flags();
		break;

/* F0-FF [24f3] LINE_F */
	case 0x78: case 0x79: case 0x7a: case 0x7b:
	case 0x7c: case 0x7d: case 0x7e: case 0x7f:

		if (version == 0)
		{
			/* hardcoded jump */
			char_out(l1c = (type8)read_reg(1, 0));
		}
		else if (version == 1)
		{
			/* single programmable shortcut */
			push(pc);
			pc = fl_sub;
		}
		else
		{
			/* programmable shortcuts from table */
#ifdef LOGEMU
			out("LINK: %.2X,%.2X", byte1, byte2);
#endif
			ptr = (byte1 & 7) << 8 | byte2;
			if (ptr >= fl_size)
			{
				if ((byte1 & 8) == 0)
					push(pc);
				ptr = byte1 << 8 | byte2 | 0x0800;
				ptr = fl_tab + 2 * (ptr ^ 0xffff);
				pc = (type32) ptr + (type16s) read_w(effective(ptr));
			}
			else
			{
				push(pc);
				pc = fl_sub;
			}
		}
		break;

	default:
		ms_fatal("Constants aren't and variables don't");
		break;
	}
#ifdef LOGEMU
	fprintf(dbg_log, "\n");
#endif
	return running;
}
