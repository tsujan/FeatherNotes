/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019 <tsujan2000@gmail.com>
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

#include "pref.h"
#include "ui_predDialog.h"

#include "fn.h"

#include <QWindow>
#include <QScreen>
#include <QPushButton>
#include <QWhatsThis>

namespace FeatherNotes {

PrefDialog::PrefDialog (QWidget *parent)
    : QDialog (parent), ui (new Ui::PrefDialog)
{
    ui->setupUi (this);
    parent_ = parent;

    if (FN *win = static_cast<FN *>(parent_))
    {
        /**************
         *** Window ***
         **************/

        QSize ag;
        if (QWindow *window = parent->windowHandle())
        {
            if (QScreen *sc = window->screen())
                ag = sc->availableVirtualGeometry().size();
        }
        if (!ag.isValid()) ag = QSize (0, 0);

        /* window size */
        ui->winSpinX->setMaximum (ag.width());
        ui->winSpinY->setMaximum (ag.height());
        ui->winSizeBox->setChecked (win->isSizeRem());
        if (win->isSizeRem())
        {
            ui->winSpinX->setValue (win->getWinSize().width());
            ui->winSpinY->setValue (win->getWinSize().height());
            ui->winSpinX->setEnabled (false);
            ui->winSpinY->setEnabled (false);
            ui->winLabel->setEnabled (false);
            ui->winXLabel->setEnabled (false);
        }
        else
        {
            ui->winSpinX->setValue (win->getStartSize().width());
            ui->winSpinY->setValue (win->getStartSize().height());
        }
        connect (ui->winSpinX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrefDialog::prefSize);
        connect (ui->winSpinY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrefDialog::prefSize);
        connect (ui->winSizeBox, &QCheckBox::stateChanged, this, &PrefDialog::prefSize);

        /* splitter position */
        ui->splitterSizeBox->setChecked (win->isSplitterRem());
        connect (ui->splitterSizeBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            if (checked == Qt::Checked)
            {
                win->remSplitter (true);
                win->setSplitterSizes (win->getSpltiterState());
            }
            else if (checked == Qt::Unchecked)
                win->remSplitter (false);
        });

        /* window position */
        ui->positionBox->setChecked (win->isPositionRem());
        connect (ui->positionBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            if (checked == Qt::Checked)
            {
                win->remPosition (true);
                win->setPosition (win->geometry().topLeft());
                if (win->isUnderE())
                {
                    ui->ELabel->setEnabled (true);
                    ui->XLabel->setEnabled (true);
                    ui->ESpinX->setEnabled (true);
                    ui->ESpinY->setEnabled (true);
                }
            }
            else if (checked == Qt::Unchecked)
            {
                win->remPosition (false);
                if (win->isUnderE())
                {
                    ui->ELabel->setEnabled (false);
                    ui->XLabel->setEnabled (false);
                    ui->ESpinX->setEnabled (false);
                    ui->ESpinY->setEnabled (false);
                }
            }
        });

