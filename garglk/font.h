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
