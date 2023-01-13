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

#include "textedit.h"
#include <QDesktopServices>
#include <QProcess>
#include <QToolTip>
#include <QTimer>
#include <QTextBlock>
#include <QRegularExpression>
#include <QBuffer>
#include <QMimeDatabase>
#include <QLocale>
#include <QTextDocumentFragment>
#include <QApplication>
#include <QClipboard>

#define SCROLL_FRAMES_PER_SEC 50
#define SCROLL_DURATION 200 // in ms

namespace FeatherNotes {

static const int scrollAnimFrames = SCROLL_FRAMES_PER_SEC * SCROLL_DURATION / 1000;
static const QRegularExpression startingSpaces (R"(^\s+)");

TextEdit::TextEdit (QWidget *parent) : QTextEdit (parent)
{
    autoIndentation = true;
    autoBracket = false;
    autoReplace = false;
    textTab_ = "    "; // the default text tab is four spaces
    pressPoint_ = QPoint();
    scrollTimer_ = nullptr;
    isCopyOrCut_ = false;

    VScrollBar *vScrollBar = new VScrollBar;
    setVerticalScrollBar (vScrollBar);
}
/*************************/
TextEdit::~TextEdit()
{
    if (scrollTimer_)
    {
        disconnect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollSmoothly);
        scrollTimer_->stop();
        delete scrollTimer_;
    }
}
/*************************/
// Finds the (remaining) spaces that should be inserted with Ctrl+Tab.
QString TextEdit::remainingSpaces (const QString& spaceTab, const QTextCursor& cursor) const
{
    QTextCursor tmp = cursor;
    QString txt = cursor.block().text().left (cursor.positionInBlock());
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    qreal spaceL = fm.horizontalAdvance (' ');
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0); // x is negative for RTL
        ++i;
    }
    n += txt.size();
    n = spaceTab.size() - n % spaceTab.size();
    QString res;
    for (int i = 0 ; i < n; ++i)
        res += " ";
    return res;
}
/*************************/
// Returns a cursor that selects the spaces to be removed by a backtab.
// If "twoSpace" is true, a 2-space backtab will be applied as far as possible.
QTextCursor TextEdit::backTabCursor (const QTextCursor& cursor, bool twoSpace) const
{
    QTextCursor tmp = cursor;
    tmp.movePosition (QTextCursor::StartOfBlock);
    /* find the start of the real text */
    const QString blockText = cursor.block().text();
    int indx = 0;
    QRegularExpressionMatch match;
    if (blockText.indexOf (startingSpaces, 0, &match) > -1)
        indx = match.capturedLength();
    else
        return tmp;
    int txtStart = cursor.block().position() + indx;

    QString txt = blockText.left (indx);
    QFontMetricsF fm = QFontMetricsF (document()->defaultFont());
    qreal spaceL = fm.horizontalAdvance (' ');
    int n = 0, i = 0;
    while ((i = txt.indexOf("\t", i)) != -1)
    { // find tab widths in terms of spaces
        tmp.setPosition (tmp.block().position() + i);
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (tmp.position() + 1);
        x = static_cast<qreal>(cursorRect (tmp).right()) - x;
        n += qMax (qRound (qAbs (x) / spaceL) - 1, 0);
        ++i;
    }
    n += txt.size();
    n = n % textTab_.size();
    if (n == 0) n = textTab_.size();

    if (twoSpace) n = qMin (n, 2);

    tmp.setPosition (txtStart);
    if (blockText.at (indx - 1) == QChar (QChar::Space))
        tmp.setPosition (txtStart - n, QTextCursor::KeepAnchor);
    else // the previous character is a tab
    {
        qreal x = static_cast<qreal>(cursorRect (tmp).right());
        tmp.setPosition (txtStart - 1, QTextCursor::KeepAnchor);
        x -= static_cast<qreal>(cursorRect (tmp).right());
        n -= qRound (qAbs (x) / spaceL);
        if (n < 0) n = 0; // impossible without "twoSpace"
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
    /* overriding copy() and cut() */
    if (event == QKeySequence::Copy)
    {
        copy();
        event->accept();
        return;
    }
    if (event == QKeySequence::Cut)
    {
        cut();
        event->accept();
        return;
    }

    /* QWidgetTextControl::undo() and QWidgetTextControl::redo() call ensureCursorVisible()
       even when there's nothing to undo/redo. Users may press the undo/redo shortcut keys
       just to make sure of the state of a document and a scroll jump can confuse them when
       there's nothing to undo/redo. */
    if (event == QKeySequence::Undo)
    {
        if (document()->isUndoAvailable())
            undo();
        event->accept();
        return;
    }
    if (event == QKeySequence::Redo)
    {
        if (document()->isRedoAvailable())
            redo();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QTextCursor cur = textCursor();
        QString selTxt = cur.selectedText();

        if (autoReplace && selTxt.isEmpty())
        {
            const int p = cur.positionInBlock();
            if (p > 1)
            {
                cur.beginEditBlock();
                cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                const QString sel = cur.selectedText();
                if (!sel.endsWith ("."))
                {
                    if (sel == "--")
                    {
                        QTextCursor prevCur = cur;
                        prevCur.setPosition (cur.position());
                        prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                        if (prevCur.selectedText() != "-")
                            cur.insertText ("—");
                    }
                    else if (sel == "->")
                        cur.insertText ("→");
                    else if (sel == "<-")
                        cur.insertText ("←");
                    else if (sel == ">=")
                        cur.insertText ("≥");
                    else if (sel == "<=")
                        cur.insertText ("≤");
                }
                else if (p > 2)
                {
                    cur = textCursor();
                    cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                    const QString sel = cur.selectedText();
                    if (sel == "...")
                    {
                        QTextCursor prevCur = cur;
                        prevCur.setPosition (cur.position());
                        if (p > 3)
                        {
                            prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != ".")
                                cur.insertText ("…");
                        }
                        else cur.insertText ("…");
                    }
                }
                cur.endEditBlock();
                cur = textCursor(); // reset the current cursor
            }
        }

        bool isBracketed (false);
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
            /* still check if a letter or number follows */
            if (i < curBlockPos)
            {
                QChar c = blockText.at (i);
                if (c.isLetter())
                {
                    if (i + 1 < curBlockPos
                        && !prefix.isEmpty() && !prefix.at (prefix.size() - 1).isSpace()
                        && blockText.at (i + 1).isSpace())
                    { // non-letter and non-space character -> singlle letter -> space
                        prefix = blockText.left (i + 2);
                        QChar cc = QChar (c.unicode() + 1);
                        if (cc.isLetter()) prefix.replace (c, cc);
                    }
                    else if (i + 2 < curBlockPos
                             && !blockText.at (i + 1).isLetterOrNumber() && !blockText.at (i + 1).isSpace()
                             && blockText.at (i + 2).isSpace())
                    { // singlle letter -> non-letter and non-space character -> space
                        prefix = blockText.left (i + 3);
                        QChar cc = QChar (c.unicode() + 1);
                        if (cc.isLetter()) prefix.replace (c, cc);
                    }
                }
                else if (c.isNumber())
                { // making lists with numbers
                    QString num;
                    while (i < curBlockPos)
                    {
                        QChar ch = blockText.at (i);
                        if (ch.isNumber())
                        {
                            num += ch;
                            ++i;
                        }
                        else
                        {
                            if (!num.isEmpty())
                            {
                                QChar ch = blockText.at (i);
                                if (ch.isSpace())
                                { // non-letter and non-space character -> number -> space
                                    if (!prefix.isEmpty() && !prefix.at (prefix.size() - 1).isSpace())
                                        num = locale().toString (locale().toInt (num) + 1) + ch;
                                    else num = QString();
                                }
                                else if (i + 1 < curBlockPos
                                         && !ch.isLetterOrNumber() && !ch.isSpace()
                                         && blockText.at (i + 1).isSpace())
                                { // number -> non-letter and non-space character -> space
                                    num = locale().toString (locale().toInt (num) + 1) + ch + blockText.at (i + 1);
                                }
                                else num = QString();
                            }
                            break;
                        }
                    }
                    if (i < curBlockPos) // otherwise, it'll be just a number
                        prefix += num;
                }
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
    else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up)
    {
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (QScrollBar* vbar = verticalScrollBar())
            { // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_Down ? 1 : -1) * vbar->singleStep());
                event->accept();
                return;
            }
        }
    }
    else if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_PageUp)
    {
        if (event->modifiers() == Qt::ControlModifier)
        {
            if (QScrollBar* vbar = verticalScrollBar())
            { // scroll without changing the cursor position
                vbar->setValue(vbar->value() + (event->key() == Qt::Key_PageDown ? 1 : -1) * vbar->pageStep());
                event->accept();
                return;
            }
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
                if (cursor.block().text().indexOf (startingSpaces, 0, &match) > -1)
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
            cursor = backTabCursor (cursor, event->modifiers() & Qt::MetaModifier
                                            ? true : false);
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
        if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier)
        {
            setOverwriteMode (!overwriteMode());
            if (!overwriteMode())
            {
                setCursorWidth (1);
                update(); // otherwise, a part of the thick cursor might remain
            }
            else
                setCursorWidth (QFontMetrics(font()).averageCharWidth());
            event->accept();
            return;
        }

    }
    /* because of a bug in Qt, the non-breaking space (ZWNJ) may not be inserted with SHIFT+SPACE */
    else if (event->key() == 0x200c)
    {
        insertPlainText (QChar (0x200C));
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_Space)
    {
        if (autoReplace && event->modifiers() == Qt::NoModifier)
        {
            QTextCursor cur = textCursor();
            if (!cur.hasSelection())
            {
                const int p = cur.positionInBlock();
                if (p > 1)
                {
                    cur.beginEditBlock();
                    cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
                    const QString selTxt = cur.selectedText();
                    if (!selTxt.endsWith ("."))
                    {
                        if (selTxt == "--")
                        {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition (cur.position());
                            prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                            if (prevCur.selectedText() != "-")
                                cur.insertText ("—");
                        }
                        else if (selTxt == "->")
                            cur.insertText ("→");
                        else if (selTxt == "<-")
                            cur.insertText ("←");
                        else if (selTxt == ">=")
                            cur.insertText ("≥");
                        else if (selTxt == "<=")
                            cur.insertText ("≤");
                    }
                    else if (p > 2)
                    {
                        cur = textCursor();
                        cur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 3);
                        const QString selTxt = cur.selectedText();
                        if (selTxt == "...")
                        {
                            QTextCursor prevCur = cur;
                            prevCur.setPosition (cur.position());
                            if (p > 3)
                            {
                                prevCur.movePosition (QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
                                if (prevCur.selectedText() != ".")
                                    cur.insertText ("…");
                            }
                            else cur.insertText ("…");
                        }
                    }
                    cur.endEditBlock();
                }
            }
        }
    }
    else if (event->key() == Qt::Key_Home)
    {
        if (!(event->modifiers() & Qt::ControlModifier))
        { // Qt's default behavior isn't acceptable
            QTextCursor cur = textCursor();
            int p = cur.positionInBlock();
            int indx = 0;
            QRegularExpressionMatch match;
            if (cur.block().text().indexOf (startingSpaces, 0, &match) > -1)
                indx = match.capturedLength();
            if (p > 0)
            {
                if (p <= indx) p = 0;
                else p = indx;
            }
            else p = indx;
            cur.setPosition (p + cur.block().position(),
                             event->modifiers() & Qt::ShiftModifier ? QTextCursor::KeepAnchor
                                                                    : QTextCursor::MoveAnchor);
            setTextCursor (cur);
            ensureCursorVisible();
            event->accept();
            return;
        }
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
    { // "this" is for Wayland, when the window isn't active
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(e);
        QString str = anchorAt (helpEvent->pos());
        if (!str.isEmpty())
            QToolTip::showText (helpEvent->globalPos(),
                                "<p style='white-space:pre'>" + str + "</p>", this);
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
QMimeData* TextEdit::createMimeDataFromSelection() const
{
    /* this is for giving only plain text to the selection clipboard
       and leaving the main clipboard to QTextEdit */
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    /* WARNING: Qt6 has a bug that doesn't include newlines in the
                plain-text data of "QTextEdit::createMimeDataFromSelection()". */
    QTextCursor cursor = textCursor();
    if (cursor.hasSelection())
    {
        QMimeData *mimeData = new QMimeData;
        mimeData->setText (cursor.selection().toPlainText());
        if (isCopyOrCut_)
            mimeData->setHtml (cursor.selection().toHtml());
        return mimeData;
    }
    return nullptr;
#else
    if (!isCopyOrCut_)
    {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection())
        {
            QMimeData *mimeData = new QMimeData;
            mimeData->setText (cursor.selection().toPlainText());
            return mimeData;
        }
        return nullptr;
    }
    return QTextEdit::createMimeDataFromSelection();
#endif
}
/*************************/
static bool containsPlainText (const QStringList &list)
{
    for (const auto &str : list)
    {
        if (str.compare ("text/plain", Qt::CaseInsensitive) == 0
            || str.startsWith ("text/plain;charset=", Qt::CaseInsensitive))
        {
            return true;
        }
    }
    return false;
}
bool TextEdit::canInsertFromMimeData (const QMimeData *source) const
{
    if (source == nullptr) return false;
    if (source->hasImage() || source->hasUrls())
        return true;
    else
    {
        return QTextEdit::canInsertFromMimeData (source)
               || (containsPlainText (source->formats()) && !source->text().isEmpty());
    }
}
void TextEdit::insertFromMimeData (const QMimeData *source)
{
    if (source == nullptr) return;
    if (source->hasImage())
    { // an image is copied directly
        QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull())
        {
            QByteArray rawarray;
            QBuffer buffer (&rawarray);
            buffer.open (QIODevice::WriteOnly);
            image.save (&buffer, "PNG");
            buffer.close();

            insertHtml (QString ("<img src=\"data:image;base64,%1\" />")
                        .arg (QString (rawarray.toBase64())));
            ensureCursorVisible();
        }
    }
    else if (source->hasUrls())
    {
        bool imageInserted = false;
        const auto urls = source->urls();
        bool multiple (urls.count() > 1);
        QTextCursor cur = textCursor();
        cur.beginEditBlock();
        for (const QUrl &url : urls)
        {
            QMimeDatabase mimeDatabase;
            QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
            QByteArray ba = mimeType.name().toUtf8();
            if (QImageReader::supportedMimeTypes().contains (ba))
            {
                emit imageDropped (url.path());
                imageInserted = true;
            }
            else
            {
                if (url.fileName().endsWith (".fnx")
                    || mimeType.name() == "text/feathernotes-fnx")
                {
                    cur.endEditBlock();
                    ensureCursorVisible();
                    /* use a timer to allow the app to process the inserted URls/images */
                    QTimer::singleShot (0, this, [this, url] () {
                        emit FNDocDropped (url.path());
                    });
                    return; // only open the first file
                }
                else
                {
                    if (imageInserted)
                    {
                        cur.insertText ("\n");
                        imageInserted = false;
                    }
                    cur.insertText (url.toString());
                    if (multiple)
                        cur.insertText ("\n");
                }
            }
        }
        cur.endEditBlock();
        ensureCursorVisible();
    }
    else if (containsPlainText (source->formats()) && !source->text().isEmpty())
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
    /* On triple clicking, select the current block without
       selecting its newline and start and end whitespaces, if any. */
    if (tripleClickTimer_.isValid())
    {
        if (!tripleClickTimer_.hasExpired (qApp->doubleClickInterval())
            && e->buttons() == Qt::LeftButton)
        {
            tripleClickTimer_.invalidate();
            if (e->modifiers() != Qt::ControlModifier)
            {
                QTextCursor txtCur = textCursor();
                const QString blockText = txtCur.block().text();
                const int l = blockText.length();
                txtCur.movePosition (QTextCursor::StartOfBlock);
                int i = 0;
                while (i < l && blockText.at (i).isSpace())
                    ++i;
                /* WARNING: QTextCursor::movePosition() can be a mess with RTL
                            but QTextCursor::setPosition() works fine. */
                if (i < l)
                {
                    txtCur.setPosition (txtCur.position() + i);
                    int j = l;
                    while (j > i && blockText.at (j -  1).isSpace())
                        --j;
                    txtCur.setPosition (txtCur.position() + j - i, QTextCursor::KeepAnchor);
                    setTextCursor (txtCur);
                }
                if (txtCur.hasSelection())
                {
                    QClipboard *cl = QApplication::clipboard();
                    if (cl->supportsSelection())
                    {
                        if (QMimeData *data = createMimeDataFromSelection())
                            cl->setMimeData (data, QClipboard::Selection);
                    }
                }
                e->accept();
                return;
            }
        }
        else
            tripleClickTimer_.invalidate();
    }

    QTextEdit::mousePressEvent (e);

    if (e->button() == Qt::LeftButton)
        pressPoint_ = e->pos();
}

