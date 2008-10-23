/*
	IOTEST.C

	to test the screen i/o functions of
	the Hugo Engine

	Copyright (c) 1995-2005 by Kent Tessman

	Uses AP(), Printout(), Promptmore(), and SetupDisplay(), and
	SpecialChar() from hemisc.c.
*/

#include "heheader.h"

/* Defined Hugo colors: */
#define HUGO_BLACK         0
#define HUGO_BLUE          1
#define HUGO_GREEN         2
#define HUGO_CYAN          3
#define HUGO_RED           4
#define HUGO_MAGENTA       5
#define HUGO_BROWN         6
#define HUGO_WHITE         7
#define HUGO_DARK_GRAY     8
#define HUGO_LIGHT_BLUE    9
#define HUGO_LIGHT_GREEN   10
#define HUGO_LIGHT_CYAN    11
#define HUGO_LIGHT_RED     12
#define HUGO_LIGHT_MAGENTA 13
#define HUGO_YELLOW        14
#define HUGO_BRIGHT_WHITE  15

/* from hemisc.c: */
unsigned char *mem;
int game_version;
int current_text_x = 0, current_text_y = 0;
char fcolor = DEF_FCOLOR, bgcolor = DEF_BGCOLOR;
char default_bgcolor = DEF_BGCOLOR;
int SCREENWIDTH, SCREENHEIGHT;
int charwidth, lineheight;
int FIXEDCHARWIDTH, FIXEDLINEHEIGHT;
int currentpos, currentline, currentfont;
int physical_windowtop = 0, physical_windowbottom = 0,
	physical_windowleft = 0, physical_windowright = 0;
char line[MAXBUFFER];
char pbuffer[MAXBUFFER*2+1];
unsigned int textto = 0;
unsigned int arraytable = 0;
int inwindow = 0;
int full = 0;
HUGO_FILE playback;
HUGO_FILE script;
 
/* from herun.c: */
int physical_windowheight;
int lowest_windowbottom = 0;
int physical_windowwidth, physical_lowest_windowbottom;
int last_window_left, last_window_top,
	last_window_right, last_window_bottom;

/* to mimic RunWindow() in herun.c: */
#define SETWINDOWBOTTOM(n) \
	physical_lowest_windowbottom = ((lowest_windowbottom=n)-1)*FIXEDLINEHEIGHT
	
/* for compiling: */
#ifdef PALMOS
int loaded_in_memory;
unsigned int objtable;
#endif

