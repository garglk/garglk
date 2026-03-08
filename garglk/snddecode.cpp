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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sys/types.h>
#include <type_traits>
#include <utility>
#include <vector>

#include <libopenmpt/libopenmpt.hpp>
#include <mpg123.h>
#include <sndfile.hh>

#ifdef GARGLK_HAS_FLUIDSYNTH
#include <fluidsynth.h>
#endif

#include "format.h"

#include "glk.h"
#include "garglk.h"
#include "gi_blorb.h"

#include "snddecode.h"

#define giblorb_ID_MOD  (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_OGG  (giblorb_make_id('O', 'G', 'G', 'V'))
#define giblorb_ID_FORM (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_AIFF (giblorb_make_id('A', 'I', 'F', 'F'))
#define giblorb_ID_MP3  (giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE (giblorb_make_id('W', 'A', 'V', 'E'))
#define giblorb_ID_MIDI (giblorb_make_id('M', 'I', 'D', 'I'))

#ifdef _MSC_VER
using ssize_t = std::make_signed_t<size_t>;
#endif

#if MPG123_API_VERSION < 46
static bool mp3_initialized;
#endif

using namespace std::literals;

namespace garglk {

std::size_t Decoder::read(void *data, std::size_t max)
{
    std::size_t n = 0;

    if (m_plays > 0) {
        n = source_read(data, max);
        if (n == 0 && (m_plays == 0xffffffff || --m_plays > 0)) {
            source_rewind();
            n = source_read(data, max);
        }
    }

    return n;
}

}

namespace {

using garglk::Decoder;
using garglk::SoundError;

class VFS {
public:
    explicit VFS(std::vector<unsigned char> buf) : m_buf(std::move(buf)) {
    }

    std::size_t size() {
        return m_buf.size();
    }

    [[nodiscard]] off_t tell() const {
        return m_offset;
    }

    off_t seek(off_t offset, int whence) {
        off_t new_offset = m_offset;

        switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset += offset;
            break;
        case SEEK_END:
            new_offset = m_buf.size() + offset;
            break;
        default:
            return -1;
        }

        if (new_offset < 0) {
            return -1;
        }

        return m_offset = new_offset;
    }

    void rewind() {
        m_offset = 0;
    }

    std::size_t read(void *ptr, off_t count) {
        if (m_offset >= static_cast<off_t>(m_buf.size())) {
            return 0;
        }

        if (m_offset + count > static_cast<off_t>(m_buf.size())) {
            count = m_buf.size() - m_offset;
        }

        std::memcpy(ptr, &m_buf[m_offset], count);
        m_offset += count;

        return count;
    }

private:
    const std::vector<unsigned char> m_buf;
    off_t m_offset = 0;
};

class OpenMPTSource : public Decoder {
public:
    OpenMPTSource(const std::vector<unsigned char> &buf, glui32 plays)
        try :
        Decoder(plays),
        m_mod(buf)
    {
        set_format(48000, 2);
    } catch (const openmpt::exception &) {
        throw SoundError("can't parse MOD file");
    }

protected:
    std::size_t source_read(void *data, std::size_t max) override {
        return 8 * m_mod.read_interleaved_stereo(samplerate(), max / 8, reinterpret_cast<float *>(data));
    }

    void source_rewind() override {
        m_mod.set_position_seconds(0);
    }

private:
    openmpt::module m_mod;
};

class SndfileSource : public Decoder {
public:
    SndfileSource(std::vector<unsigned char> buf, glui32 plays) :
        Decoder(plays),
        m_vfs(std::move(buf))
    {
        SF_VIRTUAL_IO io = get_io();
        m_soundfile = SndfileHandle(io, this);
        if (!m_soundfile) {
            throw SoundError("can't open with libsndfile");
        }

        set_format(m_soundfile.samplerate(), m_soundfile.channels());
    }

    std::size_t source_read(void *data, std::size_t max) override {
        // Read whole frames so the byte count is always frame-aligned: an SDL
        // audio stream rejects a partial sample frame, and reading per-sample
        // could return an odd number of samples for a multi-channel source.
        int frame_bytes = channels() * 4;
        return m_soundfile.readf(reinterpret_cast<float *>(data), max / frame_bytes) * frame_bytes;
    }

    void source_rewind() override {
        m_vfs.rewind();
        SF_VIRTUAL_IO io = get_io();
        m_soundfile = SndfileHandle(io, this);
    }

private:
    SndfileHandle m_soundfile;
    VFS m_vfs;

