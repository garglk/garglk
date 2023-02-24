#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "optional.hpp"

#include "garglk.h"

// C++20: std::numbers::pi
// M_PI is POSIX, but C++ can't request a POSIX environment via
// _XOPEN_SOURCE. Compilers on Unix systems seem to provide a POSIX
// environment regardless of "standard" C++ settings (e.g. "g++
// -std=c++14" will still expose POSIX functionality, whereas "gcc
// -std=c11" won't).
//
// But on Windows, if a standard C++ environment is requested, M_PI is
// _not_ defined. It requires _USE_MATH_DEFINES to be defined first.
//
// Rather than trying to coax POSIX out of various platforms, just
// define a suitable pi here. Maybe some day Gargoyle won't require
// POSIX at all.
static constexpr double pi = 3.14159265358979323846;

Bleeps::Bleeps()
{
    update(1, 0.1, 1175);
    update(2, 0.1, 440);
}

// 22050Hz, unsigned 8 bits per sample, mono
void Bleeps::update(int number, double duration, int frequency)
{
    if (number != 1 && number != 2) {
        return;
    }

    number--;

    std::uint32_t samplerate = 22050;
    std::size_t frames = duration * samplerate;

    if (frames == 0) {
        m_bleeps[number] = nonstd::nullopt;
        return;
    }

    // Construct the WAV header
    std::uint16_t pcm = 1;
    std::uint16_t channels = 1;
    std::uint16_t bytes = 1;
    std::uint32_t byterate = samplerate * channels * bytes;
    std::uint16_t blockalign = channels * bytes;
    std::uint16_t bits_per_sample = 8 * bytes;
    std::uint32_t datasize = frames * bytes * channels;
    bool need_padding = datasize % 2 == 1;

    std::vector<std::uint8_t> data;
    data.reserve(datasize + 44 + (need_padding ? 1 : 0));

#define le16(n) \
    static_cast<std::uint8_t>((static_cast<std::uint16_t>(n) >>  0) & 0xff), \
    static_cast<std::uint8_t>((static_cast<std::uint16_t>(n) >>  8) & 0xff)

#define le32(n) \
    static_cast<std::uint8_t>((static_cast<std::uint32_t>(n) >>  0) & 0xff), \
    static_cast<std::uint8_t>((static_cast<std::uint32_t>(n) >>  8) & 0xff), \
    static_cast<std::uint8_t>((static_cast<std::uint32_t>(n) >> 16) & 0xff), \
    static_cast<std::uint8_t>((static_cast<std::uint32_t>(n) >> 24) & 0xff)

    data = {
        'R', 'I', 'F', 'F',
        le32(datasize + 36 + (need_padding ? 1 : 0)),
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        le32(16),
        le16(pcm),
        le16(channels),
        le32(samplerate),
        le32(byterate),
        le16(blockalign),
        le16(bits_per_sample),
        'd', 'a', 't', 'a',
        le32(datasize),
    };

#undef le32
#undef le16

    for (std::size_t i = 0; i < frames; i++) {
        auto point = 1 + std::sin(frequency * 2 * pi * i / static_cast<double>(samplerate));
        data.push_back(127 * point);
    }

    if (need_padding) {
        data.push_back(0);
    }

    m_bleeps[number] = std::move(data);
}

void Bleeps::update(int number, const std::string &path)
{
    if (number != 1 && number != 2) {
        return;
    }

    number--;

    std::ifstream f(path, std::ios::binary);

    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());

    if (!f.fail()) {
        m_bleeps[number] = std::move(data);
    }
}

std::vector<std::uint8_t> &Bleeps::at(int number)
{
    try {
        return m_bleeps.at(number - 1).value();
    } catch (const nonstd::bad_optional_access &) {
        throw Empty();
    }
}

Bleeps gli_bleeps;
