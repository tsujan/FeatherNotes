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

#ifndef TREEDELEGATE_H
#define TREEDELEGATE_H

#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QPalette>
#include <QApplication>

namespace FeatherNotes {

class treeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    treeDelegate (QObject *parent) : QStyledItemDelegate (parent) {}

    QWidget* createEditor (QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        /* return the default editor but ensure that its background isn't transparent */
        QWidget *editor = QStyledItemDelegate::createEditor (parent, option, index);
        QPalette p = editor->palette();
        p.setColor (QPalette::Text, qApp->palette().text().color());
        p.setColor (QPalette::Base, qApp->palette().color (QPalette::Base));
        editor->setPalette (p);
        return editor;
    }
};

}

#endif // TREEDELEGATE_H