#if !defined (FRONT_END)
int main(int argc, char *argv[])
#else
int he_main(int argc, char *argv[])
#endif
{
	int i;

	/* Set up the engine interface routines */
	game_version = HEVERSION*10+HEREVISION;

	/* Set up the initial text display */
	hugo_init_screen();
	hugo_settextcolor(fcolor = DEF_FCOLOR);
	hugo_setbackcolor(bgcolor = DEF_BGCOLOR);
	hugo_settextmode();
	hugo_clearfullscreen();
	hugo_settextwindow(1, 1,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);
	SETWINDOWBOTTOM(0);


	/* Test text positioning */

	hugo_settextpos(1, 1);
	AP("X");
	hugo_settextpos(physical_windowwidth/FIXEDCHARWIDTH, 1);
	AP("X");
	hugo_settextpos(1, physical_windowheight/FIXEDLINEHEIGHT/2);
	AP("This should appear on the middle of the screen, with an 'X' in \
each of the top corners.");
	hugo_waitforkey();


	/* Test font and color output */

	hugo_clearfullscreen();
	hugo_settextpos(1, 1);
	full = -1;

	AP("This should display \\Bbold\\b, \\Iitalic\\i, and \\Uunderlined\\u \
type, wrapping properly onto the next line when it hits the right edge of the \
screen.");
	AP("\\n\\PThis should (after a double space) print the same thing (i.e. \
\\Bbold\\b, \\Iitalic\\i, and \\Uunderline\\u), except in proportional type (if \
the system provides it).\\p");

	hugo_waitforkey();

	AP("\\n\\n\\BNow, all the colors in the Hugo rainbow:\\b\\n");

	fcolor = HUGO_BLACK;
	bgcolor = HUGO_WHITE;
	AP("Black\\;");

	fcolor = HUGO_BLUE;
	bgcolor = HUGO_BLACK;
	AP(" Blue\\;");

	fcolor = HUGO_GREEN;
	AP(" Green\\;");

	fcolor = HUGO_CYAN;
	AP(" Cyan\\;");

	fcolor = HUGO_RED;
	AP(" Red\\;");

	fcolor = HUGO_MAGENTA;
	AP(" Magenta\\;");

	fcolor = HUGO_BROWN;
	AP(" Brown\\;");

	fcolor = HUGO_WHITE;
	AP(" White \\;");

	fcolor = HUGO_DARK_GRAY;
	bgcolor = HUGO_WHITE;
	AP("Dark gray\\;");

	fcolor = HUGO_LIGHT_BLUE;
	bgcolor = HUGO_BLACK;
	AP(" Light blue\\;");

	fcolor = HUGO_LIGHT_GREEN;
	AP(" Light green\\;");

	fcolor = HUGO_LIGHT_CYAN;
	AP(" Light cyan\\;");

	fcolor = HUGO_LIGHT_RED;
	AP(" Light red\\;");

	fcolor = HUGO_LIGHT_MAGENTA;
	AP(" Light magenta\\;");

	fcolor = HUGO_YELLOW;
	AP(" Yellow\\;");

	fcolor = HUGO_BRIGHT_WHITE;
	AP(" Bright white");

	hugo_waitforkey();


	/* Test special characters */

	hugo_settextcolor(fcolor = DEF_FCOLOR);
	hugo_setbackcolor(bgcolor = DEF_BGCOLOR);
	hugo_clearfullscreen();
	hugo_settextpos(1, 1);

	hugo_font(currentfont = NORMAL_FONT);		/* fixed-width font */

	fcolor = HUGO_BRIGHT_WHITE;
	bgcolor = HUGO_GREEN;
	strcpy(line, "");
	for (i=1; i<=SCREENWIDTH/hugo_charwidth(' '); i++)
		strcat(line, "\\_");
	AP(line);
	strcpy(line, "SPECIAL CHARACTERS AND SCROLLING");
	hugo_settextpos(physical_windowwidth/FIXEDCHARWIDTH/2-strlen(line)/2, 1);
	AP(line);
	hugo_settextwindow(1, 2,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);
	SETWINDOWBOTTOM(2);
	hugo_settextpos(1, 1);
	full = 0;

	bgcolor = HUGO_BLUE;
	AP("\\nNOTE:  It is not \\Iessential\\i that all accents print on  ");
	AP("every machine (although it would be nice).  The MS-DOS");
	AP("version, for example, does not print most of the      ");
	AP("capitalized accents.  (This text should, by the way,  ");
	AP("be bright white on a blue background.  The title band ");
	AP("across the top of the screen should be green.)        ");

	hugo_font(currentfont = PROP_FONT);		/* proportional font */

	fcolor = DEF_FCOLOR;
	bgcolor = DEF_BGCOLOR;
	AP("");
	AP("Accents:  \\`A \\`E \\`I \\`O \\`U \\`Y \\`a \\`e \\`i \\`o \\`u \\`y");
	AP("\\_         \\'A \\'E \\'I \\'O \\'U \\'Y \\'a \\'e \\'i \\'o \\'u \\'y");
	AP("Tildes:   \\~N \\~n");
	AP("Umlauts:  \\:A \\:E \\:I \\:O \\:U \\:Y \\:a \\:e \\:i \\:o \\:u \\:y");
	AP("Carat:    \\^A \\^E \\^I \\^O \\^U \\^Y \\^a \\^e \\^i \\^o \\^u \\^y");
	AP("Cedilla:  \\,C \\,c");
	AP("\\nAE ligatures:  \\AE \\ae");
	AP("\\nBritish pound:  \\L\\nJapanese Yen:  \\Y\\nCents sign:  \\c");
	AP("\\nSpanish quotation marks:  \\< \\>");
	AP("Upside-down question mark and exclamation point:  \\? \\!");
	AP("\\nEm (long) dash:  \\-");
	AP("\\nAn '\\#065', printed using \"\\\\#065\" instead of \"A\"");


	/* Test scrolling/page-ending */

	AP("\\n\\n\\BPROPER PAGE ENDING\\n\\b");
	AP("As may or may not have happened by now, every time a page is filled, \
a \"[MORE...]\" prompt should appear at the bottom of the screen.");
	AP("\\nThe page should only scroll when \\Iexactly\\i one screen of text \
has been printed.  That is, the last line of the previous page should disappear \
before the \"[MORE...]\" prompt appears, and a line of text should never disappear \
off the top of the screen without the \"[MORE...]\" prompt.");
	AP("\\nScrolling should take into account the current window, which in the \
current case is everything below the title band.  The title \\\\;");
	fcolor = HUGO_BRIGHT_WHITE;
	bgcolor = HUGO_GREEN;
	AP("SPECIAL CHARACTERS AND SCROLLING\\\\;");
	fcolor = DEF_FCOLOR;
	bgcolor = DEF_BGCOLOR;
	AP(" should remain at the top of the screen.  (That last bit--the printing \
of the title--should have appeared in white and green in the middle of the line.)");

	AP("");
	for (i=1; i<=50; i++)
	{
		sprintf(line, "Line %d", i);
		AP(line);
	}

	hugo_waitforkey();

	hugo_cleanup_screen();
	
	return 0;
}


