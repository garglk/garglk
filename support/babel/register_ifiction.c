/* register_ifiction.c    Register modules for babel's ifiction API
 *
 * 2006 by L. Ross Raszewski
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
 * This file depends on modules.h
 *
 * This version of register.c is stripped down to include only the
 * needed functionality for the ifiction api
 */

#include <stdlib.h>
#include "treaty.h"

char *format_registry[] = {
        #define TREATY_REGISTER
        #define CONTAINER_REGISTER
        #define FORMAT_REGISTER
        #include "modules.h"
        NULL
};
