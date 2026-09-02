#ifndef GARGLK_FONT_H
#define GARGLK_FONT_H

#include <string>
#include <optional>
#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "format.h"

#include "garglk.h"

namespace garglk {

inline std::string convert_ft_error(FT_Error err, const std::string &basemsg)
{
    // FT_Error_String() was introduced in FreeType 2.10.0.
#if FREETYPE_MAJOR == 2 && FREETYPE_MINOR < 10
    const char *errstr = nullptr;
#else
    // If FreeType was not built with FT_CONFIG_OPTION_ERROR_STRINGS,
    // this will always be null.
    const char *errstr = FT_Error_String(err);
#endif

    if (errstr == nullptr) {
        return Format("{} (error code {})", basemsg, err);
    } else {
        return Format("{}: {}", basemsg, errstr);
    }
}

}

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

    void add(Style style, std::optional<std::string> path) {
        m_fonts.insert({style, std::move(path)});
    }

    bool fill() {
        const auto &regular = m_fonts[Style::Regular];
        if (!regular.has_value()) {
            return false;
        }

        FontFiles &files = m_type == FontType::Monospace ? gli_conf_mono
                                                         : gli_conf_prop;

        files.r.base = *regular;
        files.b.base = *regular;
        files.i.base = *regular;
        files.z.base = *regular;

        const auto &bold = m_fonts[Style::Bold];
        if (bold.has_value()) {
            files.b.base = *bold;
            files.z.base = *bold;
        };

        const auto &italic = m_fonts[Style::Italic];
        if (italic.has_value()) {
            files.i.base = *italic;
            files.z.base = *italic;
        }

        const auto &bolditalic = m_fonts[Style::BoldItalic];
        if (bolditalic.has_value()) {
            files.z.base = *bolditalic;
        }

        return true;
    }

private:
    FontType m_type;
    std::unordered_map<Style, std::optional<std::string>> m_fonts;
};

#endif