/* AP

	The all-purpose printing routine that takes care of word-wrapping.
*/

void AP (char *a)
{
	char c, sticky = false, skipspchar = false, startofline = 0;
	int b, i, j, slen;
	int newline = 0;
	int thisline, thisword;           /* widths in pixels or characters */
	int tempfont;

	char q[MAXBUFFER*2+1],            /* current word      */
		r = '\0';                 /* current character */

	static int lastfcolor, lastbgcolor, lastfont;


	/* Shameless little trick to override control characters in engine-
	   printed text, such as MS-DOS filenames that contain '\'s:
	*/
	if (a[0]==NO_CONTROLCHAR)
	{
		skipspchar = true;
		a = &a[1];
	}

	/* Semi-colon overrides LF */
	if (a[strlen(a)-1]==';' && a[strlen(a)-2]=='\\')
	{
		sticky = true;
	}

	thisline = hugo_textwidth(pbuffer);

	slen = strlen(pbuffer);
	if (slen==0) startofline = true;

	/* Check for color changes */
	if (lastfcolor!=fcolor || lastbgcolor!=bgcolor || startofline)
	{
		pbuffer[slen++] = COLOR_CHANGE;
		pbuffer[slen++] = (char)(fcolor+1);
		pbuffer[slen++] = (char)(bgcolor+1);
		pbuffer[slen] = '\0';
		lastfcolor = fcolor;
		lastbgcolor = bgcolor;
	}

	/* Check for font changes */
	if (lastfont!=currentfont || startofline)
	{
		pbuffer[slen++] = FONT_CHANGE;
		pbuffer[slen++] = (char)(currentfont+1);
		pbuffer[slen] = '\0';
		lastfont = currentfont;
	}


	/* Begin by looping through the entire provided string: */

	slen = (int)strlen(a);
	if (sticky)
	{
		slen-=2;
		if (a[slen-1]=='\\') slen--;
	}

	for (i=0; i<slen; i++)
	{
		q[0] = '\0';

		do
		{
			r = a[i];

			/* First check control characters */
			if (r=='\\' && !skipspchar)
			{
				r = a[++i];

				switch (r)
				{
					case 'n':
					{
						if (!textto)
						{
							r = '\0';
							newline = true;
						}
						else
							r = '\n';
						break;
					}
					case 'B':
					{
						currentfont |= BOLD_FONT;
						goto AddFontCode;
					}
					case 'b':
					{
						currentfont &= ~BOLD_FONT;
						goto AddFontCode;
					}
					case 'I':
					{
						currentfont |= ITALIC_FONT;
						goto AddFontCode;
					}
					case 'i':
					{
						currentfont &= ~ITALIC_FONT;
						goto AddFontCode;
					}
					case 'P':
					{
						currentfont |= PROP_FONT;
						goto AddFontCode;
					}
					case 'p':
					{
						currentfont &= ~PROP_FONT;
						goto AddFontCode;
					}
					case 'U':
					{
						currentfont |= UNDERLINE_FONT;
						goto AddFontCode;
					}
					case 'u':
					{
						currentfont &= ~UNDERLINE_FONT;
AddFontCode:
						if (!textto)
						{
							q[strlen(q)+2] = '\0';
							q[strlen(q)+1] = (char)(currentfont+1);
							q[strlen(q)] = FONT_CHANGE;
						}
						goto GetNextChar;
					}
					case '_':       /* forced space */
					{
						if (textto) r = ' ';
						else r = FORCED_SPACE;
						break;
					}
					default:
						r = SpecialChar(a, &i);
				}
			}
			else if (game_version<=22)
			{
				if (r=='~')
					r = '\"';
				else if (r=='^')
				{
					r = '\0';
					newline++;
				}
			}
			else if (r=='\n')
			{
				r = '\0';
				newline++;
			}

			/* Add the new character */
			q[strlen(q)+1] = '\0';
			q[strlen(q)] = r;
GetNextChar:
			i++;
		}
		while ((r!=' ' || textto) && r!=1 && r!='\0' && i<slen);


		/* Text may be sent to an address in the array table instead
		   of being output to the screen
		*/
		if (textto)
		{
			if (q[0]=='\0') return;

			for (j=0; j<=(int)strlen(q); j++)
			{
				/* space for array length */
				b = (game_version>=23)?2:0;

				if ((unsigned)q[j] >= ' ')
				{
					SETMEM(arraytable*16L + textto*2 + b, q[j]);
					textto++;
				}
			}

			if (i>=slen) return;
			continue;       /* back to for (i=0; i<slen; i++) */
		}

		thisword = hugo_textwidth(q);

		i--;


	/* At this point, we know we've either ended the string, hit a space,
	   etc., i.e., something that signals the end of the current word.
	*/
		strcat(pbuffer, q);
		thisline += thisword;

		if (strlen(pbuffer) >= MAXBUFFER*2) FatalError(OVERFLOW_E);


		/* Check if the current screen position plus the width of the
		   to-be-printed text exceeds the width of the screen */

		if (thisline+currentpos > physical_windowwidth)
		{
			/* If so, skim backward for a place to break the
			   line, i.e., a space or a hyphen */

			thisword = thisline;    /* smaller line length */

			/* A complicated little loop, mainly that way to enable
			   breaks on appropriate punctuation, but also to make
			   sure we don't break in the middle of a '--' dash
			*/

			/* (This loop is also the reason why FORCED_SPACE must
			   be greater than the number of colors/font codes + 1;
			   if not, a color/font change might be confused for
			   FORCED_SPACE.)
			*/
			for (j=strlen(pbuffer)-1;
				((pbuffer[j]!=FORCED_SPACE
				&& pbuffer[j]!=' ' && pbuffer[j]!='-'
				&& pbuffer[j]!='/')
				|| thisword+currentpos-
					((pbuffer[j]!='-')?hugo_charwidth(pbuffer[j]):0)
					> physical_windowwidth)
				&& j>0; j--)
			{
				thisword -= hugo_charwidth(pbuffer[j]);
			}

			strcpy(q, pbuffer);

			if (!j) /* No place to break it--infrequent case */
			{
				/* So start from the beginning and trim off a single
				screen width */
				b=0;
				for (j=0; j<=(int)strlen(pbuffer); j++)
				{
					if ((b+=hugo_charwidth(pbuffer[j]))
						> physical_windowwidth)
					break;
				}
				j--;
			}

			if (q[j]=='-' && q[j+1]=='-') j--;

			/* Print the first part of the line (with the rest
			   stored in pbuffer
			*/
			c = q[j+1];
			q[j+1] = '\0';
			tempfont = currentfont;

                        /* Can't use Rtrim() because a '\t' might be
                           embedded as a font/color code
                        */
                        while (q[strlen(q)-1]==' ') q[strlen(q)-1] = '\0';

			Printout(q);
			q[j+1] = c;

			/* Make sure that the to-be-printed line starts out
			   with the right font (i.e., if a font change was
			   processed by Printout() and is now stored in
			   currentfont)
			*/

                        /* Can't use Ltrim() because a '\t' might be
                           embedded as a font/color code
                        */
                        while (q[j+1]==' ') j++;

                        sprintf(pbuffer, "%c%c%s", FONT_CHANGE, currentfont+1,
				q+j+1);
			currentfont = tempfont;

			thisline = hugo_textwidth(pbuffer);
		}

		if (newline)
		{
			Printout(pbuffer);
			strcpy(pbuffer, "");
			thisline = 0;

			while (--newline) Printout("");
		}
	}

	if (!sticky)
	{
		Printout(pbuffer);
		strcpy(pbuffer, "");
	}
}


