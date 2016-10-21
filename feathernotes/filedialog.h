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

#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QFileDialog>
#include <QTreeView>
#include <QTimer>

namespace FeatherNotes {

/* We want auto-scrolling to selected file. */
class FileDialog : public QFileDialog {
    Q_OBJECT
public:
    FileDialog (QWidget *parent) : QFileDialog (parent) {
        tView = nullptr;
        p = parent;
        setViewMode (QFileDialog::Detail);
        setOption (QFileDialog::DontUseNativeDialog);
    }

    void autoScroll() {
        tView = findChild<QTreeView *>("treeView");
        if (tView)
            connect (tView->model(), &QAbstractItemModel::layoutChanged, this, &FileDialog::scrollToSelection);
    }

protected:
    void showEvent(QShowEvent * event) {
        if (p)
            QTimer::singleShot (0, this, SLOT (center()));
        QFileDialog::showEvent (event);
    }

private slots:
    void scrollToSelection() {
        if (tView)
        {
            QModelIndexList iList = tView->selectionModel()->selectedIndexes();
            if (!iList.isEmpty())
                tView->scrollTo (iList.at (0), QAbstractItemView::PositionAtCenter);
        }
    }
    void center() {
        move (p->x() + p->width()/2 - width()/2,
              p->y() + p->height()/2 - height()/ 2);
    }

private:
    QTreeView *tView;
    QWidget *p;
};

}

#endif // FILEDIALOG_H
