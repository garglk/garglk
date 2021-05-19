/* register.c    Register modules for the babel handler api
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
 * The purpose of this file is to create the treaty_registry array.
 * This array is a null-terminated list of the known treaty modules.
 */

#include <stdlib.h>
#include "modules.h"


TREATY treaty_registry[] = {
        #define TREATY_REGISTER
        #include "modules.h"
        NULL
        };

TREATY container_registry[] = {
        #define CONTAINER_REGISTER
        #include "modules.h"
        NULL

};