/* PRINTOUT

	Since printf() does not take into account cursor-relocation, 
	color-setting and window-scrolling.
*/

void Printout(char *a)
{
	char b[2], sticky = 0, trimmed = 0;
	char tempfcolor;
	int i, l;
	int last_printed_font = currentfont;


	tempfcolor = fcolor;

	if (a[strlen(a)-1]==(char)NO_NEWLINE)
	{
		a[strlen(a)-1] = '\0';
		sticky = true;
	}

	b[0] = b[1] = '\0';


	/* The easy part is just skimming <a> and processing each code
	   or printed character, as the case may be:
	*/
	
	l = 0;	/* physical length of string */

	for (i=0; i<(int)strlen(a); i++)
	{
		if ((a[i]==' ') && !trimmed && currentpos==0)
		{
			continue;
		}

		if (a[i] > ' ' || a[i]==FORCED_SPACE)
		{
			trimmed = true;
			last_printed_font = currentfont;
		}

		switch (b[0] = a[i])
		{
			case FORCED_SPACE:
				hugo_print(" ");
				l += hugo_charwidth(b[0] = ' ');
				break;

			case FONT_CHANGE:
				hugo_font(currentfont = (int)a[++i]-1);
				break;

			case COLOR_CHANGE:
				fcolor = (char)(a[++i]-1);
				hugo_settextcolor((int)(fcolor));
				hugo_setbackcolor((int)a[++i]-1);
				hugo_font(currentfont);
				break;

			default:
				l += hugo_charwidth(b[0]);
				hugo_print(b);
		}

		if (script && b[0]>=' ')
			/* fprintf() this way for Glk */
			if (fprintf(script, "%s", b) < 0) FatalError(WRITE_E);

#if defined (SCROLLBACK_DEFINED)
		if (!inwindow) hugo_sendtoscrollback(b);
#endif
	}

	/* If we've got a linefeed and didn't hit the right edge of the
	   window
	*/
	if (!sticky && currentpos+l < physical_windowwidth)
	{
		/* The background color may have to be temporarily set if we're
		   not in a window--the reason is that full lines of the
		   current background color might be printed by the OS-specific
		   scrolling function.  (This behavior is overridden by the
		   Hugo engine for in-window printing, which always adds new
		   lines in the current background color when scrolling.)
		*/
		hugo_print("\r");
		hugo_setbackcolor((inwindow)?bgcolor:default_bgcolor);

		i = currentfont;
		hugo_font(currentfont = last_printed_font);

		if (currentline > physical_windowheight/lineheight)
		{
			hugo_scrollwindowup();
		}
		else
		{
			hugo_print("\n");
		}

		hugo_font(currentfont = i);
		hugo_setbackcolor(bgcolor);
	}

#if defined (AMIGA)
	else
	{
		if (currentpos + l >= physical_windowwidth)
			AmigaForceFlush();
	}
#endif

	/* If no newline is to be printed after the current line: */
	if (sticky)
	{
		currentpos += l;
	}

	/* Otherwise, take care of all the line-feeding, line-counting,
	   etc.
	*/
	else
	{
		currentpos = 0;
		if (currentline++ > physical_windowheight/lineheight)
			currentline = physical_windowheight/lineheight;

		if (++full >= physical_windowheight/lineheight-1)
			PromptMore();

		if (script)
		{
			/* fprintf() this way for Glk */
			if (fprintf(script, "%s", "\n")<0)
				FatalError(WRITE_E);
		}

#if defined (SCROLLBACK_DEFINED)
		if (!inwindow) hugo_sendtoscrollback("\n");
#endif
	}

	fcolor = tempfcolor;
}


