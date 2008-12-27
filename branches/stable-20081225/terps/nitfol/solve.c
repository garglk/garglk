/*  solve.c: solve cycles for the automapper
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

    The author can be reached at nitfol@deja.com
*/


#include "nitfol.h"


/* Rational number stuff - doesn't handle overflow conditions */

typedef struct rational rational;
struct rational {
  int num;
  int den;
};

static int gcd(int a, int b)
{
  if(a == 0)
    return b;
  return gcd(b % a, a);
}

static int lcm(int a, int b)
{
  return a * b / gcd(a, b);
}

static rational rational_reduce(rational r)
{
  int g = gcd(r.num, r.den);
  if(g == 0)
    return r;
  r.num /= g;
  r.den /= g;
  if(r.den < 0) {
    r.num = -r.num;
    r.den = -r.den;
  }
  return r;
}

static BOOL rational_iszero(rational r)
{
  return r.num == 0;
}

static BOOL rational_isone(rational r)
{
  return r.num == r.den;
}

static rational rational_int(int i)
{
  rational r;
  r.num = i;
  r.den = 1;
  return r;
}

static rational rational_add(rational a, rational b)
{
  rational r;
  r.num = a.num * b.den + a.den * b.num;
  r.den = a.den * b.den;
  return rational_reduce(r);
}

static rational rational_sub(rational a, rational b)
{
  rational r;
  r.num = a.num * b.den - a.den * b.num;
  r.den = a.den * b.den;
  return rational_reduce(r);
}

static rational rational_mult(rational a, rational b)
{
  a.num *= b.num;
  a.den *= b.den;
  return rational_reduce(a);
}

static rational rational_div(rational a, rational b)
{
  a.num *= b.den;
  a.den *= b.num;
  return rational_reduce(a);
}


#ifdef HEADER
typedef struct cycleequation cycleequation;
struct cycleequation {
  cycleequation *next;
  
  int *var;
  const int *min;
  int xcoefficient;
  int ycoefficient;
};
#endif

typedef struct equation equation;
struct equation {
  equation *next;

  int *var;
  const int *min;
  rational coefficient;
  int count; /* Number of times variable is used */
};

typedef struct equalist equalist;
struct equalist {
  equalist *next;
  equation *eq;
};

static equalist *equats = NULL;

void automap_add_cycle(cycleequation *cycle)
{
  equation *newx = NULL;
  equation *newy = NULL;
  equalist newlist;
  cycleequation *p;
  
  for(p = cycle; p; p=p->next) {
    equation newnode;
    newnode.var = p->var;
    newnode.min = p->min;
    newnode.coefficient.den = 1;
    newnode.coefficient.num = p->xcoefficient;
    LEadd(newx, newnode);

    newnode.coefficient.num = p->ycoefficient;
    LEadd(newy, newnode);
  }

  newlist.eq = newx;
  LEadd(equats, newlist);
  newlist.eq = newy;
  LEadd(equats, newlist);
  
  LEdestroy(cycle);
}

void automap_delete_cycles(void)
{
  equalist *p;
  for(p = equats; p; p=p->next)
    LEdestroy(p->eq);
  LEdestroy(equats);
}


/* Do Gauss-Jordan elimination. */
/* This could be done with lists instead of arrays with clever hash stuff, but
   this is simpler, and the elimination stage takes O(n^3) anyway */
