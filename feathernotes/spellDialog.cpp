/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2020 <tsujan2000@gmail.com>
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

#include "spellDialog.h"
#include "ui_spellDialog.h"
#include "spellChecker.h"

#include <QScreen>
#include <QWindow>

namespace FeatherNotes {

SpellDialog::SpellDialog (SpellChecker *spellChecker, const QString& word, QWidget *parent)
    : QDialog (parent), ui (new Ui::SpellDialog)
{
    ui->setupUi (this);
    setWindowModality (Qt::WindowModal);

    QWidget::setTabOrder (ui->replace, ui->listWidget);
    QWidget::setTabOrder (ui->listWidget, ui->ignoreOnce);
    QWidget::setTabOrder (ui->ignoreOnce, ui->ignoreAll);
    QWidget::setTabOrder (ui->ignoreAll, ui->correctOnce);
    QWidget::setTabOrder (ui->correctOnce, ui->correctAll);
    QWidget::setTabOrder (ui->correctAll, ui->addToDict);

    ui->ignoreOnce->setShortcut (QKeySequence (Qt::Key_F3));
    ui->ignoreOnce->setToolTip (QKeySequence (Qt::Key_F3).toString (QKeySequence::NativeText));
    ui->ignoreAll->setShortcut (QKeySequence (Qt::Key_F4));
    ui->ignoreAll->setToolTip (QKeySequence (Qt::Key_F4).toString (QKeySequence::NativeText));
    ui->correctOnce->setShortcut (QKeySequence (Qt::Key_F5));
    ui->correctOnce->setToolTip (QKeySequence (Qt::Key_F5).toString (QKeySequence::NativeText));
    ui->correctAll->setShortcut (QKeySequence (Qt::Key_F6));
    ui->correctAll->setToolTip (QKeySequence (Qt::Key_F6).toString (QKeySequence::NativeText));
    ui->addToDict->setShortcut (QKeySequence (Qt::Key_F7));
    ui->addToDict->setToolTip (QKeySequence (Qt::Key_F7).toString (QKeySequence::NativeText));

    spellChecker_ = spellChecker;

    connect (ui->correctOnce, &QAbstractButton::clicked, [this] {emit spellChecked (SpellAction::CorrectOnce);});
    connect (ui->ignoreOnce, &QAbstractButton::clicked, [this] {emit spellChecked (SpellAction::IgnoreOnce);});
    connect (ui->correctAll, &QAbstractButton::clicked, [this] {emit spellChecked (SpellAction::CorrectAll);});
    connect (ui->ignoreAll, &QAbstractButton::clicked, [this] {emit spellChecked (SpellAction::IgnoreAll);});

    connect (ui->addToDict, &QAbstractButton::clicked, [this] {emit spellChecked (SpellAction::AddToDict);});

    connect (ui->listWidget, &QListWidget::currentTextChanged, ui->replace, &QLineEdit::setText);

    checkWord (word);

    if (parent != nullptr)
    {
        if (QWindow *win = parent->windowHandle())
        {
            if (QScreen *sc = win->screen())
            {
                QSize ag = sc->availableGeometry().size()
                           - (parent->window()->frameGeometry().size() - parent->window()->geometry().size());
                resize (size().boundedTo (ag));
            }
        }
    }
}
/*************************/
SpellDialog::~SpellDialog()
{
    delete ui; ui = nullptr;
}
/*************************/
QString SpellDialog::replacement() const
{
    return ui->replace->text();
}
/*************************/
void SpellDialog::checkWord (const QString &word)
{
    if (word.isEmpty()) return;
    ui->replace->clear();
    ui->listWidget->clear();

    ui->misspelledLabel->setText (QString ("<b>%1</b>").arg (word));

    QStringList suggestions = spellChecker_->suggest (word);
    ui->listWidget->addItems (suggestions);
    if (!suggestions.isEmpty())
        ui->listWidget->setCurrentRow (0, QItemSelectionModel::Select);
}

}
