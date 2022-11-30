/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2022 <tsujan2000@gmail.com>
 *
 * FeatherNotes is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherNotes is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QTextStream>

#include "singleton.h"
#if not defined (Q_OS_WIN)
#include "signalDaemon.h"
#endif

#include <QLibraryInfo>
#include <QTranslator>

int main(int argc, char *argv[])
{
    const QString name = "FeatherNotes";
    const QString version = "1.1.1";
    const QString option = QString::fromUtf8 (argv[1]);
    if (option == "--help" || option == "-h")
    {
        QTextStream out (stdout);
        out << "FeatherNotes - Lightweight Qt hierarchical notes-manager\n"\
               "\nUsage:\n	feathernotes [options] [file] "\
               "Open the specified file\nOptions:\n"\
               "--version or -v   Show version information and exit.\n"\
               "--help            Show this help and exit.\n"\
               "-m, --min         Start minimized.\n"\
               "-t, --tray        Start iconified to tray if there is a tray.\n\n";
        return 0;
    }
    else if (option == "--version" || option == "-v")
    {
        QTextStream out (stdout);
        out << name << " " << version << Qt::endl;
        return 0;
    }

    FeatherNotes::FNSingleton singleton (argc, argv);
    singleton.setApplicationName (name);
    singleton.setApplicationVersion (version);

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    singleton.setAttribute (Qt::AA_UseHighDpiPixmaps, true);
#endif

    QStringList langs (QLocale::system().uiLanguages());
    QString lang; // bcp47Name() doesn't work under vbox
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    if (qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
#else
    if (qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
#endif
    {
        singleton.installTranslator (&qtTranslator);
    }
    else if (!langs.isEmpty())
    {
        lang = langs.first().split (QLatin1Char ('_')).first();
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        if (qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
#else
        if (qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
#endif
        {
            singleton.installTranslator (&qtTranslator);
        }
    }

    QTranslator FPTranslator;
#if defined (Q_OS_HAIKU)
    if (FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "/translations"))
#elif defined (Q_OS_WIN)
    if (FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "\\..\\data\\translations\\translations"))
#else
    if (FPTranslator.load ("feathernotes_" + lang, QStringLiteral (DATADIR) + "/feathernotes/translations"))
#endif
    {
        singleton.installTranslator (&FPTranslator);
    }

    QStringList info;
    if (argc > 1)
    {
        info << QString::fromUtf8 (argv[1]);
        if (argc > 2)
            info << QString::fromUtf8 (argv[2]);
    }

    if (!singleton.isPrimaryInstance())
    {
        singleton.sendInfo (info); // is sent to the primary instance
        return 0;
    }

#if not defined (Q_OS_WIN)
    // Handle SIGQUIT, SIGINT, SIGTERM and SIGHUP (-> https://en.wikipedia.org/wiki/Unix_signal).
    FeatherNotes::signalDaemon D;
    D.watchUnixSignals();
    QObject::connect (&D, &FeatherNotes::signalDaemon::sigQUIT,
                      &singleton, &FeatherNotes::FNSingleton::quitSignalReceived);
#endif

    QObject::connect (&singleton, &QCoreApplication::aboutToQuit, &singleton, &FeatherNotes::FNSingleton::quitting);
    singleton.openFile (info);

    return singleton.exec();
}
