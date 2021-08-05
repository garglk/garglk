/*
 *   Parser test - constant expressions.  This tests basic parsing, order
 *   of precedence, and constant folding.  
 */

"test << f1(1)
 >>";


'simple values';
'sstring with\ttab!';
"dstring";
1;            //  1

'unary operators';
-2;           // -2
+5;           // 5
~0x8f7f7f7f;  // 0x70808080 = 1887469696

'magnitude comparisons';
1>2;          // nil
1<2;          // true
1 >= 2;       // nil
2 >= 2;       // true
3 >= 2;       // true
1 <= 2;       // true
2 <= 2;       // true
3 <= 2;       // nil
'hello' > 'goodbye';  // true
'hello' < 'goodbye';  // nil

'equality comparisons';
1 == 2;       // nil
2 == 2;       // true
1 != 2;       // true
3 != 3;       // nil

'addition and subtraction';
5+6;          // 11
7 - 11;       // -4

'multiplication and division';
3*4;          // 12
4 * 5;        // 20
16/3;         // 5
17 % 7;       // 3

'bitwise operators';
0x101 | 0x21; // 0x121 = 289
0x70F & 0x137; // 0x107 -> 263
0x101 ^ 0x171; // 0x70 -> 112

'bit shifts';
1 << 3;       // 8
128 >> 2;     // 32

'logical AND';
1 && 2;       // true
nil && 5;     // nil
1 && nil;     // nil
nil && 0;     // nil
0 && 0;       // nil
0 && 1;       // nil
true && a;    // non-constant
nil && a;     // nil

'logical OR';
true || nil;  // true
1 || nil;     // true
1 || 2;       // true
nil || 5;     // true
1 || nil;     // true
nil || 0;     // nil
0 || 0;       // nil
0 || 1;       // true
true || a;    // true
nil || a;     // non-constant

'conditional';
1 ? 2 : 3;    // 2
0 ? 5 : 6;    // 6
0 ? 111 : 2 ? 222 : 333;  // 222
1 ? 444 : 2 ? 555 : 666;  // 444
true ? 1 + 2 + 3 : a + b + c; // 6
nil ? a + b + c : 4 + 5 + 6; // 15
true ? a + b + c : 1 + 2 + 3; // non-constant
nil ? 1 + 2 + 3 : a + b + c; // non-constant


'complex expressions';
5+6*7;        // 47
1+2 > 3+4 ? 'problem' : 'good';
1+2 < 3+4 ? 'Also Good' : 'Another Problem';
2*(3+4*5);    // 46

'string concatenation';
'a' + 'b';
'abc' + 'def' + 'ghi';
'abc' + 'defghi' + 'jklmnopqrstuvwx' + 'yz';
'one' + 1;
2 + 'two';
3 + 4 + 'five';
6 + (7 + 'eight');
'six' + 7 + 8;
'eight' + (9 + 10);
nil + '=NIL';
'TRUE=' + true;

'list values';
[];
[1, 2, 3];
[1+2, 3+4, 5+6*7];
['abc', 456];
['abc', 567, 'def', [1, 2, 3, 4, 5]];

'list concatenation';
[] + 1 + 2 + 3;
[x] + 1 + 2 + 3;
[] + 1 + 2 + x;
[1, 2, 3] + [4, 5, 6];
[1, 2, 3] + [[4, 5, 6]];

'list exclusion';
[1, 2, 3, 4] - 3; // [1, 2, 4]
[1, 2, 3, 4] - [2, 3];  // [1, 4]
[1, 2, 3, 4] - 1; // [2, 3, 4]
[1, 2, 3, 4] - 4; // [1, 2, 3]
[1, 2, 3, 4] - [1, 2, 3, 4, 5];  // []
[1, 2, 3, 4] - [5, 6, 7]; // [1, 2, 3, 4]

'list indexing';
[5, 6, 7][1+1]; // 6
['hello', 'world', 'from', 't3'][5-2]; // 'from'

'list comparisons';
[1, 2, 3] == [1, 2]; // nil
[1, 2, 3] == [3, 2, 1]; // nil
[1, 2, 3] == [1, 2, 4]; // nil
[1, 2, 3] == [1, 2, 3]; // true
[1, 2, 3] == [1, 2, '3']; // nil
[1, [2, 3], 4] == [1, 2, 3, 4]; //nil
[1, [2, 3], 4] == [1, [1+1, 1+1+1], [5, 4, 3][2]]; // true
['hello', 'a' + 'b'] == ['hello', 'ab']; // true

'address comparisons';
&a == &b;  // nil
&a == &aa; // nil
&a == &a;  // true
&a != &b;  // true
&a != &aa; // true
&a != &a;  // nil
&a == &(((a)));  // true
&a == &(((b)));  // nil

'lists with constant symbolic entries';
[obj1, obj2];
[&f1, &f2];
[&f1, obj1, [&f2, obj2]];
[obj1] + [&f1];
[obj1, obj2, &f1, &f2] - [obj2, &f2];
[obj1, obj2, &f1, &f2][2];

'comparisons of constant symbolic entries';
obj1 == obj2;  // nil
obj1 == obj1;  // true
obj1 != obj2;  // true
obj1 != obj1;  // nil
&f1 == &f2;    // nil
&f1 == &f1;    // true
&f1 != &f2;    // true
&f1 != &f1;    // nil

'&& and || expressions with constant symbolic entries';
obj1 == obj2 && &f1 == &f2; // nil
obj1 == obj1 && &f1 != &f2; // true
obj1 == obj2 || &f1 == &f2; // nil
obj1 == obj2 || &f1 == &f1; // true
!(obj1 != obj2);            // nil
!(obj1 == obj2);            // true

'assign to list element';
lcl1[2] = 5;

'function calls';
f1(obj1, 'hello', 3);
lcl1 = f2(lcl1, lcl2 + lcl1);

'property evaluation with pointer';
obj1.lcl1;
obj1.lcl1 = 5;

'embedded string expressions';
"Local 1 is <<lcl1>>, and f1(obj1) is <<
f1(obj1) >>!";

// define some symbols
local lcl1;
local lcl2;
object obj1;
object obj2;
function f1;
function f2;

