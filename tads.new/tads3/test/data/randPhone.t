#include <tads.h>

randomPhoneNumber()
{
#if 1

  return '555' 
    + makeString([1,1,1,1].mapAll({x: rand(10) + 48}));

#else

   local n;
   for (n = '555' ; n.length() < 7 ; )
      n += toString(rand(10));
   return n;

#endif
}

main(args)
{
    for (local i = 0 ; i < 4 ; ++i)
        "<<i>> = <<randomPhoneNumber()>>\n";
}
