#include <array>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>

#include <dirent.h>
#include <sys/types.h>

#define JSON_DIAGNOSTICS 1
#include "json.hpp"
#include "optional.hpp"

#include "garglk.h"

using json = nlohmann::json;

using garglk::Color;

template<size_t...Is, typename T>
std::array<T, sizeof...(Is)> make_array(const T &value, std::index_sequence<Is...>) {
    return {(static_cast<void>(Is), value)...};
}

template<size_t N, typename T>
std::array<T, N> make_array(const T &value) {
    return make_array(value, std::make_index_sequence<N>());
}

template<typename K, typename V>
static std::set<K> map_keys(const std::map<K, V> &map)
{
    std::set<K> keys;

    for (const auto &pair : map)
        keys.insert(pair.first);

    return keys;
}

const std::regex Color::m_color_re("#?[a-fA-F0-9]{6}");

void Color::to(unsigned char *rgb) const {
    rgb[0] = m_red;
    rgb[1] = m_green;
    rgb[2] = m_blue;
}

Color Color::from(const unsigned char *rgb) {
    return {rgb[0], rgb[1], rgb[2]};
}

Color Color::from(const std::string &str) {
    std::string r, g, b;

    if (!std::regex_match(str, m_color_re))
        throw std::runtime_error("invalid color: " + str);

    int pos = str[0] == '#' ? 1 : 0;

    r = str.substr(pos + 0, 2);
    g = str.substr(pos + 2, 2);
    b = str.substr(pos + 4, 2);

    return Color(std::stoul(r, nullptr, 16),
                 std::stoul(g, nullptr, 16),
                 std::stoul(b, nullptr, 16));
}

static const Color black = Color{0x00, 0x00, 0x00};
static const Color white = Color{0xff, 0xff, 0xff};
static const Color gray = Color{0x60, 0x60, 0x60};

struct ColorPair {
    Color fg;
    Color bg;
};

template <typename Iterable, typename DType>
std::string join(const Iterable &values, const DType &delim)
{
    auto it = values.begin();
    std::stringstream result;

    if (it != values.end())
        result << *it++;

    for (; it != values.end(); ++it)
    {
        result << delim;
        result << *it;
    }

    return result.str();
}

struct Styles {
    std::array<ColorPair, style_NUMSTYLES> colors;

    void to(style_t *styles) const {
        for (int i = 0; i < style_NUMSTYLES; i++)
        {
            colors[i].fg.to(styles[i].fg);
            colors[i].bg.to(styles[i].bg);
        }
    }

    static Styles from(const style_t *styles) {
        auto colors = make_array<style_NUMSTYLES>(ColorPair{white, black});

        for (int i = 0; i < style_NUMSTYLES; i++)
        {
            colors[i].fg = Color::from(styles[i].fg);
            colors[i].bg = Color::from(styles[i].bg);
        }

        return {colors};
    }
};

struct Theme {
    std::string name;
    Color windowcolor;
    Color bordercolor;
    Color caretcolor;
    Color linkcolor;
    Color morecolor;
    Styles tstyles;
    Styles gstyles;

    void apply() const {
        windowcolor.to(gli_window_color);
        windowcolor.to(gli_window_save);
        bordercolor.to(gli_border_color);
        bordercolor.to(gli_border_save);
        caretcolor.to(gli_caret_color);
        caretcolor.to(gli_caret_save);
        linkcolor.to(gli_link_color);
        linkcolor.to(gli_link_save);
        morecolor.to(gli_more_color);
        morecolor.to(gli_more_save);
        tstyles.to(gli_tstyles);
        gstyles.to(gli_gstyles);
    }

