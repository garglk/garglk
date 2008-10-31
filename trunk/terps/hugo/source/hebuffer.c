/*
	HEBUFFER.C

	Text-buffer management for the Hugo Engine

	Copyright (c) 2002-2006 by Kent Tessman

To use text-buffer management:

	1.  #define USE_TEXTBUFFER (probably in the appropriate
	    section in heheader.h).

	2.  Add a call to TB_Scroll() in hugo_scrollwindowup().

	3.  Add calls to TB_Clear() in hugo_clearwindow() and
	    hugo_clearfullscreen(), as appropriate (as well as
	    wherever else the port may clear a portion or all of
	    the display without going through the hugo_*()
	    functions).

	4.  Make sure the port manages current_text_x and
	    (especially) current_text_y with regard to positioning
	    and printing text.

Then:

	5.  Call TB_Find() to get the word at a given location.

NOTES:	All TB_*() calls are done in individual units, i.e., in
	characters if the port is character-based or in pixels if
	the port is pixel-based.
*/

#include "heheader.h"

#ifdef USE_TEXTBUFFER

#ifndef MAX_TEXTBUFFER_COUNT
#define MAX_TEXTBUFFER_COUNT 1000
#endif

#define TB_NONE (-1)
#define WINDOW_CELL " _win"

char allow_text_selection = true;

int tb_first_used;		/* indexes */
int tb_first_unused;
int tb_last_used;
int tb_last_unused;

int tb_used = 0;		/* counts */
int tb_unused;

typedef struct
{
	char *data;
	int left, top, right, bottom;
	int prev, next;
#ifdef TEXTBUFFER_FORMATTING
	int font, fcolor, bgcolor;
#endif
} tb_list_struct;
tb_list_struct tb_list[MAX_TEXTBUFFER_COUNT];

int tb_selected;


/* TB_Init()

	Initializes text-buffer management.
*/

void TB_Init()
{
	int i;

	for (i=0; i<MAX_TEXTBUFFER_COUNT; i++)
	{
		tb_list[i].data = NULL;
		tb_list[i].prev = i?i-1:TB_NONE;
		tb_list[i].next = (i<MAX_TEXTBUFFER_COUNT-1)?i+1:TB_NONE;
	}

	tb_first_unused = 0;
	tb_first_used = TB_NONE;
	tb_last_unused = MAX_TEXTBUFFER_COUNT-1;
	tb_last_used = TB_NONE;
	tb_used = 0;
	tb_unused = MAX_TEXTBUFFER_COUNT;
}


/* TB_IsUsed(n)

	Returns true if cell n is used.
*/

char TB_IsUsed(int n)
{
	return (tb_list[n].data)?false:true;
}


/* TB_FirstCell()

	Returns the first used cell, or -1 if none.
*/

int TB_FirstCell()
{
	if (tb_used==0) return TB_NONE;
	return tb_first_used;
}


/* TB_Remove(int n, list)

	Removes the specified word cell (at index n) from either
	the TB_USED or TB_UNUSED list.
*/

void TB_Remove(int n)
{
	/* Take the cell out of the used list */
	if (tb_list[n].prev != TB_NONE)
		tb_list[tb_list[n].prev].next = tb_list[n].next;
	if (tb_list[n].next != TB_NONE)
		tb_list[tb_list[n].next].prev = tb_list[n].prev;

	if (n==tb_first_used)
		tb_first_used = tb_list[n].next;
	if (n==tb_last_used)
		tb_last_used = tb_list[n].prev;
	tb_used--;

	/* Free this cell (TB_IsUsed() sanity check just in case) */
	if (!TB_IsUsed(n))
	{
#ifdef TB_DEBUG
		printf("TB_Remove(): freeing tb_list[%d]: '%s', tb_used=%d\n",
			n, tb_list[n].data, tb_used);
#endif
		hugo_blockfree(tb_list[n].data);
		tb_list[n].data = NULL;
	}
#ifdef TB_DEBUG
	else
	{
		printf("TB_Remove(): unallocated data: tb_list[%d]\n", n);
	}
#endif
	/* Move it to the unused list */
	tb_list[n].prev = tb_last_unused;
	if (tb_last_unused != TB_NONE)
		tb_list[tb_last_unused].next = n;
	tb_list[n].next = TB_NONE;
	if (tb_unused==0)
		tb_first_unused = n;
	tb_last_unused = n;
	tb_unused++;
}


