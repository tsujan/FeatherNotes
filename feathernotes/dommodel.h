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

#ifndef DOMMODEL_H
#define DOMMODEL_H

#include <QAbstractItemModel>
#include <QDomDocument>
#include <QModelIndex>
#include <QVariant>

namespace FeatherNotes {

class DomItem;

class DomModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    DomModel (QDomDocument document, QObject *parent = nullptr);
    ~DomModel();

    QVariant data (const QModelIndex &indx, int role) const;
    bool setData (const QModelIndex &indx, const QVariant &value, int role = Qt::EditRole);
    Qt::ItemFlags flags (const QModelIndex &indx) const;
    QModelIndex index (int row, int column,
                       const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent (const QModelIndex &child) const;
    int rowCount (const QModelIndex &parent = QModelIndex()) const;
    int columnCount (const QModelIndex &parent = QModelIndex()) const;
    bool insertRows (int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeRows (int row, int count, const QModelIndex &parent = QModelIndex());
    bool moveUpRow (int row, const QModelIndex &parent = QModelIndex());
    bool moveLeftRow (int row, const QModelIndex &parent = QModelIndex());
    bool moveDownRow (int row, const QModelIndex &parent = QModelIndex());
    bool moveRightRow (int row, const QModelIndex &parent = QModelIndex());
    Qt::DropActions supportedDropActions() const;
    bool dropMimeData (const QMimeData*, Qt::DropAction action,
                       int row, int column, const QModelIndex &parent);
    QModelIndexList allDescendants (const QModelIndex &ancestor) const;
    QModelIndex adjacentIndex (const QModelIndex &indx, bool down) const;

    QDomDocument domDocument;

signals:
    void treeChanged(); // For informing the user.
    void dragStarted (const QModelIndex &draggedIndex); // The index isn't needed but is used as a safeguard.
    void droppedAtIndex (const QModelIndex &droppedIndex);

private:
    DomItem *rootItem_;
    /* DVD variables: */
    QModelIndex dropIndex_;
    int dropRow_;
    DomItem *dragged_;
};

}

#endif // DOMMODEL_H
