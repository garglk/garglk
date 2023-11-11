// Copyright (C) 2006-2009 by Tor Andersson, Lorenzo Marcantonio.
// Copyright (C) 2010 by Ben Cressey
// Copyright (C) 2021 by Chris Spiegel.
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
#include <chrono>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <unistd.h>

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define HAS_QT6
#endif

#include <QAudioFormat>
#include <QIODevice>
#include <QTimer>

#ifdef HAS_QT6
#include <QAudioSink>
#include <QMediaDevices>
#else
#include <QAudioOutput>
#include <QSysInfo>
#endif

#include <libopenmpt/libopenmpt.h>
#include <mpg123.h>
#include <sndfile.hh>

#include "format.h"

#include "glk.h"
#include "garglk.h"

#include "gi_blorb.h"

#define giblorb_ID_MOD  (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_OGG  (giblorb_make_id('O', 'G', 'G', 'V'))
#define giblorb_ID_FORM (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_AIFF (giblorb_make_id('A', 'I', 'F', 'F'))

// non-standard types
#define giblorb_ID_MP3  (giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE (giblorb_make_id('W', 'A', 'V', 'E'))

#define GLK_MAXVOLUME 0x10000

static std::set<schanid_t> gli_channellist;

static schanid_t gli_bleep_channel;

#if MPG123_API_VERSION < 46
static bool mp3_initialized;
#endif

#ifdef _MSC_VER
using ssize_t = std::make_signed_t<size_t>;
#endif

namespace {

class VFSAbstract {
public:
    explicit VFSAbstract(std::vector<unsigned char> buf) : m_buf(std::move(buf)) {
    }

    qsizetype size() {
        return m_buf.size();
    }

