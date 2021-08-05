/*
 *   test of 'if (objName && expr)' - a bug in the compiler caused this to
 *   generate incorrect code 
 */
main(args)
{
    if (myObject && myObject.location)
        "then crash";
}

myObject: object
;

otherObject: object
    location = nil
;

