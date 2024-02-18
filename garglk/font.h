#ifndef GARGLK_FONT_H
#define GARGLK_FONT_H

#include <string>
#include <unordered_map>

#include "optional.hpp"

#include "garglk.h"

class FontFiller {
public:
    enum class Style {
        Regular,
        Bold,
        Italic,
        BoldItalic
    };

    explicit FontFiller(FontType type) :
        m_type(type)
    {
    }

    void add(Style style, nonstd::optional<std::string> path) {
        m_fonts.insert({style, std::move(path)});
    }

    void fill() {
        const auto &regular = m_fonts[Style::Regular];
        if (!regular.has_value()) {
            return;
        }

        FontFiles &files = m_type == FontType::Monospace ? gli_conf_mono
                                                         : gli_conf_prop;

        files.r = *regular;
        files.b = *regular;
        files.i = *regular;
        files.z = *regular;

        const auto &bold = m_fonts[Style::Bold];
        if (bold.has_value()) {
            files.b = *bold;
            files.z = *bold;
        };

        const auto &italic = m_fonts[Style::Italic];
        if (italic.has_value()) {
            files.i = *italic;
            files.z = *italic;
        }

        const auto &bolditalic = m_fonts[Style::BoldItalic];
        if (bolditalic.has_value()) {
            files.z = *bolditalic;
        }
    }

private:
    FontType m_type;
    std::unordered_map<Style, nonstd::optional<std::string>> m_fonts;
};

#endif
