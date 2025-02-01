// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#ifdef ZTERP_GLK
extern "C" {
#include <glk.h>
}
#endif

#include "io.h"
#include "options.h"
#include "osdep.h"
#include "screen.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

#if defined(ZTERP_UNIX) || defined(ZTERP_WIN32) || defined(ZTERP_DOS)
#error The operating system macros have been renamed to have a ZTERP_OS prefix.
#endif

using namespace std::literals;

// OS-specific functions should all be collected in this file for
// convenience. A sort of poor-man’s “reverse” inheritance is used: for
// each function that a particular operating system provides, it should
// #define a macro of the same name. At the end of the file, a generic
// function is provided for each function that has no associated macro
// definition.
//
// The functions required are as follows:
//
// long zterp_os_filesize(std::FILE *fp)
//
// Return the size of the file referred to by fp. It is safe to assume
// that the file is opened in binary mode. The file position indicator
// need not be maintained. If the size of the file is larger than
// LONG_MAX, -1 should be returned.
//
// std::unique_ptr<std::string> zterp_os_rcfile(bool create_parent)
//
// Different operating systems have different ideas about where
// configuration data should be stored; this function will return a
// suitable value for the bocfel configuration file. If a configuration
// file location cannot be determined, return a null pointer. If
// “create_parent” is true, attempt to create the containing directory,
// failing if this isn’t possible.
//
// std::unique_ptr<std::string> zterp_os_autosave_name()
//
// Return a suitable filename for autosaving. If there is some problem,
// a null pointer is returned. For example, the Unix implementation
// returns null if it can’t create all directory components in the
// filename. This should not verify that the file exists, because the
// first time an autosave is created, it necessarily won’t exist
// beforehand.
//
// std::unique_ptr<std::string> zterp_os_aux_file(const std::string &filename);
//
// The Z-machine allow games to save and load arbitrary files. Bocfel
// confines these files to a single (per-game) directory to avoid games
// overwriting unrelated files. This function, given a filename, returns
// a pointer to a string containing a full path to a file which
// represents the passed-in filename. The file need not exist, but its
// containing directory must. A null pointer is returned on failure. The
// file must not escape its containing directory, which may require
// platform-specific methods, but at the very least means that a file
// containing directory separators (e.g. '/' on Unix) must be rejected
// or sanitized. The incoming filename is guaranteed to consist of ASCII
// printable characters, i.e. in the range 32-126, but may need to be
// further sanitized depending on the needs of the target platform.
//
// void zterp_os_edit_file(const std::string &filename)
//
// Open the specified file in a text editor. On failure,
// std::runtime_error is thrown.
//
// std::vector<char> zterp_os_edit_notes(const std::vector<char> &notes)
//
// Given player notes in “notes”, open a text editor editing a file
// which contains those notes. When the editor successfully exits,
// return the contents of the saved file. On failure, std::runtime_error
// is thrown.
//
// void zterp_os_show_transcript(const std::vector<char> &transcript)
//
// Identical to zterp_os_edit_notes, except nothing is returned. This is
// meant to show the persistent transcript in the user’s editor, so it
// can be easily read, searched, etc.
//
// The following functions are useful for non-Glk builds only. They
// provide for some handling of screen functions that is normally taken
// care of by Glk.
//
// std::pair<unsigned int, unsigned int> zterp_os_get_screen_size()
//
// The size of the terminal, if known, is returned as a pair of (width,
// height). If terminal size is unavailable, (0, 0) should be returned.
//
// void zterp_os_init_term()
//
// If something special needs to be done to prepare the terminal for
// output, it should be done here. This function is called once at
// program startup.
//
// bool zterp_os_have_style(StyleBit style)
//
// This should return true if the provided individual style is
// available.
//
// bool zterp_os_have_colors()
//
// Returns true if the terminal supports colors.
//
// void zterp_os_set_style(const Style &style, const Color &fg, const Color &bg)
//
// Set both a style and foreground/background color. Any previous
// settings should be ignored; for example, if the last call to
// zterp_os_set_style() turned on italics and the current call sets
// bold, the result should be bold, not bold italic.
// The colors are references to “Color”: see screen.h.
// This function will be called unconditionally, so implementations must
// ignore requests if they cannot be fulfilled.

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Shared functions                                                             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#if defined(ZTERP_OS_UNIX) || defined(ZTERP_OS_WIN32)
static std::unique_ptr<std::string> env(const std::string &name)
{
    const char *val = std::getenv(name.c_str());
    if (val == nullptr) {
        return nullptr;
    }

    return std::make_unique<std::string>(val);
}
#endif

