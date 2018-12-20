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
#include <QProcess>
#include <QToolTip>
#include <QTimer>
#include <QTextBlock>
#include <QRegularExpression>

namespace FeatherNotes {

// Finds the (remaining) spaces that should be inserted with Ctrl+Tab.
QString TextEdit::remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    QString txt = cursor.block().text().left (cursor.positionInBlock());
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    qreal spaceL = fm.width (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (x / spaceL) - 1, 0);
        ++i;
    }
    n += txt.count();
    n = spaceTab.count() - n % spaceTab.count();
    QString res;
    for (int i = 0 ; i < n; ++i)
        res += " ";
    return res;
}
/*************************/
// Returns a cursor that selects the spaces to be removed by a backtab.
QTextCursor TextEdit::backTabCursor (const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    tmp.movePosition (QTextCursor::StartOfBlock);
    /* find the start of the real text */
    const QString blockText = cursor.block().text();
    int indx = 0;
    QRegularExpressionMatch match;
    if (blockText.indexOf (QRegularExpression (R"(^\s+)"), 0, &match) > -1)
        indx = match.capturedLength();
    else
        return tmp;
    int txtStart = cursor.block().position() + indx;

    QString txt = blockText.left (indx);
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    qreal spaceL = fm.width (" ");
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (x / spaceL) - 1, 0);
        ++i;
    }
    n += txt.count();
    n = n % textTab_.count();
    if (n == 0) n = textTab_.count();

    tmp.setPosition (txtStart);
    QChar ch = blockText.at (indx - 1);
    if (ch == QChar (QChar::Space))
        tmp.setPosition(txtStart - n, QTextCursor::KeepAnchor);
    else // the previous character is a tab
    {
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition(txtStart - 1, QTextCursor::KeepAnchor);
        x -= static_cast<qreal>(cursorRect (tmp).right());
        n -= qRound (x / spaceL);
        if (n < 0) n = 0; // impossible
        tmp.setPosition (tmp.position() - n, QTextCursor::KeepAnchor);
    }

    return tmp;
}
/*************************/
static inline bool isOnlySpaces (const QString &str)
{
    int i = 0;
    while (i < str.size())
    { // always skip the starting spaces
        QChar ch = str.at (i);
        if (ch == QChar (QChar::Space) || ch == QChar (QChar::Tabulation))
            ++i;
        else return false;
    }
    return true;
}

