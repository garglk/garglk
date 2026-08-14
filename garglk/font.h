#ifndef GARGLK_FONT_H
#define GARGLK_FONT_H

#include <string>
#include <optional>
#include <unordered_map>

#include "garglk.h"

class FontFiller {
public:
    enum class Style {
        Regular,
        Bold,
        Italic,
        BoldItalic
    };

    FontFiller() = default;

    void add(Style style, std::optional<std::string> path) {
        m_fonts.insert({style, std::move(path)});
    }

    // Build FontFiles from collected style paths. Bold/italic/z fall
    // back to regular when missing so FreeType can synthesize styles.
    std::optional<FontFiles> files() const {
        const auto &regular = m_fonts.find(Style::Regular);
        if (regular == m_fonts.end() || !regular->second.has_value()) {
            return std::nullopt;
        }

        FontFiles out;
        out.r.base = *regular->second;
        out.b.base = *regular->second;
        out.i.base = *regular->second;
        out.z.base = *regular->second;

        auto style_path = [this](Style style) -> const std::optional<std::string> * {
            auto it = m_fonts.find(style);
            if (it == m_fonts.end() || !it->second.has_value()) {
                return nullptr;
            }
            return &it->second;
        };

        if (const auto *bold = style_path(Style::Bold)) {
            out.b.base = **bold;
            out.z.base = **bold;
        }

        if (const auto *italic = style_path(Style::Italic)) {
            out.i.base = **italic;
            out.z.base = **italic;
        }

        if (const auto *bolditalic = style_path(Style::BoldItalic)) {
            out.z.base = **bolditalic;
        }

        return out;
    }

private:
    std::unordered_map<Style, std::optional<std::string>> m_fonts;
};

#endif