    static SF_VIRTUAL_IO get_io() {
        SF_VIRTUAL_IO io;

        io.get_filelen = vio_get_filelen;
        io.seek = vio_seek;
        io.read = vio_read;
        io.write = vio_write;
        io.tell = vio_tell;

        return io;
    }

    static VFS &vfs(void *source) {
        return reinterpret_cast<SndfileSource *>(source)->m_vfs;
    }

    static sf_count_t vio_get_filelen(void *source) {
        return vfs(source).size();
    }

    static sf_count_t vio_seek(sf_count_t offset, int whence, void *source) {
        return vfs(source).seek(offset, whence);
    }

    static sf_count_t vio_read(void *ptr, sf_count_t count, void *source) {
        return vfs(source).read(ptr, count);
    }

    static sf_count_t vio_write(const void *, sf_count_t, void *) {
        throw SoundError("writing not supported");
    }

    static sf_count_t vio_tell(void *source) {
        return vfs(source).tell();
    }
};

class Mpg123Source : public Decoder {
public:
    Mpg123Source(std::vector<unsigned char> buf, glui32 plays) :
        Decoder(plays),
#if MPG123_API_VERSION < 46
        m_handle(nullptr, mpg123_delete),
#else
        m_handle(mpg123_new(nullptr, nullptr), mpg123_delete),
#endif
        m_vfs(std::move(buf))
    {
#if MPG123_API_VERSION < 46
        if (!mp3_initialized) {
            throw SoundError("mpg123 not initialized");
        }

        m_handle.reset(mpg123_new(nullptr, nullptr));
#endif
        if (!m_handle) {
            throw SoundError("can't create mp3 handle");
        }

        mpg123_replace_reader_handle(m_handle.get(), vio_read, vio_lseek, nullptr);
        if (mpg123_open_handle(m_handle.get(), this) != MPG123_OK) {
            throw SoundError("can't open mp3");
        }

        mpg123_format_none(m_handle.get());

#if MPG123_API_VERSION < 46
        if (mpg123_format(m_handle.get(), 44100, MPG123_STEREO | MPG123_MONO, MPG123_ENC_FLOAT_32) != MPG123_OK) {
#else
        if (mpg123_format2(m_handle.get(), 0, MPG123_STEREO | MPG123_MONO, MPG123_ENC_FLOAT_32) != MPG123_OK) {
#endif
            throw SoundError("can't set mp3 format");
        }

        int encoding;
        if (mpg123_getformat(m_handle.get(), &m_rate, &m_channels, &encoding) != MPG123_OK) {
            throw SoundError("can't get mp3 format information");
        }

        if (encoding != MPG123_ENC_FLOAT_32) {
            throw SoundError("can't request floating point samples for mp3");
        }

        set_format(m_rate, m_channels);
    }

    std::size_t source_read(void *data, std::size_t max) override {
        int err;
        std::size_t done;

        if (m_eof) {
            return 0;
        }

#if MPG123_API_VERSION < 46
        err = mpg123_read(m_handle.get(), static_cast<unsigned char *>(data), max, &done);
#else
        err = mpg123_read(m_handle.get(), data, max, &done);
#endif

        if (err != MPG123_OK && err != MPG123_DONE) {
            return 0;
        }

        if (err == MPG123_DONE) {
            m_eof = true;
        }

        return done;
    }

    void source_rewind() override {
        m_vfs.rewind();
        m_eof = mpg123_open_handle(m_handle.get(), this) != MPG123_OK;
    }

private:
    std::unique_ptr<mpg123_handle, decltype(&mpg123_delete)> m_handle;

    VFS m_vfs;
    long m_rate;
    int m_channels;
    bool m_eof = false;

    static VFS &vfs(void *source) {
        return reinterpret_cast<Mpg123Source *>(source)->m_vfs;
    }

    static ssize_t vio_read(void *source, void *ptr, std::size_t count) {
        return vfs(source).read(ptr, count);
    }

    static off_t vio_lseek(void *source, off_t offset, int whence) {
        return vfs(source).seek(offset, whence);
    }
};

#ifdef GARGLK_HAS_FLUIDSYNTH
class FluidSynthSource : public Decoder {
public:
    FluidSynthSource(const std::vector<unsigned char> &buf, glui32 plays) :
        Decoder(plays)
    {
        for (const auto &level : {FLUID_PANIC, FLUID_ERR, FLUID_WARN, FLUID_INFO, FLUID_DBG}) {
            fluid_set_log_function(level, nullptr, nullptr);
        }

        m_settings.reset(new_fluid_settings());
        if (m_settings == nullptr) {
            throw SoundError("fluidsynth unable to allocate settings");
        }

        fluid_settings_setnum(m_settings.get(), "synth.gain", 0.6);
        fluid_settings_setint(m_settings.get(), "synth.reverb.active", gli_conf_fluidsynth_reverb);
        fluid_settings_setint(m_settings.get(), "synth.chorus.active", gli_conf_fluidsynth_chorus);

        double samplerate;
        fluid_settings_setnum(m_settings.get(), "synth.sample-rate", 48000);
        if (fluid_settings_getnum(m_settings.get(), "synth.sample-rate", &samplerate) == FLUID_FAILED) {
            throw SoundError("fluidsynth unable to get sample rate");
        }

        m_synth.reset(new_fluid_synth(m_settings.get()));
        if (m_synth == nullptr) {
            throw SoundError("fluidsynth unable to allocate synth");
        }

        for (const auto &soundfont : gli_conf_soundfonts) {
            fluid_synth_sfload(m_synth.get(), soundfont.c_str(), 1);
        }

        if (fluid_synth_get_sfont(m_synth.get(), 0) == nullptr) {
            char *s = nullptr;

            if (fluid_settings_dupstr(m_settings.get(), "synth.default-soundfont", &s) != FLUID_OK ||
                s == nullptr ||
                s[0] == 0)
            {
                fluid_free(s);
                throw SoundError("fluidsynth unable to find default soundfont");
            }

            std::string soundfont = s;
            fluid_free(s);

            if (fluid_synth_sfload(m_synth.get(), soundfont.c_str(), 1) == FLUID_FAILED) {
                throw SoundError("unable to load sound font");
            }
        }

        fluid_synth_set_interp_method(m_synth.get(), -1, FLUID_INTERP_7THORDER);

        m_player.reset(new_fluid_player(m_synth.get()));
        if (m_player == nullptr) {
            throw SoundError("fluidsynth unable to allocate player");
        }

        if (fluid_player_add_mem(m_player.get(), buf.data(), buf.size()) == FLUID_FAILED) {
            throw SoundError("fluidsynth unable to load midi file");
        }

        if (fluid_player_play(m_player.get()) == FLUID_FAILED) {
            throw SoundError("fluidsynth unable to play midi file");
        }

        set_format(samplerate, 2);
    }

protected:
    std::size_t source_read(void *data, std::size_t max) override {
        if (fluid_player_get_status(m_player.get()) == FLUID_PLAYER_DONE) {
            return 0;
        }

        // 2 channels * 4-byte float = 8 bytes/frame. max may not be frame
        // aligned (SDL's get-callback requests arbitrary sizes), so report only
        // the whole frames actually written, not max.
        std::size_t frames = max / 8;
        if (fluid_synth_write_float(m_synth.get(), frames, data, 0, 2, data, 1, 2) != FLUID_OK) {
            return 0;
        }

        return frames * 8;
    }

