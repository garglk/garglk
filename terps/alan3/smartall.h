/*

        Definitions for the smart memory allocator

*/

#ifdef SMARTALLOC
extern void *sm_malloc(), *sm_calloc(), *sm_realloc(),
            *actuallymalloc(), *actuallycalloc(), *actuallyrealloc();
extern void sm_free(), actuallyfree(), sm_dump(), sm_static();
extern char *sm_strdup();

/* Redefine standard memory allocator calls to use our routines
   instead. */

#define free           sm_free
#define cfree          sm_free
#define malloc(x)      sm_malloc(__FILE__, __LINE__, (x))
#define calloc(n,e)    sm_calloc(__FILE__, __LINE__, (n), (e))
#define realloc(p,x)   sm_realloc(__FILE__, __LINE__, (p), (x))
#define strdup(i)		sm_strdup(__FILE__, __LINE__, (i))

#else

/* If SMARTALLOC is disabled, define its special calls to default to
   the standard routines.  */

#define actuallyfree(x)      free(x)
#define actuallymalloc(x)    malloc(x)
#define actuallycalloc(x,y)  calloc(x,y)
#define actuallyrealloc(x,y) realloc(x,y)
#define sm_dump(x)
#define sm_static(x)
#endif