/* TB_AddWord(char *word, left, top, right, bottom)

	Adds the specified word cell with the given boundaries.
	Called by TB_AddWord() once the word has been properly
	trimmed.
*/

int TB_AddWord(char *w, int left, int top, int right, int bottom)
{
	int i, c;
	int n;
	
#ifdef MINIMAL_WINDOWING
	if (minimal_windowing && illegal_window) return TB_NONE;
#endif

	if (!w || !strcmp(w, "") || !strcmp(w, " "))
		return TB_NONE;
	
	/* Make sure the word doesn't begin with any control codes */
	while ((unsigned char)w[0]<' ')
	{
		w++;
		if (w[0]=='\0') return TB_NONE;
	}

	/* If we've used everything, use the oldest used cell */
	if (tb_used >= MAX_TEXTBUFFER_COUNT)
	{
#ifdef TB_DEBUG
		printf("TB_AddWord(): MAX_TEXTBUFFER_COUNT reached; removing tb_first_used\n");
#endif
		TB_Remove(tb_first_used);
	}

	/* Get the cell we're going to add */
	n = tb_first_unused;

	/* Try to allocate storage for the word */
	if (!(tb_list[n].data = hugo_blockalloc(strlen(w)+1)))
	{
		/* If we fail, try to free older used cells */
		while (tb_used)
		{
#ifdef TB_DEBUG
			printf("TB_AddWord(): Initial alloc failed (tb_used = %d)\n", tb_used);
#endif
			TB_Remove(tb_first_used);
			if ((tb_list[n].data = hugo_blockalloc(strlen(w)+1)))
				break;
		}
		if (tb_used==0)
		{
#ifdef TB_DEBUG
			printf("TB_AddWord(): Unable to allocate for '%s'\n", w);
#endif
			return TB_NONE;
		}
	}

	/* Take the cell out of the unused list */
	if (tb_list[n].prev != TB_NONE)
		tb_list[tb_list[n].prev].next = tb_list[n].next;
	if (tb_list[n].next != TB_NONE)
		tb_list[tb_list[n].next].prev = tb_list[n].prev;

	if (n==tb_first_unused)
		tb_first_unused = tb_list[n].next;
	if (n==tb_last_unused)
		tb_last_unused = tb_list[n].prev;
	tb_unused--;

	/* Move it to the used list */
	tb_list[n].prev = tb_last_used;
	if (tb_last_used != TB_NONE)
		tb_list[tb_last_used].next = n;
	tb_list[n].next = TB_NONE;
	if (tb_used==0)
		tb_first_used = n;
	tb_last_used = n;
	tb_used++;
	
	/* Set its coordinates */
	
	/* Just using strcpy() will let font/color change codes through */
	/* strcpy(tb_list[n].data, w); */
	c = 0;
	for (i=0; i<(int)strlen(w); i++)
	{
		if ((unsigned char)w[i]>=' ')
			tb_list[n].data[c++] = w[i];
	}
	tb_list[n].data[c] = '\0';
	
	tb_list[n].left = left;
	tb_list[n].top = top;
	tb_list[n].right = right;
	tb_list[n].bottom = bottom;
#ifdef TEXTBUFFER_FORMATTING
	tb_list[n].font = currentfont;
	tb_list[n].fcolor = fcolor;
	tb_list[n].bgcolor = bgcolor;
#endif

#ifdef TB_DEBUG
	printf("TB_AddWord(): Added tb_list[%d]: '%s' (%d, %d, %d, %d), tb_used=%d\n",
		 n, w, left, top, right, bottom, tb_used);
#endif
	return n;
}


/* TB_AddWin(left, top, right, bottom)

	Adds a window area.  Generally only useful when
	TEXTBUFFER_FORMATTING is #defined.
*/

int TB_AddWin(int left, int top, int right, int bottom)
{
	return TB_AddWord(WINDOW_CELL, left, top, right, bottom);
}


/* TB_InBounds(n, left, top, right, bottom)

	Returns true if tb_list[n] is even partially within
	the given boundaries.
*/

char TB_InBounds(int n, int left, int top, int right, int bottom)
{
	if ((tb_list[n].left>=left && tb_list[n].left<=right &&
		tb_list[n].top>=top && tb_list[n].top<=bottom) ||
		(tb_list[n].right>=left && tb_list[n].right<=right &&
		tb_list[n].bottom>=top && tb_list[n].bottom<=bottom))
	{
		return true;
	}
	return false;
}


