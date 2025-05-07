// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <climits>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#else
#include <iostream>
#endif

#include "io.h"
#include "osdep.h"
#include "types.h"
#include "unicode.h"
#include "util.h"

#ifdef ZTERP_NO_STDIO
#ifndef ZTERP_GLK_UNIX
#error ZTERP_NO_STDIO requires a Unix Glk
#endif

extern "C" {
#include <glkstart.h>
}

#ifndef GLKUNIX_FILEREF_CREATE_UNCLEANED
#error ZTERP_NO_STDIO requires the extension glkunix_fileref_create_by_name_uncleaned
// Prototype so that usage of this function doesn’t cause a compile error.
frefid_t glkunix_fileref_create_by_name_uncleaned(glui32 usage, const char *name, glui32 rock);
#endif
#endif

// Due to C++’s less-than-ideal type system, there’s no way to guarantee
// that an enum class actually contains a valid value. When checking the
// value of the I/O object’s type, this method is used as a sort of
// run-time type checker.
void IO::bad_type() const {
    die("internal error: unknown IO type %d", static_cast<int>(m_type));
}

// Certain streams are intended for use in text mode: stdin/stdout,
// transcripts, and command scripts. It is reasonable for users to
// expect newline translation to be properly handled in these cases,
// even though UNICODE_LINEFEED (10), as required by Glk (Glk API 0.7.0
// §2.2), is used internally.
bool IO::textmode() const {
    return m_purpose == Purpose::Transcript || m_purpose == Purpose::Input;
}

// Glk does not like you to be able to pass a full filename to
// glk_fileref_create_by_name(); this means that Glk cannot be used to
// open arbitrary files. However, Glk is still required to prompt for
// files, such as in a save game situation. To allow I/O to work for
// opening files both with and without a prompt, it will use stdio when
// either Glk is not available, or when Glk is available but prompting
// is not necessary.
//
// This is needed because the IFF parser is required for both opening
// games (zblorb files) and for saving/restoring. The former needs to
// be able to access any file on the filesystem, and the latter needs to
// prompt. This is a headache.
//
// Prompting is assumed to be necessary if “filename” is null.
IO::IO(const std::string *filename, Mode mode, Purpose purpose) :
    m_mode(mode),
    m_purpose(purpose)
{

#if !defined(ZTERP_GLK) || !defined(ZTERP_NO_STDIO)
    char smode[] = "wb";

    if (m_mode == Mode::ReadOnly) {
        smode[0] = 'r';
    } else if (m_mode == Mode::Append) {
        smode[0] = 'a';
    }

    if (textmode()) {
        smode[1] = 0;
    }
#endif

    // No need to prompt.
    if (filename != nullptr) {
        // Use stdio in non-Glk mode always, and in Glk mode unless
        // non-stdio mode is requested.
#if !defined(ZTERP_GLK) || !defined(ZTERP_NO_STDIO)
        m_type = Type::StandardIO;
        m_file = File(std::fopen(filename->c_str(), smode), true);
        if (m_file.stdio == nullptr) {
            throw OpenError();
        }
#else
        open_as_glk([&filename](glui32 usage, glui32) {
            return glkunix_fileref_create_by_name_uncleaned(usage, filename->c_str(), 0);
        });
#endif
    } else { // Prompt.
#ifdef ZTERP_GLK
        open_as_glk([](glui32 usage, glui32 filemode) {
            return glk_fileref_create_by_prompt(usage, filemode, 0);
        });
#else
        std::string fn, prompt;

        switch (m_purpose) {
        case Purpose::Data:
            prompt = "Enter filename for data: ";
            break;
        case Purpose::Save:
            prompt = "Enter filename for save game: ";
            break;
        case Purpose::Transcript:
            prompt = "Enter filename for transcript: ";
            break;
        case Purpose::Input:
            prompt = "Enter filename for command record: ";
            break;
        default:
            throw OpenError();
        }

        std::cout << std::endl << prompt << std::flush;
        if (!std::getline(std::cin, fn) || fn.empty()) {
            throw OpenError();
        }

        m_type = Type::StandardIO;
        m_file = File(std::fopen(fn.c_str(), smode), true);
        if (m_file.stdio == nullptr) {
            throw OpenError();
        }
#endif
    }
}