        /* use tray */
        ui->hasTrayBox->setChecked (win->hasTray());
        connect (ui->hasTrayBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->useTray (checked == Qt::Checked);
        });
        connect (ui->hasTrayBox, &QAbstractButton::toggled, ui->minTrayBox, &QWidget::setEnabled);

        /* iconify into tray */
        ui->minTrayBox->setChecked (win->doesMinToTray());
        if (!win->hasTray())
            ui->minTrayBox->setDisabled (true);
        connect (ui->minTrayBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->minToTray (checked == Qt::Checked);
        });

        /* transparent tree view */
        ui->transparentTree->setChecked (win->hasTransparentTree());
        connect (ui->transparentTree, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->makeTreeTransparent (checked == Qt::Checked);
        });

        /* small toolbar icons */
        ui->smallToolbarIcons->setChecked (win->hasSmallToolbarIcons());
        connect (ui->smallToolbarIcons, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->setToolBarIconSize (checked == Qt::Checked);
        });

        /* hide toolbar or menubar */
        ui->noToolbar->setChecked (win->withoutToolbar());
        ui->noMenubar->setChecked (win->withoutMenubar());
        connect (ui->noToolbar, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            if (checked == Qt::Checked)
            {
                win->showToolbar (false);
                ui->noMenubar->setChecked (false);
            }
            else if (checked == Qt::Unchecked)
                win->showToolbar (true);
        });
        connect (ui->noMenubar, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            if (checked == Qt::Checked)
            {
                win->showMenubar (false);
                ui->noToolbar->setChecked (false);
            }
            else if (checked == Qt::Unchecked)
                win->showMenubar (true);
        });

        /* Enlightenment */
        ui->EBox->setChecked (win->isUnderE());
        ui->ESpinX->setValue (win->EShift().width());
        ui->ESpinY->setValue (win->EShift().height());
        if (!win->isUnderE() || !win->isPositionRem())
        {
            ui->ELabel->setDisabled (true);
            ui->ESpinX->setDisabled (true);
            ui->XLabel->setDisabled (true);
            ui->ESpinY->setDisabled (true);
        }
        connect (ui->ESpinX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), win, [win] (int value) {
            win->setEShift (QSize (value, win->EShift().height())); // takes effect in FN::showEvent()
        });
        connect (ui->ESpinY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), win, [win] (int value) {
            win->setEShift (QSize (win->EShift().width(), value)); // takes effect in FN::showEvent()
        });
        connect (ui->EBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            bool isChecked (checked == Qt::Checked);
            win->setUnderE (isChecked);
            ui->ESpinX->setEnabled (isChecked);
            ui->ESpinY->setEnabled (isChecked);
            ui->ELabel->setEnabled (isChecked);
            ui->XLabel->setEnabled (isChecked);
            if (isChecked)
                win->setEShift (QSize (ui->ESpinX->value(), ui->ESpinY->value()));
        });

        /************
         *** Text ***
         ************/

        /* wrapping */
        ui->wrapBox->setChecked (win->isWrappedByDefault());
        connect (ui->wrapBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->wrapByDefault (checked == Qt::Checked);
        });

        /* indentation */
        ui->indentBox->setChecked (win->isIndentedByDefault());
        connect (ui->indentBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->indentByDefault (checked == Qt::Checked);
        });

        /* auto-bracket */
        ui->autoBracketBox->setChecked (win->hasAutoBracket());
        connect (ui->autoBracketBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->autoBracket (checked == Qt::Checked);
        });

        /* auto-saving */
        ui->autoSaveSpinBox->setRange (1, 60); // not needed
        if (win->getAutoSave() > -1)
        {
            ui->autoSaveSpinBox->setValue (win->getAutoSave());
            ui->autoSaveBox->setChecked (true);
        }
        else
        {
            ui->autoSaveSpinBox->setEnabled (false);
            ui->autoSaveSpinBox->setValue (5);
        }
        connect (ui->autoSaveSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), win, [win] (int value) {
            win->setAutoSave (value); // will take effect at closeEvent()
        });
        connect (ui->autoSaveBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            if (checked == Qt::Checked)
            {
                ui->autoSaveSpinBox->setEnabled (true);
                win->setAutoSave (ui->autoSaveSpinBox->value());
            }
            else if (checked == Qt::Unchecked)
            {
                ui->autoSaveSpinBox->setEnabled (false);
                win->setAutoSave (-1);
            }
        });

        /* scroll jump workaround */
        ui->workaroundBox->setChecked (win->isScrollJumpWorkaroundEnabled());
        connect (ui->workaroundBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->enableScrollJumpWorkaround (checked == Qt::Checked);
        });
    }

    if (QPushButton *closeBtn = ui->buttonBox->button (QDialogButtonBox::Close))
    {
        closeBtn->setAutoDefault (true);
        closeBtn->setDefault (true);
        connect (closeBtn, &QAbstractButton::clicked, this, &QDialog::close);
    }
    if (QPushButton *helpBtn = ui->buttonBox->button (QDialogButtonBox::Help))
    {
        connect (helpBtn, &QAbstractButton::clicked, [] () {
            QWhatsThis::enterWhatsThisMode();
        });
    }

    const QString EToolTip = ui->ELabel->toolTip();
    ui->ESpinX->setToolTip (EToolTip);
    ui->ESpinY->setToolTip (EToolTip);
    ui->XLabel->setToolTip (EToolTip);

    /* set tooltip as "whatsthis" */
    const auto widgets = findChildren<QWidget*>();
    for (QWidget *w : widgets)
    {
        QString tip = w->toolTip();
        if (!tip.isEmpty())
        {
            w->setWhatsThis (tip.replace ('\n', ' ').replace ("  ", "\n\n"));
            /* for the tooltip mess in Qt 5.12 */
            w->setToolTip ("<p style='white-space:pre'>" + w->toolTip() + "</p>");
        }
    }

    resize (sizeHint()); // make it compact
}
/*************************/
PrefDialog::~PrefDialog()
{
    delete ui; ui = nullptr;
}
/*************************/
void PrefDialog::closeEvent (QCloseEvent *event)
{
    if (FN *win = static_cast<FN *>(parent_))
    {
        win->writeConfig();
        win->writeGeometryConfig();
    }
    event->accept();
}
/*************************/
void PrefDialog::prefSize (int value)
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;
    if (qobject_cast<QCheckBox *>(QObject::sender()))
    {
        if (value == Qt::Checked)
        {
            win->remSize (true);
            win->setWinSize (win->size());
            ui->winSpinX->setEnabled (false);
            ui->winSpinY->setEnabled (false);
            ui->winLabel->setEnabled (false);
            ui->winXLabel->setEnabled (false);
        }
        else if (value == Qt::Unchecked)
        {
            win->remSize (false);
            win->setStartSize (win->getWinSize());
            ui->winSpinX->setEnabled (true);
            ui->winSpinY->setEnabled (true);
            ui->winLabel->setEnabled (true);
            ui->winXLabel->setEnabled (true);
        }
    }
    else if (ui->winSpinX == qobject_cast<QSpinBox *>(QObject::sender()))
        win->setStartSize (QSize (ui->winSpinX->value(), win->getStartSize().height()));
    else if (ui->winSpinY == qobject_cast<QSpinBox *>(QObject::sender()))
        win->setStartSize (QSize (win->getStartSize().width(), ui->winSpinY->value()));
}

}

