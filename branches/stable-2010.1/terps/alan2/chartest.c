#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
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


void main()
{
  int endOfInput = 0;
  char ch;

  while (!endOfInput) {
    fflush(stdout);
    if (read(0, &ch, 1) != 1) {
      return;
    }
    printf("%d %x '%c'\n", (int)ch, (int)ch, ch);
  }
  return;
}
