/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2019-2022 <tsujan2000@gmail.com>
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
#include "ui_prefDialog.h"

#include "fn.h"

#ifdef HAS_HUNSPELL
#include "filedialog.h"
#endif

#include <QWindow>
#include <QScreen>
#include <QPushButton>
#include <QAction>
#include <QWhatsThis>

namespace FeatherNotes {

static QHash<QString, QString> OBJECT_NAMES;
static QHash<QString, QString> DEFAULT_SHORTCUTS;
/*************************/
FNKeySequenceEdit::FNKeySequenceEdit (QWidget *parent) : QKeySequenceEdit (parent) {}

void FNKeySequenceEdit::keyPressEvent (QKeyEvent *event)
{ // also a workaround for a Qt bug that makes Meta a non-modifier
    clear(); // no multiple shortcuts
    int k = event->key();
    if ((event->modifiers() != Qt::NoModifier && event->modifiers() != Qt::KeypadModifier))
    {
        if (k == Qt::Key_Super_L || k == Qt::Key_Super_R)
            return;
    }
    /* don't create a shortcut without modifier because
       this is a text editor but make exceptions for Fx keys */
    else if (k < Qt::Key_F1 || k > Qt::Key_F35)
        return;
    QKeySequenceEdit::keyPressEvent (event);
}
/*************************/
Delegate::Delegate (QObject *parent)
    : QStyledItemDelegate (parent) {}

QWidget* Delegate::createEditor (QWidget *parent,
                                 const QStyleOptionViewItem& /*option*/,
                                 const QModelIndex& /*index*/) const
{
    return new FNKeySequenceEdit (parent);
}
/*************************/
bool Delegate::eventFilter (QObject *object, QEvent *event)
{
    FNKeySequenceEdit *editor = qobject_cast<FNKeySequenceEdit*>(object);
    if (editor && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        int k = ke->key();
        if (k == Qt::Key_Return || k == Qt::Key_Enter)
        {
            emit QAbstractItemDelegate::commitData (editor);
            emit QAbstractItemDelegate::closeEditor (editor);
            return true;
        }
        /* treat Tab and Backtab like other keys */
        if (k == Qt::Key_Tab || k ==  Qt::Key_Backtab)
        {
            editor->pressKey (ke);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter (object, event);
}
/*************************/
PrefDialog::PrefDialog (QWidget *parent)
    : QDialog (parent), ui (new Ui::PrefDialog)
{
    ui->setupUi (this);
    parent_ = parent;
    ui->promptLabel->setStyleSheet ("QLabel {background-color: #7d0000; color: white; border-radius: 3px; margin: 2px; padding: 5px;}");
    ui->promptLabel->hide();
    promptTimer_ = nullptr;

    Delegate *del = new Delegate (ui->tableWidget);
    ui->tableWidget->setItemDelegate (del);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode (QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionsClickable (true);
    ui->tableWidget->sortByColumn (0, Qt::AscendingOrder);

    QSize ag;
    FN *win = static_cast<FN *>(parent_);
    if (win)
    {
        /**************
         *** Window ***
         **************/

        if (QWindow *window = parent->windowHandle())
        {
            if (QScreen *sc = window->screen())
                ag = sc->availableGeometry().size();
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
            win->remSplitter (checked == Qt::Checked);
        });

        /* window position */
        ui->positionBox->setChecked (win->isPositionRem());
        connect (ui->positionBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            win->remPosition (checked == Qt::Checked);
        });

        /* use tray */
        ui->hasTrayBox->setChecked (win->hasTray());
        hasTray_ = win->hasTray();
        connect (ui->hasTrayBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            win->useTray (checked == Qt::Checked);
            showPrompt();
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
        autoBracket_ = win->hasAutoBracket();
        connect (ui->autoBracketBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            win->autoBracket (checked == Qt::Checked);
            showPrompt();
        });

        /* auto-replace */
        ui->autoReplaceBox->setChecked (win->hasAutoReplace());
        autoReplace_ = win->hasAutoReplace();
        connect (ui->autoReplaceBox, &QCheckBox::stateChanged, win, [this, win] (int checked) {
            win->autoReplace (checked == Qt::Checked);
            showPrompt();
        });

        ui->dateEdit->setPlaceholderText (locale().dateTimeFormat());
        ui->dateEdit->setText (win->getDateFormat());

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

        /* starting with the last opened file */
        ui->lastFileBox->setChecked (win->openLastFile());
        connect (ui->lastFileBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->setOpenLastFile (checked == Qt::Checked);
        });

        /* the number of recently opend files */
        ui->recentSpin->setValue (win->getRecentFilesNumber());
        connect (ui->recentSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), win, [win] (int value) {
            win->setRecentFilesNumber (value); // will take effect at closeEvent()
        });
        /* whether recent files should be opened separately */
        ui->recentBox->setChecked (win->getOpenReccentSeparately()); // up-to-date because of FN::getRecentFilesNumber()
        connect (ui->recentBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->setOpenReccentSeparately (checked == Qt::Checked);
        });

