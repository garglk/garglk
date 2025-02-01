// Copyright 2011-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <array>
#include <exception>
#include <memory>
#include <set>
#include <stdexcept>
#include <unordered_map>

#include "sound.h"
#include "iff.h"
#include "process.h"
#include "types.h"
#include "zterp.h"

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
#if defined(GLK_MODULE_SOUND2) && defined(ZTERP_GLK_BLORB)
#include <gi_blorb.h>
#endif
}
#endif

#ifdef GLK_MODULE_SOUND2
struct Channel {
    class Error : public std::exception {
    };

    Channel() {
        if (channel == nullptr) {
            throw Error();
        }
    }

    ~Channel() {
        glk_schannel_destroy(channel);
    }

    Channel(const Channel &other) = delete;
    Channel &operator=(const Channel &other) = delete;

    schanid_t channel = glk_schannel_create(0);
    bool playing = false;
    uint16_t routine = 0;

    struct {
        uint16_t number;
        uint8_t volume;
    } queued = {0, 0};
};

class Channels {
public:
    static constexpr glui32 Effects = 1;
    static constexpr glui32 Music = 2;

    void create() {
        try {
            m_channels.emplace(Effects, std::make_shared<Channel>());
        } catch (const Channel::Error &) {
            return;
        }

        // Try to create a music channel, but if that fails, alias it to
        // the effects channel. This will allow music and effects to
        // interrupt each other, but that’s better than no music.
        try {
            m_channels.emplace(Music, std::make_shared<Channel>());
        } catch (const std::exception &) {
            m_channels.emplace(Music, m_channels.at(Effects));
        }
    };

    bool loaded() const {
        return !m_channels.empty();
    }

    std::shared_ptr<Channel> at(glui32 chantype) {
        return m_channels.at(chantype);
    }

private:
    std::unordered_map<glui32, std::shared_ptr<Channel>> m_channels;
};

constexpr glui32 Channels::Effects;
constexpr glui32 Channels::Music;

static Channels channels;

static std::set<uint32_t> looping_sounds;
static std::unordered_map<uint32_t, bool> music_sounds;
#endif

void init_sound()
{
#ifdef GLK_MODULE_SOUND2
    if (sound_loaded()) {
        return;
    }

    if (glk_gestalt(gestalt_Sound2, 0)) {
        channels.create();
    }

#ifdef ZTERP_GLK_BLORB
    // Blorb files support a “Loop” chunk which is used to specify which
    // sound resources, when played, loop indefinitely. This exists
    // solely to support The Lurking Horror’s sounds, some of which are
    // looping. The Lurking Horror embedded the looping information
    // directly into the sound files themselves, which is why Blorbs
    // include the looping information. This is only used in V3 games.
    // See §11.4 of the Blorb specification.
    auto *map = giblorb_get_resource_map();
    if (zversion == 3 && sound_loaded() && map != nullptr) {
        giblorb_result_t res;
        glui32 chunktype = IFF::TypeID("Loop").val();
        if (giblorb_load_chunk_by_type(map, giblorb_method_Memory, &res, chunktype, 0) == giblorb_err_None) {
            for (size_t i = 0; i < res.length; i += 8) {
                auto read32 = [&res](size_t offset) {
                    const auto *p = static_cast<const char *>(res.data.ptr) + offset;
                    return (static_cast<uint32_t>(p[0]) << 24) |
                           (static_cast<uint32_t>(p[1]) << 16) |
                           (static_cast<uint32_t>(p[2]) <<  8) |
                           (static_cast<uint32_t>(p[3]) <<  0);
                    };
                uint32_t n = read32(i);
                uint32_t repeats = read32(i + 4);
                if (repeats == 0) {
                    looping_sounds.insert(n);
                }
            }
            giblorb_unload_chunk(map, res.chunknum);
        }
    }
#endif
#endif
}

bool sound_loaded()
{
#ifdef GLK_MODULE_SOUND2
    return channels.loaded();
#else
    return false;
#endif
}

#ifdef GLK_MODULE_SOUND2
static void start_sound(glui32 chantype, Channel *channel, uint16_t number, uint8_t repeats, uint8_t volume)
{
    const std::array<uint32_t, 8> vols = {
        0x02000, 0x04000, 0x06000, 0x08000,
        0x0a000, 0x0c000, 0x0e000, 0x10000
    };

    channel->queued.number = 0;

    try {
        glk_schannel_set_volume(channel->channel, vols.at(volume - 1));
    } catch (const std::out_of_range &) {
        return;
    }

    if (glk_schannel_play_ext(channel->channel, number, repeats == 255 ? -1 : repeats, chantype)) {
        channel->playing = true;
    } else {
        channel->playing = false;
        channel->routine = 0;
    }
}

void sound_stopped(glui32 chantype)
{
    try {
        auto channel = channels.at(chantype);

        channel->playing = false;

        if (channel->queued.number != 0) {
            start_sound(chantype, channel.get(), channel->queued.number, 1, channel->queued.volume);
            channel->queued.number = 0;
        }
    } catch (const std::out_of_range &) {
    }
}

uint16_t sound_get_routine(glui32 chantype)
{
    try {
        auto channel = channels.at(chantype);
        auto routine = channel->routine;
        channel->routine = 0;
        return routine;
    } catch (const std::out_of_range &) {
        return 0;
    }
}
#endif

