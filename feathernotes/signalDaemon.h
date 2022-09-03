/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2022 <tsujan2000@gmail.com>
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

#ifndef SIGNALDAEMON_H
#define SIGNALDAEMON_H

#include <QSocketNotifier>

namespace FeatherNotes {

class signalDaemon : public QObject
{
    Q_OBJECT
public:
    signalDaemon (QObject *parent = nullptr);
    ~signalDaemon();

    // Should be called for watching the supported Unix signals.
    void watchUnixSignals();

    // Unix signal handlers (for SIGHUP, SIGTERM, SIGINT and SIGQUIT).
    static void hupSignalHandler (int unused);
    static void termSignalHandler (int unused);
    static void intSignalHandler (int unused);
    static void quitSignalHandler (int unused);

signals:
    //void sigTERM();
    //void sigHUP();
    //void sigINT();
    void sigQUIT(); // We don't need different signals.

private slots:
    // Qt signal handlers.
    void handleSigHup();
    void handleSigTerm();
    void handleSigINT();
    void handleSigQUIT();

private:
    // Returns true if the signal can be watched,
    // although the returned value isn't used.
    bool watchSignal (int sig);

    static int sighupFd[2];
    static int sigtermFd[2];
    static int sigintFd[2];
    static int sigquitFd[2];

    QSocketNotifier *snHup_;
    QSocketNotifier *snTerm_;
    QSocketNotifier *snInt_;
    QSocketNotifier *snQuit_;
};

}

#endif // SIGNALDAEMON_H
