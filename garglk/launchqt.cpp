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

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVector>

#include "garglk.h"
#include "garversion.h"
#include "launcher.h"

#include GARGLKINI_H

static const char *AppName = GARGOYLE_NAME " " GARGOYLE_VERSION;

class Filter {
public:
    Filter(QString name, QStringList extensions) : m_name(std::move(name)), m_extensions(std::move(extensions)) {}

    QString format() const {
        return QString("%1 Games (%2)")
            .arg(m_name, format_extensions().join(" "));
    }

    QStringList format_extensions() const {
        QList<QString> mapped_extensions;
        std::transform(m_extensions.begin(), m_extensions.end(),
                std::back_inserter(mapped_extensions),
                [](const QString &ext) { return QString("*.") + ext; });
        return mapped_extensions;
    }

private:
    QString m_name;
    QStringList m_extensions;
};

void garglk::winmsg(const std::string &msg)
{
    QMessageBox::critical(nullptr, "Error", msg.c_str());
}

static QString winbrowsefile()
{
    const QVector<Filter> filters = {
        Filter("Adrift", {"taf"}),
        Filter("AdvSys", {"dat"}),
        Filter("AGT", {"agx", "d$$"}),
        Filter("Alan", {"acd", "a3c"}),
        Filter("Glulx", {"ulx", "blb", "blorb", "glb", "gblorb"}),
        Filter("Hugo", {"hex"}),
        Filter("JACL", {"jacl", "j2"}),
        Filter("Level 9", {"l9", "sna"}),
        Filter("Magnetic Scrolls", {"mag"}),
        Filter("TADS", {"gam", "t3"}),
        Filter("Z-code", {"z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "zlb", "zblorb"}),
    };
    QList<QString> mapped_filters;
    std::transform(filters.begin(), filters.end(),
            std::back_inserter(mapped_filters),
            [](const Filter &filter) { return filter.format(); });

    QStringList all_extensions;
    for (const auto &filter : filters) {
        all_extensions << filter.format_extensions();
    }

    QString filter_string = QString("All Games (%1);;All Files (*);;%2")
        .arg(all_extensions.join(" "), mapped_filters.join(";;"));

    // Hide filter details because the sheer number in "All Games" makes
    // the dialog ridiculously wide (Qt probably should cut it off, but
    // it doesn't, so try to compensate here).
    QFileDialog::Options options(QFileDialog::HideNameFilterDetails);
#ifdef GARGLK_CONFIG_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif
    return QFileDialog::getOpenFileName(nullptr, AppName, "", filter_string, nullptr, options);
}

int garglk::winterp(const std::string &exe, const std::string &flags, const std::string &game)
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
    }

    QString argv0 = QDir(interpreter_dir).absoluteFilePath(exe.c_str());

    QStringList args;

    if (flags.find('-') != std::string::npos) {
        args = QStringList({flags.c_str(), game.c_str()});
    } else {
        args = QStringList({game.c_str()});
    }

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start(argv0, args);

    if (!proc.waitForStarted(5000)) {
        garglk::winmsg("Could not start interpreter " + argv0.toStdString());
        return 1;
    }

    proc.waitForFinished(-1);

    if (proc.exitStatus() != QProcess::NormalExit) {
        return 1;
    } else {
        return proc.exitCode();
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
        {{"p", "paths"}, "Displays configuration file and theme paths."}
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
    }

    return gamefile;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    app.setOrganizationName(GARGOYLE_ORGANIZATION);
    app.setApplicationName(GARGOYLE_NAME);
    app.setApplicationVersion(GARGOYLE_VERSION);

    auto story = parse_args(app);

    if (story.isEmpty()) {
        story = winbrowsefile();
    }

    if (story.isEmpty()) {
        return 1;
    }

    // run story file
    return garglk::rungame(story.toStdString());
}
