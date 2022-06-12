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

#ifndef SPINBOX_H
#define SPINBOX_H

#include <QSpinBox>
#include <QKeyEvent>

namespace FeatherNotes {

/* We need a signal that is emitted only when Return/Enter is pressed. */
class SpinBox : public QSpinBox
{
    Q_OBJECT
public:
    SpinBox (QWidget *p = nullptr) : QSpinBox (p) {}

signals:
    void returnPressed();

protected:
    void keyPressEvent (QKeyEvent *event) {
        QAbstractSpinBox::keyPressEvent (event);
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
            emit returnPressed();
    }
};

}

#endif // SPINBOX_H
