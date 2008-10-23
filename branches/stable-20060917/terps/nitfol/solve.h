/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i solve.c' */
#ifndef CFH_SOLVE_H
#define CFH_SOLVE_H

/* From `solve.c': */
typedef struct cycleequation cycleequation;
struct cycleequation {
  cycleequation *next;
  
  int *var;
  const int *min;
  int xcoefficient;
  int ycoefficient;
} 
;
void automap_add_cycle (cycleequation *cycle );
void automap_delete_cycles (void);
void automap_cycle_elimination (void);
void automap_cycles_fill_values (void);

#endif /* CFH_SOLVE_H */
