[unicode.t]

       0 .code
         .argcount 1
         .locals 2
         .maxstack 4
       a  say            string(0x0)
       f  pushint8       0x2a
      11  builtin_b      0x1, 0x14
      14  getr0          
      15  sayval         
      16  say            string(0x0)
      1b  say            string(0x0)
      20  pushlst        list(0x0)
      25  builtin_b      0x1, 0x14
      28  getr0          
      29  sayval         
      2a  say            string(0x0)
      2f  say            string(0x0)
      34  pushstr        string(0x0)
      39  builtin_b      0x1, 0x14
      3c  getr0          
      3d  sayval         
      3e  say            string(0x0)
      43  say            string(0x0)
      48  pushint8       0xa
      4a  pushint8       0x2a
      4c  builtin_b      0x2, 0x14
      4f  getr0          
      50  sayval         
      51  say            string(0x0)
      56  say            string(0x0)
      5b  pushint8       0xa
      5d  pushlst        list(0x0)
      62  builtin_b      0x2, 0x14
      65  getr0          
      66  sayval         
      67  say            string(0x0)
      6c  say            string(0x0)
      71  pushint8       0x5
      73  pushstr        string(0x0)
      78  builtin_b      0x2, 0x14
      7b  getr0          
      7c  sayval         
      7d  say            string(0x0)
      82  pushlst        list(0x0)
      87  setlcl1        0x0
      89  pushlst        list(0x0)
      8e  addtolcl       0x0
      91  say            string(0x0)
      96  pushint8       0x14
      98  getlcl1        0x0
      9a  builtin_b      0x2, 0x14
      9d  getr0          
      9e  sayval         
      9f  say            string(0x0)
      a4  pushstr        string(0x0)
      a9  setlcl1        0x0
      ab  pushstr        string(0x0)
      b0  addtolcl       0x0
      b3  say            string(0x0)
      b8  pushint8       0x8
      ba  getlcl1        0x0
      bc  builtin_b      0x2, 0x14
      bf  getr0          
      c0  sayval         
      c1  say            string(0x0)
      c6  say            string(0x0)
      cb  pushint8       0x5
      cd  pushlst        list(0x0)
      d2  builtin_b      0x2, 0x14
      d5  getr0          
      d6  sayval         
      d7  say            string(0x0)
      dc  say            string(0x0)
      e1  onelcl1        0x1
      e3  pushstr        string(0x0)
      e8  setlcl1        0x0
      ea  getlcl1        0x1
      ec  getproplcl1    0x0, property(0x26)
      f0  getr0          
      f1  jgt            +0x002b (0x0000011d)
      f4  say            string(0x0)
      f9  getlcl1        0x0
      fb  sayval         
      fc  say            string(0x0)
     101  getlcl1        0x1
     103  sayval         
     104  say            string(0x0)
     109  getlcl1        0x1
     10b  callproplcl1   0x1, 0x0, property(0x67)
     110  getr0          
     111  sayval         
     112  say            string(0x0)
     117  inclcl         0x1
     11a  jmp            -0x0031 (0x000000ea)
     11d  say            string(0x0)
     122  getlcl1        0x0
     124  sayval         
     125  say            string(0x0)
     12a  getproplcl1    0x0, property(0x67)
     12e  getr0          
     12f  call           0x1, code(0x00000000)
     135  getr0          
     136  sayval         
     137  say            string(0x0)
     13c  pushstr        string(0x0)
     141  setlcl1        0x0
     143  say            string(0x0)
     148  getlcl1        0x0
     14a  sayval         
     14b  say            string(0x0)
     150  getproplcl1    0x0, property(0x67)
     154  getr0          
     155  call           0x1, code(0x00000000)
     15b  getr0          
     15c  sayval         
     15d  say            string(0x0)
     162  retnil         

     163 .code
         .argcount 1
         .locals 2
         .maxstack 4
     16d  getarg1        0x0
     16f  jnotnil        +0x000a (0x0000017a)
     172  say            string(0x0)
     177  retnil         
     178  nop            
     179  nop            
     17a  say            string(0x0)
     17f  onelcl1        0x0
     181  getarg1        0x0
     183  getprop        property(0x26)
     186  setlcl1r0      0x1
     188  getlcl1        0x0
     18a  getlcl1        0x1
     18c  jgt            +0x001c (0x000001a9)
     18f  getarg1        0x0
     191  getlcl1        0x0
     193  index          
     194  builtin_c      0x1, 0x0
     197  getlcl1        0x0
     199  getlcl1        0x1
     19b  je             +0x0007 (0x000001a3)
     19e  say            string(0x0)
     1a3  inclcl         0x0
     1a6  jmp            -0x001f (0x00000188)
     1a9  say            string(0x0)
     1ae  retnil         

     1af .code
         .argcount 0
         .locals 0
         .maxstack 0
     1b9  retnil         


