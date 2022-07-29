/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2022 <tsujan2000@gmail.com>
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

#include "printing.h"
#include <QPainter>
#include <QTextFrame>
#include <QAbstractTextDocumentLayout>

namespace FeatherNotes {

Printing::Printing (QTextDocument *document, const QString &fileName,
                    qreal sourceDpiX, qreal sourceDpiY, const QColor &textColor)
{
    sourceDpiX_ = sourceDpiX;
    sourceDpiY_ = sourceDpiY;
    textColor_ = textColor;

    printer_ = new QPrinter (QPrinter::HighResolution);
    if (printer_->outputFormat() == QPrinter::PdfFormat)
        printer_->setOutputFileName (fileName);

    /* clone the document */
    document_ = document->clone();
    /* ensure that there's a layout
       (QTextDocument::documentLayout will create the standard layout if there's none) */
    (void)document_->documentLayout();
}
/*************************/
Printing::~Printing()
{
    delete document_;
    delete printer_;
}
/*************************/
static void printPage (int index, QPainter *painter, const QTextDocument *doc,
                       const QRectF &body, const QPointF &pageNumberPos, QColor textColor)
{
    painter->save();
    painter->translate (body.left(), body.top() - (index - 1) * body.height());
    QRectF view (0, (index - 1) * body.height(), body.width(), body.height());

    QAbstractTextDocumentLayout *layout = doc->documentLayout();
    QAbstractTextDocumentLayout::PaintContext ctx;

    painter->setClipRect (view);
    ctx.clip = view;
    layout->draw (painter, ctx);

    if (!pageNumberPos.isNull())
    {
        painter->setPen (textColor);
        painter->setClipping (false);
        painter->setFont (QFont (doc->defaultFont()));
        const QString pageString = QString::number (index);

        painter->drawText(qRound (pageNumberPos.x() - painter->fontMetrics().horizontalAdvance (pageString) / 2),
                          qRound (pageNumberPos.y() + view.top()),
                          pageString);
    }

    painter->restore();
}

// Control printing to make it work fine with dark backgrounds and also to enable
// printing in the reverse order. This is adapted from "QTextDocument::print".
void Printing::run()
{
    QPainter p (printer_);
    if (!p.isActive()) return;

    if (printer_->pageLayout().paintRectPixels (printer_->resolution()).isEmpty())
        return;

    QAbstractTextDocumentLayout *layout = document_->documentLayout();
    layout->setPaintDevice (p.device());

    const qreal dpiScaleY = static_cast<qreal>(printer_->logicalDpiY()) / sourceDpiY_;

    const int horizontalMargin = static_cast<int>((2/2.54) * sourceDpiX_);
    const int verticalMargin = static_cast<int>((2/2.54) * sourceDpiY_);
    QTextFrameFormat fmt = document_->rootFrame()->frameFormat();
    fmt.setLeftMargin (horizontalMargin);
    fmt.setRightMargin (horizontalMargin);
    fmt.setTopMargin (verticalMargin);
    fmt.setBottomMargin (verticalMargin);
    document_->rootFrame()->setFrameFormat (fmt);

    const int dpiy = p.device()->logicalDpiY();
    QRectF body (0, 0, printer_->width(), printer_->height());
    QPointF pageNumberPos (body.width() / 2,
                           body.height() - verticalMargin * dpiScaleY
                               + QFontMetrics (document_->defaultFont(), p.device()).ascent()
                               + 5 * dpiy / 72.0);
    document_->setPageSize (body.size());

    int fromPage = printer_->fromPage();
    int toPage = printer_->toPage();

    if (fromPage == 0 && toPage == 0)
    {
        fromPage = 1;
        toPage = document_->pageCount();
    }

    fromPage = qMax (1, fromPage);
    toPage = qMin (document_->pageCount(), toPage);

    if (toPage < fromPage) return;

    bool reverse (printer_->pageOrder() == QPrinter::LastPageFirst);
    int page = reverse ? toPage : fromPage;
    while (true)
    {
        printPage (page, &p, document_, body, pageNumberPos, textColor_);
        if (reverse)
        {
            if (page == fromPage) break;
            --page;
        }
        else
        {
            if (page == toPage) break;
            ++page;
        }
        if (!printer_->newPage())
            return;
    }
}

}
