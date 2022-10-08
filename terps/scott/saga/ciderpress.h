//
//  ciderpress.h
//  Part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-09-29.
//  This is based on parts of the Ciderpress source.
//  See https://github.com/fadden/ciderpress for the full version.
//
//  Includes fragments from of a2tools by Terry Kyriacopoulos and Paul Schlyter
//

#ifndef ciderpress_h
#define ciderpress_h

void InitNibImage(uint8_t *data, size_t extent);
void InitDskImage(uint8_t *data, size_t extent);
void FreeDiskImage(void);

uint8_t *ReadImageFromNib(size_t offset, size_t size, uint8_t *data, size_t datasize);
uint8_t *ReadApple2DOSFile(uint8_t *data, size_t *len, uint8_t **invimg, size_t *invimglen, uint8_t **m2);

#endif /* ciderpress_h */
