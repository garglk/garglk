/*
 *   Test of multiple inheritance and templates - based on a bug report
 *   from Eric Eve.  
 */

class TestA : object
  weight = 0
  colour = nil
  mydesc = nil
;
 
class TestB : object
  bulk = 0
  texture = nil
;
 
class TestC :  TestB, TestA
  shape = nil
;
 
TestB template +bulk 'texture';
TestA template +weight 'colour' 'mydesc'?;
 
TestC template inherited 'shape';
 
testMe : TestC +20 'rough' ;
 
testMeAgain : TestC +30 'red' 'wooden';
 
testMeShape : TestC +10 'blue' 'large' 'square';

main(args)
{
    "Hello, world!\n";
}
