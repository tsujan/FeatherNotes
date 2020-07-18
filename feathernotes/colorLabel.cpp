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

#include "colorLabel.h"
#include <QColorDialog>
#include <QStyleOptionFrame>
#include <QPainter>

namespace FeatherNotes {

ColorLabel::ColorLabel (QWidget *parent, Qt::WindowFlags f)
    : QLabel (parent, f)
{
    setFrameStyle (QFrame::Panel | QFrame::Sunken);
    setLineWidth (1);
    setMinimumWidth (100);
    setToolTip (tr ("Click to change color."));
    color_ = palette().color (QPalette::Window);
}

ColorLabel::~ColorLabel() {}

void ColorLabel::setColor (const QColor &color)
{
    if (!color.isValid())
        return;
    color_ = color;
    color_.setAlpha (255);
}

QColor ColorLabel::getColor() const
{
    return color_;
}

void ColorLabel::mousePressEvent (QMouseEvent* /*event*/)
{
    QColor prevColor = getColor();
    QColor color = QColorDialog::getColor (prevColor, window(), tr ("Select Color"));
    if (color.isValid() && color != prevColor)
        setColor (color);
}

void ColorLabel::paintEvent (QPaintEvent* /*event*/)
{
    QPainter p (this);
    p.fillRect (contentsRect(), color_);
    QStyleOptionFrame opt;
    initStyleOption (&opt);
    style()->drawControl (QStyle::CE_ShapedFrame, &opt, &p, this);
}

}
