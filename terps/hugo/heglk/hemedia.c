/*
	HEMEDIA.C

	Glk/Gargoyle interface functions for the Hugo Engine

	Copyright (c) 1999-2004 by Kent Tessman

	Graphics support added by Simon Baldwin
	Sound support added by Tor Andersson
	Gargoyle hacks added by Tor Andersson
*/

/* SOUND & GRAPHICS

	The tack taken here is to try to build a blorb set of resources for
	each Hugo sound and picture available on the first call.

	Thereafter, each resource requested is translated into a blorb
	resource number, and this one is used to play the sound/show the picture.

	There are two options to do resources for Glk (Gargoyle).
	A is guaranteed to work. B is not portable, but works for gargoyle
	and probably most of the other non-PDA glks.

	a) put each resource in a standard named filed (PIC%d or SND%d)
	b) build an actual BLORB file

	Because Hugo resources live in many files, and there is no way to
	know which ones they are before it's too late...
	I go with option A although it does tend to litter a lot of files...
	A smarter way would be to create the Blorb file, then recreate it every
	time more resources are discovered...

	I really wish Glk had a smarter resource management facility than Blorb,
	or that Hugo would use Blorb instead.
*/

#define GRAPHICS_SUPPORTED
#define SOUND_SUPPORTED
#define SETTITLE_SUPPORTED

void hugo_setgametitle(char *t)
{
//	garglk_set_story_name(t);
}

#define PIC 0
#define SND 1
#define MAXRES 1024

static schanid_t mchannel = NULL;
static schanid_t schannel = NULL;

static long resids[2][MAXRES];
static int numres[2] = { 0, 0 };

static int loadres(HUGO_FILE infile, int reslen, int type)
{
	char buf[4096];
	frefid_t fileref;
	strid_t stream;
	long offset;
	int id;
	int i, n;

	offset = ftell(infile);
	for (i = 0; i < numres[type]; i++)
		if (resids[type][i] == offset)
			return i;

	/* Too many resources loaded... */
	if (numres[type] + 1 == MAXRES)
		return -1;

	id = numres[type]++;
	sprintf(buf, "%s%d", type == PIC ? "PIC" : "SND", id);
	resids[type][id] = offset;

	fileref = glk_fileref_create_by_name(fileusage_Data, buf, 0);
	if (!fileref)
	{
		return -1;
	}

	stream = glk_stream_open_file(fileref, filemode_Write, 0);
	if (!stream)
	{
		glk_fileref_destroy(fileref);
		return -1;
	}

	glk_fileref_destroy(fileref);

	while (reslen > 0)
	{
		n = fread(buf, 1, reslen < sizeof buf ? reslen : sizeof buf, infile);
		if (n <= 0)
			break;
		glk_put_buffer_stream(stream, buf, n);
		reslen -= n;
	}

	glk_stream_close(stream, NULL);

	return id;
}

int hugo_hasgraphics(void)
{
	/* Returns true if the current display is capable of graphics display */
	return glk_gestalt(gestalt_Graphics, 0)
		&& glk_gestalt(gestalt_DrawImage, glk_window_get_type(mainwin));
}

int hugo_displaypicture(HUGO_FILE infile, long reslen)
{
	int id;

	/* Ignore the call if the current window is set elsewhere. */
	if (currentwin != NULL && currentwin != mainwin)
	{
		fclose(infile);
		return false;
	}

	id = loadres(infile, reslen, PIC);
	if (id < 0)
	{
		fclose(infile);
		return false;
	}

#if 0
	/* Get picture width and height for scaling. */
	glui32 width, height;
	if (glk_image_get_info(id, &width, &height))
	{
		/* Scale large images. */
		if (width > PIC_MAX_WIDTH)
		{
			height = height * PIC_MAX_WIDTH / width;
			width = PIC_MAX_WIDTH;
		}
		if (height > PIC_MAX_HEIGHT)
		{
			width = width * PIC_MAX_HEIGHT / height;
			height = PIC_MAX_HEIGHT;
		}
	}
#endif

	fclose(infile);

	/* Draw, then move cursor down to the next line. */
	glk_image_draw(mainwin, id, imagealign_InlineUp, 0);
	glk_put_char('\n');

	return true;
}

void initsound()
{
	if (!glk_gestalt(gestalt_Sound, 0))
		return;
	schannel = glk_schannel_create(0);
}

void initmusic()
{
	if (!glk_gestalt(gestalt_Sound, 0) || !glk_gestalt(gestalt_SoundMusic, 0))
		return;
	mchannel = glk_schannel_create(0);
}

int hugo_playmusic(HUGO_FILE infile, long reslen, char loop_flag)
{
	int id;

	if (!mchannel)
		initmusic();
	if (mchannel)
	{
		id = loadres(infile, reslen, SND);
		if (id < 0)
		{
			fclose(infile);
			return false;
		}
		glk_schannel_play_ext(mchannel, id, loop_flag ? -1 : 1, 0);
	}

	fclose(infile);
	return true;
}

void hugo_musicvolume(int vol)
{
	if (!mchannel) initmusic();
	if (!mchannel) return;
	glk_schannel_set_volume(mchannel, (vol * 0x10000) / 100);
}

void hugo_stopmusic(void)
{
	if (!mchannel) initmusic();
	if (!mchannel) return;
	glk_schannel_stop(mchannel);
}

int hugo_playsample(HUGO_FILE infile, long reslen, char loop_flag)
{
	int id;

	if (schannel)
	{
		id = loadres(infile, reslen, SND);
		if (id < 0)
		{
			fclose(infile);
			return false;
		}
		glk_schannel_play_ext(schannel, id, loop_flag ? -1 : 1, 0);
	}

	fclose(infile);
	return true;
}

void hugo_samplevolume(int vol)
{
	if (!schannel) initsound();
	if (!schannel) return;
	glk_schannel_set_volume(schannel, (vol * 0x10000) / 100);
}

void hugo_stopsample(void)
{
	if (!schannel) initsound();
	if (!schannel) return;
	glk_schannel_stop(schannel);
}

