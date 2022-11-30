/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2022 <tsujan2000@gmail.com>
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

#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include <QTextEdit>
#include <QKeyEvent>
#include <QUrl>
#include <QFileInfo>
#include <QImageReader>
#include <QMimeData>
#include <QElapsedTimer>
#include "vscrollbar.h"

namespace FeatherNotes {

class TextEdit : public QTextEdit
{
    Q_OBJECT

public:
    TextEdit (QWidget *parent = nullptr);
    ~TextEdit();

    void setEditorFont (const QFont &f); // also sets tab stop distance
    void zooming (float range);

    bool autoIndentation;
    bool autoBracket;
    bool autoReplace;

signals:
    void resized();
    void imageDropped (const QString &path);
    void FNDocDropped (const QString &path);
    void zoomedOut (TextEdit *textEdit); // needed for reformatting text

public slots:
    void copy();
    void cut();

protected:
    void keyPressEvent (QKeyEvent *event);
    QMimeData* createMimeDataFromSelection() const;
    bool canInsertFromMimeData (const QMimeData *source) const;
    void insertFromMimeData (const QMimeData *source);
    void mouseMoveEvent (QMouseEvent *e);
    void mousePressEvent (QMouseEvent *e);
    void mouseReleaseEvent (QMouseEvent *e);
    void mouseDoubleClickEvent (QMouseEvent *e);
    void resizeEvent (QResizeEvent *e);
    bool event (QEvent *e);
    void wheelEvent (QWheelEvent *e);

private slots:
    void scrollSmoothly();

private:
    QString computeIndentation (const QTextCursor& cur) const;
    QString remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const;
    QTextCursor backTabCursor(const QTextCursor& cursor, bool twoSpace) const;

    QString textTab_; // text tab in terms of spaces
    QPoint pressPoint_;
    QElapsedTimer tripleClickTimer_;
    bool isCopyOrCut_; // to distinguish the selection clipboard
    /****************************
     ***** Smooth scrolling *****
     ****************************/
    struct scrollData {
        int delta;
        int leftFrames;
        bool vertical;
    };
    QList<scrollData> queuedScrollSteps_;
    QTimer *scrollTimer_;
};

}


#endif // TEXTEDIT_H
