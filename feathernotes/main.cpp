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

#include <signal.h>
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
    QApplication app (argc, argv);

    handleQuitSignals ({SIGQUIT, SIGINT, SIGTERM, SIGHUP}); // -> https://en.wikipedia.org/wiki/Unix_signal

    if (QString::fromUtf8 (argv[1]) == "--help")
    {
        QTextStream out (stdout);
        out << "\nUsage:\n	feathernotes [options] [file] "\
               "Open the specified file\nOptions:\n"\
               "--help          Show this help and exit\n"\
               "-m, --min	Start minimized\n"\
               "-t, --tray	Start iconified to tray if there is a tray icon\n\n";
        return 0;
    }

    QString message;
    if (argc > 1)
        message += QString::fromUtf8 (argv[1]);
    if (argc > 2)
    {
        message += "\n";
        message += QString::fromUtf8 (argv[2]);
    }

    FeatherNotes::FN w (message);
    //w.show();

    return app.exec();
}
