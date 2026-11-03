//
//  animations.h
//  Part of Plus, an interpreter for Scott Adams Graphic Adventures Plus
//
//  Created by Petter Sjölund on 2022-03-24.
//

#ifndef animations_h
#define animations_h

void UpdateAnimation(void);
void StopAnimation(void);

void AddRoomImage(int image);
void AddItemImage(int image);
void AddSpecialImage(int image);

void Animate(int frame);

void ClearAnimationBuffer(void);
void ClearFrames(void);
void InitAnimationBuffer(void);

extern int AnimationRunning;
extern int AnimationBackground;
extern int LastAnimationBackground;

/* Room where the current animation started, or -1 */
extern int AnimationRoom;

/* The Atari ST Spider-Man web animation is done by colour cycling */
extern int STWebAnimation;
extern int STWebAnimationFinished;

#endif /* animations_h */
