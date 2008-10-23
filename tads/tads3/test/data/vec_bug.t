// from Sean T Barrett - vector trash bug test case

#include "tads.h"
#include "t3.h"
#include "vector.h"

#define TEST_STRINGS     // if you disable this, builds a vector of ints,
                         // which doesn't memory trash

#ifdef TEST_STRINGS
  #define VALUE(x)  #@x
#else
  #define VALUE(x)  x
#endif

main(args)
{
   local x = new Vector(1);  // if you make this 3, it doesn't trash
                             // (the vector will eventually have 3 elements)

   x.append(VALUE(1));       // append 1 or '1'
   x.append(VALUE(2));       // append 2 or '2'

   x[2] = x[2];              // this assignment doesn't trash x[1] or x[2]

   "<<x[1]>> <<x[2]>>\n";

   x.append(VALUE(3));       // append 3 or '3'

   "<<x[1]>> <<x[2]>> <<x[3]>>\n";

   x[3] = 3;                 // ERROR! this assignment trashes x[2] NOT x[3]
   // x[3] = x[3];           // this would trash too

   "<<x[1]>> <<x[2]>>\n";    // this line will fault on x[2]

   // convert it to a list and display it
   local y = x.toList();
   "y = [<<y[1]>>, <<y[2]>>, <<y[3]>>]\n";
}
