// Startup code for Windows Git
// $Id: git_windows.c,v 1.3 2004/05/31 12:01:51 iain Exp $

#include "glk.h"
#include "WinGlk.h"
#include "git.h"

const char * gFilename = NULL;

int winglk_startup_code(const char* cmdline)
{
	winglk_set_gui(100);
	winglk_app_set_name("Git");
	winglk_set_menu_name("&Git");
	winglk_window_set_title("Windows Git");
	winglk_set_about_text("Windows Git "GIT_VERSION_STR"-WinGlk1.30");

	gFilename = (char*)winglk_get_initial_filename(cmdline,
		"Select a Glulx game to run",
		"Glulx Files (.ulx;.blb;.blorb;.glb;.gblorb)|*.ulx;*.blb;*.blorb;*.glb;*.gblorb|All Files (*.*)|*.*||");

	winglk_load_config_file(gFilename);
	return gFilename ? 1 : 0;
}

#define CACHE_SIZE (256 * 1024)
#define UNDO_SIZE (768 * 1024)

void fatalError (const char * s)
{
    MessageBox(0,s,"Git Fatal Error",MB_OK|MB_ICONERROR);
    exit (1);
}

void glk_main ()
{
    void * file    = INVALID_HANDLE_VALUE;
    void * mapping = NULL;
    void * ptr     = NULL;
    size_t size    = 0;

    // Memory-map the gamefile.

    file = CreateFile (gFilename, GENERIC_READ, FILE_SHARE_READ, 0,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (file != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize (file, 0);
        mapping = CreateFileMapping (file, 0, PAGE_READONLY, 0, 0, 0);
    }

    if (mapping)
        ptr = MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);

    // Pass the gamefile to git.

    if (ptr)
        git (ptr, size, CACHE_SIZE, UNDO_SIZE);
    else
        fatalError ("Can't open gamefile");

    // Close the gamefile.

    if (ptr) UnmapViewOfFile (ptr);
    if (mapping) CloseHandle (mapping);
    if (file != INVALID_HANDLE_VALUE) CloseHandle (file);
}