    void source_rewind() override {
        fluid_player_stop(m_player.get());
        fluid_player_play(m_player.get());
    }

private:
    // The FluidSynth code handles null pointers in its delete
    // functions, but does not document this fact, so to be future
    // proof, create wrappers that check for null.
    static void settings_deleter(fluid_settings_t *settings) {
        if (settings != nullptr) {
            delete_fluid_settings(settings);
        }
    }

    static void synth_deleter(fluid_synth_t *synth) {
        if (synth != nullptr) {
            delete_fluid_synth(synth);
        }
    }

    static void player_deleter(fluid_player_t *player) {
        if (player != nullptr) {
            delete_fluid_player(player);
        }
    }

    std::unique_ptr<fluid_settings_t, decltype(&settings_deleter)> m_settings{nullptr, settings_deleter};
    std::unique_ptr<fluid_synth_t, decltype(&synth_deleter)> m_synth{nullptr, synth_deleter};
    std::unique_ptr<fluid_player_t, decltype(&player_deleter)> m_player{nullptr, player_deleter};
};
#endif

}

namespace garglk {

void init_decoders()
{
#if MPG123_API_VERSION < 46
    mp3_initialized = mpg123_init() == MPG123_OK;
#endif
}

int detect_format(const std::vector<unsigned char> &buf)
{
    struct Magic {
        virtual ~Magic() = default;
        [[nodiscard]] virtual bool matches(const std::vector<unsigned char> &data) const = 0;
    };

    struct MagicString : public Magic {
        MagicString(long long offset, std::string string) :
            m_offset(offset),
            m_string(std::move(string))
        {
        }

        [[nodiscard]] bool matches(const std::vector<unsigned char> &data) const override {
            if (m_offset + m_string.size() > data.size()) {
                return false;
            }

            return std::memcmp(&data[m_offset], m_string.data(), m_string.size()) == 0;
        }

    private:
        const long long m_offset;
        const std::string m_string;
    };

    struct MagicMod : public Magic {
        [[nodiscard]] bool matches(const std::vector<unsigned char> &data) const override {
            std::size_t size = std::min(openmpt::probe_file_header_get_recommended_size(), static_cast<std::size_t>(data.size()));

#ifdef GARGLK_CONFIG_OLD_LIBOPENMPT
            std::uint64_t flags = openmpt::probe_file_header_flags_default;
#else
            std::uint64_t flags = openmpt::probe_file_header_flags_default2;
#endif

            return openmpt::probe_file_header(flags, data.data(), size) == openmpt::probe_file_header_result_success;
        }
    };

    std::vector<std::pair<std::shared_ptr<Magic>, int>> magics = {
        {std::make_shared<MagicString>(0, "FORM"), giblorb_ID_FORM},
        {std::make_shared<MagicString>(0, "RIFF"), giblorb_ID_WAVE},
        {std::make_shared<MagicString>(0, "OggS"), giblorb_ID_OGG},
        {std::make_shared<MagicString>(0, "MThd"), giblorb_ID_MIDI},

        {std::make_shared<MagicMod>(), giblorb_ID_MOD},

        // ID3-tagged MP3s have a magic string, but untagged MP3
        // files don't, as they are just collections of frames, each
        // with a sync header. All MP3 sync headers start with an
        // 11-bit frame sync which is set to all ones. The next 4
        // bits can be anything except all zeros, and the bit
        // following that can be 0 or 1, giving 6 possible values
        // for the first 2 bytes of the frame. This may well have
        // false positives, but mpg123 will then hopefully fail to
        // load the file.
        {std::make_shared<MagicString>(0, "ID3"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xe2"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xe3"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xf2"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xf3"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xfa"), giblorb_ID_MP3},
        {std::make_shared<MagicString>(0, "\xff\xfb"), giblorb_ID_MP3},
    };

    for (const auto &[magic, type] : magics) {
        if (magic->matches(buf)) {
            return type;
        }
    }

    return 0;
}

Expected<std::pair<int, std::vector<unsigned char>>> load_bleep_resource(glui32 snd)
{
    if (snd != 1 && snd != 2) {
        return "invalid bleep selected"s;
    }

    auto data = gli_bleeps.at(snd);
    return {{detect_format(data), std::move(data)}};
}

Expected<std::pair<int, std::vector<unsigned char>>> load_sound_resource(glui32 snd)
{
    std::vector<unsigned char> data;

    if (giblorb_get_resource_map() != nullptr) {
        glui32 type;

        if (!giblorb_copy_resource(giblorb_ID_Snd, snd, type, data)) {
            return "can't get blorb resource"s;
        }

        return {{static_cast<int>(type), std::move(data)}};
    } else {
        const auto &resource_map = gli_get_resource_map(giblorb_ID_Snd);
        if (!resource_map.empty()) {
            try {
                data = resource_map.at(snd);
            } catch (const std::out_of_range &) {
                return "invalid resource"s;
            }
        } else {
            auto filename = Format("{}/SND{}", gli_workdir, snd);

            if (!garglk::read_file(filename, data)) {
                return "can't open SND file"s;
            }
        }

        return {{detect_format(data), std::move(data)}};
    }
}

std::shared_ptr<Decoder> create_decoder(int type, std::vector<unsigned char> data, glui32 plays)
{
    try {
        switch (type) {
        case giblorb_ID_MOD:
            return std::make_shared<OpenMPTSource>(data, plays);
        case giblorb_ID_AIFF:
        case giblorb_ID_FORM:
        case giblorb_ID_OGG:
        case giblorb_ID_WAVE:
            return std::make_shared<SndfileSource>(std::move(data), plays);
        case giblorb_ID_MP3:
            return std::make_shared<Mpg123Source>(std::move(data), plays);
#ifdef GARGLK_HAS_FLUIDSYNTH
        case giblorb_ID_MIDI:
            return std::make_shared<FluidSynthSource>(data, plays);
#endif
        default:
            throw SoundError("unsupported sound type");
        }
    } catch (const std::bad_alloc &) {
        throw SoundError("unable to allocate");
    }
}

}
