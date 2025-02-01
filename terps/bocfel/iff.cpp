// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <utility>

#include "iff.h"
#include "io.h"
#include "types.h"

IFF::IFF(std::shared_ptr<IO> io, TypeID type) : m_io(std::move(io))
{
    try {
        uint32_t type_val;

        m_io->seek(0, IO::SeekFrom::Start);
        type_val = m_io->read32();
        if (type_val != TypeID("FORM").val()) {
            throw InvalidFile();
        }

        m_io->seek(4, IO::SeekFrom::Current);
        type_val = m_io->read32();
        if (type_val != type.val()) {
            throw InvalidFile();
        }

        while (true) {
            try {
                type_val = m_io->read32();
            } catch (const IO::IOError &) {
                // IOError here will mean EOF, which is not an error; it
                // just means to stop parsing the file.
                break;
            }

            auto size = m_io->read32();
            auto offset = m_io->tell();

            m_entries.emplace_back(TypeID(type_val), offset, size);

            if ((size & 1) == 1) {
                size++;
            }

            m_io->seek(size, IO::SeekFrom::Current);
        }
    } catch (const IO::IOError &) {
        throw InvalidFile();
    }
}

bool IFF::find(TypeID type_id, uint32_t &size)
{
    auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &entry){ return entry.type == type_id; });
    if (it == m_entries.end()) {
        return false;
    } else {
        try {
            m_io->seek(it->offset, IO::SeekFrom::Start);
        } catch (const IO::IOError &) {
            return false;
        }
        size = it->size;
        return true;
    }
}
