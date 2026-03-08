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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <set>
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

#include "glk.h"
#include "garglk.h"

#include "gi_blorb.h"

#include "snddecode.h"

using garglk::SoundError;

#define giblorb_ID_MOD  (giblorb_make_id('M', 'O', 'D', ' '))
#define giblorb_ID_OGG  (giblorb_make_id('O', 'G', 'G', 'V'))
#define giblorb_ID_FORM (giblorb_make_id('F', 'O', 'R', 'M'))
#define giblorb_ID_AIFF (giblorb_make_id('A', 'I', 'F', 'F'))

// non-standard types
#define giblorb_ID_MP3  (giblorb_make_id('M', 'P', '3', ' '))
#define giblorb_ID_WAVE (giblorb_make_id('W', 'A', 'V', 'E'))
#define giblorb_ID_MIDI (giblorb_make_id('M', 'I', 'D', 'I'))

#define GLK_MAXVOLUME 0x10000

static std::set<schanid_t> gli_channellist;

static schanid_t gli_bleep_channel;

namespace {

// Adapts a backend-agnostic garglk::Decoder to the QIODevice that QAudioSink
// pulls from. The decoder produces float PCM and handles repeats; this only
// adds the QAudioSink-specific silence padding.
class SoundSource : public QIODevice {
public:
    explicit SoundSource(std::shared_ptr<garglk::Decoder> decoder) :
        m_decoder(std::move(decoder))
    {
        m_format.setSampleRate(m_decoder->samplerate());
        m_format.setChannelCount(m_decoder->channels());
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

    void set_audio_buffer_size(qint64 size) {
        m_buffer_size = size;
    }

    qint64 readData(char *data, qint64 max) override {
        // QIODevice buffers data; if its buffer is full enough to
        // satisfy its current needs, it calls readData with a max
        // size of 0 (I'm not sure why it doesn't just skip calling it
        // entirely). This does not preclude further calls with a
        // positive max value in the future, so short-circuit here to
        // avoid getting 0 bytes back from the decoder and triggering
        // the end-of-stream/silence-padding code below.
        //
        // The QIODevice docs say that readData "might be called with a
        // maxSize of 0, which can be used to perform post-reading
        // operations", but that's misleading as far as I can tell,
        // given that it might also be called mid-read instead of just
        // post-read.
        if (max == 0) {
            return 0;
        }

        std::size_t n = m_decoder->read(data, max);

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

#ifdef HAS_QT6
    // From the consumer's point of view this is a sequential stream.
    bool isSequential() const override {
        return true;
    }

    // The Windows QAudioSink uses this to decide whether more audio is
    // available: a full buffer while the decoder still has repeats to play,
    // otherwise zero.
    qint64 bytesAvailable() const override {
        return m_decoder->finished() ? 0 : m_buffer_size;
    }
#endif

private:
    std::shared_ptr<garglk::Decoder> m_decoder;
    QAudioFormat m_format;
    qint64 m_buffer_size = 0;
    qint64 m_written = 0;
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
        // QAudioSink's volume is a raw linear gain; map through a perceptual
        // curve so the volume control behaves evenly across its range.
        auto gain = std::pow(static_cast<double>(current_volume) / GLK_MAXVOLUME, std::log(4));
        if (audio) {
            audio->setVolume(gain);
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
    garglk::init_decoders();
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
        // An immediate volume set overrides any in-progress fade.
        chan->timer.stop();
        chan->current_volume = chan->target_volume;
        chan->set_current_volume();
    } else {
        chan->duration = duration / 1000.0;
        chan->difference = static_cast<double>(chan->target_volume) - chan->current_volume;
        chan->timer.start(100);
    }

    chan->last_volume_bump = std::chrono::steady_clock::now();
}

static glui32 gli_schannel_play_ext(schanid_t chan, glui32 snd, glui32 repeats, glui32 notify, const std::function<garglk::Expected<std::pair<int, std::vector<unsigned char>>>(glui32)> &load_resource)
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
        auto resource = load_resource(snd);
        if (!resource.has_value()) {
            return 0;
        }
        auto [type, data] = std::move(*resource);

        source = std::make_shared<SoundSource>(garglk::create_decoder(type, std::move(data), repeats));

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

#ifdef HAS_QT6
        // Need to initially provide a little bit of audio or QAudioSink will immediately stop playback.
        chan->source->set_audio_buffer_size(16384);
#endif

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
    return gli_schannel_play_ext(chan, snd, repeats, notify, garglk::load_sound_resource);
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
        chan->audio.reset();
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
            gli_schannel_play_ext(gli_bleep_channel, number, 1, 0, garglk::load_bleep_resource);
        } catch (const Bleeps::Empty &) {
        }
    }
}
