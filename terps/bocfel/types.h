// vim: set ft=cpp:

// Because implementations are allowed to define things in the global
// namespace even when C++ headers (e.g. cstdint vs stdint.h) are
// included, include-what-you-use gets confused and thinks this header
// is superfluous. It _is_ superfluous on implementations which pollute
// the global namespace, but since that’s not guaranteed, this header is
// necessary for portability. Instruct IWYU to not suggest removing it.
//
// IWYU pragma: always_keep

#ifndef ZTERP_TYPES_H
#define ZTERP_TYPES_H

// Unless deprecated C headers are included, various types (size_t,
// uint8_t, etc) are only guaranteed to be declared in the std
// namespace, not necessary globally. Such headers are not included in
// Bocfel because they pollute the global namespace, but these types are
// used all over the place so bring them into the global namespace here.

#include <cstddef>
#include <cstdint>

using std::size_t;
using std::int8_t;
using std::int16_t;
using std::int32_t;
using std::uint8_t;
using std::uint16_t;
using std::uint32_t;

#endif