#if (defined(ZTERP_OS_WIN32) || defined(ZTERP_OS_DOS)) && !defined(ZTERP_GLK)
static void ansi_set_style(const Style &style, const Color &fg, const Color &bg)
{
    std::cout << "\33[0";

    if (style.test(STYLE_ITALIC)) {
        std::cout << ";4";
    }
    if (style.test(STYLE_REVERSE)) {
        std::cout << ";7";
    }
    if (style.test(STYLE_BOLD)) {
        std::cout << ";1";
    }

    if (fg.mode == Color::Mode::ANSI) {
        std::cout << ";" << (28 + fg.value);
    }
    if (bg.mode == Color::Mode::ANSI) {
        std::cout << ";" << (38 + bg.value);
    }

    std::cout << "m";
}
#endif

#if defined(ZTERP_OS_UNIX) || defined(ZTERP_OS_WIN32)
static std::vector<char> read_file(const std::string &filename)
{
    std::vector<char> new_file;

    try {
        IO io(&filename, IO::Mode::ReadOnly, IO::Purpose::Data);
        long size;

        size = io.filesize();
        if (size == -1) {
            throw std::exception();
        }

        io.seek(0, IO::SeekFrom::Start);

        if (size != 0) {
            new_file.resize(size);
            io.read_exact(new_file.data(), size);
        }
    } catch (...) {
        throw std::runtime_error("unable to read file");
    }

    return new_file;
}
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Unix functions                                                               ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifdef ZTERP_OS_UNIX
#include <cstring>
#include <vector>

#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __HAIKU__
#include <FindDirectory.h>
#endif

static std::string unique_name()
{
    auto slash = game_file.rfind('/');
    std::string basename;

    if (slash != std::string::npos) {
        basename = game_file.substr(slash + 1);
    } else {
        basename = game_file;
    }

    return basename + "-" + get_story_id();
}

// Given a filename (or a directory name ending in a slash), create all
// components of all directories, returning false in case of error.
static bool mkdir_p(const std::string &file)
{
    for (auto slash = file.find('/'); slash != std::string::npos; slash = file.find('/', slash + 1)) {
        auto component = file.substr(0, slash);
        if (!component.empty()) {
            struct stat st;
            mkdir(component.c_str(), 0755);
            if (stat(component.c_str(), &st) == -1 || !S_ISDIR(st.st_mode)) {
                return false;
            }
        }
    }

    return true;
}

long zterp_os_filesize(std::FILE *fp)
{
    struct stat st;
    int fd = fileno(fp);

    if (fd == -1 || fstat(fd, &st) == -1 || !S_ISREG(st.st_mode) || st.st_size > LONG_MAX) {
        return -1;
    }

    return st.st_size;
}
#define have_zterp_os_filesize

std::unique_ptr<std::string> zterp_os_rcfile(bool create_parent)
{
    std::unique_ptr<std::string> config_file;
#ifdef __HAIKU__
    char settings_dir[4096];

    if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, false, settings_dir, sizeof settings_dir) != B_OK) {
        return nullptr;
    }

    config_file = std::make_unique<std::string>(std::string(settings_dir) + "/bocfel/bocfelrc");
#else
    auto home = env("HOME");
    if (home != nullptr) {
        // This is the legacy location of the config file.
        auto s = *home + "/.bocfelrc";
        if (access(s.c_str(), R_OK) == 0) {
            return std::make_unique<std::string>(s);
        }
    }

    auto config_home = env("XDG_CONFIG_HOME");
    if (config_home != nullptr && config_home->find('/') == 0) {
        config_file = std::make_unique<std::string>(*config_home + "/bocfel/bocfelrc");
    } else if (home != nullptr) {
        config_file = std::make_unique<std::string>(*home + "/.config/bocfel/bocfelrc");
    } else {
        return nullptr;
    }
