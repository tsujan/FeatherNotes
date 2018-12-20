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

#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <QInputEvent>
#include <QApplication>
#include <QTreeView>

namespace FeatherNotes {

/* We don't want Ctrl + left click to deselect a selected node in the single-selection mode. */
class TreeView : public QTreeView
{
    Q_OBJECT
public:
    TreeView (QWidget *parent = nullptr) : QTreeView (parent) {
        setDragDropMode (QAbstractItemView::InternalMove);
        setHeaderHidden (true);
        setAnimated (true);
        setDragEnabled (true);
        setAcceptDrops (true);
        setDropIndicatorShown (true);
        setDefaultDropAction (Qt::IgnoreAction);
        setSelectionMode (QAbstractItemView::SingleSelection);
        setSelectionBehavior (QAbstractItemView::SelectRows);
        setAlternatingRowColors (false);
        setVerticalScrollMode (QAbstractItemView::ScrollPerItem);
        setHorizontalScrollMode (QAbstractItemView::ScrollPerPixel);
        setAutoScroll (true);
        setEditTriggers (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        setTextElideMode (Qt::ElideRight);
    }

protected:
    /* see "qabstractitemview.cpp" */
    virtual QItemSelectionModel::SelectionFlags selectionCommand (const QModelIndex &index, const QEvent *event = nullptr) const {
        Qt::KeyboardModifiers keyModifiers = Qt::NoModifier;
        if (event)
        {
            switch (event->type()) {
                case QEvent::MouseButtonDblClick:
                case QEvent::MouseButtonPress:
                case QEvent::MouseButtonRelease:
                case QEvent::MouseMove:
                case QEvent::KeyPress:
                case QEvent::KeyRelease:
                    keyModifiers = (static_cast<const QInputEvent*>(event))->modifiers();
                    break;
                default:
                    keyModifiers = QApplication::keyboardModifiers();
            }
        }
        if (selectionMode() == QAbstractItemView::SingleSelection && (keyModifiers & Qt::ControlModifier) && selectionModel()->isSelected (index))
            return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
        else
            return QTreeView::selectionCommand (index, event);
    }
};

}

#endif // TREEVIEW_H
