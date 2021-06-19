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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QString>
#include <QStringList>

extern "C" {
#include "glk.h"
#include "garversion.h"
#include "launcher.h"
}

static const char *AppName = "Gargoyle " VERSION;

class Filter {
public:
    Filter(const QString &name, const QStringList &extensions) : m_name(name), m_extensions(extensions) {}

    QString format() const {
        return QString("%1 Games (%2)")
            .arg(m_name)
            .arg(format_extensions().join(" "));
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

static const QList<Filter> filters = {
    Filter("Adrift", {"taf"}),
    Filter("AdvSys", {"dat"}),
    Filter("AGT", {"agx", "d$$"}),
    Filter("Alan", {"acd", "a3c"}),
    Filter("Glulx", {"ulx", "blb", "blorb", "glb", "gblorb"}),
    Filter("Hugo", {"hex"}),
    Filter("JACL", {"jacl", "j2"}),
    Filter("Level 9", {"l9", "sna"}),
    Filter("Magnetic Scrolls", {"mag"}),
    Filter("Quest", {"asl", "cas"}),
    Filter("TADS", {"gam", "t3"}),
    Filter("Z-code", {"z[1-8]", "zlb", "zblorb"}),
};

void winmsg(const char *msg)
{
    QMessageBox::critical(nullptr, "Error", msg);
}

static QString winbrowsefile()
{
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
        .arg(all_extensions.join(" "))
        .arg(mapped_filters.join(";;"));

    // Hide filter details because the sheer number in "All Games" makes
    // the dialog ridiculously wide (Qt probably should cut it off, but
    // it doesn't, so try to compensate here).
    return QFileDialog::getOpenFileName(nullptr, AppName, "", filter_string, nullptr, QFileDialog::HideNameFilterDetails);
}

#ifndef GARGLK_INTERPRETER_DIR
static QString winpath()
{
    char buf[4096];
#ifdef __FreeBSD__
    int mib[4];
    size_t len = sizeof buf;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == -1)
    {
        winmsg("Unable to locate executable path");
        exit(EXIT_FAILURE);
    }
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf);
    if (n == -1 || n >= static_cast<ssize_t>(sizeof buf))
    {
        winmsg("Unable to locate executable path");
        std::exit(EXIT_FAILURE);
    }
    buf[n] = 0;
#endif

    char *p = std::strrchr(buf, '/');
    if (p != nullptr)
        *p = '\0';

    return buf;
}
#endif

int winterp(const char *path, const char *exe, const char *flags, const char *game)
{
    // Haiku provides execv(), but if Qt is running, it fails with
    // "Operation not allowed" (errno 0x8000000f). The same execve()
    // call works fine outside of Qt, so maybe this is a BeOS GUI thing.
    // At any rate, QProcess still works, so use that on Haiku.
    //
    // Otherwise, just use execve(), since QProcess swallows stdout and
    // stderr, and it's cumbersome to redirect them back to the terminal.
#ifdef __HAIKU__
    QString argv0 = QString("%1/%2")
        .arg(path)
        .arg(exe);

    setenv("GARGLK_INI", path, 0);

    QStringList args;

    if (std::strstr(flags, "-") != nullptr)
        args = QStringList({flags, game});
    else
        args = QStringList({game});

    QProcess proc;
    proc.start(argv0, args);

    if (!proc.waitForStarted(5000))
    {
        winmsg("Could not start 'terp.\nSorry.");
        return 1;
    }

    proc.waitForFinished(-1);

    if (proc.exitStatus() != QProcess::NormalExit)
        return 1;
    else
        return proc.exitCode();
#else
    std::string argv0 = QString("%1/%2")
        .arg(path)
        .arg(exe).toStdString();
    char *argv[4] = { const_cast<char *>(argv0.c_str()) };

    setenv("GARGLK_INI", path, 0);

    if (std::strstr(flags, "-") != nullptr)
    {
        argv[1] = const_cast<char *>(flags);
        argv[2] = const_cast<char *>(game);
    }
    else
    {
        argv[1] = const_cast<char *>(game);
    }

    execv(argv[0], argv);
    winmsg("Could not start 'terp.\nSorry.");
    return 1;
#endif
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString story;

    // Find the directory that contains the interpreters. By default
    // this is GARGLK_INTERPRETER_DIR but if that is not set, it is the
    // containing directory of the gargoyle executable.
#ifdef GARGLK_INTERPRETER_DIR
    QString dir = GARGLK_INTERPRETER_DIR;
#else
    QString dir = winpath();
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
    return rungame(dir_string.c_str(), story_string.c_str());
}
