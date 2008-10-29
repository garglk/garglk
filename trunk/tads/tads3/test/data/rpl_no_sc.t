/*
 *   syntax error: replace an object with no superclass list 
 */

testObj: object
    propA = 'this is property A'
    propB = 'this is property B'
;

replace testObj
{
    propA = 'new property A'
}
