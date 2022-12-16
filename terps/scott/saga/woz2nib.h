//
//  woz2nib.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-09.
//  Based on woz2dsk.pl by LEE Sau Dan <leesaudan@gmail.com>
//
//  This is a quick translation of Perl code that I don't fully understand,
//  so it likely contains bugs and redundant code, but seems to be working.
//

#ifndef woz2nib_h
#define woz2nib_h

#include <stdint.h>

uint8_t *woz2nib(uint8_t *ptr, size_t *len);

#endif /* woz2nib_h */