/*************************/
void TextEdit::mouseReleaseEvent (QMouseEvent *e)
{
    QTextEdit::mouseReleaseEvent (e);
    if (e->button() != Qt::LeftButton)
        return;

    QString str = anchorAt (e->pos());
    if (!str.isEmpty()
        && cursorForPosition (e->pos()) == cursorForPosition (pressPoint_))
    {
        QUrl url (str);
        if (url.isRelative()) // treat relative URLs as local paths
            url = QUrl::fromUserInput (str, "/", QUrl::AssumeLocalFile);
        /* QDesktopServices::openUrl() may resort to "xdg-open", which isn't
           the best choice. "gio" is always reliable, so we check it first. */
        if (!QProcess::startDetached ("gio", QStringList() << "open" << url.toString()))
            QDesktopServices::openUrl (url);
    }
    pressPoint_ = QPoint();
}
/*************************/
void TextEdit::mouseDoubleClickEvent (QMouseEvent *e)
{
    tripleClickTimer_.start();
    QTextEdit::mouseDoubleClickEvent (e);

    /* Select the text between spaces with Ctrl.
       NOTE: QTextEdit should process the event before this. */
    if (e->button() == Qt::LeftButton
        && e->modifiers() == Qt::ControlModifier)
    {
        QTextCursor txtCur = textCursor();
        const int blockPos = txtCur.block().position();
        const QString blockText = txtCur.block().text();
        const int l = blockText.length();
        int anc = txtCur.anchor();
        int pos = txtCur.position();
        while (anc > blockPos && !blockText.at (anc - blockPos - 1).isSpace())
            -- anc;
        while (pos < blockPos + l && !blockText.at (pos - blockPos).isSpace())
            ++ pos;
        if (anc != textCursor().anchor() || pos != textCursor().position())
        {
            txtCur.setPosition (anc);
            txtCur.setPosition (pos, QTextCursor::KeepAnchor);
            setTextCursor (txtCur);
            if (txtCur.hasSelection())
            {
                QClipboard *cl = QApplication::clipboard();
                if (cl->supportsSelection())
                {
                    if (QMimeData *data = createMimeDataFromSelection())
                        cl->setMimeData (data, QClipboard::Selection);
                }
            }
            e->accept();
        }
    }
}
/*************************/
void TextEdit::wheelEvent (QWheelEvent *e)
{
    QPoint deltaPoint = e->angleDelta();
    if (e->modifiers() == Qt::ControlModifier)
    {
        float delta = deltaPoint.y() / 120.f;
        zooming (delta);
        return;
    }

    /* smooth scrolling */
    if (e->spontaneous() && e->source() == Qt::MouseEventNotSynthesized)
    {
        bool horizontal (qAbs (deltaPoint.x()) > qAbs (deltaPoint.y()));
        QScrollBar* sbar = horizontal ? horizontalScrollBar()
                                      : verticalScrollBar();
        if (sbar && sbar->isVisible())
        {
            int delta = horizontal ? deltaPoint.x() : deltaPoint.y();
            if ((delta > 0 && sbar->value() == sbar->minimum())
                || (delta < 0 && sbar->value() == sbar->maximum()))
            {
                return; // the scrollbar can't move
            }

            if (QApplication::wheelScrollLines() > 1)
            {
                if ((e->modifiers() & Qt::ShiftModifier)
                    || qAbs (delta) < 120) // touchpad
                { // scrolling with minimum speed
                    if (qAbs (delta) >= scrollAnimFrames * QApplication::wheelScrollLines())
                        delta /= QApplication::wheelScrollLines();
                }
            }

            /* wait until the angle delta reaches an acceptable value */
            static int _delta = 0;
            _delta += delta;
            if (abs(_delta) < scrollAnimFrames)
                return;

            if (!scrollTimer_)
            {
                scrollTimer_ = new QTimer();
                scrollTimer_->setTimerType (Qt::PreciseTimer);
                connect (scrollTimer_, &QTimer::timeout, this, &TextEdit::scrollSmoothly);
            }

            /* set the data for inertial scrolling */
            scrollData data;
            data.delta = _delta;
            data.leftFrames = scrollAnimFrames;
            data.vertical = !horizontal;
            queuedScrollSteps_.append (data);
            if (!scrollTimer_->isActive())
                scrollTimer_->start (1000 / SCROLL_FRAMES_PER_SEC);
            _delta = 0;
            return;
        }
    }

    /* as in QTextEdit::wheelEvent() */
    QAbstractScrollArea::wheelEvent (e);
    updateMicroFocus();
}
/*************************/
void TextEdit::scrollSmoothly()
{
    int totalDeltaH = 0, totalDeltaV = 0;
    QList<scrollData>::iterator it = queuedScrollSteps_.begin();
    while (it != queuedScrollSteps_.end())
    {
        int delta = qRound (static_cast<qreal>(it->delta) / static_cast<qreal>(scrollAnimFrames));
        int remainingDelta = it->delta - (scrollAnimFrames - it->leftFrames) * delta;
        if ((delta >= 0 && remainingDelta < 0) || (delta < 0 && remainingDelta >= 0))
            remainingDelta = 0;
        if (qAbs (delta) >= qAbs (remainingDelta))
        { // this is the last frame or, due to rounding, there can be no more frame
            if (it->vertical)
                totalDeltaV += remainingDelta;
            else
                totalDeltaH += remainingDelta;
            it = queuedScrollSteps_.erase (it);
        }
        else
        {
            if (it->vertical)
                totalDeltaV += delta;
            else
                totalDeltaH += delta;
            -- it->leftFrames;
            ++it;
        }
    }

    if (totalDeltaH != 0)
    {
        QScrollBar *hbar = horizontalScrollBar();
        if (hbar && hbar->isVisible())
        {
            QWheelEvent eventH (QPointF(),
                                QPointF(),
                                QPoint(),
                                QPoint (0, totalDeltaH),
                                Qt::NoButton,
                                Qt::NoModifier,
                                Qt::NoScrollPhase,
                                false);
            QCoreApplication::sendEvent (hbar, &eventH);
        }
    }
    if (totalDeltaV != 0)
    {
        QScrollBar *vbar = verticalScrollBar();
        if (vbar && vbar->isVisible())
        {
            QWheelEvent eventV (QPointF(),
                                QPointF(),
                                QPoint(),
                                QPoint (0, totalDeltaV),
                                Qt::NoButton,
                                Qt::NoModifier,
                                Qt::NoScrollPhase,
                                false);
            QCoreApplication::sendEvent (vbar, &eventV);
        }
    }

    if (queuedScrollSteps_.empty())
        scrollTimer_->stop();
}
/*************************/
void TextEdit::setEditorFont (const QFont &f)
{
    setFont (f);
    /* WARNING: The font given to setFont() shouldn't be used directly because,
                as Qt doc explains, its properties are combined with the widget's
                default font to form the widget's final font. */
    QFontMetricsF metrics (font()); // see FN::unZooming()
    setTabStopDistance (metrics.horizontalAdvance (textTab_));
}
/*************************/
void TextEdit::zooming (float range)
{
    if (range == 0.f) return;
    QFont f = document()->defaultFont();
    const float newSize = static_cast<float>(f.pointSizeF()) + range;
    if (newSize <= 0) return;
    f.setPointSizeF (static_cast<qreal>(newSize));
    setEditorFont (f);

    /* if this is a zoom-out, the text will need
       to be formatted and/or highlighted again */
    if (range < 0) emit zoomedOut (this);
}
/*************************/
// See createMimeDataFromSelection() for the reason.
void TextEdit::copy()
{
    isCopyOrCut_ = true;
    QTextEdit::copy();
    isCopyOrCut_ = false;
}
void TextEdit::cut()
{
    isCopyOrCut_ = true;
    QTextEdit::cut();
    isCopyOrCut_ = false;
}

}