#ifdef ZTERP_GLK
void IO::open_as_glk(const std::function<frefid_t(glui32 usage, glui32 filemode)> &create_fref)
{
    frefid_t ref;
    glui32 usage, filemode;
    usage = fileusage_BinaryMode;

    switch (m_purpose) {
    case Purpose::Data:
        usage |= fileusage_Data;
        break;
    case Purpose::Save:
        usage |= fileusage_SavedGame;
        break;
    case Purpose::Transcript:
        usage |= fileusage_Transcript;
        break;
    case Purpose::Input:
        usage |= fileusage_InputRecord;
        break;
    default:
        throw OpenError();
    }

    switch (m_mode) {
    case Mode::ReadOnly:
        filemode = filemode_Read;
        break;
    case Mode::WriteOnly:
        filemode = filemode_Write;
        break;
    case Mode::Append:
        filemode = filemode_WriteAppend;
        break;
    default:
        throw OpenError();
    }

    ref = create_fref(usage, filemode);
    if (ref == nullptr) {
        throw OpenError();
    }

    m_type = Type::Glk;
    m_file = File(glk_stream_open_file(ref, filemode, 0));
    glk_fileref_destroy(ref);
    if (m_file.glk == nullptr) {
        throw OpenError();
    }
}
#endif

// Instead of being file-backed, indicate that this I/O object is
// memory-backed. This allows internal save states (for @save_undo as
// well as meta-saves created by /ps) to use Quetzal as their save
// format. This is helpful because meta-saves have to track extra
// information, and reusing the Quetzal code (plus extensions)
// eliminates the need for code duplication.
//
// The I/O object starts out with the contents of the passed-in buffer,
// which may be empty. The offset always starts at 0.
IO::IO(std::vector<uint8_t> buf, Mode mode) :
    m_file(File(std::move(buf))),
    m_type(Type::Memory),
    m_mode(mode),
    m_purpose(Purpose::Data)
{
    // Append isn’t used with memory-backed I/O, so it’s not supported.
    if (m_mode != Mode::ReadOnly && m_mode != Mode::WriteOnly) {
        throw OpenError();
    }
}

// Return a reference to the I/O instance’s internal buffer. This
// represents the state of the “file” at the time the function is
// called. The reference is only valid until the next call to an I/O
// method on this same I/O instance.
const std::vector<uint8_t> &IO::get_memory() const
{
    if (m_type != Type::Memory) {
        throw std::runtime_error("not a memory object");
    }

    return m_file.backing.memory;
}

void IO::seek(long offset, SeekFrom whence)
{
    // To smooth over differences between Glk and standard I/O, don’t
    // allow seeking in append-only streams.
    if (m_mode == Mode::Append) {
        throw IOError();
    }

    switch (m_type) {
    case Type::StandardIO:
        if (std::fseek(m_file.stdio.get(), offset, whence == SeekFrom::Start ? SEEK_SET : whence == SeekFrom::Current ? SEEK_CUR : SEEK_END) != 0) {
            throw IOError();
        }
        return;
    case Type::Memory:
        // Negative offsets are unsupported because they aren’t used.
        if (offset < 0) {
            throw IOError();
        }

        if (whence == SeekFrom::Current) {
            // Overflow.
            if (LONG_MAX - offset < m_file.backing.offset) {
                throw IOError();
            }

            offset = m_file.backing.offset + offset;
        } else if (whence == SeekFrom::End) {
            // SeekFrom::End is only used to seek directly to the end.
            if (offset != 0) {
                throw IOError();
            }

            offset = m_file.backing.memory.size();
        } else if (whence == SeekFrom::Start) {
            // Do nothing; offset is where it should be.
        }

        // If seeking beyond the end, write zeros.
        while (offset > m_file.backing.memory.size()) {
            write8(0);
        }

        m_file.backing.offset = offset;

        return;
#ifdef ZTERP_GLK
    case Type::Glk:
        glk_stream_set_position(m_file.glk.get(), offset, whence == SeekFrom::Start ? seekmode_Start : whence == SeekFrom::Current ? seekmode_Current : seekmode_End);
        return; // glk_stream_set_position can’t signal failure
#endif
    default:
        bad_type();
    }
}

long IO::tell() const
{
    switch (m_type) {
    case Type::StandardIO: {
        auto offset = std::ftell(m_file.stdio.get());
        if (offset == -1) {
            throw IOError();
        }

        return offset;
    }
    case Type::Memory:
        return m_file.backing.offset;
#ifdef ZTERP_GLK
    case Type::Glk: {
        auto offset = glk_stream_get_position(m_file.glk.get());
#if LONG_MAX < UINT32_MAX
        if (offset > LONG_MAX) {
            throw IOError();
        }
#endif

        return offset;
    }
#endif
    default:
        bad_type();
    }
}