    static Theme from_file(const std::string &filename) {
        std::ifstream f(filename);
        if (!f.is_open())
            throw std::runtime_error("unable to open file");

        json j;
        f >> j;

        auto window = Color::from(j.at("window"));
        auto border = Color::from(j.at("border"));
        auto caret = Color::from(j.at("caret"));
        auto link = Color::from(j.at("link"));
        auto more = Color::from(j.at("more"));
        auto text_buffer = get_user_styles(j, "text_buffer");
        auto text_grid = get_user_styles(j, "text_grid");

        return {
            j.at("name"),
            window,
            border,
            caret,
            link,
            more,
            text_buffer,
            text_grid,
        };
    }

private:
    static Styles get_user_styles(const json &j, const std::string &color)
    {
        std::array<nonstd::optional<ColorPair>, style_NUMSTYLES> possible_colors;
        std::map<std::string, json> styles = j.at(color);

        static std::map<std::string, int> stylemap = {
            {"normal", 0},
            {"emphasized", 1},
            {"preformatted", 2},
            {"header", 3},
            {"subheader", 4},
            {"alert", 5},
            {"note", 6},
            {"blockquote", 7},
            {"input", 8},
            {"user1", 9},
            {"user2", 10},
        };

        auto parse_colors = [&possible_colors](const json &style, int i) {
            auto fg = Color::from(style.at("fg"));
            auto bg = Color::from(style.at("bg"));
            possible_colors[i] = ColorPair{fg, bg};
        };

        if (styles.find("default") != styles.end())
            for (int i = 0; i < style_NUMSTYLES; i++)
                parse_colors(styles["default"], i);

        styles.erase("default");
        for (const auto &style : styles)
        {
            if (stylemap.find(style.first) == stylemap.end())
                throw std::runtime_error("invalid style in " + color + ": " + style.first);

            parse_colors(style.second, stylemap[style.first]);
        }

        auto colors = make_array<style_NUMSTYLES>(ColorPair{white, black});
        std::vector<std::string> missing;
        for (const auto &s : stylemap)
        {
            if (possible_colors[s.second].has_value())
                colors[s.second] = possible_colors[s.second].value();
            else
                missing.push_back(s.first);
        }

        if (!missing.empty())
            throw std::runtime_error(color + " is missing the following styles: " + join(missing, ", "));

        return {colors};
    }
};

static Theme light{
    "light",
    white,
    black,
    black,
    Color{0x00, 0x00, 0x60},
    Color{0x00, 0x60, 0x00},
    Styles{
        ColorPair{black, white}, ColorPair{black, white},
        ColorPair{black, white}, ColorPair{black, white},
        ColorPair{black, white}, ColorPair{black, white},
        ColorPair{black, white}, ColorPair{black, white},
        ColorPair{Color{0x00, 0x60, 0x00}, white}, ColorPair{black, white},
        ColorPair{black, white},
    },
    Styles{make_array<style_NUMSTYLES>(ColorPair{gray, white})},
};

static const Color darkfg = Color{0xe7, 0xe8, 0xe9};
static const Color darkbg = Color{0x31, 0x36, 0x3b};

static Theme dark{
    "dark",
    Color{0x31, 0x36, 0x3b},
    black,
    Color{0xe7, 0xe8, 0xe9},
    Color{0x00, 0x00, 0x60},
    Color{0x00, 0xcc, 0x00},
    Styles{make_array<style_NUMSTYLES>(ColorPair{darkfg, darkbg})},
    Styles{make_array<style_NUMSTYLES>(ColorPair{darkfg, darkbg})},
};

static std::map<std::string, Theme> themes = {
    {light.name, light},
    {dark.name, dark},
};

std::vector<std::string> garglk::theme::paths()
{
    std::vector<std::string> theme_paths;

    for (const auto &path : garglk::winappdata())
        theme_paths.push_back(path + "/themes");

    return theme_paths;
}

void garglk::theme::init()
{
    for (const auto &themedir : garglk::theme::paths())
    {
        std::unique_ptr<DIR, std::function<int(DIR *)>> d(opendir(themedir.c_str()), closedir);
        if (d != nullptr)
        {
            dirent *de;

            while ((de = readdir(d.get())) != nullptr)
            {
                std::string basename = de->d_name;
                auto dot = basename.find_last_of('.');
                if (dot != std::string::npos && basename.substr(dot) == ".json")
                {
                    std::string filename = themedir + "/" + basename;
                    try
                    {
                        auto theme = Theme::from_file(filename);
                        // C++17: use insert_or_assign()
                        const auto result = themes.insert({theme.name, theme});
                        if (!result.second)
                            result.first->second = theme;
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "garglk: error parsing theme " << filename << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }
}

static std::string system_theme_name()
{
    return windark() ? dark.name : light.name;
}

void garglk::theme::set(std::string name)
{
    if (name == "system")
        name = system_theme_name();

    try
    {
        themes.at(name).apply();
    }
    catch (const std::out_of_range &)
    {
        std::cerr << "garglk: unknown theme: " << name << std::endl;
    }
}

std::vector<std::string> garglk::theme::names()
{
    std::vector<std::string> theme_names;

    for (const auto &theme : themes)
        theme_names.push_back(theme.first);

    theme_names.push_back(std::string("system (") + system_theme_name() + ")");

    return theme_names;
}
