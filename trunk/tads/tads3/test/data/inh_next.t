#include <tads.h>

/*
 *   Define a class with a property that inherits from its superclass 
 */
#define DEF_CLASS_PROP1(classname, superclasses...) \
    class classname: ##superclasses \
    prop1() \
    { \
        #classname ## ".prop1\n"; \
        inherited(); \
    }

/* simple linear inheritance pattern */
DEF_CLASS_PROP1(A, object);
DEF_CLASS_PROP1(B, A);
DEF_CLASS_PROP1(C, B);

/*
 *   diamond-shaped inheritance pattern 
 */
DEF_CLASS_PROP1(A1, object);
DEF_CLASS_PROP1(B1, A1);
DEF_CLASS_PROP1(C1, A1);
DEF_CLASS_PROP1(D1, B1, C1);

/*
 *   diamond-shaped inheritance, with only the middle level defining prop1 
 */
class A2: object;
DEF_CLASS_PROP1(B2, A2);
DEF_CLASS_PROP1(C2, A2);
class D2: B2, C2;

/*
 *   diamond with double-deep left leg 
 */
DEF_CLASS_PROP1(A3, object);
DEF_CLASS_PROP1(B3, A3);
DEF_CLASS_PROP1(C3, B3);
DEF_CLASS_PROP1(D3, A3);
DEF_CLASS_PROP1(E3, C3, D3);

/*
 *   complex pattern 
 */
class G4: object;
DEF_CLASS_PROP1(F4, G4);
DEF_CLASS_PROP1(E4, G4);
DEF_CLASS_PROP1(C4, E4);
DEF_CLASS_PROP1(B4, C4);
DEF_CLASS_PROP1(D4, E4);
DEF_CLASS_PROP1(A4, B4, D4, F4);

main(args)
{
    /* call the various subclass methods */
    "C.prop1:\n"; C.prop1();
    "\bD1.prop1:\n"; D1.prop1();
    "\bD2.prop1:\n"; D2.prop1();
    "\bE3.prop1:\n"; E3.prop1();
    "\bA4.prop1:\n"; A4.prop1();
}

