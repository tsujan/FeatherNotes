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

#ifndef X11_H
#define X11_H

#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_OPENBSD
#include <X11/Xlib.h>
#endif

namespace FeatherNotes {

#if defined Q_WS_X11 || defined Q_OS_LINUX || defined Q_OS_OPENBSD
long fromDesktop();
#endif

long onWhichDesktop (Window w);
void moveToCurrentDesktop (Window w);

}

#endif // X11_H
