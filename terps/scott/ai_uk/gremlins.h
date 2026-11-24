/*
 *	  This program is free software; you can redistribute it and/or
 *	  modify it under the terms of the GNU General Public License
 *	  as published by the Free Software Foundation; either version
 *	  2 of the License, or (at your option) any later version.
 */

/*
 *  gremlins.h
 *  Part of ScottFree, an interpreter for adventures in Scott Adams format
 *
 *  Created by Petter Sjölund on 2022-01-10.
 */

#ifndef gremlins_h
#define gremlins_h

void UpdateGremlinsAnimations(void);
void GremlinsLook(void);
void GremlinsKeepBlenderFrame(void);
void GremlinsAction(void);
void LoadExtraGermanGremlinsData(void);
void LoadExtraGermanGremlinsC64Data(void);
void LoadExtraSpanishGremlinsData(void);
void LoadExtraSpanishGremlinsC64Data(void);
void MapGremlinsSysMessages(int shift);

#endif /* gremlins_h */
