#define STR(x) 'hello(' ## #@x ## ')'
#define NOSTR(x) 'hello(' ## #x ## ')'

[STR(asdf)];
[NOSTR(jkl)];

#define H(x) #x ## ".h"

#include H(nofile)
