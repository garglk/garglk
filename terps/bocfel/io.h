// vim: set ft=cpp:

#ifndef ZTERP_IO_H
#define ZTERP_IO_H

#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "types.h"

class IO {
public:
    class Error : public std::exception {
    };

    class OpenError : public Error {
    };

    class IOError : public Error {
    };

    class EndOfFile : public Error {
    };

    enum class Mode {
        ReadOnly,
        WriteOnly,
        Append,
    };

    enum class Purpose {
        Data,
        Save,
        Transcript,
        Input,
    };

    enum class SeekFrom {
        Start,
        End,
        Current,
    };

    IO(const std::string *filename, Mode mode, Purpose purpose);
    IO(std::vector<uint8_t> buf, Mode mode);
    void operator=(IO const &) = delete;
    IO(const IO &) = delete;
    IO(IO &&) = default;

    static IO standard_in() {
        return IO(stdin, Mode::ReadOnly, Purpose::Input, false);
    }

    static IO standard_out() {
        return IO(stdout, Mode::WriteOnly, Purpose::Transcript, false);
    }

    const std::vector<uint8_t> &get_memory() const;

    void seek(long offset, SeekFrom whence);
    long tell() const;
    size_t read(void *buf, size_t n);
    void read_exact(void *buf, size_t n);
    size_t write(const void *buf, size_t n);
    void write_exact(const void *buf, size_t n);
    uint8_t read8();
    uint16_t read16();
    uint32_t read32();
    void write8(uint8_t v);
    void write16(uint16_t v);
    void write32(uint32_t v);
    long getc(bool limit16);
    void putc(uint32_t c);
    std::vector<uint16_t> readline();
    long filesize() const;
    void flush();

private:
    struct Backing {
        std::vector<uint8_t> memory;
        long offset = 0;

        Backing() = default;

        explicit Backing(std::vector<uint8_t> buf) : memory(std::move(buf)) {
        }
    };

    enum class Type {
        StandardIO,
        Memory,
#ifdef ZTERP_GLK
        Glk,
#endif
    };

    using StdioType = std::unique_ptr<std::FILE, std::function<void(std::FILE *)>>;

#ifdef ZTERP_GLK
    using GlkType = std::unique_ptr<std::remove_pointer_t<strid_t>, std::function<void(strid_t)>>;
#endif

    struct File {
        StdioType stdio;
        Backing backing;
#ifdef ZTERP_GLK
        GlkType glk;
#endif

        File() = default;

        File(std::FILE *fp, bool close) : stdio(StdioType(fp, [close](std::FILE *fp_) { if (close) { std::fclose(fp_); } })) {
        }

        explicit File(std::vector<uint8_t> buf) : backing(std::move(buf)) {
        }

#ifdef ZTERP_GLK
        explicit File(strid_t stream) : glk(GlkType(stream, [](strid_t str) { glk_stream_close(str, nullptr); })) {
        }
#endif
    };

    IO(std::FILE *fp, Mode mode, Purpose purpose, bool close) : m_file(fp, close), m_type(Type::StandardIO), m_mode(mode), m_purpose(purpose) {
    }

    [[noreturn]] void bad_type() const;
    bool textmode() const;

#ifdef ZTERP_GLK
    void open_as_glk(const std::function<frefid_t(glui32 usage, glui32 filemode)> &create_fref);
#endif

    File m_file;
    Type m_type;
    Mode m_mode;
    Purpose m_purpose;
};

#endif
