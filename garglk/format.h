#ifndef GARGLK_FORMAT_H
#define GARGLK_FORMAT_H

#define FMT_ENFORCE_COMPILE_STRING
#ifdef GARGLK_CONFIG_BUNDLED_FMT
#define FMT_HEADER_ONLY
#include "format/format.h"
#else
#include <fmt/format.h>
#endif

// This was added in 5.0.0.
#ifndef FMT_STRING
#define FMT_STRING(s) (s)
#endif

#define Format(s, ...) fmt::format(FMT_STRING(s), __VA_ARGS__)

#endif