    off_t tell() const {
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

class SoundError : public std::runtime_error {
public:
    explicit SoundError(const std::string &msg) : std::runtime_error(msg) {
    }
};

class SoundSource : public QIODevice {
public:
    explicit SoundSource(int plays) : m_plays(plays) {
    }

    virtual qint64 source_read(void *data, qint64 max) = 0;
    virtual void source_rewind() = 0;

    // Sources must use 32-bit floating point audio.
    void set_format(int samplerate, int channels) {
        m_format.setSampleRate(samplerate);
        m_format.setChannelCount(channels);
#ifdef HAS_QT6
        m_format.setSampleFormat(QAudioFormat::Float);
#else
        m_format.setSampleSize(32);
        m_format.setCodec("audio/pcm");
        m_format.setByteOrder(static_cast<QAudioFormat::Endian>(QSysInfo::Endian::ByteOrder));
        m_format.setSampleType(QAudioFormat::Float);
#endif
    }

    QAudioFormat format() {
        return m_format;
    }

    qint64 readData(char *data, qint64 max) override {
        // QIODevice buffers data; if its buffer is full enough to
        // satisfy its current needs, it calls readData with a max
        // size of 0 (I'm not sure why it doesn't just skip calling it
        // entirely). This does not preclude further calls with a
        // positive max value in the future, so short-circuit here to
        // avoid getting 0 bytes back from the decoder and triggering
        // the EOF/handle repeat code below.
        //
        // The QIODevice docs say that readData "might be called with a
        // maxSize of 0, which can be used to perform post-reading
        // operations", but that's misleading as far as I can tell,
        // given that it might also be called mid-read instead of just
        // post-read.
        if (max == 0) {
            return 0;
        }

        std::size_t n = 0;
        if (m_plays > 0) {
            n = source_read(data, max);
            if (n == 0 && (m_plays == 0xffffffff || --m_plays > 0)) {
                source_rewind();
                n = source_read(data, max);
            }
        }

        // Ensure at least one full audio buffer was written.
        if (n == 0) {
            qint64 needed = m_buffer_size - m_written;

            if (needed >= 0) {
                n = std::min(needed, max);
                std::memset(data, 0, n);
            }
        }

        m_written += n;

        return n;
    }

    qint64 writeData(const char *, qint64) override {
        return 0;
    }

    void set_audio_buffer_size(qint64 size)
    {
        m_buffer_size = size;
    }

private:
    QAudioFormat m_format;
    glui32 m_plays;
    qint64 m_buffer_size = 0;
    qint64 m_written = 0;
};

// The C++ API requires C++17, so until Gargoyle switches from C++14 to
// C++17, use the C API.
class OpenMPTSource : public SoundSource {
public:
    OpenMPTSource(const std::vector<unsigned char> &buf, int plays) :
        SoundSource(plays),
        m_mod(openmpt_module_create_from_memory2(buf.data(), buf.size(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr), openmpt_module_destroy)
    {
        if (!m_mod) {
            throw SoundError("can't parse MOD file");
        }

        set_format(48000, 2);
    }

protected:
    qint64 source_read(void *data, qint64 max) override {
        return 8 * openmpt_module_read_interleaved_float_stereo(m_mod.get(), format().sampleRate(), max / 8, reinterpret_cast<float *>(data));
    }

    void source_rewind() override {
        openmpt_module_set_position_seconds(m_mod.get(), 0);
    }

private:
    std::unique_ptr<openmpt_module, decltype(&openmpt_module_destroy)> m_mod;
};

class SndfileSource : public SoundSource {
public:
    SndfileSource(std::vector<unsigned char> buf, glui32 plays) :
        SoundSource(plays),
        m_vfs(std::move(buf))
    {
        SF_VIRTUAL_IO io = get_io();
        m_soundfile = SndfileHandle(io, this);
        if (!m_soundfile) {
            throw SoundError("can't open with libsndfile");
        }

        set_format(m_soundfile.samplerate(), m_soundfile.channels());
    }

    qint64 source_read(void *data, qint64 max) override {
        return 4 * m_soundfile.read(reinterpret_cast<float *>(data), max / 4);
    }

    void source_rewind() override {
        m_vfs.rewind();
        SF_VIRTUAL_IO io = get_io();
        m_soundfile = SndfileHandle(io, this);
    }

private:
    SndfileHandle m_soundfile;
    VFSAbstract m_vfs;

    static SF_VIRTUAL_IO get_io() {
        SF_VIRTUAL_IO io;

        io.get_filelen = vio_get_filelen;
        io.seek = vio_seek;
        io.read = vio_read;
        io.write = vio_write;
        io.tell = vio_tell;

        return io;
    }

    static VFSAbstract &vfs(void *source) {
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

class Mpg123Source : public SoundSource {
public:
    Mpg123Source(std::vector<unsigned char> buf, glui32 plays) :
        SoundSource(plays),
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

    qint64 source_read(void *data, qint64 max) override {
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

    VFSAbstract m_vfs;
    long m_rate;
    int m_channels;
    bool m_eof = false;

    static VFSAbstract &vfs(void *source) {
        return reinterpret_cast<Mpg123Source *>(source)->m_vfs;
    }

    static ssize_t vio_read(void *source, void *ptr, std::size_t count) {
        return vfs(source).read(ptr, count);
    }

    static off_t vio_lseek(void *source, off_t offset, int whence) {
        return vfs(source).seek(offset, whence);
    }
};

}

struct glk_schannel_struct {
    glk_schannel_struct(glui32 volume, glui32 rock_) :
        current_volume(volume),
        target_volume(volume),
        rock(rock_),
        disprock(gli_register_obj != nullptr ?
                gli_register_obj(this, gidisp_Class_Schannel) :
                gidispatch_rock_t{})
    {
        timer.setTimerType(Qt::TimerType::PreciseTimer);
    }

    glk_schannel_struct(const glk_schannel_struct &) = delete;
    glk_schannel_struct &operator=(const glk_schannel_struct &) = delete;

    ~glk_schannel_struct() {
        if (gli_unregister_obj != nullptr) {
            gli_unregister_obj(this, gidisp_Class_Schannel, disprock);
        }
    }

    void set_current_volume() {
        if (audio) {
            audio->setVolume(static_cast<double>(current_volume) / GLK_MAXVOLUME);
        }
    }

    // Source comes first so it's deleted last, since QAudioOutput still
    // holds a pointer to it. This is a shared_ptr to emphasize that
    // it's shared with QAudioOutput, but QAudioOutput doesn't use
    // shared_ptr, so it's really just informative. The real advantage
    // of a smart pointer here is automatic deletion of the allocated
    // pointer on deletion of the sound channel.
    std::shared_ptr<SoundSource> source;

    // It is possible for this object to be destroyed on shutdown, i.e.
    // when static objects are being destroyed (e.g. if
    // glk_schannel_delete() is called in a static object's destructor).
    // Gargoyle itself doesn't do this, but applications/interpreters
    // are allowed to. The problem is that at least some Qt audio
    // backends (PulseAudio for one) are implemented as static objects.
    // The order of destruction of static objects can't be controlled
    // between different files, meaning it's possible for the backend to
    // be destroyed before this audio object is destroyed; but the
    // destruction of this audio object relies on the audio backend to
    // exist. If it's destroyed first, the result is undefined behavior.
    // To work around this, a custom deleter is used which will only
    // delete this instance if Gargoyle is not shutting down, as
    // determined by the gli_exiting flag.
#ifdef HAS_QT6
    std::unique_ptr<QAudioSink, std::function<void(QAudioSink *)>> audio;
#else
    std::unique_ptr<QAudioOutput, std::function<void(QAudioOutput *)>> audio;
#endif

    QTimer timer;
    double duration = 0;
    double difference = 0;
    std::chrono::steady_clock::time_point last_volume_bump;
    long current_volume;
    glui32 target_volume;
    glui32 volume_notify = 0;

    bool paused = false;

    glui32 rock;
    gidispatch_rock_t disprock;
};

gidispatch_rock_t gli_sound_get_channel_disprock(const channel_t *chan)
{
    return chan->disprock;
}

void gli_initialize_sound()
{
#if MPG123_API_VERSION < 46
    mp3_initialized = mpg123_init() == MPG123_OK;
#endif
}

schanid_t glk_schannel_create(glui32 rock)
{
    return glk_schannel_create_ext(rock, GLK_MAXVOLUME);
}

schanid_t glk_schannel_create_ext(glui32 rock, glui32 volume)
{
    channel_t *chan;

    if (!gli_conf_sound) {
        return nullptr;
    }

    chan = new channel_t(volume, rock);

    auto on_timeout = [chan]() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - chan->last_volume_bump).count();
        chan->last_volume_bump = now;

        chan->current_volume += chan->difference * elapsed / chan->duration;

        if ((chan->difference > 0 && chan->current_volume >= chan->target_volume) ||
            (chan->difference < 0 && chan->current_volume <= chan->target_volume))
        {
            chan->current_volume = chan->target_volume;
        }

        if (chan->current_volume == chan->target_volume) {
            if (chan->volume_notify != 0) {
                gli_event_store(evtype_VolumeNotify, nullptr, 0, chan->volume_notify);
                gli_notification_waiting();
            }
            chan->timer.stop();
        }

        chan->set_current_volume();
    };
    QObject::connect(&chan->timer, &QTimer::timeout, on_timeout);

    gli_channellist.insert(chan);

    return chan;
}

void glk_schannel_destroy(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_destroy: invalid id.");
        return;
    }

    glk_schannel_stop(chan);

    // If this is running on exit, then it's possible for
    // gli_channellist to be destroyed before this is called.
    if (!gli_exiting) {
        gli_channellist.erase(chan);
    }

    delete chan;
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rock)
{
    std::set<schanid_t>::iterator it;

    if (chan == nullptr) {
        it = gli_channellist.begin();
    } else {
        it = gli_channellist.find(chan);
        if (it != gli_channellist.end()) {
            ++it;
        }
    }

    if (it == gli_channellist.end()) {
        if (rock != nullptr) {
            *rock = 0;
        }

        return nullptr;
    } else {
        if (rock != nullptr) {
            *rock = (*it)->rock;
        }

        return *it;
    }
}

glui32 glk_schannel_get_rock(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_get_rock: invalid id.");
        return 0;
    }
    return chan->rock;
}

glui32 glk_schannel_play(schanid_t chan, glui32 snd)
{
    return glk_schannel_play_ext(chan, snd, 1, 0);
}

glui32 glk_schannel_play_multi(schanid_t *chanarray, glui32 chancount, glui32 *sndarray, glui32 soundcount, glui32 notify)
{
    glui32 successes = 0;

    for (glui32 i = 0; i < chancount; i++) {
        successes += glk_schannel_play_ext(chanarray[i], sndarray[i], 1, notify);
    }

    return successes;
}

void glk_sound_load_hint(glui32 snd, glui32 flag)
{
}

void glk_schannel_set_volume(schanid_t chan, glui32 vol)
{
    glk_schannel_set_volume_ext(chan, vol, 0, 0);
}

void glk_schannel_set_volume_ext(schanid_t chan, glui32 vol, glui32 duration, glui32 notify)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_set_volume: invalid id.");
        return;
    }