void zsound_effect()
{
    uint16_t number = znargs == 0 ? 1 : zargs[0];

    if (number == 1 || number == 2) {
#ifdef GLK_MODULE_GARGLKBLEEP
        garglk_zbleep(number);
#endif
        return;
    }

    if (znargs < 2) {
        return;
    }

#ifdef GLK_MODULE_SOUND2
    if (!channels.loaded()) {
        return;
    }

    glui32 chantype = Channels::Effects;

#ifdef ZTERP_GLK_BLORB
    // According to §9: “Only one sound effect of each type can play at
    // any given time, so that starting a new music sound effect stops
    // any current music playing, and starting any new sample sound
    // effect stops any current sample sound playing. Samples and music
    // do not interrupt each other.”
    //
    // The Standard doesn’t go into the difference between sound effects
    // and music, but the Blorb standard (in §14.3) indicates that MOD
    // and Ogg Vorbis are music, while AIFF is a sound effect. One can
    // assume that SONGs (Blorb §3.4), although deprecated, count as
    // music as well.
    //
    // The Blorb standard (§14.5) notes that Adrift Blorbs are allowed
    // more sound formats than standard Blorb: WAV, MIDI, and MP3. While
    // there’s likely never going to be a Blorb file with these sound
    // formats meant for use with the Z-Machine, there’s no harm in
    // adding MIDI and MP3 as music types here.
    if (music_sounds.find(number) == music_sounds.end()) {
        auto *map = giblorb_get_resource_map();
        if (map != nullptr) {
            giblorb_result_t res;
            if (giblorb_load_resource(map, giblorb_method_DontLoad, &res, giblorb_ID_Snd, number) == giblorb_err_None) {
                switch (res.chunktype) {
                case 0x4d4f4420: /* MOD */
                case 0x4f474756: /* OGGV */
                case 0x534f4e47: /* SONG */
                case 0x4d494449: /* MIDI (non-standard) */
                case 0x4d503320: /* MP3 (non-standard) */
                    music_sounds.emplace(number, true);
                    break;
                }
            }
        }
    }

    if (music_sounds[number]) {
        chantype = Channels::Music;
    }
#endif

    std::shared_ptr<Channel> channel;
    try {
        channel = channels.at(chantype);
    } catch (const std::out_of_range &) {
        return;
    }

    constexpr uint16_t SOUND_EFFECT_PREPARE = 1;
    constexpr uint16_t SOUND_EFFECT_START = 2;
    constexpr uint16_t SOUND_EFFECT_STOP = 3;
    constexpr uint16_t SOUND_EFFECT_FINISH = 4;

    uint16_t effect = zargs[1];

    // Sound effect 0 is invalid, except that it can be used to mean
    // “stop all sounds”.
    if (number == 0 && effect != SOUND_EFFECT_STOP) {
        return;
    }

    switch (effect) {
    case SOUND_EFFECT_PREPARE:
        glk_sound_load_hint(number, 1);
        break;
    case SOUND_EFFECT_START: {
        if (znargs < 3) {
            return;
        }

        uint8_t repeats = zargs[2] >> 8;
        uint8_t volume = zargs[2] & 0xff;

        if (looping_sounds.find(number) != looping_sounds.end()) {
            repeats = 255;
        }

        // Illegal, but recommended by standard 1.1.
        if (repeats == 0) {
            repeats = 1;
        }

        if (volume == 0) {
            volume = 1;
        } else if (volume > 8) {
            volume = 8;
        }

        // Implement the hack described in the remarks to §9 for The
        // Lurking Horror. Moreover, only impelement this for the two
        // samples where it is relevant, since this behavior is so
        // idiosyncratic that it should be ignored everywhere else. In
        // addition, this doesn’t bother checking whether player input
        // has occurred since the previous sound was started, because
        // for the two specific samples in The Lurking Horror for which
        // this hack applies, it’s known that there will not have been
        // any input.
        //
        // Don’t queue the number of repeats: always play a queued sound
        // only once. This is because one of the queued sounds
        // (S-CRETIN, number 16) has a built-in loop; the game knows
        // this so explicitly stops the sample in the same game round as
        // it was started to prevent it from looping indefinitely.
        // Modern systems are so fast that this stop event comes in
        // roughly simultaneously with the start event, so the sound
        // doesn’t play at all, if the stop event is honored. As such,
        // stop events are ignored below if a queued sound exists, so
        // the “stopping” must happen here by only playing the sample
        // once.
        if (is_game(Game::LurkingHorror) && chantype == Channels::Effects && channel->playing && (number == 9 || number == 16)) {
            channel->queued.number = number;
            channel->queued.volume = volume;
            return;
        }

        channel->routine = znargs >= 4 ? zargs[3] : 0;
        start_sound(chantype, channel.get(), number, repeats, volume);

        break;
    }
    case SOUND_EFFECT_STOP:
        // If there is a queued sound, ignore stop requests. This is
        // because The Lurking Horror does the following in one single
        // game round:
        //
        // @sound_effect 11 2 8
        // @sound_effect 16 2 8
        // @sound_effect 16 3
        //
        // Per the remarks to §9, this is because Infocom knew the Amiga
        // interpreter (the only system where a sound-enabled game was
        // available) was slow, so sound effects would be able to play
        // while the interpreter was running between user inputs. Thus
        // there was presumably a fair amount of time between the second
        // and third @sound_effect calls, meaning that the stop event
        // came in after the sample had played, or at least partially
        // played. On fast modern systems, the stop call will be
        // processed before either sound effect has had any time to
        // play. A queued sound will only play once, so ignoring this
        // stop request is effectively just delaying it till the queued
        // sound finishes.
        if (channel->queued.number == 0) {
            glk_schannel_stop(channel->channel);
            channel->routine = 0;
        }
        break;
    case SOUND_EFFECT_FINISH:
        glk_sound_load_hint(number, 0);
        break;
    }
#endif
}
