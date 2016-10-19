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

#include <QtGui>
#include <QtXml>

#include "domitem.h"
#include "dommodel.h"

namespace FeatherNotes {

DomModel::DomModel (QDomDocument document, QObject *parent) :
    QAbstractItemModel (parent), domDocument (document),
    dropIndex (QModelIndex()), dropRow (-1), dragged (nullptr)
{
    rootItem = new DomItem (domDocument, 0);
}
/*************************/
DomModel::~DomModel()
{
    delete rootItem;
}
/*************************/
int DomModel::columnCount (const QModelIndex &/*parent*/) const
{
    return 1;
}
/*************************/
QVariant DomModel::data (const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();

    DomItem *item = static_cast<DomItem*>(index.internalPointer());

    QDomNode node = item->node();
    /*QTextStream out (stdout);
    out << node.nodeType() << "\n";*/
    QDomNamedNodeMap attributeMap = node.attributes();

    if (index.column() == 0)
        return attributeMap.namedItem ("name").nodeValue();
    else
        return QVariant();
}
/*************************/
bool DomModel::setData (const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole)
    {
        DomItem *item = static_cast<DomItem*>(index.internalPointer());
        QDomNode node = item->node();
        QDomNamedNodeMap attributeMap = node.attributes();
        QString str = value.toString();
        if (attributeMap.namedItem ("name").nodeValue() != str)
        {
            attributeMap.namedItem ("name").setNodeValue (str);
            emit dataChanged (index, index);
            return true;
        }
    }
    return false;
}
/*************************/
Qt::ItemFlags DomModel::flags (const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;

    return QAbstractItemModel::flags (index) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}
