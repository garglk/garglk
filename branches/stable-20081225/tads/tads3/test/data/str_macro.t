#define _STR(x) #x
#define STR(x) _STR(x)

#define FOO() bar
#define NEWLINE \n
#define QSPACE  \  
#define QSPACE2 \  //

STR(FOO());
STR(NEWLINE);
STR(QSPACE);
STR(QSPACE2);
