#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#ifndef _x_MINGW32__
#include <termios.h>

static struct termios term;

static void newtermio()
{
  struct termios newterm;
  tcgetattr(0, &term);
  newterm=term;
  newterm.c_lflag&=~(ECHO|ICANON);
  newterm.c_cc[VMIN]=1;
  newterm.c_cc[VTIME]=0;
  tcsetattr(0, TCSANOW, &newterm);
}

static void restoretermio()
{
  tcsetattr(0, TCSANOW, &term);
}
#endif

#ifdef __win__
#include <windows.h>
#endif

int main(int argc, char **argv)
{
  int ch;
  int endOfInput = 0;
#ifdef __win__
  INPUT_RECORD inputRecord;
  DWORD eventsRead;
  HANDLE hwnd = GetStdHandle(STD_INPUT_HANDLE);
  (void) SetConsoleMode(hwnd, ENABLE_ECHO_INPUT);

  while (!endOfInput) {
    fflush(stdout);
    if (!ReadConsoleInput(hwnd, &inputRecord, 1, &eventsRead)) {
      printf("ReadConsoleInput() failed!\n");
      return 0;
    }
    if (inputRecord.EventType == KEY_EVENT && inputRecord.Event.KeyEvent.bKeyDown) {
      if ((inputRecord.Event.KeyEvent.dwControlKeyState & ENHANCED_KEY) != 0)
	printf("Enhanced: ");
      ch = inputRecord.Event.KeyEvent.wVirtualKeyCode;
      printf("%d %x '%c'\n", (int)ch, (int)ch, ch);
    }
  }
  return 0;
#else
  while ((ch = getchar()) != EOF)
    printf("%d %x '%c'\n", (int)ch, (int)ch, ch);
#endif
}
