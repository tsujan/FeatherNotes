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

#include "textedit.h"
#include <QDesktopServices>
#include <QToolTip>
#include <QTimer>

namespace FeatherNotes {

void TextEdit::keyPressEvent (QKeyEvent *event)
{
    if (autoIndentation && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
    {
        /* first get the current cursor for computing the indentation */
        QTextCursor start = textCursor();
        QString indent = computeIndentation (start);
        /* then press Enter normally... */
        QTextEdit::keyPressEvent (event);
        /* ... and insert indentation */
        start = textCursor();
        start.insertText (indent);
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        /* when text is selected, use arrow keys
           to go to the start or end of the selection */
        QTextCursor cursor = textCursor();
        if (event->modifiers() == Qt::NoModifier && cursor.hasSelection())
        {
            QString str = cursor.selectedText();
            if (event->key() == Qt::Key_Left)
            {
                if (str.isRightToLeft())
                    cursor.setPosition (cursor.selectionEnd());
                else
                    cursor.setPosition (cursor.selectionStart());
            }
            else
            {
                if (str.isRightToLeft())
                    cursor.setPosition(cursor.selectionStart());
                else
                    cursor.setPosition(cursor.selectionEnd());
            }
            cursor.clearSelection();
            setTextCursor (cursor);
            event->accept();
            return;
        }
    }
    /* because of a bug in Qt5, the non-breaking space (ZWNJ) isn't inserted with SHIFT+SPACE */
    else if (event->key() == 0x200c)
    {
        insertPlainText (QChar (0x200C));
        event->accept();
        return;
    }

    QTextEdit::keyPressEvent (event);
}
/*************************/
QString TextEdit::computeIndentation (QTextCursor& cur) const
{
    if (cur.hasSelection())
    {// this is more intuitive to me
        if (cur.anchor() <= cur.position())
            cur.setPosition (cur.anchor());
        else
            cur.setPosition (cur.position());
    }
    QTextCursor tmp = cur;
    tmp.movePosition (QTextCursor::StartOfBlock);
    QString str;
    if (tmp.atBlockEnd())
        return str;
    int pos = tmp.position();
    tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    QString selected;
    while (!tmp.atBlockEnd()
           && tmp <= cur
           && ((selected = tmp.selectedText()) == " "
               || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
        tmp.setPosition (pos);
        tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    }
    if (tmp.atBlockEnd()
        && tmp <= cur
        && ((selected = tmp.selectedText()) == " "
            || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
    }
    return str;
}
/*************************/
// Emit resized().
void TextEdit::resizeEvent (QResizeEvent *e)
{
    QTextEdit::resizeEvent (e);
    emit resized();
}
/*************************/
// Show links as tooltips.
bool TextEdit::event (QEvent *e)
{
    if (e->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
        QString str = anchorAt (helpEvent->pos());
        if (!str.isEmpty())
            QToolTip::showText (helpEvent->globalPos(), str);
        else
        {
            QToolTip::hideText();
            e->ignore();
        }
        return true;
    }

    return QTextEdit::event (e);
}
/*************************/
bool TextEdit::canInsertFromMimeData (const QMimeData *source) const
{
    if (source->hasImage() || source->hasUrls())
        return true;
    else
        return QTextEdit::canInsertFromMimeData (source);
}
/*************************/
void TextEdit::insertFromMimeData (const QMimeData *source)
{
    if (source->hasImage())
    {
        QImage image = qvariant_cast<QImage>(source->imageData());
        document()->addResource (QTextDocument::ImageResource, QUrl ("image"), image);
        textCursor().insertImage ("image");
    }
    else if (source->hasUrls())
    {
        foreach (QUrl url, source->urls())
        {
            QFileInfo info (url.toLocalFile());
            if (QImageReader::supportedImageFormats().contains(info.suffix().toLower().toLatin1()))
                emit imageDropped (url.path());
            else
                textCursor().insertText (url.toString());
        }
    }
    else
        QTextEdit::insertFromMimeData (source);
}
/*************************/
void TextEdit::mouseMoveEvent (QMouseEvent *e)
{
    QTextEdit::mouseMoveEvent (e);

    QString str = anchorAt (e->pos());
    if (!str.isEmpty())
        viewport()->setCursor (Qt::PointingHandCursor);
    else
        viewport()->setCursor (Qt::IBeamCursor);
}
/*************************/
void TextEdit::mousePressEvent (QMouseEvent *e)
{
    QTextEdit::mousePressEvent (e);
    pressPoint = e->pos();
}

/*************************/
void TextEdit::mouseReleaseEvent (QMouseEvent *e)
{
    QTextEdit::mouseReleaseEvent (e);

    QString str = anchorAt (e->pos());
    if (!str.isEmpty()
        && cursorForPosition (e->pos()) == cursorForPosition (pressPoint))
    {
        QUrl url (str);
        if (url.isRelative()) // treat relative URLs as local paths
            url = QUrl::fromUserInput (str, "/");
        QDesktopServices::openUrl (url);
    }
    pressPoint = QPoint();
}
/*************************/
void TextEdit::wheelEvent (QWheelEvent *e)
{
    if (scrollJumpWorkaround && e->angleDelta().manhattanLength() > 240)
        e->ignore();
    else
        QTextEdit::wheelEvent(e);
}

}
