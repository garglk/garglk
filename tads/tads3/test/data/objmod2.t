/*
 *   Object modification test - modify an external object 
 */

modify obj2
    p1 = "This is modified obj2.p1 - inheriting: << inherited.p1 >>"
    p2 = "This is modified obj2.p2 - inheriting: << inherited.p2 >>"
    p3 = "This is modified obj2.p3 - inheriting: << inherited.p3 >>"
;

modify obj2
    replace p2 = "This is replaced+modified obj2.p2 - inheriting:
                  << inherited.p2 >>"
    p1 = "This is doubly-modified obj2.p1 - inheriting: <<inherited.p1>>"
;


modify obj3
    p1 = "objmod2-modified obj3.p1 - inheriting: <<inherited.p1>>"
    replace p2 = "objmod2-replaced obj3.p2 - inheriting: <<inherited.p2>>"
    replace p3 = "objmod2-replaced obj3.p3 - inheriting: <<inherited.p3>>"
;

