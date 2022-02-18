// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

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
        if (type_val != TypeID(&"FORM").val()) {
            throw InvalidFile();
        }

        m_io->seek(4, IO::SeekFrom::Current);
        type_val = m_io->read32();
        if (type_val != type.val()) {
            throw InvalidFile();
        }

        while (true) {
            uint32_t size;

            try {
                type_val = m_io->read32();
            } catch (const IO::IOError &) {
                // IOError here will mean EOF, which is not an error; it
                // just means to stop parsing the file.
                break;
            }

            size = m_io->read32();

            auto entry = Entry(TypeID(type_val), m_io->tell(), size);
            m_entries.push_back(entry);

            if (entry.offset == -1) {
                throw InvalidFile();
            }

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
