/* glkstuff.c -- non-screen related glk stuff */

#include "glkfrotz.h"

#define VERSION "2.43"

f_setup_t f_setup;

int curr_status_ht = 0;
int mach_status_ht = 0;

winid_t gos_upper = NULL;
winid_t gos_lower = NULL;
winid_t gos_curwin = NULL;

int gos_linepending = 0;
char *gos_linebuf = NULL;
winid_t gos_linewin = NULL;

schanid_t gos_channel = NULL;

#define INFORMATION ""\
"An interpreter for Infocom and other Z-Machine games.\n"\
"Complies with standard 1.0 of Graham Nelson's specification.\n"\
"Plays Z-code versions 1-5 and 8.\n"\
"\n"\
"Syntax: frotz [options] story-file\n"\
"    -a   watch attribute setting\n"\
"    -A   watch attribute testing\n"\
"    -i   ignore fatal errors\n"\
"    -I # interpreter number\n"\
"    -o   watch object movement\n"\
"    -O   watch object locating\n"\
"    -P   alter piracy opcode\n"\
"    -Q   use old-style save format\n"\
"    -s # random number seed value\n"\
"    -S # transscript width\n"\
"    -t   set Tandy bit\n"\
"    -u # slots for multiple undo\n"\
"    -x   expand abbreviations g/x/z\n"

/* A unix-like getopt, but with the names changed to avoid any problems.  */
static int zoptind = 1;
static int zoptopt = 0;
static char *zoptarg = NULL;
static int zgetopt (int argc, char *argv[], const char *options)
{
	static int pos = 1;
	const char *p;
	if (zoptind >= argc || argv[zoptind][0] != '-' || argv[zoptind][1] == 0)
		return EOF;
	zoptopt = argv[zoptind][pos++];
	zoptarg = NULL;
	if (argv[zoptind][pos] == 0)
	{
		pos = 1;
		zoptind++;
	}
	p = strchr (options, zoptopt);
	if (zoptopt == ':' || p == NULL)
	{
		fputs ("illegal option -- ", stderr);
		goto error;
	}
	else if (p[1] == ':')
	{
		if (zoptind >= argc) {
			fputs ("option requires an argument -- ", stderr);
			goto error;
		} else {
			zoptarg = argv[zoptind];
			if (pos != 1)
				zoptarg += pos;
			pos = 1; zoptind++;
		}
	}
	return zoptopt;
error:
	fputc (zoptopt, stderr);
	fputc ('\n', stderr);
	return '?';
}

static int user_random_seed = -1;
static int user_tandy_bit = 0;
static char *graphics_filename = NULL;

void os_process_arguments(int argc, char *argv[]) 
{
	int c;

#ifdef GARGLK
	garglk_set_program_name("Frotz " VERSION);
	garglk_set_program_info(
			"Glk Frotz " VERSION "\n"
			"Original Frotz by Stefan Jokisch\n"
			"Unix port by Jim Dunleavy and David Griffith\n"
			"Glk port by Tor Andersson\n");
#endif

	/* Parse the options */
	do {
		c = zgetopt(argc, argv, "aAiI:oOPQs:S:tu:xZ:");
		switch (c)
		{
			case 'a': f_setup.attribute_assignment = 1; break;
			case 'A': f_setup.attribute_testing = 1; break;
			case 'i': f_setup.ignore_errors = 1; break;
			case 'I': f_setup.interpreter_number = atoi(zoptarg); break;
			case 'o': f_setup.object_movement = 1; break;
			case 'O': f_setup.object_locating = 1; break;
			case 'P': f_setup.piracy = 1; break;
			case 'Q': f_setup.save_quetzal = 0; break;
			case 's': user_random_seed = atoi(zoptarg); break;
			case 'S': f_setup.script_cols = atoi(zoptarg); break;
			case 't': user_tandy_bit = 1; break;
			case 'u': f_setup.undo_slots = atoi(zoptarg); break;
			case 'x': f_setup.expand_abbreviations = 1; break;
			case 'Z': f_setup.err_report_mode = atoi(zoptarg);
					  if ((f_setup.err_report_mode < ERR_REPORT_NEVER) ||
							  (f_setup.err_report_mode > ERR_REPORT_FATAL))
						  f_setup.err_report_mode = ERR_DEFAULT_REPORT_MODE;
					  break;
		}
	} while (c != EOF);

	if (((argc - zoptind) != 1) && ((argc - zoptind) != 2))
	{
		winid_t win;
		char buf[256];
		win = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
		glk_set_window(win);
		glk_put_string("FROTZ V" VERSION " -- Glk 0.6.1 interface.\n");
		glk_put_string(INFORMATION);
		sprintf(buf,
				"    -Z # error checking mode (default = %d)\n"
				"         %d = don't report errors.  "
				"%d = report first error.\n"
				"         %d = report all errors.  "
				"%d = exit after any error.\n",
				ERR_DEFAULT_REPORT_MODE, ERR_REPORT_NEVER,
				ERR_REPORT_ONCE, ERR_REPORT_ALWAYS, ERR_REPORT_FATAL);
		glk_put_string(buf);
		glk_exit();
	}
	else
	{
		char *s;

		story_name = argv[zoptind++];
		if (zoptind < argc)
			graphics_filename = argv[zoptind++];

		#ifdef GARGLK
		s = strrchr(story_name, '\\');
		if (!s) s = strrchr(story_name, '/');
		garglk_set_story_name(s ? s + 1 : story_name);
		#endif
	}
}

