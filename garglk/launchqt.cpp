//
// Copyright (C) 2006-2009 by Tor Andersson.
// Copyright (C) 2009 by Baltasar García Perez-Schofield.
// Copyright (C) 2010 by Ben Cressey.
// Copyright (C) 2021 by Chris Spiegel.
//
// This file is part of Gargoyle.
//
// Gargoyle is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Gargoyle is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Gargoyle; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstdio>

#include <windows.h>
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include "garglk.h"
#include "garversion.h"
#include "launcher.h"
#include "menubarqt.h"
#include "sessionqt.h"

#include GARGLKINI_H

void garglk::winmsg(const std::string &msg)
{
    QMessageBox::critical(nullptr, "Error", msg.c_str());
}

bool garglk::winterp(const std::string &exe, const std::vector<std::string> &flags, const std::string &game)
{
    // Find the directory that contains the interpreters. By default
    // this is GARGLK_CONFIG_INTERPRETER_DIR but if that is not set, it
    // is the containing directory of the gargoyle executable.
    //
    // For development purposes, the environment variable
    // $GARGLK_INTERPRETER_DIR can be set to the interpreter build
    // directory to allow the gargoyle binary to load the newly-built
    // interpreters instead of the system-wide interpreters (or instead
    // of failing if there are no interpreters installed). If this is
    // set, the standard directory will *not* be used at all, even if no
    // interpreter is found.
    QString interpreter_dir = std::getenv("GARGLK_INTERPRETER_DIR");
    if (interpreter_dir.isNull()) {
#ifdef GARGLK_CONFIG_INTERPRETER_DIR
        interpreter_dir = GARGLK_CONFIG_INTERPRETER_DIR;
#else
        interpreter_dir = QCoreApplication::applicationDirPath();
#endif
#ifdef Q_OS_MAC
        // macOS app bundles install interpreters in Contents/PlugIns
        // (gargoyle_osx.sh; matches launchmac.mm's builtInPlugInsPath).
        // Prefer that when the requested interpreter exists there.
        QDir plugins_dir(QCoreApplication::applicationDirPath());
        if (plugins_dir.cd("../PlugIns")) {
            QString plugin_exe = plugins_dir.absoluteFilePath(QString::fromStdString(exe));
            if (QFileInfo::exists(plugin_exe)) {
                interpreter_dir = plugins_dir.absolutePath();
            }
        }
#endif
    }

    QString argv0 = QDir(interpreter_dir).absoluteFilePath(exe.c_str());

    QStringList args;
    for (const auto &flag : flags) {
        args.push_back(QString::fromStdString(flag));
    }
    args.push_back(QString::fromStdString(game));

    if (garglk::session_is_parent()) {
        // IPC session: launch non-blocking; the parent owns windows.
        QProcess proc;
        proc.setProgram(argv0);
        proc.setArguments(args);
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("GARGLK_LAUNCHER", QCoreApplication::applicationFilePath());
        // Mark children so they can hide from the macOS Dock while still
        // creating a QApplication for Qt event processing.
        env.insert("GARGLK_IPC_CHILD", "1");
#ifdef Q_OS_MAC
        if (auto resources = qgetenv("GARGLK_RESOURCES"); !resources.isEmpty()) {
            env.insert("GARGLK_RESOURCES", QString::fromUtf8(resources));
        }
#endif
        proc.setProcessEnvironment(env);
        if (!proc.startDetached()) {
            garglk::winmsg("Could not start interpreter " + argv0.toStdString());
            return false;
        }
        return true;
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);

    // So interpreters can re-launch Gargoyle (File → Open / Open Recent).
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("GARGLK_LAUNCHER", QCoreApplication::applicationFilePath());
#ifdef Q_OS_MAC
    if (auto resources = qgetenv("GARGLK_RESOURCES"); !resources.isEmpty()) {
        env.insert("GARGLK_RESOURCES", QString::fromUtf8(resources));
    }
#endif
    proc.setProcessEnvironment(env);

    proc.start(argv0, args);

    if (!proc.waitForStarted(5000)) {
        garglk::winmsg("Could not start interpreter " + argv0.toStdString());
        return false;
    }

    proc.waitForFinished(-1);

    if (proc.exitStatus() != QProcess::NormalExit) {
        return false;
    } else {
        return proc.exitCode() == 0;
    }
}