/* PROMPTMORE */

void PromptMore(void)
{
	int k, tempcurrentfont;
	int temp_current_text_y = current_text_y;

	tempcurrentfont = currentfont;
	hugo_font(currentfont = NORMAL_FONT);

	/* system colors */
	hugo_settextcolor(16); /* DEF_FCOLOR);  */
	hugo_setbackcolor(17); /* DEF_BGCOLOR); */

	/* +1 to force positioning to the very, very bottom of the window */
	hugo_settextpos(1, physical_windowheight/lineheight+1);
	hugo_print("[MORE...]");

	k = hugo_waitforkey();

	if (playback && k==27)         /* if ESC is pressed during playback */
	{
		if (fclose(playback))
			FatalError(READ_E);
		playback = NULL;
	}

	hugo_settextcolor(fcolor);      /* program colors */
	hugo_setbackcolor(bgcolor);

	hugo_settextpos(1, physical_windowheight/lineheight+1);
	hugo_print("         ");

	hugo_font(currentfont = tempcurrentfont);
	hugo_settextpos(1, physical_windowheight/lineheight);
	current_text_y = temp_current_text_y;
	full = 0;
}


/* SETUPDISPLAY */

void SetupDisplay(void)
{
	hugo_settextmode();

	hugo_settextwindow(1, 1,
		SCREENWIDTH/FIXEDCHARWIDTH, SCREENHEIGHT/FIXEDLINEHEIGHT);

	last_window_left = 1;
	last_window_top = 1;
	last_window_right = SCREENWIDTH/FIXEDCHARWIDTH;
	last_window_bottom = SCREENHEIGHT/FIXEDLINEHEIGHT;

	hugo_clearfullscreen();
}