void os_init_screen(void)
{
	glui32 width, height;

	/*
	 * Init glk stuff
	 */

	glk_stylehint_set(wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);


	gos_lower = glk_window_open(0, 0, 0, wintype_TextGrid, 0);
	if (!gos_lower)
		gos_lower = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
	glk_window_get_size(gos_lower, &width, &height);
	glk_window_close(gos_lower, NULL);

	gos_lower = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
	gos_upper = glk_window_open(gos_lower,
			winmethod_Above | winmethod_Fixed,
			0,
			wintype_TextGrid, 0);

	gos_channel = NULL;

	glk_set_window(gos_lower);
	gos_curwin = gos_lower;

	/*
	 * Icky magic bit setting
	 */

	if (h_version == V3 && user_tandy_bit)
		h_config |= CONFIG_TANDY;

	if (h_version == V3 && gos_upper)
		h_config |= CONFIG_SPLITSCREEN;

	if (h_version == V3 && !gos_upper)
		h_config |= CONFIG_NOSTATUSLINE;

	if (h_version >= V4)
		h_config |= CONFIG_BOLDFACE | CONFIG_EMPHASIS |
			CONFIG_FIXED | CONFIG_TIMEDINPUT;

	if (h_version >= V5)
		h_flags &= ~(GRAPHICS_FLAG | MOUSE_FLAG | MENU_FLAG);

	if ((h_version >= 5) && (h_flags & SOUND_FLAG))
		h_flags |= SOUND_FLAG;

	if ((h_version == 3) && (h_flags & OLD_SOUND_FLAG))
		h_flags |= OLD_SOUND_FLAG;

	if ((h_version == 6) && (f_setup.sound != 0)) 
		h_config |= CONFIG_SOUND;

	if (h_version >= V5 && (h_flags & UNDO_FLAG))
		if (f_setup.undo_slots == 0)
			h_flags &= ~UNDO_FLAG;

	h_screen_cols = width;
	h_screen_rows = height;

	h_screen_height = h_screen_rows;
	h_screen_width = h_screen_cols;

	h_font_width = 1;
	h_font_height = 1;

	/* Must be after screen dimensions are computed.  */
	if (h_version == V6) {
		h_flags &= ~GRAPHICS_FLAG;
	}

	/* Use the ms-dos interpreter number for v6, because that's the
	 * kind of graphics files we understand.  Otherwise, use DEC.  */
	h_interpreter_number = h_version == 6 ? INTERP_MSDOS : INTERP_DEC_20;
	if (f_setup.interpreter_number > 0)
		h_interpreter_number = f_setup.interpreter_number;
	h_interpreter_version = 'F';
	{
		/* Set these per spec 8.3.2. */
		h_default_foreground = WHITE_COLOUR;
		h_default_background = BLACK_COLOUR;
		if (h_flags & COLOUR_FLAG) h_flags &= ~COLOUR_FLAG;
	}
}

int os_random_seed (void)
{
    if (user_random_seed == -1)
        /* Use the epoch as seed value */
        return (time(0) & 0x7fff);
    return user_random_seed;
}

void os_restart_game (int stage) {}

void os_fatal (char *s)
{
	if (!gos_lower)
		gos_lower = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);

	glk_set_window(gos_lower);
	glk_set_style(style_Normal);
	glk_put_string("\n\nFatal error: ");
	glk_put_string(s);
	glk_put_string("\n");
	glk_exit();
}

void os_init_setup(void)
{
	f_setup.attribute_assignment = 0;
	f_setup.attribute_testing = 0;
	f_setup.context_lines = 0;
	f_setup.object_locating = 0;
	f_setup.object_movement = 0;
	f_setup.left_margin = 0;
	f_setup.right_margin = 0;
	f_setup.ignore_errors = 0;
	f_setup.interpreter_number = 0;
	f_setup.piracy = 0;
	f_setup.undo_slots = MAX_UNDO_SLOTS;
	f_setup.expand_abbreviations = 0;
	f_setup.script_cols = 80;
	f_setup.save_quetzal = 1;
	f_setup.sound = 1;
	f_setup.err_report_mode = ERR_DEFAULT_REPORT_MODE;
}

void gos_cancel_pending_line(void)
{
	event_t ev;
	glk_cancel_line_event(gos_linewin, &ev);
	gos_linebuf[ev.val1] = '\0';
	gos_linepending = 0;
}

