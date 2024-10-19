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

#ifndef PRINTING_H
#define PRINTING_H

#include <QThread>
#include <QColor>
#include <QTextDocument>
#include <QPrinter>

namespace FeatherNotes {

class Printing : public QThread {
    Q_OBJECT

public:
    Printing (QTextDocument *document, const QString &fileName,
              qreal sourceDpiX, qreal sourceDpiY, const QColor &textColor);
    ~Printing();

    QPrinter* printer() const {
        return printer_;
    }

private:
    void run();

    QTextDocument *origDoc_;
    QTextDocument *clonedDoc_;
    QPrinter *printer_;
    qreal sourceDpiX_;
    qreal sourceDpiY_;
    QColor textColor_;
};

}

#endif // PRINTING_H
