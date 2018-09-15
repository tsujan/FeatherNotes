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

#ifndef SPINBOX_H
#define SPINBOX_H

#include <QSpinBox>

namespace FeatherNotes {

/* Forget about the focus-out event and emit the signal
   editingFinished() only if Return/Enter is pressed. */
class SpinBox : public QSpinBox
{
public:
    SpinBox (QWidget *p = nullptr) : QSpinBox (p) {}

    bool event (QEvent *event) {
#ifdef QT_KEYPAD_NAVIGATION
        if (event->type() == QEvent::EnterEditFocus
                || event->type() == QEvent::LeaveEditFocus)
        {
            return QWidget::event(event);
        }
        else
#endif
            return QSpinBox::event (event);
    }

protected:
    void focusOutEvent (QFocusEvent*) {return;}
};

}

#endif // SPINBOX_H