#endif

    if (create_parent && !mkdir_p(*config_file)) {
        return nullptr;
    }

    return config_file;
}
#define have_zterp_os_rcfile

static std::unique_ptr<std::string> data_file(const std::string &filename)
{
#ifdef __HAIKU__
    char settings_dir[4096];

    if (find_directory(B_USER_SETTINGS_DIRECTORY, -1, true, settings_dir, sizeof settings_dir) != B_OK) {
        return nullptr;
    }

    auto name = std::make_unique<std::string>(std::string(settings_dir) + "/bocfel/" + filename);
#else
    std::unique_ptr<std::string> name;

    auto data_home = env("XDG_DATA_HOME");
    if (data_home != nullptr && (*data_home)[0] == '/') {
        name = std::make_unique<std::string>(*data_home + "/bocfel/" + filename);
    } else {
        auto home = env("HOME");
        if (home == nullptr) {
            return nullptr;
        }
        name = std::make_unique<std::string>(*home + "/.local/share/bocfel/" + filename);
    }
#endif

    if (!mkdir_p(*name)) {
        return nullptr;
    }

    return name;
}

std::unique_ptr<std::string> zterp_os_autosave_name()
{
    std::string filename = "autosave/"s + unique_name();

    return data_file(filename);
}
#define have_zterp_os_autosave_name

std::unique_ptr<std::string> zterp_os_aux_file(const std::string &filename_)
{
    std::string filename = filename_;
    for (auto &c : filename) {
        if (c == '/') {
            c = '_';
        }
    }

    return data_file("auxiliary/"s + unique_name() + "/" + filename);
}
#define have_zterp_os_aux_file

static pid_t launch_editor(const std::string &filename)
{
    // A list of possible text editors. The user can specify a text
    // editor using the config file. If he doesn’t, or if it doesn’t
    // exist, keep trying various possible editors in hopes that one
    // exists and works. This really should use XDG to query the user’s
    // preferred text editor, but for now, it doesn’t.
    const std::vector<const char *> editors = {
#if defined(__HAIKU__)
        "StyledEdit",
#elif defined(__serenity__)
        "TextEditor",
#elif defined(__APPLE__)
        "open -Wnt",
#else
        "kwrite",
        "kate",
        "nedit-ng",
        "nedit",
        "gedit",
#endif
    };

    auto edit_with = [&filename](const std::string &editor) {
        std::istringstream ss(editor);
        std::vector<std::string> args;
        std::string token;

        while (ss >> std::quoted(token)) {
            args.push_back(token);
        }
        args.push_back(filename);

        std::vector<char *> c_args;
        for (const auto &arg : args) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        pid_t pid;
        extern char **environ;
        int rval = posix_spawnp(&pid, c_args[0], nullptr, nullptr, c_args.data(), environ);
        if (rval != 0) {
            throw rval;
        }

        return pid;
    };

    // Try the user’s editor explicitly instead of prepending it to the
    // list of defaults so a diagnostic message can easily be displayed.
    if (options.editor != nullptr) {
        try {
            return edit_with(*options.editor);
        } catch (int err) {
            std::cerr << "Unable to execute " << *options.editor << ": " << std::strerror(err) << std::endl;
        }
    }

    for (const auto &editor : editors) {
        try {
            return edit_with(editor);
        } catch (int) {
        }
    }

    return 0;
}

void zterp_os_edit_file(const std::string &filename)
{
    pid_t pid = launch_editor(filename);
    if (pid == 0) {
        throw std::runtime_error("unable to find a text editor");
    }

    int status;

#ifdef ZTERP_GLK_TICK
    while (waitpid(pid, &status, WNOHANG) != pid) {
        std::this_thread::sleep_for(10ms);
        glk_tick();
    }
#else
    waitpid(pid, &status, 0);
#endif

    if (!WIFEXITED(status)) {
        throw std::runtime_error("editor process terminated abnormally");
    }

#ifndef __APPLE__
    if (WEXITSTATUS(status) == 127) {
        throw std::runtime_error("unable to find a text editor");
    }
#endif

    if (WEXITSTATUS(status) != 0) {
        throw std::runtime_error("editor had exit status " + std::to_string(WEXITSTATUS(status)));
    }
}
#define have_zterp_os_edit_file

