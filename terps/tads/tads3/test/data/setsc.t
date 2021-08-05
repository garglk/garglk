#include <tads.h>

class A: object
    name = 'class A'
    prop1 { "A.prop1... "; inherited(); }
;

class B: object
    name = 'class B'
    prop1 { "B.prop1... "; inherited(); }
;

class C: object
    name = 'class C'
    prop1 { "C.prop1... "; inherited(); }
;

class D: A, B, C
    name = 'class D'
    prop1 { "D.prop1... "; inherited(); }
;

objA: A;
objB: B;
objC: C;
objD: D;

main(args)
{
    local obj;

    test('objA', objA);
    test('objB', objB);
    test('objC', objC);
    test('objD', objD);

    obj = new D();
    test('new D', obj);

    objD.setSuperclassList([TadsObject]);
    test('objD modified to [TadsObject]', objD);

    objD.setSuperclassList([A, C]);
    test('objD modified to [A, C]', objD);

    obj.setSuperclassList([B, C]);
    test('new D modified to [B, C]', obj);

    obj.setSuperclassList([A, B, C]);
    test('new D restored to [A, B, C]', obj);

    C.setSuperclassList([B]);
    test('new D after changing C to [B]', obj);
}

test(id, obj)
{
    "<<id>>:
    \n\t<<obj.name>>
    \n\t<<obj.prop1>>
    \b";
}
