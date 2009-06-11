#include "tads.h"

// illegally export LastProp
export prop1 'LastProp';

// export two definitions for RuntimeError, in addition to the one in _main
export MyRuntimeError 'RuntimeError';
export AnotherError 'RuntimeError';

class MyRuntimeError: Exception
    prop1 = nil
;

class AnotherError: Exception
    prop1 = nil
;

main(args)
{
    "That's about all!";
}

