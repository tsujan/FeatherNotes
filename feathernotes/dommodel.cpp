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
    dropIndex_ (QModelIndex()), dropRow_ (-1), dragged_ (nullptr)
{
    rootItem_ = new DomItem (domDocument, 0);
}
/*************************/
DomModel::~DomModel()
{
    delete rootItem_;
}
/*************************/
int DomModel::columnCount (const QModelIndex &/*parent*/) const
{
    return 1;
}
/*************************/
QVariant DomModel::data (const QModelIndex &indx, int role) const
{
    if (!indx.isValid())
        return QVariant();

    if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::DecorationRole)
        return QVariant();

    DomItem *item = static_cast<DomItem*>(indx.internalPointer());

    QDomNode node = item->node();
    /*QTextStream out (stdout);
    out << node.nodeType() << "\n";*/
    QDomNamedNodeMap attributeMap = node.attributes();

    if (indx.column() == 0)
    {
        if (role == Qt::DecorationRole)
        {
            QString str = attributeMap.namedItem ("icon").nodeValue();
            if (str.isEmpty())
                return QVariant();
            QImage image;
            image.loadFromData (QByteArray::fromBase64 (str.toUtf8()));
            return QVariant (QIcon (QPixmap::fromImage (image)));
        }
        return attributeMap.namedItem ("name").nodeValue();
    }
    else
        return QVariant();
}
/*************************/
bool DomModel::setData (const QModelIndex &indx, const QVariant &value, int role)
{
    if (indx.isValid() && role == Qt::EditRole)
    {
        DomItem *item = static_cast<DomItem*>(indx.internalPointer());
        QDomNode node = item->node();
        QDomNamedNodeMap attributeMap = node.attributes();
        QString str = value.toString();
        if (attributeMap.namedItem ("name").nodeValue() != str)
        {
            attributeMap.namedItem ("name").setNodeValue (str);
            emit dataChanged (indx, indx);
            return true;
        }
    }
    return false;
}
/*************************/
Qt::ItemFlags DomModel::flags (const QModelIndex &indx) const
{
    if (!indx.isValid())
        return Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;

    return QAbstractItemModel::flags (indx) | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}
/*************************/
QModelIndex DomModel::index (int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex (row, column, parent))
        return QModelIndex();

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem_;
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

    if (parentItem == rootItem_)
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
        parentItem = rootItem_;
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
        parentItem = rootItem_;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    if (rowCount (parent) == 0 || row >= rowCount (parent))
    {
        for (int i = 0; i < count; ++i)
            parentItem->addChild (dragged_);
    }
    else// if (row < rowCount (parent))
    {
        for (int i = 0; i < count; ++i)
            parentItem->insertAt (row, dragged_);
    }

    endInsertRows();
    /* we need this because the nodes may be nested */
    emit layoutChanged();
    emit treeChanged();

    if (dropRow_ > -1)
    {
        emit droppedAtIndex (index (dropRow_, 0, parent)); // announce the DND end
        /* DND is finished; reset its variables */
        dropRow_ = -1;
        dropIndex_ = QModelIndex();
        dragged_ = nullptr;
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
    if (dropRow_ > row && dropIndex_ ==  parent)
        --dropRow_;
    /* no redundant operation */
    if (dropRow_ == row && dropIndex_ ==  parent)
        return true;

    if (dropRow_ > -1)
        emit dragStarted (index (row, 0, parent)); // announce the DND start

    beginRemoveRows (parent, row, row + count - 1);

    DomItem *parentItem;
    if (!parent.isValid())
        parentItem = rootItem_;
    else
        parentItem = static_cast<DomItem*>(parent.internalPointer());

    for (int k = 0; k < count; ++k)
    {
        /* maybe it's about DND */
        dragged_ = parentItem->takeChild (row + (count - 1 - k));
        if (dropRow_ == -1)
        {
            /* no DND but deletion */
            delete dragged_;
            dragged_ = nullptr;
        }
    }

    endRemoveRows();
    emit treeChanged();

    if (dropRow_ > -1)
        insertRows (dropRow_, 1, dropIndex_);

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
        parentItem = rootItem_;
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
        parentItem = rootItem_;
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
        parentItem = rootItem_;
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
        parentItem = rootItem_;
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
        dropRow_ = 0;
    else
        dropRow_ = row;
    dropIndex_ = parent;

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
        descendants << index (i, 0, ancestor);
    for (int i = 0; i < descendants.count(); ++i)
        descendants << allDescendants (descendants[i]);

    return descendants;
}
/*************************/
// Find the visually "adjacent" node of this node in the tree.
QModelIndex DomModel::adjacentIndex (const QModelIndex &indx, bool down) const
{
    QModelIndex rslt;
    if (down)
    {
        /* if this index has a child, return it... */
        if (hasChildren (indx))
            return index (0, 0, indx);
        else // ... otherwise, see if it has a lower sibling...
        {
            if (!(rslt = sibling (indx.row() + 1, 0, indx)).isValid())
            {
                /* ... and if it doesn't have any, go to its
                   parent and search for its next sibling */
                QModelIndex p;
                if (!(p = parent (indx)).isValid())
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
        if (!(rslt = sibling (indx.row() - 1, 0, indx)).isValid())
            rslt = parent (indx);
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
