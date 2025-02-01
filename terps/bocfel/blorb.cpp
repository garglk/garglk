// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "blorb.h"
#include "iff.h"
#include "io.h"
#include "types.h"

static constexpr uint32_t PICT = 0x50696374;
static constexpr uint32_t SND  = 0x536e6420;
static constexpr uint32_t EXEC = 0x45786563;
static constexpr uint32_t DATA = 0x44617461;

Blorb::Blorb(const std::shared_ptr<IO> &io)
{
    uint32_t size;
    uint32_t nresources;
    std::unique_ptr<IFF> iff;

    try {
        iff = std::make_unique<IFF>(io, IFF::TypeID("IFRS"));
    } catch (const IFF::InvalidFile &) {
        throw InvalidFile();
    }

    if (!iff->find(IFF::TypeID("RIdx"), size)) {
        throw InvalidFile();
    }

    try {
        nresources = iff->io()->read32();

        if ((nresources * 12) + 4 != size) {
            throw InvalidFile();
        }

        for (uint32_t i = 0; i < nresources; i++) {
            uint32_t usage_, number, start, type_;
            Usage usage;
            long saved;

            usage_ = iff->io()->read32();
            number = iff->io()->read32();
            start = iff->io()->read32();

            try {
                static const std::unordered_map<uint32_t, Usage> usages = {
                    {PICT, Usage::Pict},
                    {SND,  Usage::Snd},
                    {EXEC, Usage::Exec},
                    {DATA, Usage::Data},
                };

                usage = usages.at(usage_);
            } catch (const std::out_of_range &) {
                continue;
            }

            saved = iff->io()->tell();
            iff->io()->seek(start, IO::SeekFrom::Start);
            type_ = iff->io()->read32();
            size = iff->io()->read32();
            iff->io()->seek(saved, IO::SeekFrom::Start);

            IFF::TypeID type(type_);

            if (type == IFF::TypeID("FORM")) {
                start -= 8;
                size += 8;
            }

            Chunk chunk(type, start + 8, size);
            m_chunks.insert({{usage, number}, chunk});
        }
    } catch (const IO::IOError &) {
        throw InvalidFile();
    }
}

const Blorb::Chunk *Blorb::find(Blorb::Usage usage, uint32_t number)
{
    try {
        return &m_chunks.at({usage, number});
    } catch (const std::out_of_range &) {
        return nullptr;
    }
}
