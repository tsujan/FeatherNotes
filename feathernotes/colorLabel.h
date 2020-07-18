/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2020 <tsujan2000@gmail.com>
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

#ifndef COLORLABEL_H
#define COLORLABEL_H

#include <QLabel>
#include <QWidget>
#include <Qt>

namespace FeatherNotes {

class ColorLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ColorLabel (QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~ColorLabel();

    void setColor (const QColor &color);
    QColor getColor() const;

protected:
    void mousePressEvent (QMouseEvent *event) override;
    void paintEvent (QPaintEvent *event) override;

private:
    QColor color_;
};

}

#endif // COLORLABEL_H