        /* whether expanded states of nodes should be remembered */
        ui->expandBox->setChecked (win->getRememberExpanded());
        connect (ui->expandBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->setRememberExpanded (checked == Qt::Checked);
            if (checked == Qt::Checked)
                win->setCollapsedStates();
        });

        /* saving on exiting */
        ui->exitSaveBox->setChecked (win->getSaveOnExit());
        connect (ui->exitSaveBox, &QCheckBox::stateChanged, win, [win] (int checked) {
            win->setSaveOnExit (checked == Qt::Checked);
        });

        /* spell checking */
#ifdef HAS_HUNSPELL
        ui->dictEdit->setText (win->getDictPath());
        connect (ui->dictButton, &QAbstractButton::clicked, this, &PrefDialog::addDict);
        connect (ui->dictEdit, &QLineEdit::editingFinished, this, &PrefDialog::addDict);
#else
        ui->dictGroupBox->setVisible (false);
#endif

        /*****************
         *** Shortcuts ***
         *****************/

        if (DEFAULT_SHORTCUTS.isEmpty())
        { // NOTE: Shortcut strings hould be in the PortableText format.
            const auto defaultShortcuts = win->defaultShortcuts();
            QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts.constBegin();
            while (iter != defaultShortcuts.constEnd())
            {
                const QString name = iter.key()->objectName();
                DEFAULT_SHORTCUTS.insert (name, iter.value().toString());
                OBJECT_NAMES.insert (iter.key()->text().remove ("&"), name);
                ++ iter;
            }
        }
        QHash<QString, QString> ca = win->customShortcutActions();

        QList<QString> keys = ca.keys();
        QHash<QString, QString>::const_iterator iter = OBJECT_NAMES.constBegin();
        while (iter != OBJECT_NAMES.constEnd())
        {
            shortcuts_.insert (iter.key(),
                               keys.contains (iter.value()) ? ca.value (iter.value())
                                                            : DEFAULT_SHORTCUTS.value (iter.value()));
            ++ iter;
        }

        QList<QString> val = shortcuts_.values();
        for (int i = 0; i < val.size(); ++i)
        {
            if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
            {
                showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
                break;
            }
        }

        ui->tableWidget->setRowCount (shortcuts_.size());
        ui->tableWidget->setSortingEnabled (false);
        int index = 0;
        QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
        while (it != shortcuts_.constEnd())
        {
            QTableWidgetItem *item = new QTableWidgetItem (it.key());
            item->setFlags (item->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
            ui->tableWidget->setItem (index, 0, item);
            /* shortcut texts should added in the NativeText format */
            ui->tableWidget->setItem (index, 1, new QTableWidgetItem (QKeySequence (it.value(), QKeySequence::PortableText)
                                                                      .toString (QKeySequence::NativeText)));
            ++ it;
            ++ index;
        }
        ui->tableWidget->setSortingEnabled (true);
        ui->tableWidget->setCurrentCell (0, 1);
        connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
        connect (ui->defaultButton, &QAbstractButton::clicked, this, &PrefDialog::restoreDefaultShortcuts);
        ui->defaultButton->setDisabled (ca.isEmpty());
    }

    if (QPushButton *closeBtn = ui->buttonBox->button (QDialogButtonBox::Close))
    {
        closeBtn->setAutoDefault (true);
        closeBtn->setDefault (true);
        connect (closeBtn, &QAbstractButton::clicked, this, &QDialog::close);
    }
    connect (this, &QDialog::rejected, this, &PrefDialog::onClosing);
    if (QPushButton *helpBtn = ui->buttonBox->button (QDialogButtonBox::Help))
    {
        connect (helpBtn, &QAbstractButton::clicked, [] () {
            QWhatsThis::enterWhatsThisMode();
        });
    }

    /* set tooltip as "whatsthis" */
    const auto widgets = findChildren<QWidget*>();
    for (const auto &w : widgets)
    {
        QString tip = w->toolTip();
        if (!tip.isEmpty())
        {
            w->setWhatsThis (tip.replace ('\n', ' ').replace ("  ", "\n\n"));
            /* for the tooltip mess in Qt 5.12 */
            w->setToolTip ("<p style='white-space:pre'>" + w->toolTip() + "</p>");
        }
    }

    if (win)
    {
        ag -= win->window()->frameGeometry().size() - win->window()->geometry().size();
        if (win->getPrefSize().isEmpty())
            resize (sizeHint().boundedTo(ag));
        else
            resize (win->getPrefSize().boundedTo(ag));
    }
    else resize (sizeHint().boundedTo(ag)); // impossible
}
/*************************/
PrefDialog::~PrefDialog()
{
    if (promptTimer_)
    {
        promptTimer_->stop();
        delete promptTimer_;
    }
    delete ui; ui = nullptr;
}
/*************************/
void PrefDialog::closeEvent (QCloseEvent *event)
{
    onClosing();
    event->accept();
}
/*************************/
void PrefDialog::onClosing()
{
    if (FN *win = static_cast<FN *>(parent_))
    {
        QHash<QString, QString>::const_iterator it = newShortcuts_.constBegin();
        while (it != newShortcuts_.constEnd())
        {
            if (DEFAULT_SHORTCUTS.value (it.key()) == it.value())
                win->removeShortcut (it.key());
            else
                win->setActionShortcut (it.key(), it.value());
            ++it;
        }
        win->updateCustomizableShortcuts();

        QString format = ui->dateEdit->text();
        /* if "\n" is typed in the line-edit, interpret
           it as a newline because we're on Linux */
        if (!format.isEmpty())
            format.replace ("\\n", "\n");
        win->setDateFormat (format);

        win->setPrefSize (size());

        win->writeConfig();
        win->writeGeometryConfig (false); // doesn't save the last node info
        win->rememberLastOpenedFile (true); // saves everything about recent files
    }
}
/*************************/
void PrefDialog::prefSize (int value)
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;

    if (qobject_cast<QCheckBox*>(QObject::sender()))
    {
        if (value == Qt::Checked)
        {
            win->remSize (true);
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
    else if (ui->winSpinX == qobject_cast<QSpinBox*>(QObject::sender()))
        win->setStartSize (QSize (ui->winSpinX->value(), win->getStartSize().height()));
    else if (ui->winSpinY == qobject_cast<QSpinBox*>(QObject::sender()))
        win->setStartSize (QSize (win->getStartSize().width(), ui->winSpinY->value()));
}
/*************************/
// NOTE: Custom shortcuts will be saved in the PortableText format.
void PrefDialog::onShortcutChange (QTableWidgetItem *item)
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;

    QString desc = ui->tableWidget->item (ui->tableWidget->currentRow(), 0)->text();

    QString txt = item->text();
    if (!txt.isEmpty())
    {
        /* the QKeySequenceEdit text is in the NativeText format but it
           should be converted into the PortableText format for saving */
        QKeySequence keySeq (txt);
        txt = keySeq.toString();
    }

    if (!txt.isEmpty() && win->reservedShortcuts().contains (txt)
        && DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (desc)) != txt) // always true

    {
        showPrompt (tr ("The typed shortcut was reserved."), true);
        disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
        item->setText (shortcuts_.value (desc));
        connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    }
    else
    {
        shortcuts_.insert (desc, txt);
        newShortcuts_.insert (OBJECT_NAMES.value (desc), txt);

        /* check for ambiguous shortcuts */
        bool ambiguous = false;
        QList<QString> val = shortcuts_.values();
        for (int i = 0; i < val.size(); ++i)
        {
            if (!val.at (i).isEmpty() && val.indexOf (val.at (i), i + 1) > -1)
            {
                showPrompt (tr ("Warning: Ambiguous shortcut detected!"), false);
                ambiguous = true;
                break;
            }
        }
        if (!ambiguous && ui->promptLabel->isVisible())
        {
            prevtMsg_ = QString();
            showPrompt();
        }

        /* also set the state of the Default button */
        QHash<QString, QString>::const_iterator it = shortcuts_.constBegin();
        while (it != shortcuts_.constEnd())
        {
            if (DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key())) != it.value())
            {
                ui->defaultButton->setEnabled (true);
                return;
            }
            ++it;
        }
        ui->defaultButton->setEnabled (false);
    }
}
/*************************/
void PrefDialog::restoreDefaultShortcuts()
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;

    if (newShortcuts_.isEmpty()
        && win->customShortcutActions().isEmpty())
    { // do nothing if there's no custom shortcut
        return;
    }

    disconnect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);
    int cur = ui->tableWidget->currentColumn() == 0
                  ? 0
                  : ui->tableWidget->currentRow();
    ui->tableWidget->setSortingEnabled (false);
    newShortcuts_ = DEFAULT_SHORTCUTS;
    int index = 0;
    QMutableHashIterator<QString, QString> it (shortcuts_);
    while (it.hasNext())
    {
        it.next();
        ui->tableWidget->item (index, 0)->setText (it.key());
        QString s = DEFAULT_SHORTCUTS.value (OBJECT_NAMES.value (it.key()));
        ui->tableWidget->item (index, 1)->setText (s);
        it.setValue (s);
        ++ index;
    }
    ui->tableWidget->setSortingEnabled (true);
    ui->tableWidget->setCurrentCell (cur, 1);
    connect (ui->tableWidget, &QTableWidget::itemChanged, this, &PrefDialog::onShortcutChange);

    ui->defaultButton->setEnabled (false);
    if (ui->promptLabel->isVisible())
    {
        prevtMsg_ = QString();
        showPrompt();
    }
}
/*************************/
void PrefDialog::showPrompt (const QString& str, bool temporary)
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;

    if (!str.isEmpty())
    { // show the provided message
        ui->promptLabel->setText ("<b>" + str + "</b>");
        if (temporary) // show it temporarily
        {
            if (promptTimer_ == nullptr)
            {
                promptTimer_ = new QTimer();
                promptTimer_->setSingleShot (true);
                connect (promptTimer_, &QTimer::timeout, [this] {
                    if (!prevtMsg_.isEmpty()
                        && ui->tabWidget->currentIndex() == 3) // Shortcuts page
                    { // show the previous message if it exists
                        ui->promptLabel->setText (prevtMsg_);
                    }
                    else showPrompt();
                });
            }
            promptTimer_->start (3300);
        }
        else
            prevtMsg_ = "<b>" + str + "</b>";
    }
    else if (hasTray_ != win->hasTray()
             || autoBracket_ != win->hasAutoBracket()
             || autoReplace_ != win->hasAutoReplace())
    {
        ui->promptLabel->setText ("<b>" + tr ("Application restart is needed for changes to take effect.") + "</b>");
    }
    else
    {
        if (prevtMsg_.isEmpty()) // clear prompt
        {
            ui->promptLabel->clear();
            ui->promptLabel->hide();
            return;
        }
        else // show the previous message
            ui->promptLabel->setText (prevtMsg_);
    }
    ui->promptLabel->show();
}
/*************************/
#ifdef HAS_HUNSPELL
void PrefDialog::addDict()
{
    FN *win = static_cast<FN *>(parent_);
    if (win == nullptr) return;

    if (QObject::sender() == ui->dictEdit)
    {
        win->setDictPath (ui->dictEdit->text());
        return;
    }
    FileDialog dialog (this);
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (tr ("Add dictionary..."));
    dialog.setFileMode (QFileDialog::ExistingFile);
    dialog.setNameFilter (tr ("Hunspell Dictionary Files") + " (*.dic)");
    QString path = ui->dictEdit->text();
    if (path.isEmpty())
    {
        path = "/usr/share/hunspell";
        if (!QFileInfo (path).isDir())
            path = "/usr/local/share/hunspell";
    }

    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else if (QFile::exists (path))
    {
        dialog.setDirectory (path.section ("/", 0, -2));
        dialog.selectFile (path);
        dialog.autoScroll();
    }

    if (dialog.exec())
    {
        const QStringList files = dialog.selectedFiles();
        if (!files.isEmpty())
        {
            ui->dictEdit->setText (files.at (0));
            win->setDictPath (files.at (0));
        }
    }
}
#endif

}