class TempFile {
public:
    explicit TempFile(const std::string &tmpl)
    {
        auto tmpdir = env("TMPDIR");
        if (tmpdir == nullptr) {
            tmpdir = std::make_unique<std::string>("/tmp");
        }

        m_path = *tmpdir + "/" + tmpl + ".XXXXXX";

        m_fd = mkstemp(&m_path[0]);
        if (m_fd == -1) {
            throw std::runtime_error("unable to create temporary file");
        }
    }

    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;

    void write(const std::vector<char> &data) const {
        size_t bytes = 0;
        while (bytes < data.size()) {
            ssize_t n = ::write(m_fd, &data[bytes], data.size() - bytes);
            if (n == -1) {
                throw std::runtime_error("unable to write to temporary file");
            }
            bytes += n;
        }
    }

    ~TempFile() {
        close(m_fd);
        std::remove(m_path.c_str());
    }

    const std::string &path() const {
        return m_path;
    }

private:
    std::string m_path;
    int m_fd;
};

std::vector<char> zterp_os_edit_notes(const std::vector<char> &notes)
{
    TempFile tempfile("bocfel.notes");

    tempfile.write(notes);
    zterp_os_edit_file(tempfile.path());

    return read_file(tempfile.path());
}
#define have_zterp_os_edit_notes

void zterp_os_show_transcript(const std::vector<char> &transcript)
{
    TempFile tempfile("bocfel.transcript");

    tempfile.write(transcript);
    zterp_os_edit_file(tempfile.path());
}
#define have_zterp_os_show_transcript

#ifndef ZTERP_GLK

#ifndef ZTERP_NO_CURSES
#include <curses.h>
#include <sys/ioctl.h>
#include <term.h>
#ifdef TIOCGWINSZ
std::pair<unsigned int, unsigned int> zterp_os_get_screen_size()
{
    struct winsize winsize;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) == 0) {
        return {winsize.ws_col, winsize.ws_row};
    }

    return {0, 0};
}
#define have_zterp_os_get_screen_size
#endif

static const char *ital = nullptr, *rev = nullptr, *bold = nullptr, *none = nullptr;
static char *fg_string = nullptr, *bg_string = nullptr;
static bool have_colors = false;
static bool have_24bit_rgb = false;
void zterp_os_init_term()
{
    if (setupterm(nullptr, STDOUT_FILENO, nullptr) != OK) {
        return;
    }

    // prefer italics over underline for emphasized text
    ital = tgetstr(const_cast<char *>("ZH"), nullptr);
    if (ital == nullptr) {
        ital = tgetstr(const_cast<char *>("us"), nullptr);
    }
    rev  = tgetstr(const_cast<char *>("mr"), nullptr);
    bold = tgetstr(const_cast<char *>("md"), nullptr);
    none = tgetstr(const_cast<char *>("me"), nullptr);

    fg_string = tgetstr(const_cast<char *>("AF"), nullptr);
    bg_string = tgetstr(const_cast<char *>("AB"), nullptr);

    have_colors = none != nullptr && fg_string != nullptr && bg_string != nullptr;

    have_24bit_rgb = tigetflag(const_cast<char *>("RGB")) > 0 && tigetnum(const_cast<char *>("colors")) == 1U << 24;
}
#define have_zterp_os_init_term

bool zterp_os_have_style(StyleBit style)
{
    if (none == nullptr) {
        return false;
    }

    switch (style) {
    case STYLE_ITALIC:
        return ital != nullptr;
    case STYLE_REVERSE:
        return rev != nullptr;
    case STYLE_BOLD:
        return bold != nullptr;
    default:
        return false;
    }
}
#define have_zterp_os_have_style

bool zterp_os_have_colors()
{
    return have_colors;
}
#define have_zterp_os_have_colors

