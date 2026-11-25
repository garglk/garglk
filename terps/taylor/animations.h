/*
 *  animations.h
 *  Part of TaylorMade, an interpreter for Adventure Soft UK games
 *  Based on original code by Alan Cox
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of version 2 of the GNU General Public License, as
 *  published by the Free Software Foundation. See the file COPYING.
 *
 *  Created by Petter Sjölund on 2022-03-24.
 */

#ifndef animations_h
#define animations_h

void UpdateRebelAnimations(void);
void UpdateKaylethAnimations(void);
void StartAnimations(void);
void LoadKaylethAnimationData(void);

extern int AnimationRunning;

#endif /* animations_h */
