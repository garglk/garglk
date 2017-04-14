#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/test_err.cpp,v 1.2 1999/05/17 02:52:31 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_err.cpp - test of VM error handling
Function
  
Notes
  
Modified
  10/20/98 MJRoberts  - Creation
*/

#include <stdio.h>
#include "vmerr.h"
#include "t3test.h"

void tryItOut()
{
    static int num = 1;
    
    printf("tryItOut - throwing exception '%d'...\n", num);
    err_throw_a(1, 1, ERR_TYPE_INT, num++);
}

void wrapItUp()
{
    printf("wrapItUp\n");
}

void showIt(CVmException *exc)
{
    printf("error code = %d, int0 = %d\n",
           exc->get_error_code(), exc->get_param_int(0));
}

void handleIt(CVmException *exc)
{
    printf("handleIt - ");
    showIt(exc);
}

void f1()
{
    printf("f1: try, throw, finally - entering\n");
    err_try
    {
        tryItOut();
    }
    err_finally
    {
        wrapItUp();
    }
    err_end;
    printf("f1 - exiting\n");
}

void f2()
{
    printf("f2: try, throw, catch - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
    }
    err_end;
    printf("f2 - exiting\n");
}

void f3()
{
    printf("f3: try, throw, catch, finally - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
    }
    err_finally
    {
        wrapItUp();
    }
    err_end;
    printf("f3 - exiting\n");
}

void f4()
{
    printf("f4: try, catch - entering\n");
    err_try
    {
        printf("(not throwing anything)\n");
    }
    err_catch(exc)
    {
        handleIt(exc);
    }
    err_end;
    printf("f4 - exiting\n");
}

void f5()
{
    printf("f5: try, catch, finally - entering\n");
    err_try
    {
        printf("(not throwing anything)\n");
    }
    err_catch(exc)
    {
        handleIt(exc);
    }
    err_finally
    {
        wrapItUp();
    }
    err_end;
    printf("f5 - exiting\n");
}

void f6()
{
    printf("f6: try, throw, catch, throw - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
        tryItOut();
    }
    err_end;
    printf("f6 - exiting\n");
}

void f7()
{
    printf("f7: try, throw, catch, throw, finally - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
        tryItOut();
    }
    err_finally
    {
        printf("(in finally - not throwing anything)\n");
    }
    err_end;
    printf("f7 - exiting\n");
}

void f8()
{
    printf("f8: try, throw, catch, throw, finally, throw - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
        tryItOut();
    }
    err_finally
    {
        printf("in finally - throwing one more time\n");
        tryItOut();
    }
    err_end;
    printf("f8 - exiting\n");
}

void f9()
{
    printf("f9 - try, throw, catch, try, throw, catch - entering\n");
    err_try
    {
        tryItOut();
    }
    err_catch(exc)
    {
        handleIt(exc);
        printf("(trying nested handler within catch) ");
        err_try
        {
            tryItOut();
        }
        err_catch(exc2)
        {
            printf("(this is the nested catch) ");
            handleIt(exc2);
        }
        err_end;
    }
    err_end;
    printf("f9 - exiting\n");
}

void f10()
{
    printf("f10 - try, throw, catch, then call f2 (try, throw, catch) "
           "from within catch\n");
    err_try
    {
        tryItOut();
    }
    err_catch_disc
    {
        printf("(catch handler - calling f2)\n");
        f2();
    }
    err_end;
    printf("f10 - exiting\n");
}

void f11()
{
    printf("f11 - try, throw, catch, rethrow\n");
    err_try
    {
        err_try
        {
            tryItOut();
        }
        err_catch(exc2)
        {
            printf("(inner handler) ");
            showIt(exc2);
            printf("- rethrowing the same exception\n");
            err_rethrow();
        }
        err_end;
    }
    err_catch(exc)
    {
        printf("(outer handler) ");
        showIt(exc);
    }
    err_end;
    printf("f11 - exiting\n");
}

int main()
{
    /* initialize for testing */
    test_init();

    /* initialize the error subsystem */
    err_init(1024);
    
    printf("main - starting\n");
    err_try
    {
        printf("main - calling f1\n");

        err_try
        {
            f1();
        }
        err_catch(exc)
        {
            printf("main - caught exception from f1 - ");
            showIt(exc);
        }
        err_end;

        printf("main - back from f1, calling f2\n");
        f2();
        printf("main - back from f2, calling f3\n");
        f3();
        printf("main - back from f3, calling f4\n");
        f4();
        printf("main - back from f4, calling f5\n");
        f5();

        err_try
        {
            printf("main - back from f5, calling f6\n");
            f6();
        }
        err_catch(exc)
        {
            printf("main - caught exception from f6 - ");
            showIt(exc);
        }
        err_end;

        err_try
        {
            printf("main - back from f6, calling f7\n");
            f7();
        }
        err_catch(exc)
        {
            printf("main - caught exception from f7 - ");
            showIt(exc);
        }
        err_end;

        err_try
        {
            printf("main - back from f7, calling f8\n");
            f8();
        }
        err_catch(exc)
        {
            printf("main - caught exception from f8 - ");
            showIt(exc);
        }
        err_end;

        printf("main - back from f8, calling f9\n");
        f9();
        printf("main - back from f9, calling f10\n");
        f10();
        printf("main - back from f10, calling f11\n");
        f11();
        printf("main - back from f11\n");
    }
    err_catch(exc)
    {
        printf("main - caught exception - ");
        showIt(exc);
    }
    err_end;

    printf("main - done; stack depth = %d\n", 0 /*err_stack_depth()*/);
    err_terminate();

    return 0;
}

