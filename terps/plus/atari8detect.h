//
//  atari8detect.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-01-30.
//

#ifndef atari8detect_h
#define atari8detect_h

/* Finds the database on an Atari 8-bit disk image or its companion, and
   points *sf into it. The pictures are read by LoadAtari8Images. */
int DetectAtari8(uint8_t **sf, size_t *extent);
void LoadAtari8Images(void);

#endif /* atari8detect_h */
