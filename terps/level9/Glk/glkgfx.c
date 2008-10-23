/*
 * Additional code to do graphics with Level 9 / Glk.
 * Written for Gargoyle and Cugel by Tor Andersson.
 */

#define SIGN(x) ((x > 0) ? 1 : ((x < 0) ? -1 : 0))
#define ABS(x) (((x) < 0) ? -(x) : (x))

#define GLN_GRAPHICS_BORDER 1

void gln_update_graphics2(void);

extern char	*gln_gamefile;

static int	gln_graphics_enabled = 1;
static winid_t	gln_graphics_window = NULL;
static glui32	gln_graphics_palette[8+32] = { 0,0,0,0, 0,0,0,0 };
static char *	gln_graphics_bitmap = NULL;
static int	gln_graphics_width = 0;
static int	gln_graphics_height = 0;
static int	gln_graphics_scalex = 1;
static int	gln_graphics_scaley = 1;
static int	gln_graphics_dirty = 0;

static char gfxdir[1024];
static int bmptype = NO_BITMAPS;
static int lastbmp = -1;

void gln_update_graphics(int runops);

void
gln_kill_gfx_win(void)
{
    if (gln_graphics_bitmap)
	free(gln_graphics_bitmap);
	
    gln_graphics_bitmap = NULL;

    if (gln_graphics_window)
    {
	glk_window_close(gln_graphics_window, 0);
	gln_graphics_window = NULL;
    }

    gln_graphics_enabled = 0;
}

void
gln_init_gfx_win(int mode, int bmpw, int bmph)
{
    if (!gln_graphics_bitmap)
    {
	if (!glk_gestalt(gestalt_Graphics, 0))
		return;

	if (mode == 2)
	{
	    if (bmpw == 0 || bmph == 0)
		return;
	    
	    gln_graphics_width = bmpw;
	    gln_graphics_height = bmph;
	}
	else
	{
	    GetPictureSize(&gln_graphics_width, &gln_graphics_height);
	}
		
	if (gln_graphics_width == 0 || gln_graphics_height == 0)
	    return;

	gln_graphics_bitmap = malloc(gln_graphics_width * gln_graphics_height);
	if (!gln_graphics_bitmap)
		return;

	if (gln_graphics_width < 300)
		gln_graphics_scalex = 2;
	if (gln_graphics_height < 200)
		gln_graphics_scaley = 2;

	if (mode == 2)
	{
		gln_graphics_scalex = 1;
		gln_graphics_scaley = 1;
	}
    }

    if (!gln_graphics_window)
    {
	    gln_graphics_window = glk_window_open(gln_main_window,
			    winmethod_Above|winmethod_Fixed,
			    gln_graphics_height * gln_graphics_scaley + 30,
			    wintype_Graphics, 0);
	    if (!gln_graphics_window)
		    return;
    }
    
    gln_graphics_enabled = 1;    
}

void
os_graphics(int mode)
{
	/* bitmap mode */
	if (mode == 2)
	{
		char *s;
		strcpy(gfxdir, gln_gamefile);
		s = strrchr(gfxdir, '/'); if (s) s[1] = 0;
		s = strrchr(gfxdir, '\\'); if (s) s[1] = 0;
		bmptype = DetectBitmaps(gfxdir);
		if (bmptype == NO_BITMAPS)
			mode = 0; /* turn off... */
	}

	/* turn off ... just kill the glk window */
	if (mode == 0)
	{
	    gln_kill_gfx_win();
	}

	else
	{
	    gln_init_gfx_win(mode, 0, 0);
	}

	gln_update_graphics(0);
}

void
os_cleargraphics(void)
{
	gln_graphics_dirty = 1;
	memset(gln_graphics_bitmap, 0, gln_graphics_width * gln_graphics_height);
}