/* SPECIALCHAR

	SpecialChar() is passed <a> as the string and <*i> as the
	position in the string.  The character(s) at a[*i], a[*(i+1)],
	etc. are converted into a single Latin-1 (i.e., greater than
	127) character value.

	Assume that the AP() has already encountered a control 
	character ('\'), and that a[*i]... is one of:

		`a	accent grave on following character (e.g., 'a')
		'a	accent acute on following character (e.g., 'a')
		~n	tilde on following (e.g., 'n' or 'N')
		:a	umlaut on following (e.g., 'a')
		^a	circumflex on following (e.g., 'a')
		,c	cedilla on following (e.g., 'c' or 'C')
		<	Spanish left quotation marks
		>	Spanish right quotation marks
		!	upside-down exclamation mark
		?	upside-down question mark
		ae	ae ligature
		AE	AE ligature
		c	cents symbol
		L	British pound
		Y	Japanese Yen
		-	em (long) dash
		#nnn	character value given by nnn

	Note that the return value is a single character--which will
	be either unchanged or a Latin-1 character value.
*/

char SpecialChar(char *a, int *i)
{
	char r, s, skipbracket = 0;

	r = a[*i];
	s = r;

	if (r=='\"') return r;

	if (game_version <= 22)
		if (r=='~' || r=='^') return r;

	if (r=='(')
		{r = a[++*i];
		skipbracket = true;}

	switch (r)
	{
		case '`':               /* accent grave */
		{
			/* Note that the "s = '...'" characters are
			   Latin-1 and may not display properly under,
			   e.g., DOS */

			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'a':  s = 'ý'; break;
				case 'e':  s = 'Ë'; break;
				case 'i':  s = 'Ï'; break;
				case 'o':  s = 'Ú'; break;
				case 'u':  s = '˜'; break;
				case 'A':  s = '¿'; break;
				case 'E':  s = '»'; break;
				case 'I':  s = 'Ã'; break;
				case 'O':  s = '³'; break;
				case 'U':  s = ''; break;
			}
#endif
			break;
		}
		case '\'':              /* accent acute */
		{
			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'a':  s = '·'; break;
				case 'e':  s = 'È'; break;
				case 'i':  s = 'Ì'; break;
				case 'o':  s = 'Û'; break;
				case 'u':  s = '™'; break;
				case 'y':  s = (char)0xfd; break;
				case 'A':  s = '¡'; break;
				case 'E':  s = 'Š'; break;
				case 'I':  s = 'Õ'; break;
				case 'O':  s = '²'; break;
				case 'U':  s = 'Ž'; break;
				case 'Y':  s = 'ð'; break;
			}
#endif
			break;
		}
		case '~':               /* tilde */
		{
			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'a':  s = '“'; break;
				case 'n':  s = 'Ò'; break;
				case 'o':  s = 'ž'; break;
				case 'A':  s = 'ˆ'; break;
				case 'N':  s = '‹'; break;
				case 'O':  s = '¹'; break;
			}
