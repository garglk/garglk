/*
	HEGLKWIN.C

	Includes startup code for Windows MFC Glk Libraries
	by David Kinder, as well as an opening file-selection
	dialog apparatus and Win32 replacements for
	heglk_printfatalerror() and hugo_setgametitle().
*/

#define HE_PROGRAM_NAME		"heglk.exe"

#undef WIN32
#include "heheader.h"

#undef isascii
#define _CTYPE_DEFINED

#include <windows.h>
#include "glk.h"
#include "winglk.h"

#undef isascii
#define isascii (1)

int InitGlk(unsigned int iVersion);	/* from library */
int FrontEndDialog(void);
UINT APIENTRY CenterHookProc(HWND hwndDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);
void CenterWindow(HWND, HWND);
void GetCurrentDirectories(void);

/* Other variables: */
HINSTANCE AppInstance;
char executable_module[_MAX_PATH];
char filename[_MAX_FNAME];
char *invocationpath;

/* Directories: */
char reg_GameDir[_MAX_PATH];
char reg_SaveDir[_MAX_PATH];
char current_games_dir[_MAX_PATH];
char current_save_dir[_MAX_PATH];


/*
 * The Windows initialization stuff, including opening a file-open
 * dialog box (if necessary) and setting up "command-line" arguments.
 *
 */

/* WinMain

	Entry point for all Glk applications to set up David Kinder's
	Windows Glk library.
*/

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	char *arg2;		/* for command-line creation */
	char inquote = 0;


	/* Attempt to initialise Glk */
	if (InitGlk(0x00000500)==0)
		exit(0);

/*
 * Given that we've initialized Glk, we have to make up ANSI C
 * argc and argv to fake a command-line invocation.  If we haven't
 * been given a filename, we have to open a dialog box to get one.
 *
 */
	/* This instance of the application */
	AppInstance = hInstance;

	GetCurrentDirectories();

	/* Get the path and filename of this, the executable
	   module (hewin.exe)
	*/
	GetModuleFileName(GetModuleHandle(HE_PROGRAM_NAME),
       		executable_module, _MAX_PATH);

	/* Get invocation path and make up ANSI C's argc and argv in
	   rather caveman-esque fashion
	*/
 	invocationpath = GetCommandLine();
	/* Trim trailing spaces */
	while (invocationpath[strlen(invocationpath)-1]==' ')
		invocationpath[strlen(invocationpath)-1] = '\0';

	/* If there is no second argument, i.e., no supplied file to run,
	   open a dialogbox to prompt for one
	*/
	arg2 = invocationpath;
	while (*arg2!='\0' && (*arg2!=' ' || inquote))
	{
		if (*arg2=='\"') inquote = !inquote;
		arg2++;
	}

	/* If there's no second argument */
	if (*arg2=='\0')
	{
		/* Open "Filename to run" dialog */
		if (!FrontEndDialog()) exit (0);
	}

	/* Otherwise, the second arg must be the filename */
	else
	{
		strcpy(filename, arg2);
	}

	while (filename[0]==' ') strcpy(filename, filename+1);
	if (filename[0]=='\"')
		strcpy(filename, filename+1), filename[strlen(filename)-1] = '\0';
	filename[0] = toupper(filename[0]);

/*
 * Back to David's Glk setup:
 *
 */
	/* Turn off Windows errors */
	SetErrorMode(SEM_NOOPENFILEERRORBOX);

	/* Call the Windows specific initialization routine */
	if (winglk_startup_code() != 0)
	{
		frefid_t fref;

		fref = glk_fileref_create_by_name(fileusage_BinaryMode, filename, 0);
		game = glk_stream_open_file(fref, filemode_Read, 0);
		glk_fileref_destroy(fref);

		if (game)
			/* Run the application */
			glk_main();
		else
		{
			sprintf(line, "Unable to open file \"%s\".", filename);
			heglk_printfatalerror(line);
		}

		/* There is no return from this routine */
		glk_exit();
	}

	return 0;
}

