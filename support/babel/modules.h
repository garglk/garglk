/* modules.h  Declaration of treaty modules for the babel program
 * (c) 2006 By L. Ross Raszewski
 *
 * This code is freely usable for all purposes.
 *
 * This work is licensed under the Creative Commons Attribution 4.0 License.
 * To view a copy of this license, visit
 * https://creativecommons.org/licenses/by/4.0/ or send a letter to
 * Creative Commons,
 * PO Box 1866,
 * Mountain View, CA 94042, USA.
 *
 * This file depends upon treaty.h and all the references treaty modules
 *
 * Persons wishing to add support for a new module to babel need only
 * add a line in the form below.  New modules should be positioned according
 * to their popularity.  If this file is being used in tandem with register.c
 * (as it is in babel), then being dishonest about the popularity of an added
 * system will make the program non-compliant with the treaty of Babel
 *
 * REGISTER_NAME is used as a placeholder for formats which are specified
 * as existing by the treaty but for which no handler yet exists.
 * remove the REGISTER_NAME for any format which has a registered treaty.
 */


#include "treaty.h"
#undef REGISTER_TREATY
#undef REGISTER_CONTAINER
#undef REGISTER_NAME
#ifdef TREATY_REGISTER
#ifdef CONTAINER_REGISTER
#ifdef FORMAT_REGISTER
#define REGISTER_TREATY(x)        #x,
#define REGISTER_NAME(x)          #x,
#define REGISTER_CONTAINER(x)
#else
#define REGISTER_TREATY(x)
#define REGISTER_CONTAINER(x)     x##_treaty,
#define REGISTER_NAME(x)
#endif
#else
#define REGISTER_TREATY(x)        x##_treaty,
#define REGISTER_CONTAINER(x)
#define REGISTER_NAME(x)
#endif
#else
#define REGISTER_TREATY(x)        int32 x##_treaty(int32, void *, int32, void *, int32);
#define REGISTER_CONTAINER(x)        int32 x##_treaty(int32, void *, int32, void *, int32);
#define REGISTER_NAME(x)
#endif


REGISTER_CONTAINER(blorb)
REGISTER_TREATY(zcode)
REGISTER_TREATY(glulx)
REGISTER_TREATY(tads2)
REGISTER_TREATY(tads3)
REGISTER_TREATY(hugo)
REGISTER_TREATY(alan)
REGISTER_TREATY(adrift)
REGISTER_TREATY(level9)
REGISTER_TREATY(agt)
REGISTER_TREATY(magscrolls)
REGISTER_TREATY(advsys)
REGISTER_TREATY(twine)
REGISTER_TREATY(executable)




