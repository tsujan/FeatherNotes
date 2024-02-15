/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2024 <tsujan2000@gmail.com>
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

#include <QApplication>
#include "x11.h"

#include <X11/Xatom.h>

namespace FeatherNotes {

/*************************************************************
 *** These are all X11 related functions FeatherNotes uses ***
 ***       because Qt is not rooted enough in X11.         ***
 *************************************************************/

static inline Display* getDisplay()
{
    if (auto x11NativeInterfce = qApp->nativeInterface<QNativeInterface::QX11Application>())
        return x11NativeInterfce->display();
    return nullptr;
}

// Get the curent virtual desktop.
long fromDesktop()
{
    long res = -1;

    Display *disp = getDisplay();
    if (!disp) return res;

    Atom actual_type;
    int actual_format;
    long unsigned nitems;
    long unsigned bytes;
    long *data = nullptr;
    int status;

    /* QX11Info::appRootWindow() or even RootWindow (disp, 0)
       could be used instead of XDefaultRootWindow (disp) */
    status = XGetWindowProperty (disp, XDefaultRootWindow (disp),
                                 XInternAtom (disp, "_NET_CURRENT_DESKTOP", True),
                                 0, (~0L), False, AnyPropertyType,
                                 &actual_type, &actual_format, &nitems, &bytes,
                                 (unsigned char**)&data);
    if (status != Success) return res;

    if (data)
    {
        res = *data;
        XFree (data);
    }

    return res;
}
/*************************/
// Get the desktop of a window.
long onWhichDesktop (Window w)
{
    long res = -1;

    Display *disp = getDisplay();
    if (!disp) return res;

    Atom wm_desktop = XInternAtom (disp, "_NET_WM_DESKTOP", False);
    Atom type_ret;
    int fmt_ret;
    unsigned long nitems_ret;
    unsigned long bytes_after_ret;

    long *desktop = nullptr;

    int status = XGetWindowProperty (disp, w,
                                     wm_desktop,
                                     0, 1, False, XA_CARDINAL,
                                     &type_ret, &fmt_ret, &nitems_ret, &bytes_after_ret,
                                     (unsigned char**)&desktop);
    if (status != Success) return res;
    if (desktop)
    {
        res = (long)desktop[0];
        XFree (desktop);
    }

    return res;
}
/*************************/
// Adapted from gdk_x11_window_move_to_current_desktop(),
// which was defined in gdkwindow-x11.c.
void moveToCurrentDesktop (Window w)
{
    Display *disp = getDisplay();
    if (!disp) return;

    Atom type_ret;
    int fmt_ret;
    unsigned long nitems_ret;
    unsigned long bytes_after_ret;
    long *desktop = nullptr;
    long *current_desktop;

    int status = XGetWindowProperty (disp, XDefaultRootWindow (disp),
                                     XInternAtom (disp, "_NET_CURRENT_DESKTOP", False),
                                     0, 1, False, XA_CARDINAL,
                                     &type_ret, &fmt_ret, &nitems_ret, &bytes_after_ret,
                                     (unsigned char**)&desktop);
    if (status != Success) return;
    if (type_ret == XA_CARDINAL)
    {
        XClientMessageEvent xclient;
        current_desktop = desktop;

        memset (&xclient, 0, sizeof (xclient));
        xclient.type = ClientMessage;
        xclient.serial = 0;
        xclient.send_event = True;
        xclient.window = w;
        xclient.message_type = XInternAtom (disp, "_NET_WM_DESKTOP", False);
        xclient.format = 32;

        xclient.data.l[0] = *current_desktop;
        xclient.data.l[1] = 0;
        xclient.data.l[2] = 0;
        xclient.data.l[3] = 0;
        xclient.data.l[4] = 0;

        XSendEvent (disp,
                    XDefaultRootWindow (disp),
                    False,
                    SubstructureRedirectMask | SubstructureNotifyMask,
                    (XEvent *)&xclient);

        XFree (current_desktop);
    }
    else if (desktop)
        XFree (desktop);
}

}