static void set_color(const char *string, const Color &color)
{
    // Cast the first argument to tparm() below to char*, for unfortunate
    // historical purposes: X/Open defines tparm() as taking a char*, and
    // while most modern Unixes have a Curses implementation that takes
    // const char*, at least macOS still takes a char*. No reasonable Curses
    // implementation will modify the argument, so it’s safe enough to just
    // cast here.

    switch (color.mode) {
    case Color::Mode::ANSI:
        if (color.value >= 2 && color.value <= 9) {
            putp(tparm(const_cast<char *>(string), color.value - 2, 0, 0, 0, 0, 0, 0, 0, 0));
        }
        break;
    case Color::Mode::True:
        if (have_24bit_rgb) {
            // Presumably for compatibility, even on terminals capable of
            // direct color, the values 0 to 7 are still treated as if on a
            // 16-color terminal (1 is red, 2 is green, and so on). Any
            // other value is treated as a 24-bit color in the expected
            // fashion. If a value between 1 and 7 is chosen, round to 0
            // (which is still black) or 8.
            uint16_t transformed = color.value < 4 ? 0 :
                                   color.value < 8 ? 8 :
                                   color.value;
            putp(tparm(const_cast<char *>(string), screen_convert_color(transformed), 0, 0, 0, 0, 0, 0, 0, 0));
        }
        break;
    }
}

void zterp_os_set_style(const Style &style, const Color &fg, const Color &bg)
{
    // If the terminal cannot be reset, nothing can be used.
    if (none == nullptr) {
        return;
    }

    putp(none);

    if (style.test(STYLE_ITALIC) && ital != nullptr) {
        putp(ital);
    }
    if (style.test(STYLE_REVERSE) && rev != nullptr) {
        putp(rev);
    }
    if (style.test(STYLE_BOLD) && bold != nullptr) {
        putp(bold);
    }

    if (have_colors) {
        set_color(fg_string, fg);
        set_color(bg_string, bg);
    }
}
#define have_zterp_os_set_style
#endif

#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Windows functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#elif defined(ZTERP_OS_WIN32)
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

long zterp_os_filesize(std::FILE *fp)
{
    struct _stat st;
    int fd = _fileno(fp);

    if (fd == -1 || _fstat(_fileno(fp), &st) == -1 || (st.st_mode & _S_IFREG) == 0 || st.st_size > LONG_MAX) {
        return -1;
    }

    return st.st_size;
}
#define have_zterp_os_filesize

static bool mkdir_p(const std::string &filename)
{
    char drive[MAX_PATH], path[MAX_PATH];

    if (_splitpath_s(filename.c_str(), drive, sizeof drive, path, sizeof path, nullptr, 0, nullptr, 0) != 0) {
        return false;
    }

    for (char *p = path; *p != 0; p++) {
        if ((*p == '/' || *p == '\\') && p != path) {
            char slash = *p;
            struct _stat st;
            *p = 0;
            std::string component = std::string(drive) + path;
            _mkdir(component.c_str());
            if (_stat(component.c_str(), &st) == -1 || (st.st_mode & _S_IFDIR) != S_IFDIR) {
                return false;
            }
            *p = slash;
        }
    }

    return true;
}

std::unique_ptr<std::string> zterp_os_rcfile(bool create_parent)
{
    auto appdata = env("APPDATA");
    if (appdata == nullptr) {
        return nullptr;
    }

    auto config = std::make_unique<std::string>(*appdata + "\\Bocfel\\bocfel.ini");

    if (create_parent && !mkdir_p(*config)) {
        return nullptr;
    }

    return config;
}
#define have_zterp_os_rcfile

std::unique_ptr<std::string> unique_name()
{
    char fname[MAX_PATH], ext[MAX_PATH];

    if (_splitpath_s(game_file.c_str(), nullptr, 0, nullptr, 0, fname, sizeof fname, ext, sizeof ext) != 0) {
        return nullptr;
    }

    return std::make_unique<std::string>(std::string(fname) + ext + "-" + get_story_id());
}

