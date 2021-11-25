/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson.                                  *
 * Copyright (C) 2009 by Baltasar García Perez-Schofield.                     *
 * Copyright (C) 2010 by Ben Cressey.                                         *
 * Copyright (C) 2021 by Chris Spiegel.                                       *
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
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QList>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVector>

#include "glk.h"
#include "garversion.h"
#include "launcher.h"

static const char *AppName = "Gargoyle " VERSION;

class Filter {
public:
    Filter(const QString &name, const QStringList &extensions) : m_name(name), m_extensions(extensions) {}

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
    for (const auto &filter : filters)
    {
        all_extensions << filter.format_extensions();
    }

    QString filter_string = QString("All Games (%1);;All Files (*);;%2")
        .arg(all_extensions.join(" "), mapped_filters.join(";;"));

    // Hide filter details because the sheer number in "All Games" makes
    // the dialog ridiculously wide (Qt probably should cut it off, but
    // it doesn't, so try to compensate here).
    QFileDialog::Options options(QFileDialog::HideNameFilterDetails);
#ifdef GARGLK_NO_NATIVE_FILE_DIALOGS
    options |= QFileDialog::DontUseNativeDialog;
#endif
    return QFileDialog::getOpenFileName(nullptr, AppName, "", filter_string, nullptr, options);
}

int garglk::winterp(const std::string &path, const std::string &exe, const std::string &flags, const std::string &game)
{
    QString argv0 = QDir(path.c_str()).absoluteFilePath(exe.c_str());

    QStringList args;

    if (flags.find('-') != std::string::npos)
        args = QStringList({flags.c_str(), game.c_str()});
    else
        args = QStringList({game.c_str()});

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start(argv0, args);

    if (!proc.waitForStarted(5000))
    {
        garglk::winmsg("Could not start interpreter " + argv0.toStdString());
        return 1;
    }

    proc.waitForFinished(-1);

    if (proc.exitStatus() != QProcess::NormalExit)
        return 1;
    else
        return proc.exitCode();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString story;

    // Find the directory that contains the interpreters. By default
    // this is GARGLK_INTERPRETER_DIR but if that is not set, it is the
    // containing directory of the gargoyle executable.
    //
    // For development purposes, the environment variable
    // $GARGLK_INTERPRETER_DIR can be set to the interpreter build
    // directory to allow the gargoyle binary to load the newly-built
    // interpreters instead of the system-wide interpreters (or instead
    // of failing if there are no interpreters installed). If this is
    // set, the standard directory will *not* be used at all, even if no
    // interpreter is found.
    QString dir = getenv("GARGLK_INTERPRETER_DIR");
    if (dir.isNull())
#ifdef GARGLK_INTERPRETER_DIR
        dir = GARGLK_INTERPRETER_DIR;
#else
        dir = QCoreApplication::applicationDirPath();
#endif

    /* get story file */
    if (argc >= 2)
        story = argv[1];
    else
        story = winbrowsefile();

    if (story.isEmpty())
        return 1;

    /* run story file */
    std::string dir_string = dir.toStdString();
    std::string story_string = story.toStdString();
    return garglk::rungame(dir_string.c_str(), story_string.c_str());
}
