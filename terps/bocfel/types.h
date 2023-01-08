// vim: set ft=cpp:

#ifndef ZTERP_TYPES_H
#define ZTERP_TYPES_H

// Unless deprecated C headers are included, various types (size_t,
// uint8_t, etc) are only declared in the std namespace. Such headers
// are not included in Bocfel because they pollute the global namespace,
// but these types are used all over the place so bring them into the
// global namespace here.

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