int winglk_startup_code(void)
{
	winglk_app_set_name("Hugo Engine");
	return 1;
}


/* FrontEndDialog

	Opens a "Filename to run" common dialog.
*/

int FrontEndDialog(void)
{
	char *filetypes =
"Hugo executable files (*.hex)\0*.hex\0\
Hugo debuggable files (*.hdx)\0*.hdx\0\
All .HEX/.HDX files\0*.hex;*.hdx\0\
All files (*.*)\0*.*\0";
	OPENFILENAME ofn;

	/* "Open" dialog window creation stuff */
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = NULL;
	ofn.lpstrFilter = filetypes;
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = _MAX_FNAME;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = &current_games_dir[0];
	ofn.lpstrTitle = "Hugo Engine - Select File to Run";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER |
		OFN_HIDEREADONLY | OFN_ENABLEHOOK;
	ofn.lpfnHook = CenterHookProc;
	ofn.lpstrDefExt = "hex";

	return (GetOpenFileName(&ofn));
}


/* CenterHookProc

	Because the common dialog boxes supplied by Windows all
	position themselves at (0, 0), which looks dumb.
*/

UINT APIENTRY CenterHookProc(HWND hwndDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	LPOFNOTIFY lpon;

	lpon = (LPOFNOTIFY)lParam;

	if (uiMsg==WM_NOTIFY && lpon->hdr.code==CDN_INITDONE)
	{
		if (!wParam) hwndDlg = GetParent(hwndDlg);

		CenterWindow(hwndDlg, GetDesktopWindow()); //GetParent(hwndDlg));
		return TRUE;
	}
	return FALSE;
}


/* CenterWindow

	Centers one window over another.
*/

void CenterWindow(HWND wndChild, HWND wndParent)
{
	RECT rect;
	POINT p;
	int width, height;

	/* Find center of parent window */
	GetWindowRect(wndParent, &rect);
	p.x = (rect.right + rect.left)/2;
	p.y = (rect.bottom + rect.top)/2;

	GetWindowRect(wndChild, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	SetWindowPos(wndChild, NULL,
		p.x - width/2, p.y - height/2,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER);
}


/* GetCurrentDirectories

	Gets game and save directories from whatever might be set
	in the registry (for example, by Hugo for Windows).
*/

void GetCurrentDirectories(void)
{
	HKEY hkey;
	DWORD type;
	size_t size;

	if (RegOpenKey(HKEY_CURRENT_USER,
		"Software\\General Coffee Co.\\Hugo\\Engine",
		&hkey)==ERROR_SUCCESS)
	{
		size = sizeof(reg_GameDir);
		RegQueryValueEx(hkey, "GameDir", 0, &type,
			(LPBYTE)&reg_GameDir, &size);
		RegQueryValueEx(hkey, "SaveDir", 0, &type,
			(LPBYTE)&reg_SaveDir, &size);

		RegCloseKey(hkey);
	}

	strcpy(current_games_dir, reg_GameDir);
	SetEnvironmentVariable("HUGO_GAMES", reg_GameDir);
	SetEnvironmentVariable("HUGO_OBJECT", reg_GameDir);

	strcpy(current_save_dir, reg_SaveDir);
	SetEnvironmentVariable("HUGO_SAVE", reg_GameDir);
}


/*
 * Non-portable hugo_...() functions:
 *
 */

/* heglk_printfatalerror

	Win32 replacement for Glk Hugo generic error function.
*/

void heglk_printfatalerror(char *a)
{
	if (a[strlen(a)-1]=='\n') a[strlen(a)-1] = '\0';
	if (a[0]=='\n') a = &a[1];
	MessageBox(NULL, a, "Hugo Engine", MB_ICONERROR);
	exit(-1);
}


/* hugo_setgametitle */

void hugo_setgametitle(char *t)
{
/* If the system doesn't provide any way to set the caption for the
   screen/window, this can be empty */

// Can't actually change this under Windows, since it will change
// where registry settings are stored
//	winglk_app_set_name(t);
}