void automap_cycle_elimination(void)
{
  equalist *p;
  equation *q, *v;
  equation *variablelist = NULL;
  int width = 0, height = 0;
  rational *array;
  rational *r, *s;
  int row, column, i, n;

  
  /* First, figure out how many variables we're dealing with */
  for(p = equats; p; p=p->next) {
    for(q = p->eq; q; q=q->next) {
      for(v = variablelist; v; v=v->next)
	if(v->var == q->var)
	  break;
      if(!v) {
	LEadd(variablelist, *q);
	width++;
      }
    }
    height++;
  }

  if(height == 0 || width == 0)
    return;
  
  /* An array implementation is easier to think about than a linked-list one */
  array = (rational *) n_malloc(width * height * sizeof(rational));
  
  /* Fill the array */
  r = array;
  for(p = equats; p; p=p->next) {
    for(v = variablelist; v; v=v->next) {
      for(q = p->eq; q; q=q->next)
	if(q->var == v->var)
	  break;
      *r++ = q ? q->coefficient : rational_int(0);
    }
  }

  /* Do the actual elimination */
  row = 0; column = 0;
  r = array;
  while(row < height && column < width) {
    rational c;
    /* If zero, swap with a non-zero row */
    if(rational_iszero(r[column])) {
      BOOL found = FALSE;
      for(i = row+1; i < height; i++) {
	if(!rational_iszero(array[i * width + column])) {
	  n_memswap(&array[row * width], &array[i * width],
		    width * sizeof(*array));
	  found = TRUE;
	  break;
	}
      }
      if(!found) { /* Zeroed column; try next */
	column++;
	continue;
      }
    }

    /* Divide row by leading coefficient */
    c = r[column];
    for(n = 0; n < width; n++)
      r[n] = rational_div(r[n], c);

    /* Eliminate other entries in current column */
    s = array;
    for(i = 0; i < height; i++) {
      if(i != row && !rational_iszero(s[column])) {
	c = s[column];
	for(n = 0; n < width; n++)
	  s[n] = rational_sub(s[n], rational_mult(r[n], c));
      }
      s += width;
    }
    
    r += width;
    row++; column++;
  }

  /* Delete the old lists */
  automap_delete_cycles();

  /* Count how many times each variable is used */
  for(v = variablelist; v; v=v->next)
    v->count = 0;
  r = array;
  for(i = 0; i < height; i++) {
    for(v = variablelist; v; v=v->next) {
      if(!rational_iszero(*r))
	v->count++;
      r++;
    }
  }
  
  /* Make new lists from the array */
  r = array;
  for(i = 0; i < height; i++) {
    equation *neweq = NULL;
    for(v = variablelist; v; v=v->next) {
      if(!rational_iszero(*r)) {
	equation newnode;
	newnode = *v;
	newnode.coefficient = *r;
	LEadd(neweq, newnode);
      }
      r++;
    }
    if(neweq) {
      equalist newlist;
      newlist.eq = neweq;
      LEadd(equats, newlist);
    }
  }
  
  n_free(array);
}


/* Find an edge to lengthen which would cause the least amount of lengthening
   to edges in other cycles */
static equation *select_edge(equation *cycle, int *need_recalc)
{
  equation *help = NULL;     /* Increasing its length will help other cycles */
  equation *solitary = NULL; /* Only in one cycle */
  equation *nonharm = NULL;  /* Increasing its length won't destroy things */
  BOOL is_harmful_past = FALSE;
  
  equation *p;

  for(p = cycle; p; p=p->next) {
    if(p->next && p->coefficient.num < 0) {
      equalist *t;
      BOOL pastthis = FALSE;
      BOOL is_found = FALSE;
      BOOL is_harmful = FALSE;
      BOOL is_past = FALSE;
      BOOL is_help = FALSE;
      BOOL is_compensator = FALSE;
      for(t = equats; t; t=t->next) {
	if(t->eq == cycle)
	  pastthis = TRUE;
	else {
	  rational sum = rational_int(0);
	  equation *r, *foundme = NULL;
	  BOOL first_find = TRUE;
	  for(r = t->eq; r; r=r->next) {
	    if(r->next) {
	      int value = *(r->var) ? *(r->var) : *(r->min);
	      sum = rational_add(sum, rational_mult(r->coefficient,
						    rational_int(value)));
	      if(r->count == 1 && r->coefficient.num < 0)
		is_compensator = TRUE;
	      if(r->var == p->var) {
		if(foundme)
		  first_find = FALSE;
		foundme = r;
		is_past = pastthis && (is_past || !is_found);
		is_found = TRUE;
		if(r->coefficient.num > 0)
		  is_harmful = TRUE;
	      }
	    } else if(pastthis && foundme && -sum.num < *(r->min) && first_find
		      && foundme->coefficient.num < 0)
	      is_help = TRUE;
	  }
	}
      }
      if(is_help && !is_harmful && !help)
	help = p;
      if(!is_found) {
	solitary = p;
      } else if(!is_harmful || is_compensator) {
	if(!nonharm || is_past) {
	  is_harmful_past = !is_past;
	  nonharm = p;
	}
      }
    }
  }
  
  if(help)     return help;
  if(solitary) return solitary;
  if(nonharm)  { if(is_harmful_past) *need_recalc = 2; return nonharm; }
  
  return NULL;
}



