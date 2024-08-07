// vim: set ft=cpp:

#ifndef ZTERP_BLORB_H
#define ZTERP_BLORB_H

#include <exception>
#include <map>
#include <memory>
#include <utility>

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
        Chunk(IFF::TypeID type_, uint32_t offset_, uint32_t size_) :
            type(type_), offset(offset_), size(size_)
        {
        }

        const IFF::TypeID type;
        const uint32_t offset;
        const uint32_t size;
    };

    explicit Blorb(const std::shared_ptr<IO> &io);
    const Chunk *find(Usage usage, uint32_t number);

private:
    std::map<std::pair<Usage, uint32_t>, Chunk> m_chunks;
};

#endif
