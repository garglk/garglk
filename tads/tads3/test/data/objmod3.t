/*
 *   object modification test - module three 
 */

modify obj3
    p1 = "objmod3-modified obj3.p1 - inheriting: << inherited.p1 >>"
    replace p2 = "objmod3-replaced obj3.p2 - inheriting: << inherited.p2 >>"
    p3 = "objmod3-modified obj3.p3 - inheriting: << inherited.p3 >>"
;

modify obj3
    p1 = "objmod3-doubly-modified obj3.p1 - inheriting: <<inherited.p1>>"
    p2 = "objmod3-doubly-modified obj3.p2 - inheriting: <<inherited.p2>>"
    p3 = "objmod3-doubly-modified obj3.p3 - inheriting: <<inherited.p3>>"
;