/* Fill variables with valid numbers. Assumes Gauss-Jordan elimination has
   already happened. */
void automap_cycles_fill_values(void)
{
  equalist *p;
  equation *q;
  int calccount;
  int recalculate = 0;
  
  for(p = equats; p; p=p->next)
    for(q = p->eq; q; q=q->next)
      *(q->var) = 0;

  for(calccount = 0; calccount <= recalculate; calccount++) {
    recalculate = 0;
    
  /* Last variable in each list is the dependant; all others are independant */
  /* Fill independant variables with their minimums, then calculate the
     dependant one; if it's less than its minimum, play with independants */
    for(p = equats; p; p=p->next) {
      rational sum = rational_int(0);
      for(q = p->eq; q; q=q->next) {
	if(q->next) { /* Independant */
	  int value = *(q->var) ? *(q->var) : *(q->min);
	  sum = rational_add(sum, rational_mult(q->coefficient,
						rational_int(value)));
	  *(q->var) = value;
	} else { /* Dependant */
	  int value = -sum.num;
	  if(!rational_isone(q->coefficient))
	    n_show_error(E_SYSTEM, "last coefficient not 1", q->coefficient.num);
	  if(sum.den != 1)
	    n_show_error(E_SYSTEM, "unimplemented case denominator != 1", sum.den);
	  else if(value < *(q->min)) {
	    /* Edge is not long enough - try increasing lengths of another edge
	       in cycle to lengthen it */
	    equation *m = select_edge(p->eq, &recalculate);
	    
	    if(m) {
	      rational oldval = rational_mult(m->coefficient,
					      rational_int(*(m->var)));
	      rational newval;
	      int diff = value - *(q->min);
	      sum = rational_sub(sum, oldval);
	      if(oldval.den != 1)
		n_show_error(E_SYSTEM, "unimplemented case denominator != 1", oldval.den);
	      diff += oldval.num;
	      newval = rational_div(rational_int(diff), m->coefficient);
	      *(m->var) = newval.num;
	      sum = rational_add(sum, rational_mult(m->coefficient, newval));
	      value = -sum.num;
	    }
	    if(value > *(q->min))
	      n_show_error(E_SYSTEM, "met more than needed", sum.num);
	  }
	  if(value < *(q->min))
	    n_show_error(E_SYSTEM, "failed to meet needs", sum.num);
	  *(q->var) = value;
	  sum = rational_add(sum, rational_mult(q->coefficient,
						rational_int(*(q->var))));
	  if(!rational_iszero(sum))
	    n_show_error(E_SYSTEM, "doesn't add up", sum.num);
	  
	}
      }
    }
#if 0
    {
      rational checksum = rational_int(0);
      equation *cq;
      for(cq = p->eq; cq; cq=cq->next) {
	checksum = rational_add(checksum,rational_mult(cq->coefficient,
						    rational_int(*(cq->var))));
      }
      if(checksum.num != sum.num || checksum.den != sum.den) {
	n_show_error(E_SYSTEM, "correction for correction incorrect", checksum.num);
	sum = checksum;
      }
    }
#endif
  }


#if 0
  for(p = equats; p; p=p->next) {
    rational sum = rational_int(0);
    for(q = p->eq; q; q=q->next) {
      if(*(q->var) < *(q->min))
	n_show_error(E_SYSTEM, "variable less than minimum", *(q->var));
      sum = rational_add(sum, rational_mult(q->coefficient,
					    rational_int(*(q->var))));
    }
    if(!rational_iszero(sum))
      n_show_error(E_SYSTEM, "equation not equal", sum.num / sum.den);
  }
#endif


}