static std::unique_ptr<std::string> data_file(const std::string &filename)
{
    auto appdata = env("APPDATA");
    if (appdata == nullptr) {
        return nullptr;
    }

    auto name = std::make_unique<std::string>(*appdata + "\\Bocfel\\" + filename);

    if (!mkdir_p(*name)) {
        return nullptr;
    }

    return name;
}

std::unique_ptr<std::string> zterp_os_autosave_name()
{
    auto filename = unique_name();

    if (filename == nullptr) {
        return nullptr;
    }

    return data_file("autosave\\"s + *filename);
}
#define have_zterp_os_autosave_name

static std::string ascii_toupper(std::string s)
{
    for (auto &c : s) {
        if (c >= 97 && c <= 122) {
            c -= 32;
        }
    }

    return s;
}

std::unique_ptr<std::string> zterp_os_aux_file(const std::string &filename_)
{
    std::string filename = filename_;
    std::string upper = ascii_toupper(filename);

    static const std::vector<std::string> reserved_names = {
        "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4",
        "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2",
        "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
    };

    for (const auto &reserved_name : reserved_names) {
        if (upper == reserved_name || upper.find(reserved_name + ".") == 0) {
            return nullptr;
        }
    }

    static const std::string invalid_characters = "<>:\"/\\|?*";
    for (auto &c : filename) {
        if (invalid_characters.find(c) != std::string::npos) {
            c = '_';
        }
    }

    auto basename = unique_name();
    if (basename == nullptr) {
        return nullptr;
    }

    return data_file("auxiliary\\"s + *basename + "\\" + filename);
}
#define have_zterp_os_aux_file

void zterp_os_edit_file(const std::string &filename)
{
    SHELLEXECUTEINFO si;
    si.cbSize = sizeof(SHELLEXECUTEINFO);
    si.fMask = SEE_MASK_NOCLOSEPROCESS;
    si.hwnd = nullptr;
    si.lpVerb = "open";
    si.lpFile = filename.c_str();
    si.lpParameters = nullptr;
    si.lpDirectory = nullptr;
    si.nShow = SW_SHOW;
    si.hInstApp = nullptr;

    if (!ShellExecuteEx(&si)) {
        throw std::runtime_error("unable to launch text editor");
    }

#ifdef ZTERP_GLK_TICK
    while (WaitForSingleObject(si.hProcess, 10) != 0) {
        glk_tick();
    }
#else
    WaitForSingleObject(si.hProcess, INFINITE);
#endif

    CloseHandle(si.hProcess);
}
#define have_zterp_os_edit_file

class Remover {
public:
    explicit Remover(const std::string &filename) : m_filename(filename) {
    }

    Remover(const Remover &) = delete;
    Remover &operator=(const Remover &) = delete;

    Remover(Remover &&other) noexcept : m_filename(std::move(other.m_filename)) {
        other.m_filename.clear();
    }

    ~Remover() {
        if (!m_filename.empty()) {
            std::remove(m_filename.c_str());
        }
    }

    const std::string &filename() {
        return m_filename;
    }

private:
    std::string m_filename;
};

static std::string generate_random_prefix()
{
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string prefix;
    for (size_t i = 0; i < 12; ++i) {
        prefix += charset[dist(rng)];
    }
    return prefix;
}

static Remover create_temp_file(const std::string &filename, const std::vector<char> &data)
{
    char tempdir[300];

    DWORD ret = GetTempPath(sizeof tempdir, tempdir);
    if (ret == 0 || ret > sizeof tempdir) {
        throw std::runtime_error("unable to find temporary directory");
    }

    Remover remover(std::string(tempdir) + "\\" + generate_random_prefix() + "-" + filename);
    try {
        IO io(&remover.filename(), IO::Mode::WriteOnly, IO::Purpose::Data);
        io.write_exact(data.data(), data.size());
    } catch (const IO::OpenError &) {
        throw std::runtime_error("unable to create temporary file");
    } catch (const IO::IOError &) {
        throw std::runtime_error("unable to write to temporary file");
    }

    return remover;
}

std::vector<char> zterp_os_edit_notes(const std::vector<char> &notes)
{
    auto notes_name = create_temp_file("bocfel_notes.txt", notes);

    zterp_os_edit_file(notes_name.filename());

    return read_file(notes_name.filename());
}
#define have_zterp_os_edit_notes