zchar os_read_key (int timeout, bool show_cursor)
{
	event_t ev;
	winid_t win = gos_curwin ? gos_curwin : gos_lower;

	if (gos_linepending)
		gos_cancel_pending_line();

	glk_request_char_event(win);
	if (timeout != 0)
		glk_request_timer_events(timeout * 100);

	while (1)
	{
		glk_select(&ev);
		if (ev.type == evtype_Arrange) {
			gos_update_height();
			gos_update_width();
		}
		else if (ev.type == evtype_Timer)
		{
			glk_cancel_char_event(win);
			glk_request_timer_events(0);
			return ZC_TIME_OUT;
		}
		else if (ev.type == evtype_CharInput)
			break;
	}

	glk_request_timer_events(0);

	if (gos_upper && mach_status_ht < curr_status_ht)
		reset_status_ht();
	curr_status_ht = 0;

	switch (ev.val1)
	{
		case keycode_Escape: return ZC_ESCAPE;
		case keycode_PageUp: return ZC_ARROW_MIN;
		case keycode_PageDown: return ZC_ARROW_MAX;
		case keycode_Left: return ZC_ARROW_LEFT;
		case keycode_Right: return ZC_ARROW_RIGHT;
		case keycode_Up: return ZC_ARROW_UP;
		case keycode_Down: return ZC_ARROW_DOWN;
		case keycode_Return: return ZC_RETURN;
		case keycode_Delete: return ZC_BACKSPACE;
		case keycode_Tab: return ZC_INDENT;
		default:
			return ev.val1;
	}
}

zchar os_read_line (int max, zchar *buf, int timeout, int width, int continued)
{
	event_t ev;
	winid_t win = gos_curwin ? gos_curwin : gos_lower;

	if (!continued && gos_linepending)
		gos_cancel_pending_line(); 

	if (!continued || !gos_linepending)
	{
		glk_request_line_event(win, buf, max - 1, strlen(buf));
		if (timeout != 0)
			glk_request_timer_events(timeout * 100);
	}

	gos_linepending = 0;

	while (1)
	{
		glk_select(&ev);
		if (ev.type == evtype_Arrange) {
			gos_update_height();
			gos_update_width();
		}
		else if (ev.type == evtype_Timer)
		{
			gos_linewin = win;
			gos_linepending = 1;
			gos_linebuf = buf;
			return ZC_TIME_OUT;
		}
		else if (ev.type == evtype_LineInput)
			break;
	}

	glk_request_timer_events(0);
	buf[ev.val1] = '\0';

	if (gos_upper && mach_status_ht < curr_status_ht)
		reset_status_ht();
	curr_status_ht = 0;

	return ZC_RETURN;
}

zword os_read_mouse(void)
{
	/* NOT IMPLEMENTED */
	return 0;
}

static glui32 flag2usage(int flag)
{
	switch (flag)
	{	
		case FILE_RESTORE:
			return fileusage_SavedGame | fileusage_BinaryMode;
		case FILE_SAVE:
			return fileusage_SavedGame | fileusage_BinaryMode;
		case FILE_SCRIPT:
			return fileusage_Transcript | fileusage_TextMode;
		case FILE_PLAYBACK:
			return fileusage_InputRecord | fileusage_TextMode;
		case FILE_RECORD:
			return fileusage_InputRecord | fileusage_TextMode;
		case FILE_LOAD_AUX:
			return fileusage_Data | fileusage_BinaryMode;
		case FILE_SAVE_AUX:
			return fileusage_Data | fileusage_BinaryMode;
	}
	return 0;
}

static glui32 flag2mode(int flag)
{
	switch (flag)
	{	
	case FILE_RESTORE:
		return filemode_Read;
	case FILE_SAVE:
		return filemode_Write;
	case FILE_SCRIPT:
		return filemode_ReadWrite;	/* append really, but with erase option */
	case FILE_PLAYBACK:
		return filemode_Read;
	case FILE_RECORD:
		return filemode_Write;
	case FILE_LOAD_AUX:
		return filemode_Read;
	case FILE_SAVE_AUX:
		return filemode_Write;
	}
	return filemode_ReadWrite;
}

strid_t frotzopenprompt(int flag)
{
	frefid_t fref;
	strid_t stm;
	glui32 gusage = flag2usage(flag);
	glui32 gmode = flag2mode(flag);

	fref = glk_fileref_create_by_prompt(gusage, gmode, 0);
	if (fref == NULL)
		return NULL;

	stm = glk_stream_open_file(fref, gmode, 0);

	glk_fileref_destroy(fref);

	return stm;
}

strid_t frotzopen(char *filename, int flag)
{
	frefid_t fref;
	strid_t stm;
	glui32 gusage = flag2usage(flag);
	glui32 gmode = flag2mode(flag);

	fref = glk_fileref_create_by_name(gusage, filename, 0);
	if (!fref)
		return NULL;

	stm = glk_stream_open_file(fref, gmode, 0);

	glk_fileref_destroy(fref);

	return stm;
}

