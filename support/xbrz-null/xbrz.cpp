#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "xbrz.h"

void xbrz::scale(std::size_t factor, const std::uint32_t *src, std::uint32_t *trg, int srcWidth, int srcHeight, ColorFormat colFmt)
{
    std::cerr << "warning: xBRZ suport requested, but disabled due to licensing issues\n";
    std::copy(src, src + srcWidth * srcHeight, trg);
}