#endif
			break;
		}
		case '^':               /* circumflex */
		{
			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'a':  s = '’'; break;
				case 'e':  s = 'Í'; break;
				case 'i':  s = 'Ó'; break;
				case 'o':  s = 'Ù'; break;
				case 'u':  s = 'š'; break;
				case 'A':  s = '¬'; break;
				case 'E':  s = ' '; break;
				case 'I':  s = '‘'; break;
				case 'O':  s = 'Œ'; break;
				case 'U':  s = '¤'; break;
			}
#endif
			break;
		}
		case ':':               /* umlaut */
		{
			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'a':  s = '”'; break;
				case 'e':  s = 'Î'; break;
				case 'i':  s = 'Ô'; break;
				case 'o':  s = '–'; break;
				case 'u':  s = '¸'; break;
				case 'y':  s = ''; break;
				case 'A':  s = 'Ÿ'; break;
				case 'E':  s = 'À'; break;
				case 'I':  s = '¦'; break;
				case 'O':  s = '÷'; break;
				case 'U':  s = 'Ð'; break;
			}
#endif
			break;
		}
		case ',':               /* cedilla */
		{
			s = a[++*i];
#ifndef NO_LATIN1_CHARSET
			switch (s)
			{
				case 'C':  s = '«'; break;
				case 'c':  s = 'Á'; break;
			}
#endif
			break;
		}
		case '<':               /* Spanish left quotation marks */
#ifndef NO_LATIN1_CHARSET
			s = '´';
#endif
			break;
		case '>':               /* Spanish right quotation marks */
#ifndef NO_LATIN1_CHARSET
			s = 'ª';
			break;
#endif
		case '!':               /* upside-down exclamation mark */
#ifndef NO_LATIN1_CHARSET
			s = '°';
#endif
			break;
		case '?':               /* upside-down question mark */
#ifndef NO_LATIN1_CHARSET
			s = 'ø';
#endif
			break;
		case 'a':               /* ae ligature */
#ifndef NO_LATIN1_CHARSET
			s = 'Ê'; ++*i;
#else
			s = 'e'; ++*i;
#endif
			break;
		case 'A':               /* AE ligature */
#ifndef NO_LATIN1_CHARSET
			s = ''; ++*i; 
#else
			s = 'E'; ++*i;
#endif
			break;
		case 'c':               /* cents symbol */
#ifndef NO_LATIN1_CHARSET
			s = '¢';
#endif
			break;
		case 'L':               /* British pound */
#ifndef NO_LATIN1_CHARSET
			s = '£';
#endif
			break;
		case 'Y':               /* Japanese Yen */
#ifndef NO_LATIN1_CHARSET
			s = '€';
#endif
			break;
		case '-':               /* em dash */
#ifndef NO_LATIN1_CHARSET
			s = 'ó';
#endif
			break;
		case '#':               /* 3-digit decimal ASCII code */
		{
			s = (char)((a[++*i]-'0')*100);
			s += (a[++*i]-'0')*10;
			s += (a[++*i]-'0');
#ifdef NO_LATIN1_CHARSET
			if ((unsigned)s>127) s = '?';
#endif
		}
	}

	if (skipbracket)
	{
		++*i;
		if (a[*i+1]==')') ++*i;
		if (s==')') s = r;
	}

	return s;
}


/* Stub routines: */
void RunInput(void)
{}

void FatalError(int e)
{}
