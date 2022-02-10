// vim: set ft=cpp:

#ifndef ZTERP_BLORB_H
#define ZTERP_BLORB_H

#include <exception>
#include <memory>
#include <vector>

#include "iff.h"
#include "io.h"
#include "types.h"

class Blorb {
public:
    class InvalidFile : public std::exception {
    };

    enum class Usage {
        Pict,
        Snd,
        Exec,
        Data,
    };

    struct Chunk {
        Chunk(Usage usage_, uint32_t number_, IFF::TypeID type_, uint32_t offset_, uint32_t size_) :
            usage(usage_), number(number_), type(type_), offset(offset_), size(size_)
        {
        }

        const Usage usage;
        const uint32_t number;
        const IFF::TypeID type;
        const uint32_t offset;
        const uint32_t size;
    };

    explicit Blorb(const std::shared_ptr<IO> &io);
    const Chunk *find(Usage usage, uint32_t number);

private:
    std::vector<Chunk> m_chunks;
};

#endif
