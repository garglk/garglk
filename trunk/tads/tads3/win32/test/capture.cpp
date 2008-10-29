#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
Name
  capture.cpp - run a child process, redirecting both stdout and stderr
Function
  Runs the command line as a child process, capturing both stdout and
  stderr to stdout.  This allows redirection of both stdout and stderr to
  a single file, with a command line this:

    capture > file.txt mycmd arg1 arg2 arg3

  This runs the command "mycmd arg1 arg2 arg3", capturing both stdout
  and stderr and redirecting them to file.txt.

  Although the Windows NT command shell has a mechanism for redirecting
  stderr explicitly, the Windows 95 command shell has no such feature.
  'capture' provides a mechanism for capturing all output that is portable
  between 95 and NT.
Notes
  This program is specific to Win32 - it is not portable to other operating
  systems.
Modified
  07/30/99 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

static void errexit(char *msg)
{
    printf("%s\n", msg);
    exit(2);
}

int main(int argc, char **argv)
{
    HANDLE new_stdout;
    STARTUPINFO sinfo;
    PROCESS_INFORMATION pinfo;
    DWORD exit_code;
    char cmdline[4096];
    char exe[MAX_PATH];
    char cmd_dir[MAX_PATH];
    int i;
    DWORD result;

    /* make sure we have a command to run */
    if (argc < 2)
        errexit("usage: capture command args...");

    /* build the command line */
    for (i = 1, cmdline[0] = '\0' ; i < argc ; ++i)
    {
        strcat(cmdline, argv[i]);
        if (i + 1 < argc)
            strcat(cmdline, " ");
    }

    /* build the executable name */
    sprintf(exe, "%s.exe", argv[1]);

    /* get the current working directory */
    GetCurrentDirectory(sizeof(cmd_dir), cmd_dir);
    
    /* create an inheritable duplicate of our own stdout handle */
    if (!DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_OUTPUT_HANDLE),
                         GetCurrentProcess(), &new_stdout, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
        errexit("unable to duplicate stdout");

    /* set up our STARTUPINFO */
    memset(&sinfo, 0, sizeof(sinfo));
    sinfo.cb = sizeof(sinfo);
    sinfo.dwFlags = STARTF_USESTDHANDLES;
    sinfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    sinfo.hStdOutput = new_stdout;
    sinfo.hStdError = new_stdout;

    /* run the child program */
    result = CreateProcess(0, (char *)cmdline, 0, 0, TRUE,
                           0, 0, cmd_dir, &sinfo, &pinfo);

    /* close the thread handle */
    if (result == 0)
        errexit("unable to spawn process");

    /* close the thread handle */
    CloseHandle(pinfo.hThread);

    /* wait until the program exits */
    WaitForSingleObject(pinfo.hProcess, INFINITE);

    /* get the child result code */
    GetExitCodeProcess(pinfo.hProcess, &exit_code);

    /* done with the process handle */
    CloseHandle(pinfo.hProcess);

    /* exit with the process's exit code */
    return exit_code;
}
