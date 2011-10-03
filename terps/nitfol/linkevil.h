/*  Evil linked-list implementation in C with macros

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    The author can be reached at ecr+@andrew.cmu.edu
*/


#define LEaddm(list, member, mallocfunc) do {                              \
                              void *temppointer = list;       \
			      list = mallocfunc(sizeof(*list)); \
			      *list = member;                 \
			      list->next = temppointer;       \
                            } while(0)

#define LEadd(list, member) LEaddm(list, member, n_malloc)

#define LEremovem(list, freefunc)  do {                       \
                              if(list) {                      \
                                void *temppointer = list;     \
			        list = list->next;            \
			        freefunc(temppointer);        \
			      }                               \
                            } while(0);

#define LEremove(list) LEremovem(list, n_free)


#define LEdestructm(list, destruct, freefunc) do {            \
			      while(list) {                   \
                                void *temppointer = list;     \
                                destruct;                     \
				list = list->next;            \
				freefunc(temppointer);        \
			      }                               \
			    } while(0)

#define LEdestruct(list, destruct) LEdestructm(list, destruct, n_free)
     
#define LEdestroym(list, freefunc) LEdestructm(list, , freefunc)

#define LEdestroy(list) LEdestroym(list, n_free)


#define LEsearch(list, dest, cond)                            \
                            do {                              \
			      for(dest = list; dest; dest=dest->next)  \
				if(cond) break;	              \
			    } while(0)

/* temp != NULL if it successfully performed the removal */
#define LEsearchremovem(list, dest, temp, cond, destruct, freefunc) \
                            do {			      \
                              dest = list;		      \
                              if(dest && (cond)) {	      \
                                temp = dest;                  \
                                destruct;                     \
                                LEremovem(list, freefunc);    \
                              } else {			      \
                                for(temp = list; temp; temp=temp->next) { \
                                  dest=temp->next;	      \
                                  if(dest && (cond)) {	      \
                            	    temp->next = dest->next;  \
                                    destruct;                 \
                            	    freefunc(dest);	      \
                                  }			      \
                                }			      \
                              }				      \
                            } while(0)

#define LEsearchremove(list, dest, temp, cond, destruct) LEsearchremovem(list, dest, temp, cond, destruct, n_free)

#define LEappend(dest, src) do {                              \
			      void *temppointer = dest;       \
			      if(dest) {                      \
			        while(dest->next)             \
				  dest = dest->next;          \
			        dest->next = src;             \
			        dest = temppointer;           \
			      } else {                        \
				dest = scr;                   \
			      }                               \
			    } while(0)




/* This is just an example: */
#if 0

/*
Use: Declare a struct like this:
*/
typedef struct {
  struct mylist *next;     /* The evil linked list library depends on this */
  int myint;
  char *mystring;
} mylist;


void foo() {
  mylist *foo = NULL;
  mylist *found;
  mylist *another = NULL;

  mylist mymember1 = { NULL, 42, "frogs" };
  mylist mymember2 = { NULL, 38, "potato" };
  mylist mymember3 = { NULL, 21, "monkey" };

  LEremove(foo);           /* Remove nonexistent head element - this is safe */

  LEadd(foo, mymember1);   /* Add an element to the head */
  LEadd(foo, mymember2);   /* Add another element to the head */
  LEadd(foo, mymember3);
  LEadd(foo, mymember1);

  LEremove(foo);           /* Remove the head element */

  LEsearch(foo, found, (strcmp(found->mystring, "potato") == 0));

  LEdup(another, found);

  LEdestroy(foo);

  LEadd(foo, mymember3);

  LEprepend(another, foo);
}

#endif
