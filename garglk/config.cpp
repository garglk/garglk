/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if __cplusplus >= 201703L
#include <filesystem>
#endif

#ifdef _WIN32
#include <shlwapi.h>
#else
#include <fnmatch.h>
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

#include "glk.h"
#include "glkstart.h"
#include "garglk.h"

#include GARGLKINI_H

float gli_backingscalefactor = 1.0f;
float gli_zoom = 1.0f;

FontFiles gli_conf_prop, gli_conf_mono, gli_conf_prop_override, gli_conf_mono_override;

std::string gli_conf_monofont = "Gargoyle Mono";
std::string gli_conf_propfont = "Gargoyle Serif";
float gli_conf_monosize = 12.6;	/* good size for Gargoyle Mono */
float gli_conf_propsize = 14.7;	/* good size for Gargoyle Serif */

Styles gli_tstyles{{
    {FontFace::propr(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Normal */
    {FontFace::propi(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Emphasized */
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Preformatted */
    {FontFace::propb(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Header */
    {FontFace::propb(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Subheader */
    {FontFace::propz(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Alert */
    {FontFace::propi(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Note */
    {FontFace::propr(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* BlockQuote */
    {FontFace::propb(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* Input */
    {FontFace::propr(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* User1 */
    {FontFace::propr(), Color(0xff,0xff,0xff), Color(0x00,0x00,0x00), false}, /* User2 */
}};

Styles gli_gstyles{{
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Normal */
    {FontFace::monoi(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Emphasized */
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Preformatted */
    {FontFace::monob(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Header */
    {FontFace::monob(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Subheader */
    {FontFace::monoz(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Alert */
    {FontFace::monoi(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Note */
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* BlockQuote */
    {FontFace::monob(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* Input */
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* User1 */
    {FontFace::monor(), Color(0xff,0xff,0xff), Color(0x60,0x60,0x60), false}, /* User2 */
}};

const Styles gli_tstyles_def = gli_tstyles;
const Styles gli_gstyles_def = gli_gstyles;

std::vector<garglk::ConfigFile> garglk::all_configs;

static FontFace font2idx(const std::string &font)
{
    if (font == "monor") return FontFace::monor();
    if (font == "monob") return FontFace::monob();
    if (font == "monoi") return FontFace::monoi();
    if (font == "monoz") return FontFace::monoz();
    if (font == "propr") return FontFace::propr();
    if (font == "propb") return FontFace::propb();
    if (font == "propi") return FontFace::propi();
    if (font == "propz") return FontFace::propz();
    return FontFace::monor();
}

float gli_conf_gamma = 1.0;

Color gli_window_color(0xff, 0xff, 0xff);
Color gli_caret_color(0x00, 0x00, 0x00);
Color gli_border_color(0x00, 0x00, 0x00);
Color gli_more_color(0x00, 0x60, 0x00);
Color gli_link_color(0x00, 0x00, 0x60);

Color gli_window_save(0xff, 0xff, 0xff);
Color gli_caret_save(0x00, 0x00, 0x00);
Color gli_border_save(0x00, 0x00, 0x00);
Color gli_more_save(0x00, 0x60, 0x00);
Color gli_link_save(0x00, 0x00, 0x60);

bool gli_override_fg_set = false;
Color gli_override_fg_val(0x00, 0x00, 0x00);
bool gli_override_bg_set = false;
Color gli_override_bg_val(0x00, 0x00, 0x00);
bool gli_override_reverse = false;

static std::string base_more_prompt = "— more —";
std::vector<glui32> gli_more_prompt;
glui32 gli_more_prompt_len;
int gli_more_align = 0;
FontFace gli_more_font = FontFace::propb();

Color gli_scroll_bg(0xb0, 0xb0, 0xb0);
Color gli_scroll_fg(0x80, 0x80, 0x80);
int gli_scroll_width = 0;

int gli_caret_shape = 2;
int gli_link_style = 1;

bool gli_conf_lcd = true;
std::array<unsigned char, 5> gli_conf_lcd_weights = {28, 56, 85, 56, 28};

int gli_wmarginx = 20;
int gli_wmarginy = 20;
int gli_wpaddingx = 0;
int gli_wpaddingy = 0;
int gli_wborderx = 0;
int gli_wbordery = 0;
int gli_tmarginx = 0;
int gli_tmarginy = 7;

int gli_wmarginx_save = 15;
int gli_wmarginy_save = 15;

int gli_cols = 60;
int gli_rows = 25;

bool gli_conf_lockcols = false;
bool gli_conf_lockrows = false;

bool gli_conf_save_window_size = false;
bool gli_conf_save_window_location = false;

float gli_conf_propaspect = 1.0;
float gli_conf_monoaspect = 1.0;

int gli_baseline = 15;
int gli_leading = 20;

bool gli_conf_justify = false;
int gli_conf_quotes = 1;
int gli_conf_dashes = 1;
int gli_conf_spaces = 1;
bool gli_conf_caps = false;

bool gli_conf_graphics = true;
bool gli_conf_sound = true;
bool gli_conf_speak = false;
bool gli_conf_speak_input = false;
std::string gli_conf_speak_language;

bool gli_conf_fullscreen = false;

bool gli_conf_stylehint = true;
bool gli_conf_safeclicks = false;

std::string garglk::downcase(const std::string &string)
{
    std::string lowered;

    for (unsigned char c : string)
        lowered.push_back(std::tolower(c));

    return lowered;
}

static void parsecolor(const std::string &str, Color &rgb)
{
    try
    {
        rgb = gli_parse_color(str);
    }
    catch (const std::runtime_error &)
    {
    }
}

// Return a vector of all possible config files. This is in order of
// highest priority to lowest (i.e. earlier entries should take
// precedence over later entries).
//
// The following is the list of locations tried:
//
// 1. Name of game file with extension replaced by .ini (e.g. zork1.z3
//    becomes zork1.ini)
// 2. <directory containing game file>/garglk.ini
// 3. Platform-specific locations:
//        $XDG_CONFIG_HOME/garglk.ini or $HOME/.config/garglk.ini (Unix only)
//        $HOME/.garglkrc (Unix only)
//        <user settings directory>/Gargoyle (Haiku only)
//        %APPDATA%/Gargoyle/garglk.ini (Windows only)
//        <current directory>/garglk.ini (Windows only)
//        $HOME/garglk.ini (macOS only)
// 4. $GARGLK_RESOURCES/garglk.ini (macOS only)
// 5. /etc/garglk.ini (or other location set at build time, Unix only)
// 6. <directory containing gargoyle executable>/garglk.ini (Windows only)
//
// exedir is the directory containing the gargoyle executable
// gamepath is the path to the game file being run
std::vector<garglk::ConfigFile> garglk::configs(const std::string &exedir = "", const std::string &gamepath = "")
{
    std::vector<ConfigFile> configs;
    if (!gamepath.empty())
    {
        std::string config;

        // game file .ini
        config = gamepath;
        auto dot = config.rfind('.');
        if (dot != std::string::npos)
            config.replace(dot, std::string::npos, ".ini");
        else
            config += ".ini";

        configs.emplace_back(config, false);

        // game directory .ini
        config = gamepath;
        auto slash = config.find_last_of("/\\");
        if (slash != std::string::npos)
            config.replace(slash + 1, std::string::npos, "garglk.ini");
        else
            config = "garglk.ini";

        configs.emplace_back(config, false);
    }

#if defined(__HAIKU__)
    char settings_dir[PATH_MAX + 1];
    if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settings_dir, sizeof settings_dir) == B_OK)
        configs.push_back(ConfigFile(std::string(settings_dir) + "/Gargoyle", true));
#elif defined(_WIN32)
    // $APPDATA/Gargoyle/garglk.ini (Windows only). This has a higher
    // priority than $PWD/garglk.ini since it's a more "proper" location.
    const char *appdata = std::getenv("APPDATA");
    if (appdata != nullptr)
        configs.push_back(ConfigFile(std::string(appdata) + "/Gargoyle/garglk.ini", true));

    // current directory .ini
    // Historically this has been the location of garglk.ini on Windows,
    // so treat it as a user config there.
    configs.push_back(ConfigFile("garglk.ini", true));
#else

    const char *home = getenv("HOME");

#ifdef __APPLE__
    // On macOS, when the config file is edited via the UI, it is placed in
    // $HOME/garglk.ini. At some point this probably should move to somewhere in
    // $HOME/Library, but for now, make sure this config file is used.
    if (home != nullptr)
        configs.push_back(ConfigFile(std::string(home) + "/garglk.ini", true));
#endif

    // XDG Base Directory Specification
    std::string xdg_path;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr)
        xdg_path = xdg;
    else if (home != nullptr)
        xdg_path = std::string(home) + "/.config";

    if (!xdg_path.empty())
        configs.emplace_back(xdg_path + "/garglk.ini", true);

    // $HOME/.garglkrc
    if (home != nullptr)
        configs.emplace_back(std::string(home) + "/.garglkrc", true);
#endif

#ifdef __APPLE__
    // macOS sets $GARGLK_RESOURCES to the bundle directory containing a
    // default garglk.ini.
    const char *garglkini = std::getenv("GARGLK_RESOURCES");
    if (garglkini != nullptr)
        configs.push_back(ConfigFile(std::string(garglkini) + "/garglk.ini", false));
#endif

#ifdef GARGLKINI
    // system directory
    configs.emplace_back(GARGLKINI, false);
#endif

#ifdef _WIN32
    // install directory
    if (!exedir.empty())
        configs.push_back(ConfigFile(exedir + "/garglk.ini", false));
#endif

    return configs;
}

std::string garglk::user_config()
{
    auto cfgs = configs();

    // Filter out non-user configs.
    cfgs.erase(
            std::remove_if(cfgs.begin(),
                           cfgs.end(),
                           [](const ConfigFile &config) { return !config.user; }),
            cfgs.end());

    if (cfgs.empty())
        throw std::runtime_error("No valid configuration files found.");

    // Find first user config which already exists, if any.
    auto cfg = std::find_if(cfgs.begin(), cfgs.end(), [](const ConfigFile &config) {
        return std::ifstream(config.path).good();
    });

    if (cfg != cfgs.end())
        return cfg->path;

    // No config exists, so create the highest-priority config.
    auto path = cfgs.front().path;

    // If building with C++17, ensure the parent directory exists. This
    // is difficult to do portably before C++17, so just don't do it.
#if __cplusplus >= 201703L
    std::filesystem::path fspath(path);
    try
    {
        if (!fspath.parent_path().empty())
            std::filesystem::create_directories(fspath.parent_path());
    }
    catch (const std::runtime_error &e)
    {
        throw std::runtime_error("Unable to create parent directory for configuration file " + path + ": " + e.what());
    }
#endif

    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Unable to open configuration file " + path + " for writing.");

    f.write(reinterpret_cast<const char *>(garglkini), sizeof garglkini);

    if (f.bad())
    {
        std::remove(path.c_str());
        throw std::runtime_error("Error writing to configuration file " + path + ".");
    }

    return path;
}

void garglk::config_entries(const std::string &fname, bool accept_bare, const std::vector<std::string> &matches, std::function<void(const std::string &cmd, const std::string &arg)> callback)
{
    std::string line;
    bool accept = accept_bare;

    std::ifstream f(fname);
    if (!f.is_open())
        return;

    while (std::getline(f >> std::ws, line))
    {
        auto comment = line.find('#');
        if (comment != std::string::npos)
            line.erase(comment);
        line.erase(line.find_last_not_of(" \t\r") + 1);

        if (line.empty())
            continue;

        if (line[0] == '[' && line.back() == ']')
        {
            accept = std::any_of(matches.begin(), matches.end(),[&line](const std::string &match) {
                std::istringstream s(line.substr(1, line.size() - 2));
                std::string pattern;
                while (s >> pattern)
                {
#ifdef _WIN32
                    if (PathMatchSpec(garglk::downcase(match).c_str(), garglk::downcase(pattern).c_str()))
#else
                    if (fnmatch(garglk::downcase(pattern).c_str(), garglk::downcase(match).c_str(), 0) == 0)
#endif
                        return true;
                }
                return false;
            });
            continue;
        }

        if (!accept)
            continue;

        std::string cmd;
        std::istringstream linestream(line);

        if (linestream >> cmd)
        {
            std::string arg;
            std::getline(linestream >> std::ws, arg);

            if (linestream)
                callback(cmd, arg);
        }
    }
}

// Replace with std::clamp when moving to C++17.
template<typename T>
constexpr const T clamp(const T &v, const T &lo, const T &hi)
{
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

static void readoneconfig(const std::string &fname, const std::string &argv0, const std::string &gamefile)
{
    std::vector<std::string> matches = {argv0, gamefile};

    garglk::config_entries(fname, true, matches, [](const std::string &cmd, const std::string &arg) {
        if (cmd == "moreprompt") {
            base_more_prompt = arg;
        } else if (cmd == "morecolor") {
            parsecolor(arg, gli_more_color);
            parsecolor(arg, gli_more_save);
        } else if (cmd == "morefont") {
            gli_more_font = font2idx(arg);
        } else if (cmd == "morealign") {
            gli_more_align = clamp(std::stoi(arg), 0, 2);
        } else if (cmd == "monoaspect") {
            gli_conf_monoaspect = std::stof(arg);
        } else if (cmd == "propaspect") {
            gli_conf_propaspect = std::stof(arg);
        } else if (cmd == "monosize") {
            gli_conf_monosize = std::stof(arg);
        } else if (cmd == "monor") {
            gli_conf_mono_override.r = arg;
        } else if (cmd == "monob") {
            gli_conf_mono_override.b = arg;
        } else if (cmd == "monoi") {
            gli_conf_mono_override.i = arg;
        } else if (cmd == "monoz") {
            gli_conf_mono_override.z = arg;
        } else if (cmd == "monofont") {
            gli_conf_monofont = arg;
        } else if (cmd == "propsize") {
            gli_conf_propsize = std::stof(arg);
        } else if (cmd == "propr") {
            gli_conf_prop_override.r = arg;
        } else if (cmd == "propb") {
            gli_conf_prop_override.b = arg;
        } else if (cmd == "propi") {
            gli_conf_prop_override.i = arg;
        } else if (cmd == "propz") {
            gli_conf_prop_override.z = arg;
        } else if (cmd == "propfont") {
            gli_conf_propfont = arg;
        } else if (cmd == "leading") {
            gli_leading = std::max(1.0, std::round(std::stod(arg)));
        } else if (cmd == "baseline") {
            gli_baseline = std::max(0.0, std::round(std::stod(arg)));
        } else if (cmd == "rows") {
            gli_rows = std::max(1, std::stoi(arg));
        } else if (cmd == "cols") {
            gli_cols = std::max(1, std::stoi(arg));
        } else if (cmd == "minrows") {
            int r = std::max(0, std::stoi(arg));
            if (gli_rows < r)
                gli_rows = r;
        } else if (cmd ==  "maxrows") {
            int r = std::max(0, std::stoi(arg));
            if (gli_rows > r)
                gli_rows = r;
        } else if (cmd == "mincols") {
            int r = std::max(0, std::stoi(arg));
            if (gli_cols < r)
                gli_cols = r;
        } else if (cmd ==  "maxcols") {
            int r = std::max(0, std::stoi(arg));
            if (gli_cols > r)
                gli_cols = r;
        } else if (cmd == "lockrows") {
            gli_conf_lockrows = !!std::stoi(arg);
        } else if (cmd == "lockcols") {
            gli_conf_lockcols = !!std::stoi(arg);
        } else if (cmd == "save_window") {
            std::stringstream ss(arg);
            std::string entry;

            while (ss >> entry) {
                entry = garglk::downcase(entry);
                if (entry == "size") {
                    gli_conf_save_window_size = true;
                } else if (entry == "location") {
                    gli_conf_save_window_location = true;
                }
            }
        } else if (cmd == "wmarginx") {
            gli_wmarginx = std::max(0, std::stoi(arg));
            gli_wmarginx_save = gli_wmarginx;
        } else if (cmd == "wmarginy") {
            gli_wmarginy = std::max(0, std::stoi(arg));
            gli_wmarginy_save = gli_wmarginy;
        } else if (cmd == "wpaddingx") {
            gli_wpaddingx = std::max(0, std::stoi(arg));
        } else if (cmd == "wpaddingy") {
            gli_wpaddingy = std::max(0, std::stoi(arg));
        } else if (cmd == "wborderx") {
            gli_wborderx = std::max(0, std::stoi(arg));
        } else if (cmd == "wbordery") {
            gli_wbordery = std::max(0, std::stoi(arg));
        } else if (cmd == "tmarginx") {
            gli_tmarginx = std::max(0, std::stoi(arg));
        } else if (cmd == "tmarginy") {
            gli_tmarginy = std::max(0, std::stoi(arg));
        } else if (cmd == "gamma") {
            gli_conf_gamma = std::max(0.0, std::stod(arg));
        } else if (cmd == "caretcolor") {
            parsecolor(arg, gli_caret_color);
            parsecolor(arg, gli_caret_save);
        } else if (cmd == "linkcolor") {
            parsecolor(arg, gli_link_color);
            parsecolor(arg, gli_link_save);
        } else if (cmd == "bordercolor") {
            parsecolor(arg, gli_border_color);
            parsecolor(arg, gli_border_save);
        } else if (cmd == "windowcolor") {
            parsecolor(arg, gli_window_color);
            parsecolor(arg, gli_window_save);
        } else if (cmd == "lcd") {
            gli_conf_lcd = !!std::stoi(arg);
        } else if (cmd == "lcdfilter") {
            garglk::set_lcdfilter(arg);
        } else if (cmd == "lcdweights") {
            std::istringstream argstream(arg);
            int weight;
            std::vector<unsigned char> weights;

            while (argstream >> weight)
                weights.push_back(weight);

            if (weights.size() == gli_conf_lcd_weights.size())
                std::copy(weights.begin(), weights.end(), gli_conf_lcd_weights.begin());
        } else if (cmd == "caretshape") {
            gli_caret_shape = clamp(std::stoi(arg), 0, 4);
        } else if (cmd == "linkstyle") {
            gli_link_style = !!std::stoi(arg);
        } else if (cmd == "scrollwidth") {
            gli_scroll_width = std::max(0, std::stoi(arg));
        } else if (cmd == "scrollbg") {
            parsecolor(arg, gli_scroll_bg);
        } else if (cmd == "scrollfg") {
            parsecolor(arg, gli_scroll_fg);
        } else if (cmd == "justify") {
            gli_conf_justify = clamp(std::stoi(arg), 0, 1);
        } else if (cmd == "quotes") {
            gli_conf_quotes = clamp(std::stoi(arg), 0, 2);
        } else if (cmd == "dashes") {
            gli_conf_dashes = clamp(std::stoi(arg), 0, 2);
        } else if (cmd == "spaces") {
            gli_conf_spaces = clamp(std::stoi(arg), 0, 2);
        } else if (cmd == "caps") {
            gli_conf_caps = !!std::stoi(arg);
        } else if (cmd == "graphics") {
            gli_conf_graphics = !!std::stoi(arg);
        } else if (cmd == "sound") {
            gli_conf_sound = !!std::stoi(arg);
        } else if (cmd == "fullscreen") {
            gli_conf_fullscreen = !!std::stoi(arg);
        } else if (cmd == "zoom") {
            gli_zoom = std::max(0.1, std::stod(arg));
        } else if (cmd == "speak") {
            gli_conf_speak = !!std::stoi(arg);
        } else if (cmd == "speak_input") {
            gli_conf_speak_input = !!std::stoi(arg);
        } else if (cmd == "speak_language") {
            gli_conf_speak_language = arg;
        } else if (cmd == "stylehint") {
            gli_conf_stylehint = !!std::stoi(arg);
        } else if (cmd == "safeclicks") {
            gli_conf_safeclicks = !!std::stoi(arg);
        } else if (cmd == "theme") {
            garglk::theme::set(arg);
        } else if (cmd == "tcolor" || cmd == "gcolor") {
            std::istringstream argstream(arg);
            std::string style, fg, bg;

            if (argstream >> style >> fg >> bg)
            {
                Styles &styles = cmd[0] == 't' ? gli_tstyles : gli_gstyles;

                if (style == "*")
                {
                    for (int i = 0; i < style_NUMSTYLES; i++)
                    {
                        parsecolor(fg, styles[i].fg);
                        parsecolor(bg, styles[i].bg);
                    }
                }
                else
                {
                    int i = std::stoi(style);

                    if (i >= 0 && i < style_NUMSTYLES)
                    {
                        parsecolor(fg, styles[i].fg);
                        parsecolor(bg, styles[i].bg);
                    }
                }
            }
        } else if (cmd == "tfont" || cmd == "gfont") {
            std::istringstream argstream(arg);
            std::string style, font;

            if (argstream >> style >> font)
            {
                int i = std::stoi(style);

                if (i >= 0 && i < style_NUMSTYLES)
                {
                    if (cmd[0] == 't')
                        gli_tstyles[i].font = font2idx(font);
                    else
                        gli_gstyles[i].font = font2idx(font);
                }
            }
        }
    });
}

static void gli_read_config(int argc, char **argv)
{
#if __cplusplus >= 201703L
    /* load argv0 with name of executable without suffix */
    std::string argv0 = std::filesystem::path(argv[0])
        .filename()
        .replace_extension()
        .string();

    /* load gamefile with basename of last argument */
    std::string gamefile = std::filesystem::path(argv[argc - 1])
        .filename()
        .string();

    /* load exefile with directory containing main executable */
    std::string exedir = std::filesystem::path(argv[0])
        .parent_path()
        .string();
#else
    auto basename = [](std::string path) {
        auto slash = path.find_last_of("/\\");
        if (slash != std::string::npos)
            path.erase(0, slash + 1);

        return path;
    };

    /* load argv0 with name of executable without suffix */
    std::string argv0 = basename(argv[0]);
    auto dot = argv0.rfind('.');
    if (dot != std::string::npos)
        argv0.erase(dot);

    /* load gamefile with basename of last argument */
    std::string gamefile = basename(argv[argc - 1]);

    /* load exefile with directory containing main executable */
    std::string exedir = argv[0];
    exedir = exedir.substr(0, exedir.find_last_of("/\\"));
#endif

    /* load gamepath with the path to the story file itself */
    std::string gamepath;
    if (argc > 1)
        gamepath = argv[argc - 1];

    /* store so other parts of the code have access to full config information,
     * including execdir and gamepath components.
     */
    garglk::all_configs = garglk::configs(exedir, gamepath);

    /* load from all config files */
    auto configs = garglk::configs(exedir, gamepath);
    std::reverse(configs.begin(), configs.end());

    for (const auto &config : configs)
        readoneconfig(config.path, argv0, gamefile);
}

strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, glui32 rock)
{
    return gli_stream_open_pathname(pathname, false, (textmode != 0), rock);
}

void gli_startup(int argc, char *argv[])
{
    wininit(&argc, argv);

    if (argc > 1)
        glkunix_set_base_file(argv[argc-1]);

    garglk::theme::init();

    gli_read_config(argc, argv);

    gli_more_prompt.resize(base_more_prompt.size() + 1);
    gli_more_prompt_len = gli_parse_utf8(reinterpret_cast<const unsigned char *>(base_more_prompt.data()), base_more_prompt.size(), gli_more_prompt.data(), base_more_prompt.size());

    if (gli_baseline == 0)
        gli_baseline = std::round(gli_conf_propsize);

    gli_zoom *= gli_backingscalefactor;
    gli_baseline = gli_zoom_int(gli_baseline);
    gli_conf_monosize = gli_conf_monosize * gli_zoom;
    gli_conf_propsize = gli_conf_propsize * gli_zoom;
    gli_leading = gli_zoom_int(gli_leading);
    gli_tmarginx = gli_zoom_int(gli_tmarginx);
    gli_tmarginy = gli_zoom_int(gli_tmarginy);
    gli_wborderx = gli_zoom_int(gli_wborderx);
    gli_wbordery = gli_zoom_int(gli_wbordery);
    gli_wmarginx = gli_zoom_int(gli_wmarginx);
    gli_wmarginx_save = gli_zoom_int(gli_wmarginx_save);
    gli_wmarginy = gli_zoom_int(gli_wmarginy);
    gli_wmarginy_save = gli_zoom_int(gli_wmarginy_save);
    gli_wpaddingx = gli_zoom_int(gli_wpaddingx);
    gli_wpaddingy = gli_zoom_int(gli_wpaddingy);

    gli_initialize_tts();
    if (gli_conf_speak)
        gli_conf_quotes = 0;

    gli_initialize_misc();
    gli_initialize_fonts();
    gli_initialize_windows();
    gli_initialize_sound();

    winopen();
    gli_initialize_babel();
}
