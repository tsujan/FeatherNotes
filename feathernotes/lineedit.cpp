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

#include "lineedit.h"
#include <QToolButton>
#include <QApplication>

namespace FeatherNotes {

LineEdit::LineEdit (QWidget *parent)
    : QLineEdit (parent)
{
    klear = nullptr;
    returnOnClear = true;
    setClearButtonEnabled (true);
    QList<QToolButton*> list = findChildren<QToolButton*>();
    if (list.isEmpty()) return;
    QToolButton *clearButton = list.at (0);
    if (clearButton)
    {
        clearButton->setToolTip ("Clear text (Ctrl+K)");
        connect (clearButton, &QAbstractButton::clicked, this, &LineEdit::Klear);
    }
    connect (this, &QLineEdit::editingFinished, this, &LineEdit::unfocused);
    connect (this, &LineEdit::receivedFocus, this, &LineEdit::focused);
}
/*************************/
LineEdit::~LineEdit()
{
    if (klear) delete klear;
}
/*************************/
void LineEdit::focusInEvent (QFocusEvent * ev)
{
    /* first do what QLineEdit does */
    QLineEdit::focusInEvent (ev);
    emit receivedFocus();
}
/*************************/
void LineEdit::Klear()
{
    if (!qobject_cast<QToolButton*>(QObject::sender()))
        clear();
    /* we'll need this for clearing found matches highlighting
       because the compiler won't know that clearButton is a QObject */
    if (returnOnClear)
        returnPressed();
}
/*************************/
void LineEdit::unfocused()
{
    /* filter out pressing of Return or Enter */
    if (hasFocus()) return;

    if (klear)
    {
        disconnect (klear, &QShortcut::activated, this, &LineEdit::Klear);
        delete klear;
        klear = nullptr;
    }
}
/*************************/
void LineEdit::focused()
{
    if (klear) return;

    klear = new QShortcut (QKeySequence (tr ("Ctrl+K", "Clear text")), this);
    connect (klear, &QShortcut::activated, this, &LineEdit::Klear);
}

}
