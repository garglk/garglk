//
//  taylordraw.h
//  Part of the AI UK graphics library (shared by ScottFree and TaylorMade)
//
//  Created by Administrator on 2025-12-10.
//

#ifndef taylordraw_h
#define taylordraw_h

#include <stdint.h>

/* Seas of Blood draws the objects of the room in the middle of the
   picture: its instruction 0xF9 calls this with its two operands. */
typedef void (*draw_obj_fn)(uint8_t x, uint8_t y);

/* The older instruction format: every game except He-Man.
   Instructions 0xFC and 0xF8 take other operands. */
#define TAYLOR_OLDER 1
/* Rebel Planet: hide the bottles in room 43 until the alcove is open. */
#define TAYLOR_REBEL_PLANET 2

typedef struct {
    uint8_t *data; /* The draw instructions: one 0xFF-terminated stream per location */
    uint8_t *end;
    uint8_t *objloc; /* Location of every object, or NULL if there are no
                        instructions that depend on them (Seas of Blood) */
    unsigned flags;
    draw_obj_fn draw_objects; /* NULL except in Seas of Blood */
} taylor_draw_config;

void InitTaylor(const taylor_draw_config *config);
int DrawTaylor(int loc, int current_location);

#endif // !taylordraw_h
