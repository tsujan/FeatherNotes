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

#include <QtXml>
#include "domitem.h"

namespace FeatherNotes {

DomItem::DomItem (QDomNode &node, int row, DomItem *parent)
{
    domNode = node;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
}
/*************************/
DomItem::~DomItem()
{
    QHash<int,DomItem*>::iterator it;
    for (it = childItems.begin(); it != childItems.end(); ++it)
        delete it.value();
}
/*************************/
QDomNode DomItem::node() const
{
    return domNode;
}
/*************************/
DomItem *DomItem::parent()
{
    return parentItem;
}
/*************************/
DomItem *DomItem::child (int i)
{
    if (childItems.contains (i))
        return childItems[i];

    int k = 0;
    QDomNodeList list;
    /* when this node is a QDomDocument... */
    if (!domNode.firstChildElement ("feathernotes").isNull())
        /* ... find the children of <feathernotes> */
        list = domNode.firstChildElement ("feathernotes").childNodes();
    else
    {
        list = domNode.childNodes();
        /* if there's a text, it'll be the first child */
        if (!list.isEmpty() && !list.at (0).isElement())
            k = 1;
    }
    if (i >= 0 && i + k < list.count())
    {
        QDomNode childNode = list.item (i + k);
        DomItem *childItem = new DomItem (childNode, i, this);
        childItems[i] = childItem;
        return childItem;
    }

    return nullptr;
}
/*************************/
int DomItem::childCount()
{
    int N = 0;
    while (child (N))
        ++N;
    return N;
}
/*************************/
int DomItem::row()
{
    return rowNumber;
}
/*************************/
void DomItem::addChild (DomItem *item)
{
    QDomNode itemNode;
    if (item)
        itemNode = item->node();
    else
    {
        /*QDomElement e = QDomDocument().createElement ("node");
        e.setAttribute ("name", "New Node");*/
        QDomDocument doc;
#if (QT_VERSION >= QT_VERSION_CHECK(6,8,0))
        doc.setContent (QString ("<node name=\""+ QObject::tr ("New Node") + "\"></node>").toUtf8());
#else
        doc.setContent (QString ("<node name=\""+ QObject::tr ("New Node") + "\"></node>"), false);
#endif
        itemNode = doc.documentElement();
    }

    int k = 0;
    QDomNodeList list;
    if (!domNode.firstChildElement ("feathernotes").isNull())
    {
        domNode.firstChildElement ("feathernotes").appendChild (itemNode);
        list = domNode.firstChildElement ("feathernotes").childNodes();
    }
    else
    {
        domNode.appendChild (itemNode);
        list = domNode.childNodes();
        if (!list.isEmpty() && !list.at (0).isElement())
            k = 1;
    }

    if (item)
    {
        childItems[list.count() - k - 1] = item;
        childItems[list.count() - k - 1]->rowNumber = list.count() - k - 1;
        childItems[list.count() - k - 1]->parentItem = this;
    }
}
/*************************/
void DomItem::insertAt (int n, DomItem *item)
{
    if (n < 0 || n >= childCount()) return;

    QDomNode itemNode;
    if (item)
        itemNode = item->node();
    else
    {
        QDomDocument doc;
#if (QT_VERSION >= QT_VERSION_CHECK(6,8,0))
        doc.setContent (QString ("<node name=\""+ QObject::tr ("New Node") + "\"></node>").toUtf8());
#else
        doc.setContent (QString ("<node name=\""+ QObject::tr ("New Node") + "\"></node>"), false);
#endif
        itemNode = doc.documentElement();
    }

    int k = 0;
    QDomNodeList list;
    if (!domNode.firstChildElement ("feathernotes").isNull())
    {
        domNode.firstChildElement ("feathernotes").insertBefore (itemNode, child (n)->node());
        list = domNode.firstChildElement ("feathernotes").childNodes();
    }
    else
    {
        domNode.insertBefore (itemNode, child (n)->node());
        list = domNode.childNodes();
        if (!list.isEmpty() && !list.at (0).isElement())
            k = 1;
    }

    /* back up the nth value before creating it anew */
    DomItem *tmp = childItems[n];
    if (!item)
    {
        QDomNode childNode = list.item (n + k);
        DomItem *childItem = new DomItem (childNode, n, this);
        childItems[n] = childItem;
    }
    else
    {
        childItems[n] = item;
        childItems[n]->rowNumber = n;
        childItems[n]->parentItem = this;
    }
    /* now, move down the values for all indexes > n */
    DomItem* tmp1 = nullptr;
    for (int i = n + 1; i + k < list.count(); ++i)
    {
        if (childItems.contains (i))
        {
            tmp1 = childItems[i];
            childItems[i] = tmp;
            tmp = tmp1;
        }
        else // the last index
            childItems[i] = tmp;
        /* we only need to correct the row number */
        childItems[i]->rowNumber = i;
    }
}
/*************************/
void DomItem::moveUp (int n)
{
    if (n <= 0 || n >= childCount()) return;

    if (!domNode.firstChildElement ("feathernotes").isNull())
        domNode.firstChildElement ("feathernotes").insertBefore (child (n)->node(), child (n - 1)->node());
    else
        domNode.insertBefore (child (n)->node(), child (n - 1)->node());

    DomItem *tmp = childItems[n - 1];
    childItems[n - 1] = childItems[n];
    childItems[n - 1]->rowNumber = n - 1;
    childItems[n] = tmp;
    childItems[n]->rowNumber = n;
}
/*************************/
// Move the nth child of this item above it as its sibling.
void DomItem::moveLeft (int n)
{
    if (n < 0 || n >= childCount()) return;

    DomItem *p = parent();
    /* do nothing if this is the root row */
    if (!p) return;

    int pK = 0;
    QDomNodeList pList;
    if (!p->domNode.firstChildElement ("feathernotes").isNull())
    {
        p->domNode.firstChildElement ("feathernotes").insertBefore (child (n)->node(), domNode);
        pList = p->domNode.firstChildElement ("feathernotes").childNodes();
    }
    else
    {
        p->domNode.insertBefore (child (n)->node(), domNode);
        pList = p->domNode.childNodes();
        if (!pList.isEmpty() && !pList.at (0).isElement())
            pK = 1;
    }
    int k = 0;
    QDomNodeList list = domNode.childNodes();
    if (!list.isEmpty() && !list.at (0).isElement())
        k = 1;

    /********************************
       The child should be removed.
          See takeChild() below.
     ********************************/

    /* first, back up the first child */
    DomItem *del = childItems[n];
    for (int i = n; i + k < list.count(); ++i)
    {
        childItems[i] = childItems[i + 1];
        childItems[i]->rowNumber = i;
    }
    childItems.remove (list.count() - k);

    /*********************************
      A child should be inserted into
      the parent's children at row().
          See insertAt() above.
     *********************************/

    /* back up the item at row() */
    DomItem *tmp = p->childItems[row()];

    /* replace the item at row() with the backed-up
       first child and correct its member variables */
    p->childItems[row()] = del;
    p->childItems[row()]->parentItem = p;
    p->childItems[row()]->rowNumber = row();

    DomItem* tmp1 = nullptr;
    for (int i = row() + 1; i + pK < pList.count(); ++i)
    {
        if (p->childItems.contains (i))
        {
            tmp1 = p->childItems[i];
            p->childItems[i] = tmp;
            tmp = tmp1;
        }
        else
            p->childItems[i] = tmp;
        p->childItems[i]->rowNumber = i;
    }
}
/*************************/
void DomItem::moveDown (int n)
{
    if (n < 0 || n >= childCount() - 1) return;

    if (!domNode.firstChildElement ("feathernotes").isNull())
        domNode.firstChildElement ("feathernotes").insertAfter (child (n)->node(), child (n + 1)->node());
    else
        domNode.insertAfter (child (n)->node(), child (n + 1)->node());

    DomItem *tmp = childItems[n];
    childItems[n] = childItems[n + 1];
    childItems[n]->rowNumber = n;
    childItems[n + 1] = tmp;
    childItems[n + 1]->rowNumber = n + 1;
}
/*************************/
// Make the nth child of this item the last child of the child above it.
void DomItem::moveRight (int n)
{
    if (n <= 0 || n >= childCount()) return;

    child (n - 1)->node().appendChild (child (n)->node());
    // or: childItems[n - 1]->domNode.appendChild (childItems[n]->domNode);
    int k = 0;
    QDomNodeList list;
    if (!domNode.firstChildElement ("feathernotes").isNull())
        list = domNode.firstChildElement ("feathernotes").childNodes();
    else
    {
        list = domNode.childNodes();
        if (!list.isEmpty() && !list.at (0).isElement())
            k = 1;
    }
    int cK = 0;
    QDomNodeList cList = child (n - 1)->node().childNodes();
    if (!cList.isEmpty() && !cList.at (0).isElement())
        cK = 1;

    /*****************************
      The node should be removed
     *****************************/

    DomItem *del = childItems.value (n);
    /* now, move up the values for all indexes >= n */
    for (int i = n; i + k < list.count(); ++i)
    {
        childItems[i] = childItems[i + 1];
        childItems[i]->rowNumber = i;
    }
    childItems.remove (list.count() - k);

    /**************************************
      A child should be inserted after all
      children of the node above this one.
     **************************************/

    child (n - 1)->childItems[cList.count() - cK - 1] = del;
    child (n - 1)->childItems[cList.count() - cK - 1]->parentItem = child (n - 1);
    child (n - 1)->childItems[cList.count() - cK - 1]->rowNumber = cList.count() - cK - 1;
}
/*************************/
DomItem *DomItem::takeChild (int n)
{
    if (n < 0 || n >= childCount()) return nullptr;

    int k = 0;
    QDomNodeList list;
    if (!domNode.firstChildElement ("feathernotes").isNull())
    {
        domNode.firstChildElement ("feathernotes").removeChild (child (n)->node());
        list = domNode.firstChildElement ("feathernotes").childNodes();
    }
    else
    {
        domNode.removeChild (child (n)->node());
        list = domNode.childNodes();
        if (!list.isEmpty() && !list.at (0).isElement())
            k = 1;
    }

    DomItem *res = childItems.value (n);
    /* now, move up the values for all indexes >= n */
    for (int i = n; i + k < list.count(); ++i)
    {
        childItems[i] = childItems[i + 1];
        childItems[i]->rowNumber = i;
    }
    childItems.remove (list.count() - k);

    return res;
}

}