/* TB_Clear(left, top, right, bottom)

	Removes all cells within the given boundaries.
*/

void TB_Clear(int left, int top, int right, int bottom)
{
	int i = tb_first_used;

	/* right may be <0 when called from Printout() with
	   only control characters in the string */
	if (right < 0) return;

#ifdef TB_DEBUG
	printf("TB_Clear(%d, %d, %d, %d)\n", left, top, right, bottom);
#endif
	while (i!=TB_NONE)
	{
		/* Have to find out what the next cell is
		   before we remove this current one */
		int next = tb_list[i].next;

		if (TB_InBounds(i, left, top, right, bottom))
		{
			TB_Remove(i);
			i = next;
			continue;
		}
		if (tb_list[i].top < physical_windowtop
			|| tb_list[i].bottom<0)
		{
			TB_Remove(i);
		}

		i = next;
	}
}


/* TB_Scroll(left, top, right, bottom, size)

	Scrolls all cells within the given window up by <size>
	units.
*/

void TB_Scroll()
{
	int i;
	
	i = tb_first_used;
	while (i!=TB_NONE)
	{
		int next = tb_list[i].next;

		/* Remove first the cells which will be scrolled away */
		if (TB_InBounds(i, physical_windowleft, physical_windowtop,
			physical_windowright, physical_windowtop+lineheight/2))
		{
#ifdef TB_DEBUG
			printf("TB_Scroll(): ");
#endif
			TB_Remove(i);
		}

		/* Then scroll up the remaining cells */
		if (TB_InBounds(i, physical_windowleft, physical_windowtop+lineheight,
			physical_windowright, physical_windowbottom))
		{
#ifdef TB_DEBUG
//			printf("TB_Scroll(): scrolling tb_list[%d]: '%s'\n", i, tb_list[i].data);
#endif
			tb_list[i].top -= lineheight;
			tb_list[i].bottom -= lineheight;
			
			/* Don't scroll areas into the invalidation zone until the
			   bottom is invalidated
			*/
			if ((tb_list[i].data) &&
				!strcmp(tb_list[i].data, WINDOW_CELL) && tb_list[i].top<0 &&
				tb_list[i].bottom > tb_list[i].top)
			{
				tb_list[i].top = 0;
			}
		}

		i = next;
	}
}


/* TB_FindWord(x, y)

	Returns the (first) word found at (x, y).  Returns NULL if no
	word is found.
*/

char *TB_FindWord(int x, int y)
{
	int i;
	
	tb_selected = TB_NONE;

	if (!allow_text_selection) return NULL;

	for (i=tb_last_used; i!=TB_NONE; i=tb_list[i].prev)
	{
		if (x >= tb_list[i].left && x <=tb_list[i].right &&
			y >= tb_list[i].top && y <= tb_list[i].bottom)
		{
			static char buf[255];
			char instring = false;
			int n = 0, len;
			char *w = tb_list[i].data;
			char c;
			
			if (!strcmp(w, WINDOW_CELL)) return NULL;
			
			tb_selected = i;

			len = strlen(w);
			for (i=0; i<len; i++)
			{
				/* Start only on a useful word */
				c = w[i];
				if ((c>='0' && c<='9') || (unsigned char)c>='A')
				{
#ifdef USE_SMARTFORMATTING
					if (smartformatting)
					{
						switch ((unsigned char)c)
						{
							case 145:
							case 146:
								c = '\'';
								break;
							case 147:
							case 148:
								c = '\"';
								break;
							case 151:
								buf[n++] = '-';
								c = '-';
								break;
						}
					}
#endif
					buf[n++] = c;
					instring = true;
				}
				else if ((unsigned char)c>=' ' && instring)
				{
					buf[n++] = c;
				}
			}
			buf[n] = '\0';
		
			/* Strip off any trailing punctuation */
			i = strlen(buf);
			for (; i; i--)
			{
				if ((buf[i]>='0' && buf[i]<='9') || (unsigned char)buf[i]>='A')
					break;
				buf[i] = '\0';
			}
		
			if (n==0)
			{
				tb_selected = TB_NONE;
				return NULL;
			}
#ifdef TB_DEBUG
			printf("TB_FindWord(%d, %d) = '%s'\n", x, y, w);
#endif
			return buf;
		}
	}

#ifdef TB_DEBUG
	printf("TB_FindWord(%d, %d): no match\n", x, y);
#endif
	return NULL;
}

#endif	/* USE_TEXTBUFFER */