void
os_setcolour(int color, int index)
{
	glui32 rgb = 0;

	gln_graphics_dirty = 1;

#if 1 /* Amiga colors */
	switch (index)
	{
	case 0: rgb = 0x000000; break; /* black */
	case 1: rgb = 0xFF0000; break; /* red */
	case 2: rgb = 0x30E830; break; /* green */
	case 3: rgb = 0xFFFF00; break; /* yellow */
	case 4: rgb = 0x0000FF; break; /* blue */
	case 5: rgb = 0xA06800; break; /* brown */
	case 6: rgb = 0x00FFFF; break; /* cyan */
	case 7: rgb = 0xFFFFFF; break; /* white */
	}
#else /* more muted colors */
	switch (index)
	{
	case 0: rgb = 0x000000; break; /* black */
	case 1: rgb = 0xb03030; break; /* red */
	case 2: rgb = 0x30b030; break; /* green */
	case 3: rgb = 0xe0c030; break; /* yellow */
	case 4: rgb = 0x303080; break; /* blue */
	case 5: rgb = 0xA07020; break; /* brown */
	case 6: rgb = 0x40c0b0; break; /* cyan */
	case 7: rgb = 0xf0f0f0; break; /* white */
	}
#endif

	gln_graphics_palette[color] = rgb;
}

static inline int
getpixel(int x, int y)
{
	if (x >= 0 && y >= 0 && x < gln_graphics_width && y < gln_graphics_height)
		return gln_graphics_bitmap[y * gln_graphics_width + x];
	return 0;
}

static inline void
setpixel(int x, int y, int color)
{
	if (x >= 0 && y >= 0 && x < gln_graphics_width && y < gln_graphics_height)
		gln_graphics_bitmap[y * gln_graphics_width + x] = color;
}

void
os_drawline(int x1, int y1, int x2, int y2, int color1, int color2)
{
	int dx, dy;
	int s1, s2;
	int x, y;
	int swap;
	int temp;
	int i, e;

	x = x1;
	y = y1;
	dx = ABS(x2 - x1);
	dy = ABS(y2 - y1);
	s1 = SIGN(x2 - x1);
	s2 = SIGN(y2 - y1);
	swap = 0;

	if (dy > dx)
	{
		temp = dx; dx = dy; dy = temp; swap = 1;
	}

	e = 2 * dy - dx;

	for (i = 0; i < dx; i++)
	{
		if (getpixel(x, y) == color2)
			setpixel(x, y, color1);

		while (e >= 0)
		{
			e = e - 2 * dx;
			if (swap) x += s1; else y += s2;
		}

		e = e + 2 * dy;
		if (swap) y += s2; else x += s1;
	}

	if (getpixel(x2, y2) == color2)
		setpixel(x2, y2, color1);

	gln_graphics_dirty = 1;
}

static void
floodfill(int x, int y, int color1, int color2)
{
	if (x < 0 || y < 0 || x >= gln_graphics_width || y >= gln_graphics_height)
		return;
	if (getpixel(x, y) == color2)
	{
		setpixel(x, y, color1);
		floodfill(x - 1, y, color1, color2);
		floodfill(x + 1, y, color1, color2);
		floodfill(x, y + 1, color1, color2);
		floodfill(x, y - 1, color1, color2);
	}
}

void
os_fill(int x, int y, int color1, int color2)
{
	if (color1 == color2)
		return;
//	if (getpixel(x, y) == color2)
//		setpixel(x, y, color1);
	floodfill(x, y, color1, color2);
	gln_graphics_dirty = 1;
}

