/*
   winstart.c: Windows-specific code for the Glulxe interpreter.
*/

#include <windows.h>
#include "glk.h"
#include "gi_blorb.h"
#include "WinGlk.h"

int InitGlk(unsigned int iVersion);

/* Entry point for all Glk applications */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  /* Turn off Windows errors */
  SetErrorMode(SEM_NOOPENFILEERRORBOX);

  /* Attempt to initialise Glk */
  if (InitGlk(0x00000601) == 0)
    exit(0);

  /* Call the Windows specific initialization routine */
  if (winglk_startup_code(lpCmdLine) != 0)
  {
    /* Run the application */
    glk_main();

    /* There is no return from this routine */
    glk_exit();
  }

  return 0;
}

extern strid_t gamefile; /* This is defined in glulxe.h */

int winglk_startup_code(const char* cmdline)
{
	char* pszFileName;
	char* pszSep;
	char Buffer[12];
	int BufferCount;

	winglk_app_set_name("Glulxe");
	winglk_window_set_title("Glulxe Interpreter");

	pszFileName = winglk_get_initial_filename(cmdline,
		"Select a Glulx game to interpret",
		"Glulx Files (.blb;.ulx)|*.blb;*.ulx|All Files (*.*)|*.*||");

	if (pszFileName != 0)
	{
		frefid_t gameref = glk_fileref_create_by_name(fileusage_BinaryMode|fileusage_Data,pszFileName,0);
		if (gameref)
		{
			gamefile = glk_stream_open_file(gameref,filemode_Read,0);
			glk_fileref_destroy(gameref);
		}
		else
			return 0;
	}
	else
		return 0;

	/* Examine the loaded file to see what type it is. */
	glk_stream_set_position(gamefile,0,seekmode_Start);
	BufferCount = glk_get_buffer_stream(gamefile,Buffer,12);
	if (BufferCount < 12)
		return 0;

	if (Buffer[0] == 'G' && Buffer[1] == 'l' && Buffer[2] == 'u' && Buffer[3] == 'l')
	{
		if (locate_gamefile(0) == 0)
			return 0;
	}
	else if (Buffer[0] == 'F' && Buffer[1] == 'O' && Buffer[2] == 'R' && Buffer[3] == 'M'
		&& Buffer[8] == 'I' && Buffer[9] == 'F' && Buffer[10] == 'R' && Buffer[11] == 'S')
	{
		if (locate_gamefile(1) == 0)
			return 0;
	}
	else
		return 0;

	/* Set up the resource directory. */
	pszSep = strrchr(pszFileName,'\\');
	if (pszSep)
	{
		*pszSep = '\0';
		winglk_set_resource_directory(pszFileName);
	}

	return 1;
}

