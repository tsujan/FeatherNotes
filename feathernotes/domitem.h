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

#ifndef DOMITEM_H
#define DOMITEM_H

#include <QDomNode>
#include <QHash>

namespace FeatherNotes {

class DomItem
{
public:
    DomItem (QDomNode &node, int row, DomItem *parent = nullptr);
    ~DomItem();
    DomItem *child (int i);
    int childCount();
    DomItem *parent();
    QDomNode node() const;
    int row();
    void addChild (DomItem *item);
    void insertAt (int n, DomItem *item);
    void moveUp (int n);
    void moveLeft (int n);
    void moveDown (int n);
    void moveRight (int n);
    DomItem *takeChild(int n);

private:
    QDomNode domNode;
    QHash<int,DomItem*> childItems;
    DomItem *parentItem;
    int rowNumber;
};

}

#endif // DOMITEM_H
