/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016 <tsujan2000@gmail.com>
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

#include <csignal>
#include <QApplication>
#include "fn.h"

void handleQuitSignals (const std::vector<int>& quitSignals)
{
    auto handler = [](int sig) ->void {
        Q_UNUSED (sig);
        //printf("\nUser signal = %d.\n", sig);
        QCoreApplication::quit();
    };

    for (int sig : quitSignals )
        signal (sig, handler); // handle these signals by quitting gracefully
}

int main(int argc, char *argv[])
{
    const QString name = "FeatherNotes";
    const QString version = "0.4.5";
    const QString option = QString::fromUtf8 (argv[1]);
    if (option == "--help" || option == "-h")
    {
        QTextStream out (stdout);
        out << "FeatherNotes - Lightweight Qt5 hierarchical notes-manager\n"\
               "\nUsage:\n	feathernotes [options] [file] "\
               "Open the specified file\nOptions:\n"\
               "--version or -v  Show version information and exit.\n"\
               "--help          Show this help and exit\n"\
               "-m, --min	Start minimized\n"\
               "-t, --tray	Start iconified to tray if there is a tray icon\n\n";
        return 0;
    }
    else if (option == "--version" || option == "-v")
    {
        QTextStream out (stdout);
        out << name << " " << version <<  endl;
        return 0;
    }

    QApplication app (argc, argv);
    app.setApplicationName (name);
    app.setApplicationVersion (version);

    handleQuitSignals ({SIGQUIT, SIGINT, SIGTERM, SIGHUP}); // -> https://en.wikipedia.org/wiki/Unix_signal

#if QT_VERSION >= 0x050500
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QStringList langs (QLocale::system().uiLanguages());
    QString lang; // bcp47Name() doesn't work under vbox
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
    if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
    { // not needed; doesn't happen
        if (!langs.isEmpty())
        {
            lang = langs.first().split (QLatin1Char ('-')).first();
            qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath));
        }
    }
    app.installTranslator (&qtTranslator);

    QTranslator FPTranslator;
    FPTranslator.load ("feathernotes_" + lang, DATADIR "/feathernotes/translations");
    app.installTranslator (&FPTranslator);

    QString message;
    if (argc > 1)
        message += QString::fromUtf8 (argv[1]);
    if (argc > 2)
    {
        message += "\n\r"; // a string that can't be used in file names
        message += QString::fromUtf8 (argv[2]);
    }

    FeatherNotes::FN w (message);
    //w.show();

    return app.exec();
}
