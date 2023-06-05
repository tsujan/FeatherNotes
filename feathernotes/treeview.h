/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2023 <tsujan2000@gmail.com>
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
#include <QScrollBar>
#include <QTreeView>
#include <QHeaderView>
#include <QMimeDatabase>
#include <QMimeData>
#include <QFileInfo>

namespace FeatherNotes {

/* We don't want Ctrl + left click to deselect a selected node in the single-selection mode.
   In addition, we want to open FeatherNotes docs by DND. */
class TreeView : public QTreeView
{
    Q_OBJECT
public:
    TreeView (QWidget *parent = nullptr) : QTreeView (parent) {
        setDragDropMode (QAbstractItemView::InternalMove);
        header()->setOffset (0);
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

        /* show the horizontal scrollbar if needed */
        header()->setStretchLastSection (false);
        header()->setSectionResizeMode (QHeaderView::ResizeToContents);
    }

    void scrollTo (const QModelIndex &index, QAbstractItemView::ScrollHint hint = QAbstractItemView::EnsureVisible) override {
        QTreeView::scrollTo (index, hint);
        if (index.isValid())
        { // ensure that the item is visible horizontally too
            int viewportWidth = viewport()->width();
            QRect vr = visualRect (index);
            int itemWidth = vr.width() + indentation();
            int hPos;
            if (QApplication::layoutDirection() == Qt::RightToLeft)
                hPos = viewportWidth - vr.x() - vr.width() - indentation(); // horizontally mirrored
            else
                hPos = vr.x() - indentation();
            if (hPos < 0 || itemWidth > viewportWidth)
                horizontalScrollBar()->setValue (hPos);
            else if (hPos + itemWidth > viewportWidth)
                horizontalScrollBar()->setValue (hPos + itemWidth - viewportWidth);
        }
    }

signals:
    void FNDocDropped (const QString &path);
    void searchRequest();

protected:
    /* see Qt -> "qabstractitemview.cpp" */
    QItemSelectionModel::SelectionFlags selectionCommand (const QModelIndex &index, const QEvent *event = nullptr) const override {
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
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
#else
        Qt::KeyboardModifiers keyModifiers = event != nullptr && event->isInputEvent()
                                                 ? (static_cast<const QInputEvent*>(event))->modifiers()
                                                 : Qt::NoModifier;

#endif
        if (selectionMode() == QAbstractItemView::SingleSelection)
        {
            if (!index.isValid())
                return QItemSelectionModel::NoUpdate;
            if ((keyModifiers & Qt::ControlModifier)
                && selectionModel()->isSelected (index))
            {
                return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
            }
        }
        return QTreeView::selectionCommand (index, event);
    }

    void dragEnterEvent (QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls())
        {
            bool FNDocFound (false);
            const auto urls = event->mimeData()->urls();
            for (const QUrl &url : urls)
            {
                if (url.fileName().endsWith (".fnx"))
                {
                    FNDocFound = true;
                    event->acceptProposedAction();
                    break;
                }
                QMimeDatabase mimeDatabase;
                QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
                if (mimeType.name() == "text/feathernotes-fnx")
                {
                    FNDocFound = true;
                    event->acceptProposedAction();
                    break;
                }
            }
            if (!FNDocFound)
            {
                event->ignore();
                return;
            }
        }
        QTreeView::dragEnterEvent (event);
    }

    void dropEvent (QDropEvent *event) override {
        if (event->mimeData()->hasUrls())
        {
            const auto urls = event->mimeData()->urls();
            for (const QUrl &url : urls)
            {
                if (url.fileName().endsWith (".fnx"))
                {
                    emit FNDocDropped (url.path());
                    break;
                }
                QMimeDatabase mimeDatabase;
                QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
                if (mimeType.name() == "text/feathernotes-fnx")
                {
                    emit FNDocDropped (url.path());
                    break;
                }
            }
        }
        QTreeView::dropEvent (event);
    }

    void mousePressEvent (QMouseEvent *event) override {
        QModelIndex index = indexAt (event->pos());
        /* get the global press position if it's inside an item to know
           whether there will be a real mouse movement at mouseMoveEvent() */
        if (event->buttons() == Qt::LeftButton
            && qApp->keyboardModifiers() == Qt::NoModifier)
        {
            if (index.isValid())
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
                itemPressPoint_ = event->globalPos();
#else
                itemPressPoint_ = event->globalPosition().toPoint();
#endif
            else itemPressPoint_ = QPoint();
        }
        else itemPressPoint_ = QPoint();
        QTreeView::mousePressEvent (event);

        /* immediately scroll to the index if it's the current index
           ("QAbstractItemView::mousePressEvent()" adds a delay) */
        if (index.isValid() && index == currentIndex())
            scrollTo (index);
    }

    void mouseMoveEvent (QMouseEvent *event) override {
        /* prevent dragging if there is no real mouse movement */
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
        if (event->buttons() == Qt::LeftButton && (event->globalPos() - itemPressPoint_).manhattanLength() < QApplication::startDragDistance())
#else
        if (event->buttons() == Qt::LeftButton && (event->globalPosition().toPoint() - itemPressPoint_).manhattanLength() < QApplication::startDragDistance())
#endif
            return;
        QTreeView::mouseMoveEvent (event);
    }

    void keyPressEvent (QKeyEvent *event) override {
        if (event->key() == Qt::Key_Slash)
        {
            emit searchRequest();
            event->accept();
            return;
        }
        QTreeView::keyPressEvent (event);
    }

private:
    QPoint itemPressPoint_;
};

}

#endif // TREEVIEW_H
