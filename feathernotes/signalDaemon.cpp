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

#include <signal.h>
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
        snHup_ = new QSocketNotifier (sighupFd[1], QSocketNotifier::Read, this);
        connect (snHup_, &QSocketNotifier::activated, this, &signalDaemon::handleSigHup);
    }
    else
    {
        snHup_ = nullptr;
        qDebug ("Couldn't create HUP socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigtermFd) == 0)
    {
        snTerm_ = new QSocketNotifier (sigtermFd[1], QSocketNotifier::Read, this);
        connect (snTerm_, &QSocketNotifier::activated, this, &signalDaemon::handleSigTerm);
    }
    else
    {
        snTerm_ = nullptr;
        qDebug ("Couldn't create TERM socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigintFd) == 0)
    {
        snInt_ = new QSocketNotifier (sigintFd[1], QSocketNotifier::Read, this);
        connect (snInt_, &QSocketNotifier::activated, this, &signalDaemon::handleSigINT);
    }
    else
    {
        snInt_ = nullptr;
        qDebug ("Couldn't create INT socketpair");
    }

    if (::socketpair (AF_UNIX, SOCK_STREAM, 0, sigquitFd) == 0)
    {
        snQuit_ = new QSocketNotifier (sigquitFd[1], QSocketNotifier::Read, this);
        connect (snQuit_, &QSocketNotifier::activated, this, &signalDaemon::handleSigQUIT);
    }
    else
    {
        snQuit_ = nullptr;
        qDebug ("Couldn't create QUIT socketpair");
    }
}
/*************************/
signalDaemon::~signalDaemon()
{
    delete snHup_;
    delete snTerm_;
    delete snInt_;
    delete snQuit_;
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
    snHup_->setEnabled (false);
    char tmp;
    auto w = ::read (sighupFd[1], &tmp, sizeof (tmp));
    Q_UNUSED (w);
    //emit sigHUP();
    emit sigQUIT();
    snHup_->setEnabled (true);
}

void signalDaemon::handleSigTerm()
{
    snTerm_->setEnabled (false);
    char tmp;
    auto w = ::read (sigtermFd[1], &tmp, sizeof (tmp));
    Q_UNUSED (w);
    //emit sigTERM();
    emit sigQUIT();
    snTerm_->setEnabled (true);
}

void signalDaemon::handleSigINT()
{
    snInt_->setEnabled (false);
    char tmp;
    auto w = ::read (sigintFd[1], &tmp, sizeof (tmp));
    Q_UNUSED (w);
    //emit sigINT();
    emit sigQUIT();
    snInt_->setEnabled (true);
}

void signalDaemon::handleSigQUIT()
{
    snQuit_->setEnabled (false);
    char tmp;
    auto w = ::read (sigquitFd[1], &tmp, sizeof (tmp));
    Q_UNUSED (w);
    emit sigQUIT();
    snQuit_->setEnabled (true);
}
/*************************/
bool signalDaemon::watchSignal (int sig) {
    struct sigaction sigact;
    switch (sig) {
    case SIGHUP:
        if (snHup_ == nullptr)
            return false;
        sigact.sa_handler = hupSignalHandler;
        break;
    case SIGTERM:
        if (snTerm_ == nullptr)
            return false;
        sigact.sa_handler = termSignalHandler;
        break;
    case SIGINT:
        if (snInt_ == nullptr)
            return false;
        sigact.sa_handler = intSignalHandler;
        break;
    case SIGQUIT:
        if (snQuit_ == nullptr)
            return false;
        sigact.sa_handler = quitSignalHandler;
        break;
    default:
        return false;
    }
    sigemptyset (&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_flags |= SA_RESTART;
    if (sigaction (sig, &sigact, nullptr) != 0)
       return false;
    return true;
}
/*************************/
void signalDaemon::watchUnixSignals() {
    watchSignal (SIGHUP);
    watchSignal (SIGTERM);
    watchSignal (SIGINT);
    watchSignal (SIGQUIT);
}

}
