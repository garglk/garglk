#ifndef XBRZNULL_H
#define XBRZNULL_H

#include <cstddef>
#include <cstdint>

namespace xbrz
{

enum class ColorFormat
{
    ARGB,
};

const int SCALE_FACTOR_MAX = 1;
void scale(std::size_t factor, const std::uint32_t *src, std::uint32_t *trg, int srcWidth, int srcHeight, ColorFormat colFmt);

}

#endif
