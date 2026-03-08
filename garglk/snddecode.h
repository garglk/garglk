// Copyright (C) 2026 by Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef GARGLK_SNDDECODE_H
#define GARGLK_SNDDECODE_H

// Backend-agnostic audio decoding. Each Decoder turns an in-memory sound
// resource into interleaved 32-bit float PCM at the resource's native sample
// rate; resampling to the output device is the sink's job (QAudioSink or
// SDL_AudioStream both resample), so decoders never resample. This lets the Qt
// and SDL sinks share one decoder layer.

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "glk.h"

namespace garglk {

class SoundError : public std::runtime_error {
public:
    explicit SoundError(const std::string &msg) : std::runtime_error(msg) {
    }
};

// A minimal stand-in for C++23's std::expected: holds either a T or an error
// string. Used where failure is routine rather than exceptional (e.g. a missing
// sound resource), so callers branch on has_value() instead of catching. The
// error string is carried for diagnostics even though nothing logs it yet.
template <typename T>
class Expected {
public:
    Expected(T value) : m_value(std::move(value)) {
    }

    Expected(std::string error) : m_error(std::move(error)) {
    }

    [[nodiscard]] bool has_value() const {
        return m_value.has_value();
    }

    T &operator*() {
        return *m_value;
    }

    [[nodiscard]] const std::string &error() const {
        return m_error;
    }

private:
    std::optional<T> m_value;
    std::string m_error;
};

class Decoder {
public:
    explicit Decoder(glui32 plays) : m_plays(plays) {
    }

    virtual ~Decoder() = default;

    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;

    // Decoders emit 32-bit float PCM at this native rate/channel count.
    [[nodiscard]] int samplerate() const {
        return m_samplerate;
    }

    [[nodiscard]] int channels() const {
        return m_channels;
    }

    // True once every repeat has been played out.
    [[nodiscard]] bool finished() const {
        return m_plays == 0;
    }

    // Fill up to max bytes of interleaved float PCM, applying repeat handling.
    // Returns the number of bytes produced; 0 only at the true end of all
    // repeats.
    std::size_t read(void *data, std::size_t max);

protected:
    // Decode up to max bytes of float PCM; return bytes produced, 0 at the end
    // of the stream (before any repeat).
    virtual std::size_t source_read(void *data, std::size_t max) = 0;
    virtual void source_rewind() = 0;

    void set_format(int samplerate, int channels) {
        m_samplerate = samplerate;
        m_channels = channels;
    }

private:
    glui32 m_plays;
    int m_samplerate = 0;
    int m_channels = 0;
};

// Sniff the blorb resource type (giblorb_ID_*) of an in-memory sound, or 0 if
// it isn't recognized. Tracker/MOD formats are detected with libopenmpt's
// prober; everything else by a magic string.
int detect_format(const std::vector<unsigned char> &buf);

// Load a sound resource (blorb resource, resource map, or SND<n> file) into
// memory, returning its type (giblorb_ID_*, or 0 if loaded but unrecognized)
// and the bytes, or an error string if it can't be loaded.
Expected<std::pair<int, std::vector<unsigned char>>> load_sound_resource(glui32 snd);

// Load built-in bleep 1 or 2, or an error string for any other number;
// propagates Bleeps::Empty if the requested bleep isn't configured.
Expected<std::pair<int, std::vector<unsigned char>>> load_bleep_resource(glui32 snd);

// Build a decoder for an already-detected blorb resource type (giblorb_ID_*),
// taking ownership of the resource bytes. Throws SoundError on failure or for
// an unsupported type.
std::shared_ptr<Decoder> create_decoder(int type, std::vector<unsigned char> data, glui32 plays);

// One-time decoder library initialization (e.g. mpg123 on old API versions).
void init_decoders();

}

#endif
