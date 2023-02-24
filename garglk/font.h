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

        std::string *r, *b, *i, *z;
        if (m_type == FontType::Monospace) {
            r = &gli_conf_mono.r;
            b = &gli_conf_mono.b;
            i = &gli_conf_mono.i;
            z = &gli_conf_mono.z;
        } else {
            r = &gli_conf_prop.r;
            b = &gli_conf_prop.b;
            i = &gli_conf_prop.i;
            z = &gli_conf_prop.z;
        }

        *r = *regular;
        *b = *regular;
        *i = *regular;
        *z = *regular;

        const auto &bold = m_fonts[Style::Bold];
        if (bold.has_value()) {
            *b = *bold;
            *z = *bold;
        };

        const auto &italic = m_fonts[Style::Italic];
        if (italic.has_value()) {
            *i = *italic;
            *z = *italic;
        }

        const auto &bolditalic = m_fonts[Style::BoldItalic];
        if (bolditalic.has_value()) {
            *z = *bolditalic;
        }
    }

private:
    FontType m_type;
    std::unordered_map<Style, nonstd::optional<std::string>> m_fonts;
};