// read() and write() always operate in terms of bytes, not characters.
size_t IO::read(void *buf, size_t n)
{
    size_t total = 0;

    while (total < n) {
        size_t s;

        switch (m_type) {
        case Type::StandardIO:
            s = std::fread(buf, 1, n - total, m_file.stdio.get());
            break;
        case Type::Memory: {
            Backing *b = &m_file.backing;
            auto remaining = b->memory.size() - b->offset;

            if (m_mode != Mode::ReadOnly) {
                return 0;
            }

            s = remaining < n ? remaining : n;
            if (s != 0) {
                std::memcpy(buf, &b->memory[b->offset], s);
                b->offset += s;
            }

            break;
        }
#ifdef ZTERP_GLK
        case Type::Glk: {
            glui32 s32 = glk_get_buffer_stream(m_file.glk.get(), static_cast<char *>(buf), n - total);
            // This should only happen if m_file.glk is invalid.
            if (s32 == static_cast<glui32>(-1)) {
                s = 0;
            } else {
                s = s32;
            }

            break;
        }
#endif
        default:
            bad_type();
        }

        if (s == 0) {
            break;
        }
        total += s;
        buf = (static_cast<char *>(buf)) + s;
    }

    return total;
}

void IO::read_exact(void *buf, size_t n)
{
    if (read(buf, n) != n) {
        throw IOError();
    }
}

size_t IO::write(const void *buf, size_t n)
{
    if (n == 0) {
        return 0;
    }

    switch (m_type) {
    case Type::StandardIO: {
        size_t s, total = 0;

        while (total < n && (s = std::fwrite(buf, 1, n - total, m_file.stdio.get())) > 0) {
            total += s;
            buf = (static_cast<const char *>(buf)) + s;
        }

        return total;
    }
    case Type::Memory: {
        Backing *b = &m_file.backing;
        auto remaining = b->memory.size() - b->offset;

        if (m_mode != Mode::WriteOnly) {
            return 0;
        }

        if (n > remaining) {
            try {
                b->memory.resize(b->memory.size() + (n - remaining));
            } catch (const std::bad_alloc &) {
                throw IOError();
            }
        }

        std::memcpy(&b->memory[b->offset], buf, n);

        b->offset += n;

        return n;
    }
#ifdef ZTERP_GLK
    case Type::Glk:
        glk_put_buffer_stream(m_file.glk.get(), const_cast<char *>(static_cast<const char *>(buf)), n);
        return n; // glk_put_buffer_stream() can’t signal a short write
#endif
    default:
        bad_type();
    }
}

void IO::write_exact(const void *buf, size_t n)
{
    if (write(buf, n) != n) {
        throw IOError();
    }
}

uint8_t IO::read8()
{
    uint8_t v;

    read_exact(&v, sizeof v);

    return v;
}

uint16_t IO::read16()
{
    uint8_t buf[2];

    read_exact(buf, sizeof buf);

    return (static_cast<uint16_t>(buf[0]) << 8) | (static_cast<uint16_t>(buf[1]));
}

uint32_t IO::read32()
{
    uint8_t buf[4];

    read_exact(buf, sizeof buf);

    return ((static_cast<uint32_t>(buf[0])) << 24) |
           ((static_cast<uint32_t>(buf[1])) << 16) |
           ((static_cast<uint32_t>(buf[2])) <<  8) |
           ((static_cast<uint32_t>(buf[3]))      );
}

void IO::write8(uint8_t v)
{
    return write_exact(&v, sizeof v);
}

void IO::write16(uint16_t v)
{
    uint8_t buf[2];

    buf[0] = v >> 8;
    buf[1] = v & 0xff;

    return write_exact(buf, sizeof buf);
}

void IO::write32(uint32_t v)
{
    uint8_t buf[4];

    buf[0] = (v >> 24) & 0xff;
    buf[1] = (v >> 16) & 0xff;
    buf[2] = (v >>  8) & 0xff;
    buf[3] = (v >>  0) & 0xff;

    return write_exact(buf, sizeof buf);
}

// getc() and putc() are meant to operate in terms of characters, not
// bytes. That is, unlike C++, bytes and characters are not equivalent
// as far as Zterp’s I/O system is concerned.

