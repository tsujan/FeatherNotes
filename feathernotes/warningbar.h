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

#ifndef WARNINGBAR_H
#define WARNINGBAR_H

#include <QPointer>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QGridLayout>
#include <QPalette>
#include <QLabel>
#include <QPropertyAnimation>

#define DURATION 150

namespace FeatherNotes {

class WarningBar : public QWidget
{
    Q_OBJECT
public:
    WarningBar (const QString& message, int timeout = 10, QWidget *parent = nullptr)
        : QWidget (parent), timer_ (nullptr) {
        int anotherBar (false);
        if (parent)
        { // show only one warning bar at a time
            const QList<WarningBar*> warningBars = parent->findChildren<WarningBar*>();
            for (WarningBar *wb : warningBars)
            {
                if (wb != this)
                {
                    wb->closeBar();
                    anotherBar = true;
                }
            }
        }

        message_ = message;
        isClosing_ = false;
        mousePressed_ = false;

        /* make it like a translucent layer */
        setAutoFillBackground (true);
        QPalette p = palette();
        p.setColor (foregroundRole(), Qt::white);
        p.setColor (backgroundRole(), timeout > 0 ? QColor (125, 0, 0, 200)
                                                  : timeout == 0 ? QColor (0, 70, 0, 210)
                                                                 : QColor (0, 0, 90, 200));
        setPalette (p);

        grid_ = new QGridLayout;
        grid_->setContentsMargins (5, 0, 5 ,5); // the top margin is added when setting the geometry
        /* use a spacer to compress the label vertically */
        QSpacerItem *spacer = new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
        grid_->addItem (spacer, 0, 0);
        /* add the label */
        QLabel *warningLabel = new QLabel (message);
        warningLabel->setAttribute (Qt::WA_TransparentForMouseEvents, true); // not needed
        warningLabel->setWordWrap (true);
        grid_->addWidget (warningLabel, 1, 0);
        setLayout (grid_);

        if (parent)
        { // compress the bar vertically and show it with animation
            QTimer::singleShot (anotherBar ? DURATION + 10 : 0, this, [=]() {
                parent->installEventFilter (this);
                int h = grid_->minimumHeightForWidth (parent->width()) + grid_->contentsMargins().bottom();
                QRect g (0, parent->height() - h, parent->width(), h);
                setGeometry (g);

                animation_ = new QPropertyAnimation (this, "geometry", this);
                animation_->setEasingCurve (QEasingCurve::Linear);
                animation_->setDuration (DURATION);
                animation_->setStartValue (QRect (0, parent->height(), parent->width(), 0));
                animation_->setEndValue (g);
                animation_->start();
                show();
            });
        }
        else show();

        /* auto-close */
        setTimeout (timeout);
    }

    bool isPermanent() const {
        return timer_ == nullptr;
    }

    void setTimeout (int timeout) { // can be used to change the timeout
        if (timeout <= 0)
        {
            if (timer_ != nullptr)
            {
                timer_->stop();
                delete timer_;
                timer_ = nullptr;
            }
        }
        else
        {
            if (timer_ == nullptr)
            {
                timer_ = new QTimer (this);
                timer_->setSingleShot (true);
                connect (timer_, &QTimer::timeout, this, [this]() {
                    if (!mousePressed_) closeBar();
                });
            }
            timer_->start (timeout * 1000);
        }
    }

    bool eventFilter (QObject *o, QEvent *e) {
        if (e->type() == QEvent::Resize)
        {
            if (QWidget *w = qobject_cast<QWidget*>(o))
            {
                if (w == parentWidget())
                { // compress the bar as far as its text is shown completely
                    int h = grid_->minimumHeightForWidth (w->width()) + grid_->contentsMargins().bottom();
                    setGeometry (QRect (0, w->height() - h, w->width(), h));
                }
            }
        }
        return false;
    }

    QString getMessage() const {
        return message_;
    }

    bool isClosing() const {
        return isClosing_;
    }

public slots:
    void closeBar() {
        if (animation_ && parentWidget())
        {
            if (!isClosing_)
            {
                isClosing_ = true;
                parentWidget()->removeEventFilter (this); // no movement on closing
                animation_->stop();
                animation_->setStartValue (geometry());
                animation_->setEndValue (QRect (0, parentWidget()->height(), parentWidget()->width(), 0));
                animation_->start();
                connect (animation_, &QAbstractAnimation::finished, this, &QObject::deleteLater);
            }
        }
        else delete this;
    }

protected:
    void mousePressEvent (QMouseEvent *event) {
        QWidget::mousePressEvent (event);
        mousePressed_ = true;
    }
    void mouseReleaseEvent (QMouseEvent *event) {
        QWidget::mouseReleaseEvent (event);
        mousePressed_ = false;
        QTimer::singleShot (0, this, &WarningBar::closeBar);
    }

private:
    QString message_;
    bool isClosing_;
    bool mousePressed_;
    QTimer *timer_;
    QGridLayout *grid_;
    QPointer<QPropertyAnimation> animation_;
};

}

#endif // WARNINGBAR_H
