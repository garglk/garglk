//
//  animations.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-03-24.
//

#ifndef animations_h
#define animations_h

#include <stdio.h>

void UpdateAnimation(void);
void StartAnimations(void);
void StopAnimation(void);

void AddRoomImage(int image);
void AddItemImage(int image);
void AddSpecialImage(int image);

void Animate(int frame);

void ClearAnimationBuffer(void);
void ClearFrames(void);
void InitAnimationBuffer(void);

extern int AnimationRunning;
extern int ColorCyclingRunning;
extern int AnimationBackground;
extern int LastAnimationBackground;


#endif /* animations_h */
