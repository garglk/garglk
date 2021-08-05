/*
 *   Test of custom event list example from System Manual - String Literals
 *   (strlit.htm). 
 */

atmosphereList: object
   eventList = [
      'To err is human; to really screw things up you need
      <<one of>>a computer<<or>>a committee of experts<<or>>an
      Act of Parliament<<or>>Vulcan logic<<or>>a roomful of
      lawyers<<cycling>>. ' ,

      'This message has displayed <<if ++count == 1>>once<<
      else if count == 2>>twice<<else>><<count>> times<<end>>. '
   ]
        
   count = 0

   curIndex = 1
   getNext()
   {
       if (curIndex > eventList.length())
           curIndex = 1;

       return eventList[curIndex++];
   }
;

main(args)
{
    for (local i in 1..10)
        "<<atmosphereList.getNext()>>\b";
}
