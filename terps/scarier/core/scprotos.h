/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Umbrella over the module prototype headers, in dependency order, for the
 * translation units that use most of the engine (library/, runner/, the
 * front ends).  A core/ module includes only the headers it uses.
 */

#ifndef SCARIER_PROTOTYPES_H
#define SCARIER_PROTOTYPES_H

#include "sctypes.h"
#include "scutils.h"
#include "sctaffil.h"
#include "scprops.h"
#include "sctafpar.h"
#include "scvars.h"
#include "scexpr.h"
#include "scprintf.h"
#include "scmemos.h"
#include "scgamest_api.h"
#include "scparser.h"
#include "sclibrar_api.h"
#include "scresour.h"
#include "scrunner_api.h"
#include "scevents.h"
#include "sctasks.h"
#include "screstrs.h"
#include "scnpcs.h"
#include "scbattle.h"
#include "scobjcts.h"
#include "scserial.h"
#include "sclocale.h"
#include "scdebug.h"
#include "scinterf.h"
#include "scdump.h"

#endif
