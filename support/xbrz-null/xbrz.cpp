#include <cstddef>
#include <cstdint>
#include <iostream>

#include "xbrz.h"

bool xbrz::scale(std::size_t factor, const std::uint32_t *src, std::uint32_t *trg, int srcWidth, int srcHeight, ColorFormat colFmt)
{
    std::cerr << "warning: xBRZ suport requested, but the current interpreter's licens is incompatible with xBRZ\n";
    return false;
}