    if (vol > GLK_MAXVOLUME) {
        vol = GLK_MAXVOLUME;
    }

    chan->current_volume = chan->target_volume;
    chan->target_volume = vol;
    chan->volume_notify = notify;

    if (duration == 0) {
        chan->current_volume = chan->target_volume;
        chan->set_current_volume();
    } else {
        chan->duration = duration / 1000.0;
        chan->difference = static_cast<double>(chan->target_volume) - chan->current_volume;
        chan->timer.start(100);
    }

    chan->last_volume_bump = std::chrono::steady_clock::now();
}

static int detect_format(const std::vector<unsigned char> &data)
{
    struct Magic {
        virtual ~Magic() = default;
        virtual bool matches(const std::vector<unsigned char> &data) const = 0;
    };

    struct MagicString : public Magic {
        MagicString(qint64 offset, std::string string) :
            m_offset(offset),
            m_string(std::move(string))
        {
        }

        bool matches(const std::vector<unsigned char> &data) const override {
            if (m_offset + m_string.size() > data.size()) {
                return false;
            }

            return std::memcmp(&data[m_offset], m_string.data(), m_string.size()) == 0;
        }

    private:
        const qint64 m_offset;
        const std::string m_string;
    };

    struct MagicMod : public Magic {
        bool matches(const std::vector<unsigned char> &data) const override {
            std::size_t size = std::min(openmpt_probe_file_header_get_recommended_size(), static_cast<std::size_t>(data.size()));

            return openmpt_probe_file_header(OPENMPT_PROBE_FILE_HEADER_FLAGS_DEFAULT,
                    data.data(), size, data.size(),
                    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == OPENMPT_PROBE_FILE_HEADER_RESULT_SUCCESS;
        }
    };

    std::vector<std::pair<std::shared_ptr<Magic>, int>> magics = {
        {std::make_shared<MagicString>(0, "FORM"), giblorb_ID_FORM},
        {std::make_shared<MagicString>(0, "RIFF"), giblorb_ID_WAVE},
        {std::make_shared<MagicString>(0, "OggS"), giblorb_ID_OGG},

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

    for (const auto &pair : magics) {
        if (pair.first->matches(data)) {
            return pair.second;
        }
    }

    throw SoundError("no matching magic");
}

static std::pair<int, std::vector<unsigned char>> load_bleep_resource(glui32 snd)
{
    if (snd != 1 && snd != 2) {
        throw SoundError("invalid bleep selected");
    }

    auto data = gli_bleeps.at(snd);
    return {detect_format(data), data};
}

static std::pair<int, std::vector<unsigned char>> load_sound_resource(glui32 snd)
{
    std::vector<unsigned char> data;

    if (giblorb_get_resource_map() == nullptr) {
        auto filename = Format("{}/SND{}", gli_workdir, snd);

        if (!garglk::read_file(filename, data)) {
            throw SoundError("can't open SND file");
        }

        return {detect_format(data), data};
    } else {
        glui32 type;

        if (!giblorb_copy_resource(giblorb_ID_Snd, snd, type, data)) {
            throw SoundError("can't get blorb resource");
        }

        return std::make_pair(type, data);
    }
}

static glui32 gli_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify, const std::function<std::pair<int, std::vector<unsigned char>>(glui32)> &load_resource)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_play_ext: invalid id.");
        return 0;
    }

    glk_schannel_stop(chan);

    if (repeats == 0) {
        return 1;
    }

    std::shared_ptr<SoundSource> source;
    try {
        int type;
        std::vector<unsigned char> data;

        std::tie(type, data) = load_resource(snd);

        try {
            switch (type) {
            case giblorb_ID_MOD:
                source.reset(new OpenMPTSource(data, repeats));
                break;
            case giblorb_ID_AIFF:
            case giblorb_ID_FORM:
            case giblorb_ID_OGG:
            case giblorb_ID_WAVE:
                source.reset(new SndfileSource(data, repeats));
                break;
            case giblorb_ID_MP3:
                source.reset(new Mpg123Source(data, repeats));
                break;
            default:
                return 0;
            }
        } catch (const std::bad_alloc &) {
            throw SoundError("unable to allocate");
        }

        if (!source->open(QIODevice::ReadOnly)) {
            throw SoundError("unable to open source");
        }

        QAudioFormat format = source->format();
#ifdef HAS_QT6
        auto device = QMediaDevices::defaultAudioOutput();
        if (!device.isFormatSupported(format)) {
            throw SoundError("unsupported source format");
        }

        chan->audio = decltype(chan->audio)(new QAudioSink(device, format), [](QAudioSink *audio) {
            if (!gli_exiting) {
                delete audio;
            }
        });
#else
        QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
        if (!info.isFormatSupported(format)) {
            throw SoundError("unsupported source format");
        }

        chan->audio = decltype(chan->audio)(new QAudioOutput(format), [](QAudioOutput *audio) {
            if (!gli_exiting) {
                delete audio;
            }
        });
#endif
        chan->source = std::move(source);

        if (notify != 0) {
            auto on_change = [snd, notify](QAudio::State state) {
                if (state == QAudio::State::IdleState) {
                    gli_event_store(evtype_SoundNotify, nullptr, snd, notify);
                    gli_notification_waiting();
                }
            };

#ifdef HAS_QT6
            QObject::connect(chan->audio.get(), &QAudioSink::stateChanged, on_change);
#else
            QObject::connect(chan->audio.get(), &QAudioOutput::stateChanged, on_change);
#endif
        }

        chan->set_current_volume();

        chan->audio->start(chan->source.get());
        if (chan->audio->error() != QAudio::NoError) {
            throw SoundError("unable to start sound");
        }

        chan->source->set_audio_buffer_size(chan->audio->bufferSize());

        if (chan->paused) {
            chan->audio->suspend();
        }

        return 1;
    } catch (const SoundError &) {
        return 0;
    }
}

