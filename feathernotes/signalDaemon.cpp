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

#include "signalDaemon.h"

#include <QDebug>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace FeatherNotes {

int signalDaemon::sighupFd[2];
int signalDaemon::sigtermFd[2];
int signalDaemon::sigintFd[2];
int signalDaemon::sigquitFd[2];

signalDaemon::signalDaemon (QObject *parent) : QObject (parent)
{
    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sighupFd) == 0)
    {
        snHup = new QSocketNotifier (sighupFd[1], QSocketNotifier::Read, this);
        connect (snHup, &QSocketNotifier::activated, this, &signalDaemon::handleSigHup);
    }
    else
    {
        snHup = nullptr;
        qDebug ("Couldn't create HUP socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigtermFd) == 0)
    {
        snTerm = new QSocketNotifier (sigtermFd[1], QSocketNotifier::Read, this);
        connect (snTerm, &QSocketNotifier::activated, this, &signalDaemon::handleSigTerm);
    }
    else
    {
        snTerm = nullptr;
        qDebug ("Couldn't create TERM socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigintFd) == 0)
    {
        snInt = new QSocketNotifier (sigintFd[1], QSocketNotifier::Read, this);
        connect (snInt, &QSocketNotifier::activated, this, &signalDaemon::handleSigINT);
    }
    else
    {
        snInt = nullptr;
        qDebug ("Couldn't create INT socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigquitFd) == 0)
    {
        snQuit = new QSocketNotifier (sigquitFd[1], QSocketNotifier::Read, this);
        connect (snQuit, &QSocketNotifier::activated, this, &signalDaemon::handleSigQUIT);
    }
    else
    {
        snQuit = nullptr;
        qDebug ("Couldn't create QUIT socketpair");
    }
}
/*************************/
signalDaemon::~signalDaemon()
{
    delete snHup;
    delete snTerm;
    delete snInt;
    delete snQuit;
}
/*************************/
// Write a byte to the "write" end of a socket pair and return.
void signalDaemon::hupSignalHandler (int)
{
    char a = 1;
    auto w = ::write (sighupFd[0], &a, sizeof (a));
    Q_UNUSED (w);
}

void signalDaemon::termSignalHandler (int)
{
    char a = 1;
    auto w = ::write (sigtermFd[0], &a, sizeof (a));
    Q_UNUSED (w);
}

void signalDaemon::intSignalHandler (int)
{
    char a = 1;
    auto w = ::write (sigintFd[0], &a, sizeof (a));
    Q_UNUSED (w);
}

void signalDaemon::quitSignalHandler (int)
{
    char a = 1;
    auto w = ::write (sigquitFd[0], &a, sizeof (a));
    Q_UNUSED (w);
}
/*************************/
// Read the byte and emit the corresponding Qt signal.
void signalDaemon::handleSigHup()
{
    snHup->setEnabled (false);
    char tmp;
    if (::read (sighupFd[1], &tmp, sizeof (tmp)) != -1)
        emit sigHUP();
    snHup->setEnabled (true);
}

void signalDaemon::handleSigTerm()
{
    snTerm->setEnabled (false);
    char tmp;
    if (::read (sigtermFd[1], &tmp, sizeof (tmp)) != -1)
        emit sigTERM();
    snTerm->setEnabled (true);
}

void signalDaemon::handleSigINT()
{
    snInt->setEnabled (false);
    char tmp;
    if (::read (sigintFd[1], &tmp, sizeof (tmp)) != -1)
        emit sigINT();
    snInt->setEnabled (true);
}

void signalDaemon::handleSigQUIT()
{
    snQuit->setEnabled (false);
    char tmp;
    if (::read (sigquitFd[1], &tmp, sizeof (tmp)) != -1)
        emit sigQUIT();
    snQuit->setEnabled (true);
}

}