static QString parse_args(const QApplication &app)
{
    QCommandLineParser parser;

    // Manually add -h to avoid --help-all: don't show Qt-specific
    // options, as this only affects the launcher. If the user selects a
    // style, for example, that won't carry on to the interpreter, as
    // it's a separate program. Qt options are still _supported_ (as
    // they're passed to QApplication's constructor), but at least don't
    // advertise their existence.
    //
    // The "right" approach would be to synthesize empty arguments for
    // Qt and then parse arguments with something like getopt_long(),
    // but that's a GNU extension and would have to be pulled in from
    // glibc, musl libc, or similar. This is good enough.
    parser.addOptions({
        {{"d", "dump-config"}, "Dump the default config file to standard out."},
        {{"e", "edit-config"}, "Edit the configuration file."},
        {{"h", "help"}, "Displays help on commandline options."},
        {{"m", "migrate-config"}, "Move a legacy configuration file to the preferred location."},
        {{"p", "paths"}, "Displays configuration file and theme paths."},
        {{"t", "themes"}, "Displays all available color themes."},
    });

    parser.addVersionOption();
    parser.addPositionalArgument("STORY", "The story/game file to run. If not provided, a file chooser will be displayed.", "[STORY]");
    parser.process(app);

    auto positional = parser.positionalArguments();

    if (positional.size() > 1) {
        std::cerr << "warning: extra positional arguments are ignored." << std::endl;
    }

    QString gamefile = positional.isEmpty() ?
        "" :
        positional.first();

    if (parser.isSet("d")) {
        std::cout << garglkini;
        std::exit(0);
    } else if (parser.isSet("e")) {
        gli_edit_config();
        std::exit(0);
    } else if (parser.isSet("h")) {
        std::cout << parser.helpText().toStdString() << std::endl;
        std::exit(0);
    } else if (parser.isSet("m")) {
        auto configs = garglk::configs("");
        configs.erase(std::remove_if(configs.begin(), configs.end(), [](const auto &config) {
            return config.type != garglk::ConfigFile::Type::User;
        }), configs.end());

        if (configs.empty()) {
            std::cerr << "Unable to determine configuration file locations.\n";
            std::exit(1);
        }

        auto preferred = QString::fromStdString(configs.front().path);
        if (QFile::exists(preferred)) {
            std::cout << "Preferred configuration file " << preferred.toStdString() << " already exists.\n";
        } else {
            std::vector<garglk::ConfigFile> existing;

            std::copy_if(configs.begin(), configs.end(), std::back_inserter(existing), [&preferred](const auto &config) {
                auto path = QString::fromStdString(config.path);
                return path != preferred && QFile::exists(path);
            });

            if (existing.empty()) {
                std::cout << "No existing configuration files found.\n";
            } else if (existing.size() != 1) {
                std::cout << "Won't migrate, found multiple existing configuration files:\n\n";
                for (const auto &config : existing) {
                    std::cout << config.path << std::endl;
                }
            } else {
                auto old = existing.front().path;
                std::cout << "Renaming " << old << " to " << preferred.toStdString() << std::endl;
                QFile file(QString::fromStdString(old));
                if (!file.rename(preferred)) {
                    std::cerr << "Unable to rename file: " << file.errorString().toStdString() << std::endl;
                    std::exit(1);
                }
            }
        }

        std::exit(0);
    } else if (parser.isSet("p")) {
        // Convert to native separators and return absolute path.
        auto canonicalize = [](const std::string &path) {
            auto qpath = QString::fromStdString(path);
            qpath = QDir(qpath).absolutePath();
            return QDir::toNativeSeparators(qpath).toStdString();
        };

        std::cout << "Configuration file paths:\n\n";
        for (const auto &config : garglk::configs(gamefile.toStdString())) {
            auto path = canonicalize(config.path);
            auto type = QString::fromStdString(config.format_type());

            std::cout << path << " " << type.toStdString() << std::endl;
        }

        std::cout << "\nTheme paths:\n\n";
        auto theme_paths = garglk::theme::paths();
        std::reverse(theme_paths.begin(), theme_paths.end());
        for (const auto &path : theme_paths) {
            std::cout << canonicalize(path) << std::endl;
        }

        std::exit(0);
    } else if (parser.isSet("t")) {
        for (const auto &theme_name : garglk::theme::names()) {
            std::cout << theme_name << std::endl;
        }

        std::exit(0);
    }

    return gamefile;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    // The WIN32 CMake flag builds a GUI subsystem executable, which has
    // no console attached. If running from a terminal (cmd, PowerShell),
    // attach to it so that stdout/stderr output from --help, --paths,
    // etc. is visible.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif

    QApplication app(argc, argv);

    QApplication::setApplicationName("gargoyle");
    QApplication::setApplicationVersion(GARGOYLE_VERSION);

#ifdef Q_OS_MAC
    // Match launchmac.mm: point interpreters/fontload at Contents/Resources.
    QDir resources_dir(QCoreApplication::applicationDirPath());
    if (resources_dir.cd("../Resources")) {
        qputenv("GARGLK_RESOURCES", resources_dir.absolutePath().toUtf8());
    }
#endif

    garglk::theme::init();

    auto story = parse_args(app);

#ifdef _WIN32
    // Resolve the story path while the working directory it might be
    // relative to is still current.
    if (!story.isEmpty()) {
        story = QFileInfo(story).absoluteFilePath();
    }

    // On Windows, when started from the start menu, Gargoyle's CWD is
    // set to the install directory. That means that default file
    // dialogs open there, which is not a useful location. If the CWD is
    // in fact there, change dir to the desktop. If the CWD is anywhere
    // else, assume it's the user's doing and leave it alone.
    if (QDir::currentPath() == QCoreApplication::applicationDirPath()) {
        auto desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (!desktop.isEmpty()) {
            QDir::setCurrent(desktop);
        }
    }
#endif

    // Read config early so ipc / ipc_server are available for handoff.
    // If a story was passed on the CLI, per-game config applies too.
    gli_read_config(argc, argv);

    if (gli_conf_ipc) {
        if (!story.isEmpty() && garglk::session_try_handoff(story)) {
            return 0;
        }

        if (story.isEmpty()) {
            story = garglk::browse_for_game();
        }
        if (story.isEmpty()) {
            return 1;
        }

        // Re-read so per-game config from the chosen story applies.
        // Build a synthetic argv with the story path.
        std::string story_std = story.toStdString();
        std::vector<char *> config_argv;
        config_argv.push_back(argv[0]);
        config_argv.push_back(story_std.data());
        gli_read_config(static_cast<int>(config_argv.size()), config_argv.data());

        if (!garglk::session_init_parent()) {
            return 1;
        }

        garglk::session_set_launcher([](const std::string &game) {
            return garglk::rungame(game);
        });

        if (!garglk::session_open_game(story)) {
            return 1;
        }

        return garglk::session_exec();
    }

    if (story.isEmpty()) {
        story = garglk::browse_for_game();
    }

    if (story.isEmpty()) {
        return 1;
    }

    // run story file
    return garglk::rungame(story.toStdString()) ? 0 : 1;
}