glui32 glk_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify)
{
    return gli_schannel_play_ext(chan, snd, repeats, notify, load_sound_resource);
}

void glk_schannel_pause(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_pause: invalid id.");
        return;
    }

    chan->paused = true;

    if (chan->audio) {
        chan->audio->suspend();
    }
}

void glk_schannel_unpause(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_unpause: invalid id.");
        return;
    }

    chan->paused = false;

    if (chan->audio) {
        chan->audio->resume();
    }
}

void glk_schannel_stop(schanid_t chan)
{
    if (chan == nullptr) {
        gli_strict_warning("schannel_stop: invalid id.");
        return;
    }

    if (!chan->audio) {
        return;
    }

    // If Gargoyle is exiting, then Qt's audio backend may have already
    // been destroyed, meaning calling chan->audio functions can lead to
    // undefined behavior. Simply ignore this request in that case.
    if (!gli_exiting) {
        chan->audio->stop();
        chan->timer.stop();
        chan->audio.reset(nullptr);
    }
}

void garglk_zbleep(glui32 number)
{
    if (gli_bleep_channel == nullptr) {
        gli_bleep_channel = glk_schannel_create(0);
        if (gli_bleep_channel != nullptr) {
            glk_schannel_set_volume(gli_bleep_channel, 0x8000);
        }
    }

    if (gli_bleep_channel != nullptr) {
        try {
            gli_schannel_play_ext(gli_bleep_channel, number, 1, 0, load_bleep_resource);
        } catch (const Bleeps::Empty &) {
        }
    }
}