void
os_show_bitmap(int pic, int dx, int dy)
{
	Bitmap *bmp;
	int x, y, i;

	gln_graphics_dirty = 1;

	if (lastbmp == pic)
		return;

	bmp = DecodeBitmap(gfxdir, bmptype, pic, dx, dy);
	if (!bmp)
		return;
	
	/* kill graphics buffer if it's not the right size */
	if (gln_graphics_width != bmp->width || gln_graphics_height != bmp->height)
	    gln_kill_gfx_win();

	/* create graphics buffer */
	gln_init_gfx_win(2, bmp->width, bmp->height);

	for (y = 0; y < bmp->height; y++)
		for (x = 0; x < bmp->width; x++)
			setpixel(x, y, bmp->bitmap[ y * bmp->width + x ] + 8);

	for (i = 0; i < bmp->npalette; i++)
	{
		glui32 rgb =
			(bmp->palette[i].red << 16) |
			(bmp->palette[i].green << 8) |
			(bmp->palette[i].blue);
		gln_graphics_palette[i + 8] = rgb;
	}

	lastbmp = pic;

	// XXX huh? if (pic == 0) sleep(1000);

	/* because showbitmap is not called from RunGraphics,
	   we have to call the repainting code ourselves */

	gln_update_graphics2();
}

void
gln_update_graphics2(void)
{
	glui32 glkwidth, glkheight;
	int xoffset;
	int yoffset;
	int y, x;
	glui32 pixel;
	glui32 color;

	if (!gln_graphics_window)
		return;

	if (!gln_graphics_dirty)
		return;

	glk_window_get_size(gln_graphics_window, &glkwidth, &glkheight);
	xoffset = ((int)glkwidth - gln_graphics_width * gln_graphics_scalex ) / 2;
	yoffset = ((int)glkheight - gln_graphics_height * gln_graphics_scaley ) / 2;

	/* clear glk window */
	glk_window_clear(gln_graphics_window);

	/* draw black border */
	{
		int x = xoffset - GLN_GRAPHICS_BORDER;
		int y = yoffset - GLN_GRAPHICS_BORDER;
		int w = gln_graphics_width * gln_graphics_scalex + 2 * GLN_GRAPHICS_BORDER - 1;
		int h = gln_graphics_height * gln_graphics_scaley + 2 * GLN_GRAPHICS_BORDER - 1;
		
		//glk_window_fill_rect(gln_graphics_window, 0x00707070, x + 2, y + 2, w, h);
		//glk_window_fill_rect(gln_graphics_window, 0x00000000, x, y, w, h);
		
		glk_window_fill_rect(gln_graphics_window, 0x00000000, x + 0, y + 0, w + 1, GLN_GRAPHICS_BORDER);
		glk_window_fill_rect(gln_graphics_window, 0x00000000, x + 0, y + h, w + 1, GLN_GRAPHICS_BORDER);
		glk_window_fill_rect(gln_graphics_window, 0x00000000, x + 0, y + 0, GLN_GRAPHICS_BORDER, h + 1);
		glk_window_fill_rect(gln_graphics_window, 0x00000000, x + w, y + 0, GLN_GRAPHICS_BORDER, h + 1);
		
		//glk_window_fill_rect(gln_graphics_window, 0x00707070, x + 2, y + h + 1, w + 1, 2);
		//glk_window_fill_rect(gln_graphics_window, 0x00707070, x + w + 1, y + 2, 2, h - 1);
	}

	/* blit bitmap using fill_rect */
	for (y = 0; y < gln_graphics_height; y++)
	{
		for (x = 0; x < gln_graphics_width; x++)
		{
			pixel = gln_graphics_bitmap[y * gln_graphics_width + x];
			color = gln_graphics_palette[pixel];
			glk_window_fill_rect(gln_graphics_window, color,
				xoffset + x * gln_graphics_scalex,
				yoffset + y * gln_graphics_scaley,
				gln_graphics_scalex,
				gln_graphics_scaley);
		}
	}

	gln_graphics_dirty = 0;
}

void
gln_update_graphics(int runops)
{
	if (!gln_graphics_window)
		return;
	if (!gln_graphics_enabled)
		return;

	if (runops)
	{
		/* make level9 update the graphics bitmap */
		gln_graphics_dirty = 0;
		while (RunGraphics())
			;
		if (!gln_graphics_dirty)
			return;
	}

	gln_update_graphics2();
}

