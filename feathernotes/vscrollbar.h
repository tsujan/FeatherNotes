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

#ifndef VSCROLLBAR_H
#define VSCROLLBAR_H

#include <QScrollBar>
#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
#include <QWheelEvent>
#endif

namespace FeatherNotes {

/* We want faster mouse wheel scrolling
   when the mouse cursor in on the scrollbar. */
class VScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    VScrollBar (QWidget *parent = nullptr);

protected:
    bool event (QEvent *event);

private:
    int defaultWheelSpeed;
};

#if (QT_VERSION == QT_VERSION_CHECK(5,14,0))
/* Workaround. */
class HScrollBar : public QScrollBar
{
    Q_OBJECT
public:
    HScrollBar (QWidget *parent = nullptr) : QScrollBar (parent) {};

protected:
    void wheelEvent (QWheelEvent *event);
};
#endif

}

#endif // VSCROLLBAR_H
