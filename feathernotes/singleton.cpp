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


#include "singleton.h"
#include "feathernotesadaptor.h"

#ifdef HAS_X11
#include "x11.h"
#endif

namespace FeatherNotes {

static const char *serviceName = "org.feathernotes.FeatherNotes";
static const char *ifaceName = "org.feathernotes.Application";

FNSingleton::FNSingleton (int &argc, char **argv) : QApplication (argc, argv)
{
#ifdef HAS_X11
#if defined Q_OS_LINUX || defined Q_OS_FREEBSD || defined Q_OS_OPENBSD || defined Q_OS_NETBSD || defined Q_OS_HURD
    isX11_ = (QString::compare (QGuiApplication::platformName(), "xcb", Qt::CaseInsensitive) == 0);
#else
    isX11_ = false;
#endif
#else
    isX11_ = false;
#endif

    setQuitOnLastWindowClosed (false); // because windows can be iconified into the tray

    if (isX11_)
        isWayland_ = false;
    else
        isWayland_ = (QString::compare (QGuiApplication::platformName(), "wayland", Qt::CaseInsensitive) == 0);

    trayChecked_ = false;

    isPrimaryInstance_ = false;
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.isConnected()) // interpret it as the lack of D-Bus
        isPrimaryInstance_ = true;
    else if (dbus.registerService (QLatin1String (serviceName)))
    {
        isPrimaryInstance_ = true;
        new FeatherNotesAdaptor (this);
        dbus.registerObject (QStringLiteral ("/Application"), this);
    }
}
/*************************/
FNSingleton::~FNSingleton()
{
    qDeleteAll (Wins);
}
/*************************/
void FNSingleton::quitting()
{
    for (int i = 0; i < Wins.size(); ++i)
        Wins.at (i)->quitting();
}
/*************************/
void FNSingleton::sendInfo (const QStringList &info)
{
    QDBusConnection dbus = QDBusConnection::sessionBus();
    QDBusInterface iface (QLatin1String (serviceName),
                          QStringLiteral ("/Application"),
                          QLatin1String (ifaceName), dbus, this);
    iface.call (QStringLiteral ("openFile"), info);
}
/*************************/
void FNSingleton::openFile (const QStringList &info)
{
    /* If there is an FN window with the same file path, ignore the command-line option
       and activate it. This is the whole point of making FN a single-instance app. */
    if (!Wins.isEmpty())
    {
        QString filePath;
        if (!info.isEmpty())
        {
            if (info.at (0) != "--min" && info.at (0) != "-m"
                && info.at (0) != "--tray" && info.at (0) != "-t")
            {
                filePath = info.at (0);
            }
            else if (info.size() > 1)
                filePath = info.at (1);
        }
        if (filePath.isEmpty())
            filePath = lastFile_;
        for (int i = 0; i < Wins.size(); ++i)
        {
            if (Wins.at (i)->currentPath() == filePath)
            {
                Wins.at (i)->activateFNWindow();
                return;
            }
        }
    }

    FN *fn = new FN (info, nullptr);
    Wins.append (fn);
}
/*************************/
void FNSingleton::removeWin (FN *win)
{
    Wins.removeOne (win);
    win->deleteLater();
}

}