void TextEdit::keyPressEvent (QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QTextCursor cur = textCursor();
        bool isBracketed (false);
        QString selTxt = cur.selectedText();
        QString prefix, indent;
        bool withShift (event->modifiers() & Qt::ShiftModifier);

        /* with Shift+Enter, find the non-letter prefix */
        if (withShift)
        {
            cur.clearSelection();
            setTextCursor (cur);
            const QString blockText = cur.block().text();
            int i = 0;
            int curBlockPos = cur.position() - cur.block().position();
            while (i < curBlockPos)
            {
                QChar ch = blockText.at (i);
                if (!ch.isLetterOrNumber())
                {
                    prefix += ch;
                    ++i;
                }
                else break;
            }
        }
        else
        {
            /* find the indentation */
            if (autoIndentation)
                indent = computeIndentation (cur);
            /* check whether a bracketed text is selected
               so that the cursor position is at its start */
            QTextCursor anchorCur = cur;
            anchorCur.setPosition (cur.anchor());
            if (autoBracket
                && cur.position() == cur.selectionStart()
                && !cur.atBlockStart() && !anchorCur.atBlockEnd())
            {
                cur.setPosition (cur.position());
                cur.movePosition (QTextCursor::PreviousCharacter);
                cur.movePosition (QTextCursor::NextCharacter,
                                  QTextCursor::KeepAnchor,
                                  selTxt.size() + 2);
                QString selTxt1 = cur.selectedText();
                if (selTxt1 == "{" + selTxt + "}" || selTxt1 == "(" + selTxt + ")")
                    isBracketed = true;
                cur = textCursor(); // reset the current cursor
            }
        }

        if (withShift || autoIndentation || isBracketed)
        {
            cur.beginEditBlock();
            /* first press Enter normally... */
            cur.insertText (QChar (QChar::ParagraphSeparator));
            /* ... then, insert indentation... */
            cur.insertText (indent);
            /* ... and handle Shift+Enter or brackets */
            if (withShift)
                cur.insertText (prefix);
            else if (isBracketed)
            {
                cur.movePosition (QTextCursor::PreviousBlock);
                cur.movePosition (QTextCursor::EndOfBlock);
                int start = -1;
                QStringList lines = selTxt.split (QChar (QChar::ParagraphSeparator));
                if (lines.size() == 1)
                {
                    cur.insertText (QChar (QChar::ParagraphSeparator));
                    cur.insertText (indent);
                    start = cur.position();
                    if (!isOnlySpaces (lines. at (0)))
                        cur.insertText (lines. at (0));
                }
                else // multi-line
                {
                    for (int i = 0; i < lines.size(); ++i)
                    {
                        if (i == 0 && isOnlySpaces (lines. at (0)))
                            continue;
                        cur.insertText (QChar (QChar::ParagraphSeparator));
                        if (i == 0)
                        {
                            cur.insertText (indent);
                            start = cur.position();
                        }
                        else if (i == 1 && start == -1)
                            start = cur.position(); // the first line was only spaces
                        cur.insertText (lines. at (i));
                    }
                }
                cur.setPosition (start, start >= cur.block().position()
                                            ? QTextCursor::MoveAnchor
                                            : QTextCursor::KeepAnchor);
                setTextCursor (cur);
            }
            cur.endEditBlock();
            ensureCursorVisible();
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_ParenLeft
             || event->key() == Qt::Key_BraceLeft
             || event->key() == Qt::Key_BracketLeft
             || event->key() == Qt::Key_QuoteDbl)
    {
        if (autoBracket)
        {
            QTextCursor cursor = textCursor();
            bool autoB (false);
            if (!cursor.hasSelection())
            {
                if (cursor.atBlockEnd())
                    autoB = true;
                else
                {
                    QTextCursor tmp = cursor;
                    tmp.movePosition (QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                    if (!tmp.selectedText().at (0).isLetterOrNumber())
                        autoB = true;
                }
            }
            else if (cursor.position() == cursor.selectionStart())
                autoB = true;
            if (autoB)
            {
                int pos = cursor.position();
                int anch = cursor.anchor();
                cursor.beginEditBlock();
                cursor.setPosition (anch);
                if (event->key() == Qt::Key_ParenLeft)
                {
                    cursor.insertText (")");
                    cursor.setPosition (pos);
                    cursor.insertText ("(");
                }
                else if (event->key() == Qt::Key_BraceLeft)
                {
                    cursor.insertText ("}");
                    cursor.setPosition (pos);
                    cursor.insertText ("{");
                }
                else if (event->key() == Qt::Key_BracketLeft)
                {
                    cursor.insertText ("]");
                    cursor.setPosition (pos);
                    cursor.insertText ("[");
                }
                else// if (event->key() == Qt::Key_QuoteDbl)
                {
                    cursor.insertText ("\"");
                    cursor.setPosition (pos);
                    cursor.insertText ("\"");
                }
                /* select the text and set the cursor at its start */
                cursor.setPosition (anch + 1, QTextCursor::MoveAnchor);
                cursor.setPosition (pos + 1, QTextCursor::KeepAnchor);
                cursor.endEditBlock();
                /* WARNING: Why does putting "setTextCursor()" before "endEditBlock()"
                            cause a crash with huge lines? Most probably, a Qt bug. */
                setTextCursor (cursor);
                event->accept();
                return;
            }
        }
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
    else if (event->key() == Qt::Key_Tab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        if (newLines > 0)
        {
            cursor.beginEditBlock();
            cursor.setPosition (qMin (cursor.anchor(), cursor.position())); // go to the first block
            cursor.movePosition (QTextCursor::StartOfBlock);
            for (int i = 0; i <= newLines; ++i)
            {
                /* skip all spaces to align the real text */
                int indx = 0;
                QRegularExpressionMatch match;
                if (cursor.block().text().indexOf (QRegularExpression (R"(^\s+)"), 0, &match) > -1)
                    indx = match.capturedLength();
                cursor.setPosition (cursor.block().position() + indx);
                if (event->modifiers() & Qt::ControlModifier)
                {
                    cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                        ? "  " : textTab_, cursor));
                }
                else
                    cursor.insertText ("\t");
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break; // not needed
            }
            cursor.endEditBlock();
            event->accept();
            return;
        }
        else if (event->modifiers() & Qt::ControlModifier)
        {
            QTextCursor tmp (cursor);
            tmp.setPosition (qMin (tmp.anchor(), tmp.position()));
            cursor.insertText (remainingSpaces (event->modifiers() & Qt::MetaModifier
                                                ? "  " : textTab_, tmp));
            event->accept();
            return;
        }
    }
    else if (event->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = textCursor();
        int newLines = cursor.selectedText().count (QChar (QChar::ParagraphSeparator));
        cursor.setPosition (qMin (cursor.anchor(), cursor.position()));
        cursor.beginEditBlock();
        cursor.movePosition (QTextCursor::StartOfBlock);
        for (int i = 0; i <= newLines; ++i)
        {
            if (cursor.atBlockEnd())
            {
                if (!cursor.movePosition (QTextCursor::NextBlock))
                    break; // not needed
                continue;
            }
            cursor = backTabCursor (cursor);
            cursor.removeSelectedText();
            if (!cursor.movePosition (QTextCursor::NextBlock))
                break; // not needed
        }
        cursor.endEditBlock();

        /* otherwise, do nothing with SHIFT+TAB */
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Insert)
    {
        setOverwriteMode (!overwriteMode());
        if (!overwriteMode())
        {
            setCursorWidth (1);
            update(); // otherwise, a part of the thick cursor might remain
        }
        else
            setCursorWidth (QFontMetrics(font()).averageCharWidth());
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
QString TextEdit::computeIndentation (const QTextCursor& cur) const
{
    QTextCursor cusror = cur;
    if (cusror.hasSelection())
    {// this is more intuitive to me
        if (cusror.anchor() <= cusror.position())
            cusror.setPosition (cusror.anchor());
        else
            cusror.setPosition (cusror.position());
    }
    QTextCursor tmp = cusror;
    tmp.movePosition (QTextCursor::StartOfBlock);
    QString str;
    if (tmp.atBlockEnd())
        return str;
    int pos = tmp.position();
    tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    QString selected;
    while (!tmp.atBlockEnd()
           && tmp <= cusror
           && ((selected = tmp.selectedText()) == " "
               || (selected = tmp.selectedText()) == "\t"))
    {
        str.append (selected);
        tmp.setPosition (pos);
        tmp.setPosition (++pos, QTextCursor::KeepAnchor);
    }
    if (tmp.atBlockEnd()
        && tmp <= cusror
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
            url = QUrl::fromUserInput (str, "/", QUrl::AssumeLocalFile);
        /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
           the best choice. "gio" is always reliable, so we check it first. */
        if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
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
    {
        if (e->modifiers() & Qt::ControlModifier)
        {
            float delta = e->angleDelta().y() / 120.f;
            zooming (delta);
            return;
        }
        /* as in QTextEdit::wheelEvent() */
        QAbstractScrollArea::wheelEvent (e);
        updateMicroFocus();
    }
}
/*************************/
void TextEdit::zooming (float range)
{
    if (range == 0.f) return;
    QFont f = document()->defaultFont();
    const float newSize = static_cast<float>(f.pointSizeF()) + range;
    if (newSize <= 0) return;
    f.setPointSizeF (static_cast<qreal>(newSize));
    setFont (f);
    QFontMetrics metrics (f);
    setTabStopWidth (4 * metrics.width (' '));

    /* if this is a zoom-out, the text will need
       to be formatted and/or highlighted again */
    if (range < 0) emit zoomedOut (this);
}

}