/*************************/
QModelIndex DomModel::index (int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex (row, column, parent))
        return QModelIndex();

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    DomItem *childItem = parentItem->child (row);
    if (childItem)
        return createIndex (row, column, childItem);
    else
        return QModelIndex();
}
/*************************/
QModelIndex DomModel::parent (const QModelIndex &child) const
{
    if (!child.isValid()) return QModelIndex();

    DomItem *childItem = static_cast<DomItem*>(child.internalPointer());
    DomItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex (parentItem->row(), 0, parentItem);
}
/*************************/
int DomModel::rowCount (const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    return parentItem->childCount();
}
/*************************/
bool DomModel::insertRows (int row, int count, const QModelIndex &parent)
{
    if (row < 0) return true;

    emit layoutAboutToBeChanged();
    beginInsertRows (parent, row, row + count - 1);

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    if (rowCount (parent) == 0 || row >= rowCount (parent))
    {
        for (int i = 0; i < count; ++i)
            parentItem->addChild (dragged);
    }
    else// if (row < rowCount (parent))
    {
        for (int i = 0; i < count; ++i)
            parentItem->insertAt (row, dragged);
    }

    endInsertRows();
    /* we need this because the nodes may be nested */
    emit layoutChanged();
    emit treeChanged();

    if (dropRow > -1)
    {
        /* DND is finished; reset its variables */
        dropRow = -1;
        dropIndex = QModelIndex();
        dragged = nullptr;
    }

    return true;
}
/*************************/
bool DomModel::removeRows (int row, int count, const QModelIndex &parent)
{
    if (row < 0 || rowCount (parent) == 0)
        return true;

    /* workaround for Qt's drop indicator
       pointing to an incorrect position */
    if (dropRow > row && dropIndex ==  parent)
        --dropRow;
    /* no redundant operation */
    if (dropRow == row && dropIndex ==  parent)
        return true;

    beginRemoveRows (parent, row, row + count - 1);

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    for (int k = 0; k < count; ++k)
    {
        /* maybe it's about DND */
        dragged = parentItem->takeChild (row + (count - 1 - k));
        if (dropRow == -1)
        {
            /* no DND but deletion */
            delete dragged;
            dragged = nullptr;
        }
    }

    endRemoveRows();
    emit treeChanged();

    if (dropRow > -1)
        insertRows (dropRow, 1, dropIndex);

    return true;
}
/*************************/
bool DomModel::moveUpRow (int row, const QModelIndex &parent)
{
    if (row <= 0 || rowCount (parent) == 0)
        return true;

    beginMoveRows (parent, row, row,
                   parent, row - 1);

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());
    parentItem->moveUp (row);

    endMoveRows();
    emit treeChanged();
    return true;
}
/*************************/
bool DomModel::moveLeftRow (int row, const QModelIndex &parent)
{
    if (row < 0 || rowCount (parent) == 0)
        return true;

    /* move this row above its parent */
    beginMoveRows (parent, row, row,
                   parent.parent(), parent.row());

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());
    parentItem->moveLeft (row);

    endMoveRows();
    emit treeChanged();
    return true;
}
/*************************/
bool DomModel::moveDownRow (int row, const QModelIndex &parent)
{
    if (row < 0 || rowCount (parent) == 0 || row == rowCount (parent) - 1)
        return true;

    beginMoveRows (parent, row + 1, row + 1,
                   parent, row);

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());
    parentItem->moveDown (row);

    endMoveRows();
    emit treeChanged();
    return true;
}
/*************************/
bool DomModel::moveRightRow (int row, const QModelIndex &parent)
{
    if (row <= 0 || rowCount (parent) == 0)
        return true;

    /* move this row to the bottom
       of its above node's children */
    QModelIndex aboveIndex = index (row - 1, 0, parent);
    beginMoveRows (parent, row, row,
                   aboveIndex, rowCount (aboveIndex));

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());
    parentItem->moveRight (row);

    endMoveRows();
    emit treeChanged();
    return true;
}
/*************************/
Qt::DropActions DomModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}
/*************************/
bool DomModel::dropMimeData (const QMimeData*, Qt::DropAction action,
                             int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (column > 0) return false;

    /* here we just set DND variables because we want
       removeRows() to be called before insertRows() */
    if (row == -1)
        dropRow = 0;
    else
        dropRow = row;
    dropIndex = parent;

    return true;
}
/*************************/
// Give a list of all descendants of a valid index.
// The farther is a descendant, the greater is its index position in the list.
QModelIndexList DomModel::allDescendants (const QModelIndex &ancestor) const
{
    QModelIndexList descendants;
    if (!ancestor.isValid()) return descendants; // empty

    for (int i = 0; i < rowCount (ancestor) ; ++i)
        descendants << ancestor.child (i, 0);
    for (int i = 0; i < descendants.count(); ++i)
        descendants << allDescendants (descendants[i]);

    return descendants;
}
/*************************/
// Find the visually "adjacent" node of this node in the tree.
QModelIndex DomModel::adjacentIndex (const QModelIndex &index, bool down) const
{
    QModelIndex rslt;
    if (down)
    {
        /* if this index has a child, return it... */
        if (hasChildren (index))
            return index.child (0, 0);
        else // ... otherwise, see if it has a lower sibling...
        {
            if (!(rslt = sibling (index.row() + 1, 0, index)).isValid())
            {
                /* ... and if it doesn't have any, go to its
                   parent and search for its next sibling */
                QModelIndex p;
                if (!(p = parent (index)).isValid())
                    return QModelIndex();
                while (!(rslt = sibling (p.row() + 1, 0, p)).isValid())
                {
                    if (!(p = parent (p)).isValid())
                        return QModelIndex();
                }
            }
        }
    }
    else
    {
        /* see if this index has an upper sibling... */
        if (!(rslt = sibling (index.row() - 1, 0, index)).isValid())
            rslt = parent (index);
        else // .. and if it does, return its farthest descendant
        {
            QModelIndexList list = allDescendants (rslt);
            if (list.isEmpty())
                list << rslt;
            rslt = list.last();
        }
    }

    return rslt;
}

}