void zterp_os_show_transcript(const std::vector<char> &transcript)
{
    auto transcript_name = create_temp_file("bocfel_transcript.txt", transcript);
    zterp_os_edit_file(transcript_name.filename());
}
#define have_zterp_os_show_transcript

#ifndef ZTERP_GLK
std::pair<unsigned int, unsigned int> zterp_os_get_screen_size()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO screen;
        GetConsoleScreenBufferInfo(handle, &screen);

        return {screen.srWindow.Right - screen.srWindow.Left + 1,
                screen.srWindow.Bottom - screen.srWindow.Top + 1};
    }

    return {0, 0};
}
#define have_zterp_os_get_screen_size

static bool terminal_processing_enabled = false;
void zterp_os_init_term()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD mode;
        if (GetConsoleMode(handle, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            terminal_processing_enabled = SetConsoleMode(handle, mode);
        }
    }
}
#define have_zterp_os_init_term

bool zterp_os_have_style(StyleBit)
{
    return terminal_processing_enabled;
}
#define have_zterp_os_have_style

bool zterp_os_have_colors()
{
    return terminal_processing_enabled;
}
#define have_zterp_os_have_colors

void zterp_os_set_style(const Style &style, const Color &fg, const Color &bg)
{
    if (!terminal_processing_enabled) {
        return;
    }

    ansi_set_style(style, fg, bg);
}
#define have_zterp_os_set_style
#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ DOS functions                                                                ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#elif defined(ZTERP_OS_DOS)
#ifndef ZTERP_GLK
// This assumes ANSI.SYS (or equivalent) is loaded.
bool zterp_os_have_style(StyleBit)
{
    return true;
}
#define have_zterp_os_have_style

bool zterp_os_have_colors()
{
    return true;
}
#define have_zterp_os_have_colors

void zterp_os_set_style(const Style &style, const Color &fg, const Color &bg)
{
    ansi_set_style(style, fg, bg);
}
#define have_zterp_os_set_style
#endif

#endif

// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║ Generic functions                                                            ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#ifndef have_zterp_os_filesize
long zterp_os_filesize(std::FILE *fp)
{
    // Assume fseek() can seek to the end of binary streams.
    if (std::fseek(fp, 0, SEEK_END) == -1) {
        return -1;
    }

    return std::ftell(fp);
}
#endif

#ifndef have_zterp_os_edit_file
void zterp_os_edit_file(const std::string &)
{
    throw std::runtime_error("editing unimplemented on this platform");
}
#endif

#ifndef have_zterp_os_rcfile
std::unique_ptr<std::string> zterp_os_rcfile(bool)
{
    return std::make_unique<std::string>("bocfelrc");
}
#endif

#ifndef have_zterp_os_autosave_name
std::unique_ptr<std::string> zterp_os_autosave_name()
{
    return nullptr;
}
#endif

#ifndef have_zterp_os_aux_file
std::unique_ptr<std::string> zterp_os_aux_file(const std::string &)
{
    return nullptr;
}
#endif

#ifndef have_zterp_os_edit_notes
std::vector<char> zterp_os_edit_notes(const std::vector<char> &)
{
    throw std::runtime_error("notes unimplemented on this platform");
}
#endif

#ifndef have_zterp_os_show_transcript
void zterp_os_show_transcript(const std::vector<char> &transcript)
{
    throw std::runtime_error("transcript display unimplemented on this platform");
}
#endif

#ifndef ZTERP_GLK
#ifndef have_zterp_os_get_screen_size
std::pair<unsigned int, unsigned int> zterp_os_get_screen_size()
{
    return {0, 0};
}
#endif

#ifndef have_zterp_os_init_term
void zterp_os_init_term()
{
}
#endif

#ifndef have_zterp_os_have_style
bool zterp_os_have_style(StyleBit)
{
    return false;
}
#endif

#ifndef have_zterp_os_have_colors
bool zterp_os_have_colors()
{
    return false;
}
#endif

#ifndef have_zterp_os_set_style
void zterp_os_set_style(const Style &, const Color &, const Color &)
{
}
#endif
#endif
