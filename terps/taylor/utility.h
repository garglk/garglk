//
//  utility.h
//  Part of TaylorMade, an interpreter for Adventure Soft UK games
//
//  Created by Petter Sjölund on 2022-03-10.
//

#ifndef utility_h
#define utility_h

#include <stdio.h>
#include "glk.h"

void Display(winid_t w, const char *fmt, ...);
void CleanupAndExit(void);
void Fatal(const char *x);
void *MemAlloc(size_t size);
uint8_t *SeekToPos(uint8_t *buf, size_t offset);
void print_memory(int address, int length);
uint8_t *readFile(const char *name, size_t *size);

int rotate_left_with_carry(uint8_t *byte, int last_carry);
int rotate_right_with_carry(uint8_t *byte, int last_carry);

extern uint8_t *FileImage;
extern size_t FileImageLen;
extern uint8_t *EndOfData;
extern strid_t Transcript;

#endif /* utility_h */