// Read a UTF-8 character, returning it. If limit16 is true, any Unicode
// values which are greater than UINT16_MAX will be converted to the
// Unicode replacement character. Otherwise, values are returned as-is.
// -1 is returned on EOF.
//
// If an invalid UTF-8 sequence is found, the Unicode replacement
// character is returned.
long IO::getc(bool limit16)
{
    long ret;
    uint8_t c;
    class NotUnicode : public std::exception {};

    try {
        c = read8();
    } catch (const IOError &) {
        return -1;
    }

    // Read a byte and make sure it’s part of a valid UTF-8 sequence.
    auto read_byte = [this] {
        uint8_t b = read8();

        if ((b & 0xc0) != 0x80) {
            throw NotUnicode();
        } else {
            return b;
        }
    };

    try {
        if ((c & 0x80) == 0) { // One byte.
            ret = c;
        } else if ((c & 0xe0) == 0xc0) { // Two bytes.
            ret = (c & 0x1f) << 6;
            ret |= (read_byte() & 0x3f);
            if (ret < 0x80) {
                throw NotUnicode();
            }
        } else if ((c & 0xf0) == 0xe0) { // Three bytes.
            ret = (c & 0x0f) << 12;
            ret |= (read_byte() & 0x3f) << 6;
            ret |= (read_byte() & 0x3f);
            if (ret < 0x800) {
                throw NotUnicode();
            }
        } else if ((c & 0xf8) == 0xf0) { // Four bytes.
            ret = (static_cast<long>(c) & 0x07) << 18;
            ret |= (read_byte() & 0x3f) << 12;
            ret |= (read_byte() & 0x3f) << 6;
            ret |= (read_byte() & 0x3f);
            if (ret < 0x10000) {
                throw NotUnicode();
            }
        } else { // Invalid value.
            ret = UNICODE_REPLACEMENT;
        }
    } catch (const IOError &) {
        return -1;
    } catch (const NotUnicode &) {
        return UNICODE_REPLACEMENT;
    }

    if (ret > (limit16 ? UINT16_MAX : 0x10ffff)) {
        ret = UNICODE_REPLACEMENT;
    }

    if (textmode() && ret == '\n') {
        ret = UNICODE_LINEFEED;
    }

    return ret;
}

// Write a Unicode character as UTF-8. If this fails it may write a
// partial character.
void IO::putc(uint32_t c)
{
    if (textmode() && c == UNICODE_LINEFEED) {
        c = '\n';
    }

    if (c >= 0x110000) {
        c = UNICODE_REPLACEMENT;
    }

    if (c < 0x80) {
        write8(c);
    } else if (c < 0x800) {
        write8(0xc0 | ((c >> 6) & 0x1f));
        write8(0x80 | ((c     ) & 0x3f));
    } else if (c < 0x10000) {
        write8(0xe0 | ((c >> 12) & 0x0f));
        write8(0x80 | ((c >>  6) & 0x3f));
        write8(0x80 | ((c      ) & 0x3f));
    } else if (c < 0x110000) {
        write8(0xf0 | ((c >> 18) & 0x07));
        write8(0x80 | ((c >> 12) & 0x3f));
        write8(0x80 | ((c >>  6) & 0x3f));
        write8(0x80 | ((c      ) & 0x3f));
    }
}

// Read and return a line, where a line is defined as a collection of
// characters terminated by a Unicode linefeed (0xa). If EOF is
// encountered at any point (including after characters have been read,
// but before a linefeed), EndOfFile is thrown, which means that all
// lines, including the last one, must end in a linefeed. If the last
// line does not end in a linefeed, its characters will be read, but
// ultimately discarded, so can be considered lost and unrecoverable.
std::vector<uint16_t> IO::readline()
{
    std::vector<uint16_t> result;

    while (true) {
        long c = getc(true);

        if (c == -1) {
            throw EndOfFile();
        }

        if (c == UNICODE_LINEFEED) {
            break;
        }

        result.push_back(c);
    }

    return result;
}

long IO::filesize() const
{
    switch (m_type) {
    case Type::StandardIO:
        if (!textmode()) {
            return zterp_os_filesize(m_file.stdio.get());
        }
        break;
    case Type::Memory:
        return m_file.backing.memory.size();
#ifdef ZTERP_GLK
    case Type::Glk: {
        auto orig = glk_stream_get_position(m_file.glk.get());
        glk_stream_set_position(m_file.glk.get(), 0, seekmode_End);
        long size = glk_stream_get_position(m_file.glk.get());
        glk_stream_set_position(m_file.glk.get(), orig, seekmode_Start);
        return size;
    }
#endif
    }

    return -1;
}

void IO::flush()
{
    if (m_type == Type::StandardIO && (m_mode == Mode::WriteOnly || m_mode == Mode::Append)) {
        std::fflush(m_file.stdio.get());
    }
}
