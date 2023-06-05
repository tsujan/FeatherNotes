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

#include "singleton.h"
#include "ui_fn.h"
#include "ui_about.h"
#include "dommodel.h"
#include "treedelegate.h"
#include "warningbar.h"
#include "spinbox.h"
#include "simplecrypt.h"
#include "settings.h"
#include "help.h"
#include "filedialog.h"
#include "messagebox.h"
#include "svgicons.h"
#include "pref.h"
#include "colorLabel.h"
#include "printing.h"

#ifdef HAS_HUNSPELL
#include "spellChecker.h"
#include "spellDialog.h"
#include <QStandardPaths>
#endif

#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QActionGroup>
#include <QToolButton>
#include <QPrinter>
#include <QPrintDialog>
#include <QFontDialog>
#include <QColorDialog>
#include <QCheckBox>
#include <QGroupBox>
#include <QToolTip>
#include <QScreen>
#include <QWindow>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextTableCell>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextDocumentWriter>
#include <QClipboard>
#include <QApplication>
#include <QMimeDatabase>
#include <QProcess>
#include <QtMath>

#ifdef HAS_X11
#include "x11.h"
#endif

#define DEFAULT_RECENT_NUM 10
#define MAX_RECENT_NUM 20

namespace FeatherNotes {

// Regex of an embedded image (should be checked for the image):
static const QRegularExpression EMBEDDED_IMG (R"(<\s*img(?=\s)[^<>]*(?<=\s)src\s*=\s*"data:[^<>]*;base64\s*,[a-zA-Z0-9+=/\s]+"[^<>]*/*>)");

// Stylesheet of a customized document:
static const QString DOC_STYLESHEET ("body{background-color: %1; color: %2;}");

FN::FN (const QStringList& message, QWidget *parent) : QMainWindow (parent), ui (new Ui::FN)
{
    ui->setupUi (this);

    closed_ = false;
    imgScale_ = 100;

    /* use our delegate with opaque editor */
    treeDelegate *treeDel = new treeDelegate (this);
    ui->treeView->setItemDelegate (treeDel);

    ui->treeView->setContextMenuPolicy (Qt::CustomContextMenu);

    /* NOTE: The auto-saving timer starts only when a new note is created,
       a file is opened, or it is enabled in Preferences. It saves the doc
       only if it belongs to an existing file that needs saving. */
    autoSave_ = -1;
    saveNeeded_ = 0;
    timer_ = new QTimer (this);
    connect (timer_, &QTimer::timeout, this, &FN::autoSaving);

    /* appearance */
    setAttribute (Qt::WA_AlwaysShowToolTips);
    ui->statusBar->setVisible (false);

    setAttribute (Qt::WA_DeleteOnClose, false); // we delete windows in singleton

    defaultFont_ = QFont ("Monospace");
    defaultFont_.setPointSize (qMax (font().pointSize(), 9));
    nodeFont_ = font();

    bgColor_ = QColor (Qt::white);
    fgColor_ = QColor (Qt::black);

    /* search bar */
    ui->lineEdit->setVisible (false);
    ui->nextButton->setVisible (false);
    ui->prevButton->setVisible (false);
    ui->caseButton->setVisible (false);
    ui->wholeButton->setVisible (false);
    ui->everywhereButton->setVisible (false);
    ui->tagsButton->setVisible (false);
    ui->namesButton->setVisible (false);
    searchingOtherNode_ = false;
    rplOtherNode_ = false;
    replCount_ = 0;
    recentNum_ = 0;
    openReccentSeparately_ = false;

    /* replace dock */
    ui->dockReplace->setVisible (false);

    model_ = new DomModel (QDomDocument(), this);
    ui->treeView->setModel (model_);

    /* get the default (customizable) shortcuts before any change */
    static const QStringList excluded = {"actionCut", "actionCopy", "actionPaste", "actionSelectAll"};
    const auto allMenus = ui->menuBar->findChildren<QMenu*>();
    for (const auto &thisMenu : allMenus)
    {
        const auto menuActions = thisMenu->actions();
        for (const auto &menuAction : menuActions)
        {
            QKeySequence seq = menuAction->shortcut();
            if (!seq.isEmpty() && !excluded.contains (menuAction->objectName()))
                defaultShortcuts_.insert (menuAction, seq);
        }
    }
    /* exceptions */
    defaultShortcuts_.insert (ui->actionPrintNodes, QKeySequence());
    defaultShortcuts_.insert (ui->actionPrintAll, QKeySequence());
    defaultShortcuts_.insert (ui->actionExportHTML, QKeySequence());
    defaultShortcuts_.insert (ui->actionPassword, QKeySequence());
    defaultShortcuts_.insert (ui->actionDocFont, QKeySequence());
    defaultShortcuts_.insert (ui->actionNodeFont, QKeySequence());
    defaultShortcuts_.insert (ui->actionDocColors, QKeySequence());
    defaultShortcuts_.insert (ui->actionDate, QKeySequence());

    reservedShortcuts_
    /* QTextEdit */
    << QKeySequence (Qt::CTRL | Qt::SHIFT | Qt::Key_Z).toString() << QKeySequence (Qt::CTRL | Qt::Key_Z).toString() << QKeySequence (Qt::CTRL | Qt::Key_X).toString() << QKeySequence (Qt::CTRL | Qt::Key_C).toString() << QKeySequence (Qt::CTRL | Qt::Key_V).toString() << QKeySequence (Qt::CTRL | Qt::Key_A).toString()
    << QKeySequence (Qt::SHIFT | Qt::Key_Insert).toString() << QKeySequence (Qt::SHIFT | Qt::Key_Delete).toString() << QKeySequence (Qt::CTRL | Qt::Key_Insert).toString()
    << QKeySequence (Qt::CTRL | Qt::Key_Left).toString() << QKeySequence (Qt::CTRL | Qt::Key_Right).toString() << QKeySequence (Qt::CTRL | Qt::Key_Up).toString() << QKeySequence (Qt::CTRL | Qt::Key_Down).toString() << QKeySequence (Qt::CTRL | Qt::Key_PageUp).toString() << QKeySequence (Qt::CTRL | Qt::Key_PageDown).toString()
    << QKeySequence (Qt::CTRL | Qt::Key_Home).toString() << QKeySequence (Qt::CTRL | Qt::Key_End).toString()
    << QKeySequence (Qt::CTRL | Qt::SHIFT | Qt::Key_Up).toString() << QKeySequence (Qt::CTRL | Qt::SHIFT | Qt::Key_Down).toString()
    << QKeySequence (Qt::META | Qt::Key_Up).toString() << QKeySequence (Qt::META | Qt::Key_Down).toString() << QKeySequence (Qt::META | Qt::SHIFT | Qt::Key_Up).toString() << QKeySequence (Qt::META | Qt::SHIFT | Qt::Key_Down).toString()
    /* search and replacement */
    << QKeySequence (Qt::Key_F3).toString() << QKeySequence (Qt::Key_F4).toString() << QKeySequence (Qt::Key_F5).toString() << QKeySequence (Qt::Key_F6).toString() << QKeySequence (Qt::Key_F7).toString()
    << QKeySequence (Qt::Key_F8).toString() << QKeySequence (Qt::Key_F9).toString() << QKeySequence (Qt::Key_F10).toString()
    << QKeySequence (Qt::Key_F11).toString() << QKeySequence (Qt::SHIFT | Qt::Key_F7).toString() << QKeySequence (Qt::CTRL | Qt::SHIFT | Qt::Key_F7).toString()
    /* zooming */
    << QKeySequence (Qt::CTRL | Qt::Key_Equal).toString() << QKeySequence (Qt::CTRL | Qt::Key_Plus).toString() << QKeySequence (Qt::CTRL | Qt::Key_Minus).toString() << QKeySequence (Qt::CTRL | Qt::Key_0).toString()
    /* text tabulation */
    << QKeySequence (Qt::SHIFT | Qt::Key_Enter).toString() << QKeySequence (Qt::SHIFT | Qt::Key_Return).toString() << QKeySequence (Qt::CTRL | Qt::Key_Tab).toString() << QKeySequence (Qt::CTRL | Qt::META | Qt::Key_Tab).toString()
    /* used by LineEdit as well as QTextEdit */
    << QKeySequence (Qt::CTRL | Qt::Key_K).toString()
    /* gives the focus to the side-pane */
    << QKeySequence (Qt::CTRL | Qt::Key_Escape).toString();

    readShortcuts();

    QHash<QString, QString>::const_iterator it = customActions_.constBegin();
    while (it != customActions_.constEnd())
    { // NOTE: Custom shortcuts are saved in the PortableText format.
        if (QAction *action = findChild<QAction*>(it.key()))
            action->setShortcut (QKeySequence (it.value(), QKeySequence::PortableText));
        ++it;
    }

    /* isMaxed_, isFull_, winSize_ and position_ are set when needed,
       regardless of remSize_ and remSize_ */
    isMaxed_ = false;
    isFull_= false;

    remSize_ = true;
    remSplitter_ = true;
    remPosition_ = true;
    wrapByDefault_ = true;
    indentByDefault_ = true;
    transparentTree_ = false;
    smallToolbarIcons_ = true; // there are many icons
    noToolbar_ = false;
    noMenubar_ = false;
    autoBracket_ = false;
    autoReplace_ = false;
    openLastFile_ = false;
    saveOnExit_ = false;
    rememberExpanded_ = false;
    shownBefore_ = false;
    treeViewDND_ = false;
    readAndApplyConfig();

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->insertWidget (ui->actionMenu, spacer);
    QMenu *menu = new QMenu (ui->mainToolBar);
    menu->addMenu (ui->menuFile);
    menu->addMenu (ui->menuEdit);
    menu->addMenu (ui->menuFormat);
    menu->addMenu (ui->menuTree);
    menu->addMenu (ui->menuOptions);
    menu->addMenu (ui->menuSearch);
    menu->addMenu (ui->menuHelp);
    ui->actionMenu->setMenu(menu);
    QList<QToolButton*> tbList = ui->mainToolBar->findChildren<QToolButton*>();
    if (!tbList.isEmpty())
        tbList.at (tbList.count() - 1)->setPopupMode (QToolButton::InstantPopup);

    quitting_ = !hasTray_;

    QActionGroup *aGroup = new QActionGroup (this);
    ui->actionLeft->setActionGroup (aGroup);
    ui->actionCenter->setActionGroup (aGroup);
    ui->actionRight->setActionGroup (aGroup);
    ui->actionJust->setActionGroup (aGroup);

    QActionGroup *aGroup1 = new QActionGroup (this);
    ui->actionLTR->setActionGroup (aGroup1);
    ui->actionRTL->setActionGroup (aGroup1);

    /* signal connections */

    connect (ui->actionQuit, &QAction::triggered, this, &FN::close);

    connect (ui->treeView, &QWidget::customContextMenuRequested, this, &FN::showContextMenu);
    connect (ui->treeView, &QTreeView::expanded, this, &FN::indexExpanded);
    connect (ui->treeView, &QTreeView::collapsed, this, &FN::indexCollapsed);
    connect (ui->treeView, &TreeView::FNDocDropped, this, &FN::openFNDoc);
    connect (ui->treeView, &TreeView::searchRequest, this, [this] {
        if (model_->rowCount() == 0) return;
        if (!ui->lineEdit->isVisible())
            showHideSearch();
        else
        {
            ui->lineEdit->setFocus();
            ui->lineEdit->selectAll();
        }
        ui->namesButton->setChecked (true);
    });

    connect (ui->actionNew, &QAction::triggered, this, &FN::newNote);
    connect (ui->actionOpen, &QAction::triggered, this, &FN::openFile);
    connect (ui->actionSave, &QAction::triggered, this, &FN::saveFile);
    connect (ui->actionSaveAs, &QAction::triggered, this, &FN::saveFile);

    connect (ui->actionPassword, &QAction::triggered, this, &FN::setPswd);

    connect (ui->actionPrint, &QAction::triggered, this, &FN::txtPrint);
    connect (ui->actionPrintNodes, &QAction::triggered, this, &FN::txtPrint);
    connect (ui->actionPrintAll, &QAction::triggered, this, &FN::txtPrint);
    connect (ui->actionExportHTML, &QAction::triggered, this, &FN::exportHTML);

    connect (ui->actionUndo, &QAction::triggered, this, &FN::undoing);
    connect (ui->actionRedo, &QAction::triggered, this, &FN::redoing);

    connect (ui->actionCut, &QAction::triggered, this, &FN::cutText);
    connect (ui->actionCopy, &QAction::triggered, this, &FN::copyText);
    connect (ui->actionPaste, &QAction::triggered, this, &FN::pasteText);
    connect (ui->actionPasteHTML, &QAction::triggered, this, &FN::pasteHTML);
    connect (ui->actionDelete, &QAction::triggered, this, &FN::deleteText);
    connect (ui->actionSelectAll, &QAction::triggered, this, &FN::selectAllText);

    connect (ui->actionDate, &QAction::triggered, this, &FN::insertDate);

    connect (ui->actionBold, &QAction::triggered, this, &FN::makeBold);
    connect (ui->actionItalic, &QAction::triggered, this, &FN::makeItalic);
    connect (ui->actionUnderline, &QAction::triggered, this, &FN::makeUnderlined);
    connect (ui->actionStrike, &QAction::triggered, this, &FN::makeStriked);
    connect (ui->actionSuper, &QAction::triggered, this, &FN::makeSuperscript);
    connect (ui->actionSub, &QAction::triggered, this, &FN::makeSubscript);
    connect (ui->actionTextColor, &QAction::triggered, this, &FN::textColor);
    connect (ui->actionBgColor, &QAction::triggered, this, &FN::bgColor);
    connect (ui->actionClear, &QAction::triggered, this, &FN::clearFormat);

    connect (ui->actionH3, &QAction::triggered, this, &FN::makeHeader);
    connect (ui->actionH2, &QAction::triggered, this, &FN::makeHeader);
    connect (ui->actionH1, &QAction::triggered, this, &FN::makeHeader);

    connect (ui->actionLink, &QAction::triggered, this, &FN::insertLink);
    connect (ui->actionCopyLink, &QAction::triggered, this, &FN::copyLink);

    connect (ui->actionEmbedImage, &QAction::triggered, this, &FN::embedImage);
    connect (ui->actionImageScale, &QAction::triggered, this, &FN::scaleImage);
    connect (ui->actionImageSave, &QAction::triggered, this, &FN::saveImage);

    connect (ui->actionTable, &QAction::triggered, this, &FN::addTable);
    connect (ui->actionTableMergeCells, &QAction::triggered, this, &FN::tableMergeCells);
    connect (ui->actionTablePrependRow, &QAction::triggered, this, &FN::tablePrependRow);
    connect (ui->actionTableAppendRow, &QAction::triggered, this, &FN::tableAppendRow);
    connect (ui->actionTablePrependCol, &QAction::triggered, this, &FN::tablePrependCol);
    connect (ui->actionTableAppendCol, &QAction::triggered, this, &FN::tableAppendCol);
    connect (ui->actionTableDeleteRow, &QAction::triggered, this, &FN::tableDeleteRow);
    connect (ui->actionTableDeleteCol, &QAction::triggered, this, &FN::tableDeleteCol);

    connect(aGroup, &QActionGroup::triggered, this,&FN::textAlign);
    connect(aGroup1, &QActionGroup::triggered, this, &FN::textDirection);

    connect (ui->actionExpandAll, &QAction::triggered, this, &FN::expandAll);
    connect (ui->actionCollapseAll, &QAction::triggered, this, &FN::collapseAll);

    connect (ui->actionNewSibling, &QAction::triggered, this, &FN::newNode);
    connect (ui->actionNewChild, &QAction::triggered, this, &FN::newNode);
    connect (ui->actionPrepSibling, &QAction::triggered, this, &FN::newNode);
    connect (ui->actionDeleteNode, &QAction::triggered, this, &FN::deleteNode);
    connect (ui->actionMoveUp, &QAction::triggered, this, &FN::moveUpNode);
    connect (ui->actionMoveDown, &QAction::triggered, this, &FN::moveDownNode);

    if (QApplication::layoutDirection() == Qt::RightToLeft)
    {
        connect (ui->actionMoveLeft, &QAction::triggered, this, &FN::moveRightNode);
        connect (ui->actionMoveRight, &QAction::triggered, this, &FN::moveLeftNode);
    }
    else
    {
        connect (ui->actionMoveLeft, &QAction::triggered, this, &FN::moveLeftNode);
        connect (ui->actionMoveRight, &QAction::triggered, this, &FN::moveRightNode);
    }

    connect (ui->actionTags, &QAction::triggered, this, &FN::handleTags);
    connect (ui->actionRenameNode, &QAction::triggered, this, &FN::renameNode);
    connect (ui->actionNodeIcon, &QAction::triggered, this, &FN::nodeIcon);
    connect (ui->actionProp, &QAction::triggered, this, &FN::toggleStatusBar);

    connect (ui->actionDocFont, &QAction::triggered, this, &FN::textFontDialog);
    connect (ui->actionNodeFont, &QAction::triggered, this, &FN::nodeFontDialog);

    connect (ui->actionDocColors, &QAction::triggered, this, &FN::docColorDialog);

    connect (ui->actionWrap, &QAction::triggered, this, &FN::toggleWrapping);
    connect (ui->actionIndent, &QAction::triggered, this, &FN::toggleIndent);
    connect (ui->actionPref, &QAction::triggered, this, &FN::prefDialog);

    connect (ui->actionFind, &QAction::triggered, this, &FN::showHideSearch);
    connect (ui->nextButton, &QAbstractButton::clicked, this, &FN::find);
    connect (ui->prevButton, &QAbstractButton::clicked, this, &FN::find);
    connect (ui->lineEdit, &QLineEdit::returnPressed, this, &FN::find);
    connect (ui->wholeButton, &QAbstractButton::clicked, this, &FN::setSearchFlags);
    connect (ui->caseButton, &QAbstractButton::clicked, this, &FN::setSearchFlags);
    connect (ui->everywhereButton, &QAbstractButton::toggled, this, &FN::allBtn);
    connect (ui->tagsButton, &QAbstractButton::toggled, this, &FN::tagsAndNamesBtn);
    connect (ui->namesButton, &QAbstractButton::toggled, this, &FN::tagsAndNamesBtn );

    connect (ui->actionReplace, &QAction::triggered, this, &FN::replaceDock);
    connect (ui->dockReplace, &QDockWidget::visibilityChanged, this, &FN::closeReplaceDock);
    connect (ui->dockReplace, &QDockWidget::topLevelChanged, this, &FN::resizeDock);
    connect (ui->rplNextButton, &QAbstractButton::clicked, this, &FN::replace);
    connect (ui->rplPrevButton, &QAbstractButton::clicked, this, &FN::replace);
    connect (ui->allButton, &QAbstractButton::clicked, this, &FN::replaceAll);

    connect (ui->actionAbout, &QAction::triggered, this, &FN::aboutDialog);
    connect (ui->actionHelp, &QAction::triggered, this, &FN::showHelpDialog);

    connect (ui->menuFile, &QMenu::aboutToShow, this, &FN::updateRecentAction);
    connect (ui->menuOpenRecently, &QMenu::aboutToShow, this, &FN::updateRecenMenu);
    connect (ui->actionClearRecent, &QAction::triggered, this, &FN::clearRecentMenu);

    /* enable/disable paste actions appropriately */
    connect (ui->menuEdit, &QMenu::aboutToShow, this, [this] {
        if (QWidget *cw = ui->stackedWidget->currentWidget())
        {
            bool enablePaste = qobject_cast<TextEdit*>(cw)->canPaste();
            ui->actionPaste->setEnabled (enablePaste);
            ui->actionPasteHTML->setEnabled (enablePaste);
        }
        else
        {
            ui->actionPaste->setEnabled (false);
            ui->actionPasteHTML->setEnabled (false);
        }
    });
    connect (ui->menuEdit, &QMenu::aboutToHide, this, [this] {
        ui->actionPaste->setEnabled (true);
        ui->actionPasteHTML->setEnabled (true);
    });

#ifdef HAS_HUNSPELL
    connect (ui->actionCheckSpelling, &QAction::triggered, this, &FN::checkSpelling);
#else
    ui->actionCheckSpelling->setVisible (false);
#endif

    /* Once the tray icon is created, it'll persist even if the systray
       disappears temporarily. But for the tray icon to be created, the
       systray should exist. Hence, we wait 60 sec for the systray at startup. */
    tray_ = nullptr;
    trayCounter_ = 0;
    if (hasTray_)
    {
        if (QSystemTrayIcon::isSystemTrayAvailable())
            createTrayIcon();
        else if (!static_cast<FNSingleton*>(qApp)->isTrayChecked())
        {
            QTimer *trayTimer = new QTimer (this);
            trayTimer->setSingleShot (true);
            trayTimer->setInterval (5000);
            connect (trayTimer, &QTimer::timeout, this, &FN::checkTray);
            trayTimer->start();
            ++trayCounter_;
        }
        else activateFNWindow (true);
    }

    QShortcut *focusView = new QShortcut (QKeySequence (Qt::Key_Escape), this);
    connect (focusView, &QShortcut::activated, [this] {
        if (QWidget *cw = ui->stackedWidget->currentWidget())
            cw->setFocus();
    });
    QShortcut *focusSidePane = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Escape), this);
    connect (focusSidePane, &QShortcut::activated, [this] {
        ui->treeView->viewport()->setFocus();
    });

    QShortcut *fullscreen = new QShortcut (QKeySequence (Qt::Key_F11), this);
    connect (fullscreen, &QShortcut::activated, this, &FN::fullScreening);

    QShortcut *zoomin = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Equal), this);
    QShortcut *zoominPlus = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Plus), this);
    QShortcut *zoomout = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_Minus), this);
    QShortcut *unzoom = new QShortcut (QKeySequence (Qt::CTRL | Qt::Key_0), this);
    connect (zoomin, &QShortcut::activated, this, &FN::zoomingIn);
    connect (zoominPlus, &QShortcut::activated, this, &FN::zoomingIn);
    connect (zoomout, &QShortcut::activated, this, &FN::zoomingOut);
    connect (unzoom, &QShortcut::activated, this, &FN::unZooming);

    /* parse the message */
    QString filePath;
    if (message.isEmpty())
    {
        if (!hasTray_ || !minToTray_)
            activateFNWindow (true);
    }
    else
    {
        if (message.at (0) != "--min" && message.at (0) != "-m"
            && message.at (0) != "--tray" && message.at (0) != "-t")
        {
            if (!hasTray_ || !minToTray_)
                activateFNWindow (true);
            filePath = message.at (0);
        }
        else
        {
            if (message.at (0) == "--min" || message.at (0) == "-m")
                showMinimized();
            else if (!hasTray_)
                activateFNWindow (true);
            if (message.count() > 1)
                filePath = message.at (1);
        }
    }

    bool startWithLastFile = true;
    /* always an absolute path */
    if (!filePath.isEmpty())
    {
        startWithLastFile = false;
        if (filePath.startsWith ("file://"))
            filePath = QUrl (filePath).toLocalFile();
        filePath = QDir::current().absoluteFilePath (filePath);
        filePath = QDir::cleanPath (filePath);
    }
    else
        filePath = xmlPath_;

    fileOpen (filePath, true, startWithLastFile);

    /*dummyWidget = nullptr;
    if (hasTray_)
        dummyWidget = new QWidget();*/

    setAcceptDrops (true);
}
/*************************/
FN::~FN()
{
    if (timer_)
    {
        if (timer_->isActive()) timer_->stop();
        delete timer_;
    }
    delete tray_; // also deleted at closeEvent() (this is for Ctrl+C in terminal)
    tray_ = nullptr;
    delete ui;
}
/*************************/
bool FN::close()
{
    QObject *sender = QObject::sender();
    if (sender != nullptr && sender->objectName() == "trayQuit" && hasBlockingDialog())
    { // don't respond to the tray icon when there's a blocking dialog
        activateFNWindow (true);
        return false;
    }

    quitting_ = true;
    return QWidget::close();
}
/*************************/
void FN::closeEvent (QCloseEvent *event)
{
    FNSingleton *singleton = static_cast<FNSingleton*>(qApp);
    /* NOTE: With Qt6, "QCoreApplication::quit()" calls "closeEvent()" when the window is
             visible. But we want the app to quit without any prompt when receiving SIGTERM
             and similar signals. Here, we handle the situation by checking if a quit signal
             is received. This is also safe with Qt5. */
    if (singleton->isQuitSignalReceived())
    {
        event->accept();
        return;
    }

    if (!quitting_)
    {
        closeNonModalDialogs();
        event->ignore();
        if (!isMaximized() && !isFullScreen() && !singleton->isWayland())
        {
            position_.setX (geometry().x());
            position_.setY (geometry().y());
        }
        if (tray_ != nullptr && QSystemTrayIcon::isSystemTrayAvailable())
            QTimer::singleShot (0, this, &QWidget::hide);
        else
            QTimer::singleShot (0, this, &QWidget::showMinimized);
        return;
    }

    if (timer_->isActive()) timer_->stop();

    bool keep = false;
    if (ui->stackedWidget->currentIndex() > -1)
    {
        if (!xmlPath_.isEmpty() && (!QFile::exists (xmlPath_) || !QFileInfo (xmlPath_).isFile()))
        {
            if (tray_ && (!isVisible() || !isActiveWindow()))
            {
                activateFNWindow (true);
                QCoreApplication::processEvents();
            }
            closeWinDialogs();
            if (unSaved (false))
                keep = true;
        }
        else if (saveNeeded_ > 0)
        {
            if (saveOnExit_ && !xmlPath_.isEmpty())
                fileSave (xmlPath_);
            else
            {
                if (tray_ && (!isVisible() || !isActiveWindow()))
                {
                    activateFNWindow (true);
                    QCoreApplication::processEvents();
                }
                closeWinDialogs();
                if (unSaved (true))
                    keep = true;
            }
        }
    }

    if (keep)
    {
        if (tray_)
            quitting_ = false;
        if (autoSave_ >= 1)
            timer_->start (autoSave_ * 1000 * 60);
        event->ignore();
    }
    else
    {
        writeGeometryConfig();
        delete tray_; // otherwise the app might not quit under KDE
        tray_ = nullptr;
        singleton->removeWin (this);
        closed_ = true; // window info shouldn't be saved after closing
        event->accept();

        /* We set "quitOnLastWindowClosed" to "false" to prevent the
           app from quitting when the remaining windows are iconified.
           So, we should quit explicitly after the last window is closed. */
        if (!singleton->quitOnLastWindowClosed() && singleton->Wins.isEmpty())
            QCoreApplication::quit();
    }
}
/*************************/
// This method should be called only when the app quits without closing its windows
// (e.g., with SIGTERM).
void FN::quitting()
{
    /* WARNING: Qt5 has a bug that will cause a crash if "QDockWidget::visibilityChanged"
                isn't disconnected here. This is also good with Qt6. */
    disconnect (ui->dockReplace, &QDockWidget::visibilityChanged, this, &FN::closeReplaceDock);

    if (!closed_)
    {
        if (saveOnExit_) autoSaving();
        FNSingleton *singleton = static_cast<FNSingleton*>(qApp);
        if (!singleton->Wins.isEmpty() && this == singleton->Wins.last())
            writeGeometryConfig();
    }
}
/*************************/
void FN::checkTray()
{
    if (QTimer *trayTimer = qobject_cast<QTimer*>(sender()))
    {
        if (QSystemTrayIcon::isSystemTrayAvailable())
        {
            static_cast<FNSingleton*>(qApp)->setTrayChecked();
            trayTimer->deleteLater();
            createTrayIcon();
            trayCounter_ = 0; // not needed
        }
        else if (trayCounter_ < 12)
        {
            if (trayCounter_ == 4) // show the window if the systray isn't found after 20 sec
                activateFNWindow (true);
            trayTimer->start();
            ++trayCounter_;
        }
        else
        {
            static_cast<FNSingleton*>(qApp)->setTrayChecked();
            trayTimer->deleteLater();
            activateFNWindow (true);
            showWarningBar ("<center><b><big>"
                            + tr ("System tray is not available.\nPlease disable tray in Preferences.")
                            + "</big></b></center>", -1);
        }
    }
}
/*************************/
void FN::createTrayIcon()
{
    QIcon icn = QIcon::fromTheme ("feathernotes");
    if (icn.isNull())
        icn = QIcon (":icons/feathernotes.svg");
    tray_ = new QSystemTrayIcon (icn, this);
    if (xmlPath_.isEmpty())
        tray_->setToolTip ("FeatherNotes");
    else
    {
        QString shownName = QFileInfo (xmlPath_).fileName();
        if (shownName.endsWith (".fnx"))
            shownName.chop (4);
        tray_->setToolTip (/*"<p style='white-space:pre'>"
                           +*/ shownName // KDE's buggy tray can't show styled tooltips
                           /*+ "</p>"*/);
    }
    QMenu *trayMenu = new QMenu (this);
    /* we don't want shortcuts to be shown here */
    QAction *actionshowMainWindow = trayMenu->addAction (tr ("&Raise/Hide"));
    connect (actionshowMainWindow, &QAction::triggered, this, &FN::activateTray);
    /* use system icons with the tray menu because it gets its style from the panel */
    QAction *actionNewTray = trayMenu->addAction (QIcon::fromTheme ("document-new"), tr ("&New Note"));
    QAction *actionOpenTray = trayMenu->addAction (QIcon::fromTheme ("document-open"), tr ("&Open"));
    trayMenu->addSeparator();
    QAction *antionQuitTray = trayMenu->addAction (QIcon::fromTheme ("application-exit"), tr ("&Quit"));
    connect (actionNewTray, &QAction::triggered, this, &FN::newNote);
    connect (actionOpenTray, &QAction::triggered, this, &FN::openFile);
    connect (antionQuitTray, &QAction::triggered, this, &FN::close);
    actionNewTray->setObjectName ("trayNew");
    actionOpenTray->setObjectName ("trayOpen");
    antionQuitTray->setObjectName ("trayQuit");
    tray_->setContextMenu (trayMenu);
    tray_->setVisible (true);
    connect (tray_, &QSystemTrayIcon::activated, this, &FN::trayActivated );
}
/*************************/
void FN::showContextMenu (const QPoint &p)
{
    QModelIndex index = ui->treeView->indexAt (p);
    if (!index.isValid()) return;

    QMenu menu (this); // "this" is for Wayland, when the window isn't active
    menu.addAction (ui->actionPrepSibling);
    menu.addAction (ui->actionNewSibling);
    menu.addAction (ui->actionNewChild);

    if (ui->actionDeleteNode->isEnabled())
    {
        menu.addSeparator();
        menu.addAction (ui->actionDeleteNode);
    }

    if (ui->actionMoveUp->isEnabled()
        || ui->actionMoveDown->isEnabled()
        || ui->actionMoveLeft->isEnabled()
        || ui->actionMoveRight->isEnabled())
    {
        menu.addSeparator();
    }
    if (ui->actionMoveUp->isEnabled())
        menu.addAction (ui->actionMoveUp);
    if (ui->actionMoveDown->isEnabled())
        menu.addAction (ui->actionMoveDown);
    if (ui->actionMoveLeft->isEnabled())
        menu.addAction (ui->actionMoveLeft);
    if (ui->actionMoveRight->isEnabled())
        menu.addAction (ui->actionMoveRight);

    menu.addSeparator();
    menu.addAction (ui->actionTags);
    menu.addAction (ui->actionNodeIcon);
    menu.addAction (ui->actionRenameNode);

    menu.exec (ui->treeView->viewport()->mapToGlobal (p));
}
/*************************/
void FN::indexExpanded (const QModelIndex &index)
{
    if (!rememberExpanded_ || !index.isValid()) return;
    if (DomItem *item = static_cast<DomItem*>(index.internalPointer()))
    {
        QDomNode node = item->node();
        if (!node.toElement().attribute ("collapse").isEmpty())
        {
            node.toElement().removeAttribute ("collapse");
            noteModified();
        }
    }
}
/*************************/
void FN::indexCollapsed (const QModelIndex &index)
{
    if (!rememberExpanded_ || !index.isValid()) return;
    if (DomItem *item = static_cast<DomItem*>(index.internalPointer()))
    {
        QDomNode node = item->node();
        node.toElement().setAttribute ("collapse", "1");
        noteModified();
    }
}
/*************************/
void FN::setCollapsedStates()
{
    QModelIndex indx = model_->index (0, 0);
    while (indx.isValid())
    {
        if (model_->hasChildren (indx))
        {
            if (DomItem *item = static_cast<DomItem*>(indx.internalPointer()))
            {
                QDomNode node = item->node();
                if (ui->treeView->isExpanded (indx))
                {
                    if (!node.toElement().attribute ("collapse").isEmpty())
                    {
                        node.toElement().removeAttribute ("collapse");
                        noteModified();
                    }
                }
                else if (node.toElement().attribute ("collapse") != "1")
                {
                    node.toElement().setAttribute ("collapse", "1");
                    noteModified();
                }
            }
        }
        indx = model_->adjacentIndex (indx, true);
    }
}
/*************************/
void FN::fullScreening()
{
    setWindowState (windowState() ^ Qt::WindowFullScreen);
}
/*************************/
void FN::zoomingIn()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    textEdit->zooming (1.f);
}
/*************************/
void FN::zoomingOut()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    textEdit->zooming (-1.f);

    rehighlight (textEdit);
}
/*************************/
void FN::unZooming()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    textEdit->setEditorFont (defaultFont_);

    /* this may be a zoom-out */
    rehighlight (textEdit);
}
/*************************/
void FN::resizeEvent (QResizeEvent *event)
{
    if (!isMaximized() && !isFullScreen())
        winSize_ = event->size();
    QWidget::resizeEvent (event);
}
/*************************/
void FN::changeEvent (QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (windowState() == Qt::WindowFullScreen)
        {
            isFull_ = true;
            isMaxed_ = false;
        }
        else if (windowState() == (Qt::WindowFullScreen ^ Qt::WindowMaximized))
        {
            isFull_ = true;
            isMaxed_ = true;
        }
        else
        {
            isFull_ = false;
            if (windowState() == Qt::WindowMaximized)
                isMaxed_ = true;
            else
                isMaxed_ = false;
        }
        /* if the window gets maximized/fullscreen, remember its position */
        if (!static_cast<FNSingleton*>(qApp)->isWayland()
            && ((windowState() & Qt::WindowMaximized)
                || (windowState() & Qt::WindowFullScreen)))
        {
            if (auto stateEvent = static_cast<QWindowStateChangeEvent*>(event))
            {
                if (!(stateEvent->oldState() & Qt::WindowMaximized)
                    && !(stateEvent->oldState() & Qt::WindowFullScreen))
                {
                    position_.setX (geometry().x());
                    position_.setY (geometry().y());
                }
            }
        }
    }
    QWidget::changeEvent (event);
}
/*************************/
void FN::newNote()
{
    QObject *sender = QObject::sender();
    if (sender != nullptr && sender->objectName() == "trayNew" && hasBlockingDialog())
    { // don't respond to the tray icon when there's a blocking dialog
        activateFNWindow (true);
        return;
    }
    closeWinDialogs();

    if (timer_->isActive()) timer_->stop();

    if (tray_ && (!isVisible() || !isActiveWindow()))
    {
        activateFNWindow (true);
        QCoreApplication::processEvents();
    }

    if (!xmlPath_.isEmpty() && !QFile::exists (xmlPath_))
    {
        if (unSaved (false))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }
    else if (saveNeeded_ > 0)
    {
        if (unSaved (true))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }

    /* show user a prompt */
    if (!xmlPath_.isEmpty())
    {
        MessageBox msgBox;
        msgBox.setIcon (QMessageBox::Question);
        msgBox.setWindowTitle ("FeatherNotes"); // kwin sets an ugly title
        msgBox.setText (tr ("<center><b><big>New note?</big></b></center>"));
        msgBox.setInformativeText (tr ("<center><i>Do you really want to leave this document</i></center>\n"\
                                       "<center><i>and create an empty one?</i></center>"));
        msgBox.setStandardButtons (QMessageBox::Yes | QMessageBox::No);
        msgBox.changeButtonText (QMessageBox::Yes, tr ("Yes"));
        msgBox.changeButtonText (QMessageBox::No, tr ("No"));
        msgBox.setDefaultButton (QMessageBox::No);
        msgBox.setParent (this, Qt::Dialog);
        msgBox.setWindowModality (Qt::WindowModal);
        msgBox.show();
        if (!static_cast<FNSingleton*>(qApp)->isWayland())
        {
            msgBox.move (x() + width()/2 - msgBox.width()/2,
                         y() + height()/2 - msgBox.height()/ 2);
        }
        switch (msgBox.exec())
        {
        case QMessageBox::Yes:
            break;
        case QMessageBox::No:
        default:
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }

    /* first reset the password and fonts */
    pswrd_.clear();
    defaultFont_ = QFont ("Monospace");
    defaultFont_.setPointSize (qMax (font().pointSize(), 9));
    nodeFont_ = font();

    QDomDocument doc;
    QDomProcessingInstruction inst = doc.createProcessingInstruction ("xml", "version=\'1.0\' encoding=\'utf-8\'");
    doc.insertBefore(inst, QDomNode());
    QDomElement root = doc.createElement ("feathernotes");
    root.setAttribute ("txtfont", defaultFont_.toString());
    root.setAttribute ("nodefont", nodeFont_.toString());
    if (bgColor_ != QColor (Qt::white) || fgColor_ != QColor (Qt::black))
    {
        root.setAttribute ("bgcolor", bgColor_.name());
        root.setAttribute ("fgcolor", fgColor_.name());
    }
    else
    {
        root.removeAttribute ("bgcolor");
        root.removeAttribute ("fgcolor");
    }
    doc.appendChild (root);
    QDomElement e = doc.createElement ("node");
    e.setAttribute ("name", tr ("New Node"));
    root.appendChild (e);

    showDoc (doc);
    xmlPath_ = QString();
    setTitle (xmlPath_);
    /* may be saved later */
    if (autoSave_ >= 1)
        timer_->start (autoSave_ * 1000 * 60);
    docProp();
}
/*************************/
void FN::setTitle (const QString &fname)
{
    QFileInfo fileInfo;
    if (fname.isEmpty()
        || !(fileInfo = QFileInfo (fname)).exists())
    {
        setWindowTitle (QString ("[*]%1").arg ("FeatherNotes"));
        if (tray_)
            tray_->setToolTip ("FeatherNotes");
        return;
    }

    QString shownName = fileInfo.fileName();
    if (shownName.endsWith (".fnx"))
        shownName.chop (4);

    QString path (fileInfo.dir().path());
    setWindowTitle (QString ("[*]%1 (%2)").arg (shownName).arg (path));

    if (tray_)
    {
        tray_->setToolTip (/*"<p style='white-space:pre'>"
                           +*/ shownName
                           /*+ "</p>"*/);
    }
}
/*************************/
// NOTE: closeWinDialogs() should be already called wherever this method is used.
bool FN::unSaved (bool modified)
{
    int unsaved = false;
    MessageBox msgBox;
    msgBox.setIcon (QMessageBox::Warning);
    msgBox.setText (tr ("<center><b><big>Save changes?</big></b></center>"));
    if (modified)
        msgBox.setInformativeText (tr ("<center><i>The document has been modified.</i></center>"));
    else
        msgBox.setInformativeText (tr ("<center><i>The document has been removed.</i></center>"));
    msgBox.setStandardButtons (QMessageBox::Save
                               | QMessageBox::Discard
                               | QMessageBox::Cancel);
    msgBox.changeButtonText (QMessageBox::Save, tr ("Save"));
    msgBox.changeButtonText (QMessageBox::Discard, tr ("Discard changes"));
    msgBox.changeButtonText (QMessageBox::Cancel, tr ("Cancel"));
    msgBox.setDefaultButton (QMessageBox::Save);
    msgBox.setParent (this, Qt::Dialog);
    msgBox.setWindowModality (Qt::WindowModal);
    /* enforce a central position */
    msgBox.show();
    if (!static_cast<FNSingleton*>(qApp)->isWayland())
    {
        msgBox.move (x() + width()/2 - msgBox.width()/2,
                     y() + height()/2 - msgBox.height()/ 2);
    }
    switch (msgBox.exec())
    {
    case QMessageBox::Save:
        if (!saveFile())
            unsaved = true;
        break;
    case QMessageBox::Discard: // false
        break;
    case QMessageBox::Cancel: // true
    default:
        unsaved = true;
        break;
    }

    return unsaved;
}
/*************************/
void FN::enableActions (bool enable)
{
    ui->actionSaveAs->setEnabled (enable);
    ui->actionPrint->setEnabled (enable);
    ui->actionPrintNodes->setEnabled (enable);
    ui->actionPrintAll->setEnabled (enable);
    ui->actionExportHTML->setEnabled (enable);
    ui->actionPassword->setEnabled (enable);

    ui->actionPaste->setEnabled (enable);
    ui->actionPasteHTML->setEnabled (enable);
    ui->actionSelectAll->setEnabled (enable);

    ui->actionDate->setEnabled (enable);

    ui->actionClear->setEnabled (enable);
    ui->actionBold->setEnabled (enable);
    ui->actionItalic->setEnabled (enable);
    ui->actionUnderline->setEnabled (enable);
    ui->actionStrike->setEnabled (enable);
    ui->actionSuper->setEnabled (enable);
    ui->actionSub->setEnabled (enable);
    ui->actionTextColor->setEnabled (enable);
    ui->actionBgColor->setEnabled (enable);
    ui->actionLeft->setEnabled (enable);
    ui->actionCenter->setEnabled (enable);
    ui->actionRight->setEnabled (enable);
    ui->actionJust->setEnabled (enable);

    ui->actionLTR->setEnabled (enable);
    ui->actionRTL->setEnabled (enable);

    ui->actionH3->setEnabled (enable);
    ui->actionH2->setEnabled (enable);
    ui->actionH1->setEnabled (enable);

    ui->actionEmbedImage->setEnabled (enable);
    ui->actionTable->setEnabled (enable);

    ui->actionExpandAll->setEnabled (enable);
    ui->actionCollapseAll->setEnabled (enable);
    ui->actionPrepSibling->setEnabled (enable);
    ui->actionNewSibling->setEnabled (enable);
    ui->actionNewChild->setEnabled (enable);
    if (enable)
        updateNodeActions();
    else
    {
        ui->actionDeleteNode->setEnabled (false);
        ui->actionMoveUp->setEnabled (false);
        ui->actionMoveDown->setEnabled (false);
        ui->actionMoveLeft->setEnabled (false);
        ui->actionMoveRight->setEnabled (false);
    }

    ui->actionTags->setEnabled (enable);
    ui->actionRenameNode->setEnabled (enable);
    ui->actionNodeIcon->setEnabled (enable);

    ui->actionDocFont->setEnabled (enable);
    ui->actionNodeFont->setEnabled (enable);
    ui->actionWrap->setEnabled (enable);
    ui->actionIndent->setEnabled (enable);

    ui->actionFind->setEnabled (enable);
    ui->actionReplace->setEnabled (enable);

    if (!enable)
    {
        ui->actionUndo->setEnabled (false);
        ui->actionRedo->setEnabled (false);
    }
}
/*************************/
void FN::updateNodeActions()
{
    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    ui->actionDeleteNode->setEnabled (pIndex.isValid() || model_->rowCount() > 1);
    ui->actionMoveUp->setEnabled (index.row() != 0);
    ui->actionMoveDown->setEnabled (index.row() != model_->rowCount (pIndex) - 1);
    if (QApplication::layoutDirection() == Qt::RightToLeft)
    {
        ui->actionMoveRight->setEnabled (pIndex.isValid());
        ui->actionMoveLeft->setEnabled (index.row() != 0);
    }
    else
    {
        ui->actionMoveRight->setEnabled (index.row() != 0);
        ui->actionMoveLeft->setEnabled (pIndex.isValid());
    }
}
/*************************/
// Qt6 has changed "QFont::toString()" in a backward incompatible way, so that,
// if a document is saved by the Qt6 version, the Qt5 version will show wrong fonts.
// This function circumvents the problem when it's used instead of "QFont::fromString()".
static inline void fontFromString (QFont &f, const QString &str)
{
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    const QChar comma (QLatin1Char (','));
    QStringList l = str.split (comma);
    if (l.size() <= 10)
        f.fromString (str);
    else
    {
        l = l.mid (0, 10);
        int weight = l.at (4).toInt();
        switch (weight) {
        case 100:
            weight = 0;
            break;
        case 200:
            weight = 12;
            break;
        case 300:
            weight = 25;
            break;
        case 400:
            weight = 50;
            break;
        case 500:
            weight = 57;
            break;
        case 600:
            weight = 63;
            break;
        case 700:
            weight = 75;
            break;
        case 800:
            weight = 81;
            break;
        case 900:
            weight = 87;
            break;
        default:
            weight = 50;
            break;
        }
        l[4] = QString::number (weight);
        f.fromString (l.join (comma));
    }
#else
    f.fromString (str);
#endif
}
/*************************/
void FN::showDoc (QDomDocument &doc, int node)
{
    if (saveNeeded_ > 0)
    {
        saveNeeded_ = 0;
        ui->actionSave->setEnabled (false);
        setWindowModified (false);
    }

    widgets_.clear();
    searchEntries_.clear();
    greenSels_.clear();
    while (ui->stackedWidget->count() > 0)
    {
        QWidget *cw = ui->stackedWidget->currentWidget();
        TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
        ui->stackedWidget->removeWidget (cw);
        delete textEdit; textEdit = nullptr;
    }

    QDomElement root = doc.firstChildElement ("feathernotes");
    QString fontStr = root.attribute ("txtfont");
    if (!fontStr.isEmpty())
    {
        /* since defaultFont_ may have been set in textFontDialog(),
           it needs to be redefined completely */
        QFont f; fontFromString (f, fontStr);
        defaultFont_ = f;
    }
    else
    {
        defaultFont_ = QFont ("Monospace");
        defaultFont_.setPointSize (qMax (font().pointSize(), 9));
    }
    fontStr = root.attribute ("nodefont");
    if (!fontStr.isEmpty())
        fontFromString (nodeFont_, fontStr);
    else // nodeFont_ may have changed by the user
        nodeFont_ = font();

    QColor bgColor;
    QString docColor = root.attribute ("bgcolor");
    if (!docColor.isEmpty())
        bgColor = QColor (docColor);
    if (bgColor.isValid())
        bgColor_ = bgColor;
    else
        bgColor_ = QColor (Qt::white);

    QColor fgColor;
    docColor = root.attribute ("fgcolor");
    if (!docColor.isEmpty())
        fgColor = QColor (docColor);
    if (fgColor.isValid())
        fgColor_ = fgColor;
    else
        fgColor_ = QColor (Qt::black);

    DomModel *newModel = new DomModel (doc, this);
    QItemSelectionModel *m = ui->treeView->selectionModel();
    ui->treeView->setModel (newModel);
    ui->treeView->setFont (nodeFont_);
    delete m;
    /* first connect to selectionChanged()... */
    connect (ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FN::selChanged);
    /* ... and then, select the (first) row */
    QModelIndex indx = newModel->index (0, 0);
    while (indx.isValid() && node > 0)
    {
        indx = newModel->adjacentIndex (indx, true);
        --node;
    }
    if (!indx.isValid())
        indx = newModel->index (0, 0);
    ui->treeView->setCurrentIndex (indx);
    QTimer::singleShot (0, this, [this, indx] () {
        ui->treeView->scrollTo (indx);
    });

    if (!rememberExpanded_)
        ui->treeView->expandAll();
    else
    {
        QModelIndex _indx = newModel->index (0, 0);
        ui->treeView->setUpdatesEnabled (false);
        while (_indx.isValid())
        {
            if (DomItem *item = static_cast<DomItem*>(_indx.internalPointer()))
            {
                QDomNode node = item->node();
                if (node.toElement().attribute ("collapse").isEmpty())
                    ui->treeView->expand (_indx);
            }
            _indx = newModel->adjacentIndex (_indx, true);
        }
        ui->treeView->setUpdatesEnabled (true);
    }

    delete model_;
    model_ = newModel;
    connect (model_, &QAbstractItemModel::dataChanged, this, &FN::nodeChanged);
    connect (model_, &DomModel::treeChanged, this, &FN::treeChanged);

    connect (model_, &DomModel::dragStarted, [this] (const QModelIndex &draggedIndex) {
        if (draggedIndex == ui->treeView->currentIndex())
        {
            treeViewDND_ = true;
            ui->treeView->setAutoScroll (false);
        }
    });
    connect (model_, &DomModel::droppedAtIndex, [this] (const QModelIndex &droppedIndex) {
        ui->treeView->setAutoScroll (true);
        ui->treeView->setCurrentIndex (droppedIndex);
        treeViewDND_ = false;
        /* WARNING: QTreeView collapses the index and its children on dropping,
                    without emitting "QTreeView::collapsed()". */
        if (rememberExpanded_)
        {
            QModelIndex indx = droppedIndex;
            QModelIndex sibling = model_->sibling (indx.row() + 1, 0, indx);
            while (indx != sibling)
            {
                if (model_->hasChildren (indx))
                {
                    if (DomItem *item = static_cast<DomItem*>(indx.internalPointer()))
                    {
                        QDomNode node = item->node();
                        if (node.toElement().attribute ("collapse").isEmpty())
                            ui->treeView->expand (indx);
                    }
                }
                indx = model_->adjacentIndex (indx, true);
            }
        }
    });

    /* enable widgets */
    enableActions (true);
}
/*************************/
void FN::fileOpen (const QString &filePath, bool startup, bool startWithLastFile)
{
    bool success = false;
    if (!filePath.isEmpty())
    {
        QFile file (filePath);
        if (file.open (QIODevice::ReadOnly))
        {
            QTextStream in (&file);
            QString cntnt = in.readAll();
            file.close();
            SimpleCrypt crypto (Q_UINT64_C(0xc9a25eb1610eb104));
            QString decrypted = crypto.decryptToString (cntnt);
            if (decrypted.isEmpty())
                decrypted = cntnt;
            QDomDocument document;
            if (document.setContent (decrypted))
            {
                QDomElement root = document.firstChildElement ("feathernotes");
                if (root.isNull())
                {
                    if (startup)
                    {
                        xmlPath_.clear();
                        QTimer::singleShot (0, this, [this] () {
                            notSavedOrOpened (false);
                        });
                        return;
                    }
                }
                else
                {
                    QString oldPswrd = pswrd_;
                    pswrd_ = root.attribute ("pswrd");
                    if (!pswrd_.isEmpty() && !isPswrdCorrect (filePath))
                    {
                        pswrd_ = oldPswrd;
                        if (startup)
                        {
                            xmlPath_.clear();
                            notSavedOrOpened (false);
                            return;
                        }
                    }
                    else
                    {
                        success = true;
                        if (startup && startWithLastFile && lastNode_ > -1)
                            showDoc (document, lastNode_);
                        else
                            showDoc (document);
                        if (xmlPath_ != filePath)
                        {
                            xmlPath_ = filePath;
                            QTimer::singleShot (0, this, [this] () {
                                rememberLastOpenedFile();
                            });
                        }
                        setTitle (xmlPath_);
                        docProp();
                    }
                }
            }
        }
    }

    if (!success)
    {
        if (startup)
        {
            xmlPath_.clear();
            if (!filePath.isEmpty())
            {
                QTimer::singleShot (0, this, [this] () {
                    notSavedOrOpened (false);
                });
            }
            return;
        }
        if (!filePath.isEmpty())
            notSavedOrOpened (false);
    }

    /* start the timer (again) if file
       opening is done or canceled */
    if (!xmlPath_.isEmpty() && autoSave_ >= 1)
        timer_->start (autoSave_ * 1000 * 60);
}
/*************************/
void FN::openFile()
{
    QObject *sender = QObject::sender();
    if (sender != nullptr && sender->objectName() == "trayOpen" && hasBlockingDialog())
    { // don't respond to the tray icon when there's a blocking dialog
        activateFNWindow (true);
        return;
    }
    closeWinDialogs();

    if (timer_->isActive()) timer_->stop();

    if (tray_ && (!isVisible() || !isActiveWindow()))
    {
        activateFNWindow (true);
        QCoreApplication::processEvents();
    }

    if (!xmlPath_.isEmpty() && !QFile::exists (xmlPath_))
    {
        if (unSaved (false))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }
    else if (saveNeeded_ > 0)
    {
        if (unSaved (true))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }

    QString path;
    if (!xmlPath_.isEmpty())
    {
        if (QFile::exists (xmlPath_))
            path = xmlPath_;
        else
        {
            QDir dir = QFileInfo (xmlPath_).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
    }
    else
    {
        QDir dir = QDir::home();
        path = dir.path();
    }

    FileDialog *dialog = new FileDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setAcceptMode (QFileDialog::AcceptOpen);
    dialog->setWindowTitle (tr ("Open file..."));
    dialog->setFileMode (QFileDialog::ExistingFiles);
    dialog->setNameFilter (tr ("FeatherNotes documents") + " (*.fnx);;" + tr ("All Files") + " (*)");
    if (QFileInfo (path).isDir())
        dialog->setDirectory (path);
    else
    {
        dialog->setDirectory (path.section ("/", 0, -2)); // workaround for KDE
        dialog->selectFile (path);
        dialog->autoScroll();
    }
    dialog->open();
    connect (dialog, &QDialog::finished, this, [this, dialog] (int res) {
        QString filePath;
        if (res == QDialog::Accepted)
            filePath = dialog->selectedFiles().at (0);

        /* let the dialog be closed */
        QTimer::singleShot (0, this, [this, filePath] () {
            /* fileOpen() restarts auto-saving even when opening is canceled */
            fileOpen (filePath);
        });
    });
}
/*************************/
void FN::openFNDoc (const QString &filePath)
{
    if (filePath.isEmpty()) return; // impossible

    closeWinDialogs();

    if (timer_->isActive()) timer_->stop();

    if (!xmlPath_.isEmpty() && !QFile::exists (xmlPath_))
    {
        if (unSaved (false))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }
    else if (saveNeeded_ > 0)
    {
        if (unSaved (true))
        {
            if (autoSave_ >= 1)
                timer_->start (autoSave_ * 1000 * 60);
            return;
        }
    }

    /* in the case of dropping,
       TextEdit::insertFromMimeData() should first return */
    QTimer::singleShot (0, this, [this, filePath] () {
        fileOpen (filePath);
        raise();
        if (!static_cast<FNSingleton*>(qApp)->isWayland())
            activateWindow();
    });
}
/*************************/
void FN::openRecentFile()
{
    QAction *action = qobject_cast<QAction*>(QObject::sender());
    if (!action) return;

    /* openReccentSeparately_ is up-to-date because updateRecentAction() is already called */
    if (openReccentSeparately_)
    {
        QProcess::startDetached (QCoreApplication::applicationFilePath(),
                                 QStringList() << action->data().toString());
    }
    else
        openFNDoc (action->data().toString());
}
/*************************/
void FN::dragMoveEvent (QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        const auto urls = event->mimeData()->urls();
        for (const QUrl &url : urls)
        {
            if (url.fileName().endsWith (".fnx"))
            {
                event->acceptProposedAction();
                return;
            }
            QMimeDatabase mimeDatabase;
            QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
            if (mimeType.name() == "text/feathernotes-fnx")
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}
/*************************/
void FN::dragEnterEvent (QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        const auto urls = event->mimeData()->urls();
        for (const QUrl &url : urls)
        {
            if (url.fileName().endsWith (".fnx"))
            {
                event->acceptProposedAction();
                return;
            }
            QMimeDatabase mimeDatabase;
            QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
            if (mimeType.name() == "text/feathernotes-fnx")
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}
/*************************/
void FN::dropEvent (QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        const auto urls = event->mimeData()->urls();
        for (const QUrl &url : urls)
        {
            if (url.fileName().endsWith (".fnx"))
            {
                openFNDoc (url.path());
                break;
            }
            QMimeDatabase mimeDatabase;
            QMimeType mimeType = mimeDatabase.mimeTypeForFile (QFileInfo (url.toLocalFile()));
            if (mimeType.name() == "text/feathernotes-fnx")
            {
                openFNDoc (url.path());
                break;
            }
        }
    }

    event->acceptProposedAction();
}
/*************************/
void FN::autoSaving()
{
    if (xmlPath_.isEmpty() || saveNeeded_ == 0
        || !QFile::exists (xmlPath_)) // the file is deleted (but might be restored later)
    {
        return;
    }
    fileSave (xmlPath_);
}
/*************************/
void FN::showWarningBar (const QString &message, int timeout)
{
    /* don't close and show the same warning bar */
    if (auto prevBar = ui->centralWidget->findChild<WarningBar*>())
    {
        if (!prevBar->isClosing() && prevBar->getMessage() == message)
        {
            prevBar->setTimeout (timeout);
            return;
        }
    }

    new WarningBar (message, timeout, ui->centralWidget);
}
/*************************/
void FN::notSavedOrOpened (bool notSaved)
{
    showWarningBar (notSaved ? tr ("<center><b><big>Cannot be saved!</big></b></center>")
                             : tr ("<center><b><big>Cannot be opened!</big></b></center>"), 10);
}
/*************************/
void FN::setNodesTexts()
{
    /* first set the default font */
    QDomElement root = model_->domDocument.firstChildElement ("feathernotes");
    root.setAttribute ("txtfont", defaultFont_.toString());
    root.setAttribute ("nodefont", nodeFont_.toString());
    if (bgColor_ != QColor (Qt::white) || fgColor_ != QColor (Qt::black))
    {
        root.setAttribute ("bgcolor", bgColor_.name());
        root.setAttribute ("fgcolor", fgColor_.name());
    }
    else
    {
        root.removeAttribute ("bgcolor");
        root.removeAttribute ("fgcolor");
    }
    if (!pswrd_.isEmpty())
        root.setAttribute ("pswrd", pswrd_);
    else
        root.removeAttribute ("pswrd");

    QHash<DomItem*, TextEdit*>::iterator it;
    for (it = widgets_.begin(); it != widgets_.end(); ++it)
    {
        if (!it.value()->document()->isModified())
            continue;

        QString txt;
        /* don't write useless HTML code */
        if (!it.value()->toPlainText().isEmpty())
        {
            /* unzoom the text if it's zoomed */
            if (it.value()->document()->defaultFont() != defaultFont_)
            {
                QTextDocument *tempDoc = it.value()->document()->clone();
                tempDoc->setDefaultFont (defaultFont_);
                txt = tempDoc->toHtml();
                delete tempDoc;
            }
            else
                txt = it.value()->toHtml();
        }
        DomItem *item = it.key();
        QDomNodeList list = item->node().childNodes();

        if (list.isEmpty())
        {
            /* if this node doesn't have any child,
               append a text child node to it... */
            QDomText t = model_->domDocument.createTextNode (txt);
            item->node().appendChild (t);
        }
        else if (list.item (0).isElement())
        {
            /* ... but if its first child is an element node,
               insert the text node before that node... */
            QDomText t = model_->domDocument.createTextNode (txt);
            item->node().insertBefore (t, list.item (0));
        }
        else if (list.item (0).isText())
            /* ... finally, if this node's first child
               is a text node, replace its text */
            list.item (0).setNodeValue (txt);
    }
}
/*************************/
bool FN::saveFile()
{
    int index = ui->stackedWidget->currentIndex();
    if (index == -1) return false;

    QString fname = xmlPath_;

    if (fname.isEmpty() || !QFile::exists (fname))
    {
        if (fname.isEmpty())
        {
            QDir dir = QDir::home();
            fname = dir.filePath (tr ("Untitled") + ".fnx");
        }
        else
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            /* add the file name */
            fname = dir.filePath (QFileInfo (fname).fileName());
        }

        /* use Save-As for Save or saving */
        if (QObject::sender() != ui->actionSaveAs)
        {
            closeWinDialogs();
            FileDialog dialog (this);
            dialog.setWindowModality (Qt::WindowModal);
            dialog.setAcceptMode (QFileDialog::AcceptSave);
            dialog.setWindowTitle (tr ("Save As..."));
            dialog.setFileMode (QFileDialog::AnyFile);
            dialog.setNameFilter (tr ("FeatherNotes documents") + " (*.fnx);;" + tr ("All Files") + " (*)");
            dialog.setDirectory (fname.section ("/", 0, -2)); // workaround for KDE
            dialog.selectFile (fname);
            dialog.autoScroll();
            if (dialog.exec())
            {
                fname = dialog.selectedFiles().at (0);
                if (fname.isEmpty() || QFileInfo (fname).isDir())
                    return false;
            }
            else
                return false;
        }
    }

    if (QObject::sender() == ui->actionSaveAs)
    {
        closeWinDialogs();
        FileDialog dialog (this);
        dialog.setWindowModality (Qt::WindowModal);
        dialog.setAcceptMode (QFileDialog::AcceptSave);
        dialog.setWindowTitle (tr ("Save As..."));
        dialog.setFileMode (QFileDialog::AnyFile);
        dialog.setNameFilter (tr ("FeatherNotes documents") + " (*.fnx);;" + tr ("All Files") + " (*)");
        dialog.setDirectory (fname.section ("/", 0, -2)); // workaround for KDE
        dialog.selectFile (fname);
        dialog.autoScroll();
        if (dialog.exec())
        {
            fname = dialog.selectedFiles().at (0);
            if (fname.isEmpty() || QFileInfo (fname).isDir())
                return false;
        }
        else
            return false;
    }

    bool overwrite (fname == xmlPath_);
    if (!fileSave (fname))
    {
        notSavedOrOpened (true);
        return false;
    }
    else if (!overwrite)
        rememberLastOpenedFile();

    return true;
}
/*************************/
bool FN::fileSave (const QString &filePath)
{
    QFile outputFile (filePath);
    if (pswrd_.isEmpty())
    {
        if (outputFile.open (QIODevice::WriteOnly))
        {
            /* now, it's the time to set the nodes' texts */
            setNodesTexts();

            QTextStream outStream (&outputFile);
            model_->domDocument.save (outStream, 1);
            outputFile.close();

            if (xmlPath_ != filePath)
            {
                xmlPath_ = filePath;
                setTitle (xmlPath_);
            }
            QHash<DomItem*, TextEdit*>::iterator it;
            for (it = widgets_.begin(); it != widgets_.end(); ++it)
                it.value()->document()->setModified (false);
            if (saveNeeded_ > 0)
            {
                saveNeeded_ = 0;
                ui->actionSave->setEnabled (false);
                setWindowModified (false);
            }
            docProp();
        }
        else return false;
    }
    else
    {
        if (outputFile.open (QFile::WriteOnly))
        {
            setNodesTexts();
            SimpleCrypt crypto (Q_UINT64_C (0xc9a25eb1610eb104));
            QString encrypted = crypto.encryptToString (model_->domDocument.toString());

            QTextStream out (&outputFile);
            out << encrypted;
            outputFile.close();

            if (xmlPath_ != filePath)
            {
                xmlPath_ = filePath;
                setTitle (xmlPath_);
            }
            QHash<DomItem*, TextEdit*>::iterator it;
            for (it = widgets_.begin(); it != widgets_.end(); ++it)
                it.value()->document()->setModified (false);
            if (saveNeeded_ > 0)
            {
                saveNeeded_ = 0;
                ui->actionSave->setEnabled (false);
                setWindowModified (false);
            }
            docProp();
        }
        else return false;
    }

    return true;
}
/*************************/
void FN::undoing()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    /* remove green highlights */
    QHash<TextEdit*,QList<QTextEdit::ExtraSelection> >::iterator it;
    for (it = greenSels_.begin(); it != greenSels_.end(); ++it)
    {
        QList<QTextEdit::ExtraSelection> extraSelectionsIth;
        greenSels_[it.key()] = extraSelectionsIth;
        it.key()->setExtraSelections (QList<QTextEdit::ExtraSelection>());
    }

    qobject_cast<TextEdit*>(cw)->undo();
}
/*************************/
void FN::redoing()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    qobject_cast<TextEdit*>(cw)->redo();
}
/*************************/
void FN::cutText()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    qobject_cast<TextEdit*>(cw)->cut();
}
/*************************/
void FN::copyText()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    qobject_cast<TextEdit*>(cw)->copy();
}
/*************************/
void FN::pasteText()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    qobject_cast<TextEdit*>(cw)->paste();
}
/*************************/
void FN::pasteHTML()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    textEdit->setAcceptRichText (true);
    textEdit->paste();
    textEdit->setAcceptRichText (false);
}
/*************************/
void FN::deleteText()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    if (!textEdit->isReadOnly())
        textEdit->insertPlainText ("");
}
/*************************/
void FN::selectAllText()
{
    if (QWidget *cw = ui->stackedWidget->currentWidget())
        qobject_cast<TextEdit*>(cw)->selectAll();
}
/*************************/
void FN::insertDate()
{
    if (QWidget *cw = ui->stackedWidget->currentWidget())
    {
        qobject_cast<TextEdit*>(cw)->insertPlainText (QDateTime::currentDateTime().toString (dateFormat_.isEmpty()
                                                          ? locale().dateTimeFormat()
                                                          : dateFormat_));
    }
}
/*************************/
static inline qreal luminance (const QColor &col)
{
    qreal R = col.redF();
    qreal G = col.greenF();
    qreal B = col.blueF();
    if (R <= 0.03928) R = R/12.92; else R = qPow ((R + 0.055)/1.055, 2.4);
    if (G <= 0.03928) G = G/12.92; else G = qPow ((G + 0.055)/1.055, 2.4);
    if (B <= 0.03928) B = B/12.92; else B = qPow ((B + 0.055)/1.055, 2.4);
    return 0.2126*R + 0.7152*G + 0.0722*B;
}

static inline bool enoughContrast (const QColor &col1, const QColor &col2)
{
    if (!col1.isValid() || !col2.isValid()) return false;
    qreal rl1 = luminance (col1);
    qreal rl2 = luminance (col2);
    if ((qMax (rl1, rl2) + 0.05) / (qMin (rl1, rl2) + 0.05) < 3.5)
        return false;
    return true;
}

void FN::setEditorStyleSheet (TextEdit *textEdit)
{
    if (bgColor_ == QColor (Qt::white) && fgColor_ == QColor (Qt::black))
    {
        QPalette p = QApplication::palette();
        QColor hCol = p.color(QPalette::Active, QPalette::Highlight);
        QBrush brush = p.window();
        if (brush.color().value() <= 120)
        {
            if (236 - qGray (hCol.rgb()) < 30)
                textEdit->setStyleSheet ("QTextEdit {"
                                         "color: black;"
                                         "selection-color: black;"
                                         "selection-background-color: rgb(200, 200, 200);}");
            else
                textEdit->setStyleSheet ("QTextEdit {"
                                         "color: black;}");
            /* with dark themes, a totally white background might not be good for the eyes */
            textEdit->viewport()->setStyleSheet (".QWidget {"
                                                 "color: black;"
                                                 "background-color: rgb(236, 236, 236);}");
        }
        else
        {
            if (255 - qGray (hCol.rgb()) < 30)
                textEdit->setStyleSheet ("QTextEdit {"
                                         "color: black;"
                                         "selection-color: black;"
                                         "selection-background-color: rgb(200, 200, 200);}");
            else
                textEdit->setStyleSheet ("QTextEdit {"
                                         "color: black;}");
            textEdit->viewport()->setStyleSheet (".QWidget {"
                                                 "color: black;"
                                                 "background-color: rgb(255, 255, 255);}");
        }
    }
    else
    {
        QPalette p = textEdit->palette();
        QColor hCol = p.color(QPalette::Active, QPalette::Highlight);
        QColor hTextColor = p.color(QPalette::Active, QPalette::HighlightedText);
        if (!enoughContrast (bgColor_, hCol))
        {
            if (bgColor_.value() <= 120)
            {
                hCol = QColor (200, 200, 200);
                hTextColor = QColor (Qt::black);
            }
            else
            {
                hCol = QColor (50, 50, 50);
                hTextColor = QColor (Qt::white);
            }
        }
        const QString fgColorName = fgColor_.name();
        textEdit->setStyleSheet (QString ("QTextEdit {"
                                          "color: %1;"
                                          "selection-color: %2;"
                                          "selection-background-color: %3;}")
                                 .arg (fgColorName, hTextColor.name(), hCol.name()));
        textEdit->viewport()->setStyleSheet (QString (".QWidget {"
                                                      "color: %1;"
                                                      "background-color: %2;}")
                                             .arg (fgColorName, bgColor_.name()));
    }
}

TextEdit *FN::newWidget()
{
    TextEdit *textEdit = new TextEdit;
    //textEdit->autoIndentation = true; // auto-indentation is enabled by default
    textEdit->autoBracket = autoBracket_;
    textEdit->autoReplace = autoReplace_;
    setEditorStyleSheet (textEdit);
    textEdit->setAcceptRichText (false);
    textEdit->viewport()->setMouseTracking (true);
    textEdit->setContextMenuPolicy (Qt::CustomContextMenu);
    /* we want consistent widgets */
    textEdit->setEditorFont (defaultFont_); // needed when the application font changes
    textEdit->document()->setDefaultFont (defaultFont_);

    int index = ui->stackedWidget->currentIndex();
    ui->stackedWidget->insertWidget (index + 1, textEdit);
    ui->stackedWidget->setCurrentWidget (textEdit);

    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode (QTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->autoIndentation = false;

    connect (textEdit, &QTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    connect (textEdit, &QTextEdit::copyAvailable, ui->actionCopy, &QAction::setEnabled);
    connect (textEdit, &QTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    connect (textEdit, &QTextEdit::copyAvailable, ui->actionLink, &QAction::setEnabled);
    connect (textEdit, &QTextEdit::copyAvailable, this, &FN::setCursorInsideSelection);
    connect (textEdit, &TextEdit::imageDropped, this, &FN::imageEmbed);
    connect (textEdit, &TextEdit::FNDocDropped, this, &FN::openFNDoc);
    connect (textEdit, &TextEdit::zoomedOut, this, &FN::rehighlight);
    connect (textEdit, &QWidget::customContextMenuRequested, this, &FN::txtContextMenu);
    /* The remaining connections to QTextEdit signals are in selChanged(). */

    /* I don't know why, under KDE, when a text is selected for the first time,
       it may not be copied to the selection clipboard. Perhaps it has something
       to do with Klipper. I neither know why the following line is a workaround
       but it can cause a long delay when FeatherNotes is started. */
    //QApplication::clipboard()->text (QClipboard::Selection);

    return textEdit;
}
/*************************/
// If some text is selected and the cursor is put somewhere inside
// the selection with mouse, Qt may not emit currentCharFormatChanged()
// when it should, as if the cursor isn't really set. The following method
// really sets the text cursor and can be used as a workaround for this bug.
void FN::setCursorInsideSelection (bool sel)
{
    if (!sel)
    {
        if (QWidget *cw = ui->stackedWidget->currentWidget())
        {
            TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
            /* WARNING: Qt4 didn't need disconnecting. Why?! */
            disconnect (textEdit, &QTextEdit::copyAvailable, this, &FN::setCursorInsideSelection);
            QTextCursor cur = textEdit->textCursor();
            textEdit->setTextCursor (cur);
            connect (textEdit, &QTextEdit::copyAvailable, this, &FN::setCursorInsideSelection);
        }
    }
}
/*************************/
void FN::txtContextMenu (const QPoint &p)
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    bool hasSel = cur.hasSelection();
    /* set the text cursor at the position of
       right clicking if there's no selection */
    if (!hasSel)
    {
        cur = textEdit->cursorForPosition (p);
        textEdit->setTextCursor (cur);
    }
    linkAtPos_ = textEdit->anchorAt (p);
    QMenu *menu = textEdit->createStandardContextMenu (p);
    bool sepAdded (false);

    const QList<QAction*> list = menu->actions();
    int copyIndx = -1, pasteIndx = -1;
    for (int i = 0; i < list.count(); ++i)
    {
        const auto thisAction = list.at (i);
        /* remove the shortcut strings because shortcuts may change */
        QString txt = thisAction->text();
        if (!txt.isEmpty())
            txt = txt.split ('\t').first();
        if (!txt.isEmpty())
            thisAction->setText (txt);
        /* find appropriate places for actionCopyLink and actionPasteHTML */
        if (thisAction->objectName() == "edit-copy")
        {
            copyIndx = i;
            /* also, we want to override QTextEdit::copy() */
            disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
            connect (thisAction, &QAction::triggered, textEdit, &TextEdit::copy);
        }
        else if (thisAction->objectName() == "edit-paste")
            pasteIndx = i;
        else if (thisAction->objectName() == "edit-undo")
        { // overriding QTextEdit::undo()
            disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
            connect (thisAction, &QAction::triggered, textEdit, &TextEdit::undo);
        }
        else if (thisAction->objectName() == "edit-cut")
        { // overriding QTextEdit::cut()
            disconnect (thisAction, &QAction::triggered, nullptr, nullptr);
            connect (thisAction, &QAction::triggered, textEdit, &TextEdit::cut);
        }
    }
    if (!linkAtPos_.isEmpty())
    {
        if (copyIndx > -1 && copyIndx + 1 < list.count())
            menu->insertAction (list.at (copyIndx + 1), ui->actionCopyLink);
        else
        {
            menu->addSeparator();
            menu->addAction (ui->actionCopyLink);
        }
    }
    if (pasteIndx > -1 && pasteIndx + 1 < list.count())
        menu->insertAction (list.at (pasteIndx + 1), ui->actionPasteHTML);
    else
    {
        menu->addAction (ui->actionPasteHTML);
        menu->addSeparator();
        sepAdded = true;
    }
    ui->actionPasteHTML->setEnabled (textEdit->canPaste());

    if (hasSel)
    {
        if (!sepAdded)
        {
            menu->addSeparator();
            sepAdded = true;
        }
        menu->addAction (ui->actionLink);
        if (isImageSelected())
        {
            menu->addSeparator();
            menu->addAction (ui->actionImageScale);
            menu->addAction (ui->actionImageSave);
        }
        menu->addSeparator();
    }
    if (!sepAdded) menu->addSeparator();
    menu->addAction (ui->actionEmbedImage);
    menu->addAction (ui->actionTable);
    if (QTextTable *table = cur.currentTable())
    {
        menu->addSeparator();

        QMenu *submenu = menu->addMenu (symbolicIcon::icon (":icons/alignment.svg"), tr ("Align Table"));
        QAction *a = submenu->addAction (tr ("&Left"));
        connect (a, &QAction::triggered, table, [table] {
            QTextTableFormat tf = table->format();
            tf.setAlignment (Qt::AlignLeft | Qt::AlignVCenter);
            table->setFormat (tf);
        });
        a = submenu->addAction (tr ("&Right"));
        connect (a, &QAction::triggered, table, [table] {
            QTextTableFormat tf = table->format();
            tf.setAlignment (Qt::AlignRight | Qt::AlignVCenter);
            table->setFormat (tf);
        });
        a = submenu->addAction (tr ("&Center"));
        connect (a, &QAction::triggered, table, [table] {
            QTextTableFormat tf = table->format();
            tf.setAlignment (Qt::AlignCenter);
            table->setFormat (tf);
        });

        if (cur.hasComplexSelection())
            menu->addAction (ui->actionTableMergeCells);
        else
        {
            menu->addAction (ui->actionTablePrependRow);
            menu->addAction (ui->actionTableAppendRow);
            menu->addAction (ui->actionTablePrependCol);
            menu->addAction (ui->actionTableAppendCol);
            menu->addAction (ui->actionTableDeleteRow);
            menu->addAction (ui->actionTableDeleteCol);
        }
    }

    menu->addSeparator();
    menu->addAction (ui->actionCheckSpelling);
    menu->addSeparator();
    menu->addAction (ui->actionDate);

    menu->exec (textEdit->viewport()->mapToGlobal (p));
    delete menu;
}
/*************************/
void FN::copyLink()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText (linkAtPos_);
}
/*************************/
void FN::selChanged (const QItemSelection &selected, const QItemSelection &deselected)
{
    closeWarningBar();

    if (selected.isEmpty())
    {
        if (model_->rowCount() == 0) // if the last node is closed
        {
            if (ui->lineEdit->isVisible())
                showHideSearch();
            if (ui->dockReplace->isVisible())
                replaceDock();
            enableActions (false);
            /*if (QWidget *widget = ui->stackedWidget->currentWidget())
                widget->setVisible (false);*/
        }
        return;
    }
    /*else if (deselected.isEmpty()) // a row is selected after Ctrl + left click
    {
        if (QWidget *widget = ui->stackedWidget->currentWidget())
            widget->setVisible (true);
        enableActions (true);
    }*/

    updateNodeActions();
    if (treeViewDND_) return;

    QString globalSearchTxt;
    if (ui->everywhereButton->isChecked())
    { // find the globally searched text (the text of "ui->lineEdit" isn't reliable)
        if (!deselected.isEmpty())
        {
            QModelIndex oldIndx = deselected.indexes().at (0);
            QHash<DomItem*, TextEdit*>::iterator iter;
            for (iter = widgets_.begin(); iter != widgets_.end(); ++iter)
            {
                if (iter.key() == static_cast<DomItem*>(oldIndx.internalPointer()))
                {
                    globalSearchTxt = searchEntries_[iter.value()];
                    break;
                }
            }
        }
    }

    /* if a widget is paired with this DOM item, show it;
       otherwise create a widget and pair it with the item */
    QModelIndex index = selected.indexes().at (0);
    TextEdit *textEdit = nullptr;
    bool found = false;
    QHash<DomItem*, TextEdit*>::iterator it;
    for (it = widgets_.begin(); it != widgets_.end(); ++it)
    {
        if (it.key() == static_cast<DomItem*>(index.internalPointer()))
        {
            found = true;
            break;
        }
    }
    if (found)
    {
        textEdit = it.value();
        ui->stackedWidget->setCurrentWidget (textEdit);
        QString searchTxt = searchEntries_[textEdit];
        if (ui->everywhereButton->isChecked())
        { // keep the previous search string when searching in all nodes
            searchEntries_[textEdit] = globalSearchTxt;
            ui->lineEdit->setText (globalSearchTxt);
            if (globalSearchTxt.isEmpty())
            {
               if (!searchTxt.isEmpty())
               { // remove all yellow and green highlights
                    disconnect (textEdit->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                    disconnect (textEdit->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                    disconnect (textEdit, &TextEdit::resized, this, &FN::hlight);
                    disconnect (textEdit, &QTextEdit::textChanged, this, &FN::hlight);
                    QList<QTextEdit::ExtraSelection> extraSelections;
                    greenSels_[textEdit] = extraSelections;
                    textEdit->setExtraSelections (extraSelections);
               }
            }
            else
            {
                hlight();
                if (searchTxt.isEmpty())
                { // there was no connection before
                    connect (textEdit->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                    connect (textEdit->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                    connect (textEdit, &TextEdit::resized, this, &FN::hlight);
                    connect (textEdit, &QTextEdit::textChanged, this, &FN::hlight);
                }
            }
        }
        else if (!ui->tagsButton->isChecked() && !ui->namesButton->isChecked())
        { // change the search entry's text only if the search isn't done in tags or names
            ui->lineEdit->setText (searchTxt);
            if (!searchTxt.isEmpty()) hlight();
        }
    }
    else
    {
        QString text;
        DomItem *item = static_cast<DomItem*>(index.internalPointer());
        QDomNodeList list = item->node().childNodes();
        text = list.item (0).nodeValue();
        /* NOTE: This is needed for text zooming. */
        static const QString htmlStr ("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
                                      "<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
                                      "p, li { white-space: pre-wrap; }\n"
                                      "</style></head><body>");
        QRegularExpressionMatch match;
        bool defaultDocColor (bgColor_ == QColor (Qt::white) && fgColor_ == QColor (Qt::black));
        if (defaultDocColor)
        {
            static const QRegularExpression htmlRegex (R"(^<!DOCTYPE[A-Za-z0-9/<>,;.:\-={}\s"\\]+</style></head><body\sstyle=[A-Za-z0-9/<>;:\-\s"']+>)");
            if (text.indexOf (htmlRegex, 0, &match) > -1)
                text.replace (0, match.capturedLength(), htmlStr);
        }
        textEdit = newWidget();
        textEdit->setHtml (text);
        if (!defaultDocColor)
        {
            /* To enable the default stylesheet, we should set the HTML text of the document.
               Setting the HTML text of the editor above is needed for empty nodes. */
            QString str = textEdit->document()->toHtml();
            static const QRegularExpression htmlRegex1 (R"(^<!DOCTYPE[A-Za-z0-9/<>,;.:\-={}\s"\\]+</style></head><body\sstyle=[A-Za-z0-9/;:\-\s"'#=]+>(<br\s*/>(</p>)?)?)");
            if (str.indexOf (htmlRegex1, 0, &match) > -1)
                str.replace (0, match.capturedLength(), htmlStr);
            textEdit->document()->setHtml (str);
            QTextCursor cur = textEdit->textCursor();
            cur.setPosition (0);
            textEdit->setTextCursor (cur);
            textEdit->document()->setModified (false);
        }
        else
        {
            /* because of a Qt bug, this is needed for
               QTextEdit::currentCharFormat() to be correct */
            QTextCursor cur = textEdit->textCursor();
            cur.clearSelection ();
            textEdit->setTextCursor (cur);
        }

        connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FN::setSaveEnabled);
        connect (textEdit->document(), &QTextDocument::undoAvailable, this, &FN::setUndoEnabled);
        connect (textEdit->document(), &QTextDocument::redoAvailable, this, &FN::setRedoEnabled);
        connect (textEdit, &QTextEdit::currentCharFormatChanged, this, &FN::formatChanged);
        connect (textEdit, &QTextEdit::cursorPositionChanged, this, &FN::alignmentChanged);
        connect (textEdit, &QTextEdit::cursorPositionChanged, this, &FN::directionChanged);
        connect (textEdit->document(), &QTextDocument::contentsChange,
                 [this] (int/* pos*/, int charsRemoved, int charsAdded) {
            if (charsRemoved == charsAdded)
            { // a special case, where the following slots aren't called automatically
                alignmentChanged();
                directionChanged();
            }
        });

        /* focus the text widget only if a document is opened just now */
        if (widgets_.isEmpty())
            textEdit->setFocus();

        widgets_[static_cast<DomItem*>(index.internalPointer())] = textEdit;
        greenSels_[textEdit] = QList<QTextEdit::ExtraSelection>();
        if (ui->everywhereButton->isChecked())
        {
            searchEntries_[textEdit] = globalSearchTxt;
            ui->lineEdit->setText (globalSearchTxt);
            if (!globalSearchTxt.isEmpty())
            {
                hlight();
                connect (textEdit->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                connect (textEdit->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                connect (textEdit, &TextEdit::resized, this, &FN::hlight);
                connect (textEdit, &QTextEdit::textChanged, this, &FN::hlight);
            }
        }
        else
        {
            searchEntries_[textEdit] = QString();
            if (!ui->tagsButton->isChecked() && !ui->namesButton->isChecked())
                ui->lineEdit->setText (QString());
        }
    }

    ui->actionUndo->setEnabled (textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled (textEdit->document()->isRedoAvailable());

    bool textIsSelected = textEdit->textCursor().hasSelection();
    ui->actionCopy->setEnabled (textIsSelected);
    ui->actionCut->setEnabled (textIsSelected);
    ui->actionDelete->setEnabled (textIsSelected);
    ui->actionLink->setEnabled (textIsSelected);

    formatChanged (textEdit->currentCharFormat());
    alignmentChanged();
    directionChanged();
}
/*************************/
void FN::setSaveEnabled (bool modified)
{
    if (modified)
        noteModified();
    else
    {
        if (saveNeeded_ > 0)
            --saveNeeded_;
        if (saveNeeded_ == 0)
        {
            ui->actionSave->setEnabled (false);
            setWindowModified (false);
        }
    }
}
/*************************/
void FN::setUndoEnabled (bool enabled)
{
    ui->actionUndo->setEnabled (enabled);
}
/*************************/
void FN::setRedoEnabled (bool enabled)
{
    ui->actionRedo->setEnabled (enabled);
}
/*************************/
void FN::formatChanged (const QTextCharFormat &format)
{
    ui->actionSuper->setChecked (format.verticalAlignment() == QTextCharFormat::AlignSuperScript ?
                                 true : false);
    ui->actionSub->setChecked (format.verticalAlignment() == QTextCharFormat::AlignSubScript ?
                               true : false);

    if (format.fontWeight() > QFont::Medium) // who knows what will happen after Qt upgrades?
        ui->actionBold->setChecked (true);
    else
        ui->actionBold->setChecked (false);
    ui->actionItalic->setChecked (format.fontItalic());
    ui->actionUnderline->setChecked (format.fontUnderline());
    ui->actionStrike->setChecked (format.fontStrikeOut());
}
/*************************/
void FN::alignmentChanged()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    Qt::Alignment a = qobject_cast<TextEdit*>(cw)->alignment();
    if (a & Qt::AlignLeft)
    {
        if (a & Qt::AlignAbsolute)
            ui->actionLeft->setChecked (true);
        else // get alignment from text layout direction
        {
            QTextCursor cur = qobject_cast<TextEdit*>(cw)->textCursor();
            QTextBlockFormat fmt = cur.blockFormat();
            if (fmt.layoutDirection() == Qt::LeftToRight)
                ui->actionLeft->setChecked (true);
            else if (fmt.layoutDirection() == Qt::RightToLeft)
                ui->actionRight->setChecked (true);
            else // Qt::LayoutDirectionAuto
            {
                /* textDirection() returns either Qt::LeftToRight
                   or Qt::RightToLeft (-> qtextobject.cpp) */
                QTextBlock blk = cur.block();
                if (blk.textDirection() == Qt::LeftToRight)
                    ui->actionLeft->setChecked (true);
                else
                    ui->actionRight->setChecked (true);
            }
        }
    }
    else if (a & Qt::AlignHCenter)
        ui->actionCenter->setChecked (true);
    else if (a & Qt::AlignRight)
    {
        if (a & Qt::AlignAbsolute)
            ui->actionRight->setChecked (true);
        else // get alignment from text layout direction
        {
            QTextCursor cur = qobject_cast<TextEdit*>(cw)->textCursor();
            QTextBlockFormat fmt = cur.blockFormat();
            if (fmt.layoutDirection() == Qt::RightToLeft)
                ui->actionRight->setChecked (true);
            else if (fmt.layoutDirection() == Qt::LeftToRight)
                ui->actionLeft->setChecked (true);
            else // Qt::LayoutDirectionAuto
            {
                QTextBlock blk = cur.block();
                if (blk.textDirection() == Qt::LeftToRight)
                    ui->actionLeft->setChecked (true);
                else
                    ui->actionRight->setChecked (true);
            }
        }
    }
    else if (a & Qt::AlignJustify)
        ui->actionJust->setChecked (true);
}
/*************************/
void FN::directionChanged()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    QTextCursor cur = qobject_cast<TextEdit*>(cw)->textCursor();
    QTextBlockFormat fmt = cur.blockFormat();
    if (fmt.layoutDirection() == Qt::LeftToRight)
        ui->actionLTR->setChecked (true);
    else if (fmt.layoutDirection() == Qt::RightToLeft)
        ui->actionRTL->setChecked (true);
    else // Qt::LayoutDirectionAuto
    {
        QTextBlock blk = cur.block();
        if (blk.textDirection() == Qt::LeftToRight)
            ui->actionLTR->setChecked (true);
        else
            ui->actionRTL->setChecked (true);
    }
}
/*************************/
void FN::mergeFormatOnWordOrSelection (const QTextCharFormat &format)
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (!cur.hasSelection())
    { // apply to the word only if the cusor isn't at its end
        QTextCursor tmp = cur;
        tmp.movePosition (QTextCursor::EndOfWord);
        if (tmp.position() > cur.position())
            cur.select (QTextCursor::WordUnderCursor);
    }
    if (!cur.hasSelection())
        textEdit->mergeCurrentCharFormat (format); // alows to type with the new format
    else
        cur.mergeCharFormat (format);
    /* correct the pressed states of the format buttons if necessary */
    formatChanged (textEdit->currentCharFormat());
}
/*************************/
void FN::makeBold()
{
    QTextCharFormat fmt;
    fmt.setFontWeight (ui->actionBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::makeItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic (ui->actionItalic->isChecked());
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::makeUnderlined()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline (ui->actionUnderline->isChecked());
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::makeStriked()
{
    QTextCharFormat fmt;
    fmt.setFontStrikeOut (ui->actionStrike->isChecked());
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::makeSuperscript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment (ui->actionSuper->isChecked() ?
                              QTextCharFormat::AlignSuperScript :
                              QTextCharFormat::AlignNormal);
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::makeSubscript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment (ui->actionSub->isChecked() ?
                              QTextCharFormat::AlignSubScript :
                              QTextCharFormat::AlignNormal);
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::textColor()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    QColor color = qobject_cast<TextEdit*>(cw)->textColor();
    if (color == fgColor_)
    {
        if (lastTxtColor_.isValid())
            color = lastTxtColor_;
    }
    auto dlg = new QColorDialog (color, this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Select Text Color"));
    dlg->open();
    connect (dlg, &QDialog::finished, this, [this, dlg] (int res) {
        if (res == QDialog::Accepted)
        {
            QColor col = dlg->selectedColor();
            if (col.isValid())
            {
                lastTxtColor_ = col;
                QTextCharFormat fmt;
                fmt.setForeground (col);
                mergeFormatOnWordOrSelection (fmt);
            }
        }
    });
}
/*************************/
void FN::bgColor()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    QColor color = qobject_cast<TextEdit*>(cw)->textBackgroundColor();
    if (color == QColor (Qt::black)) // this is a Qt bug
    {
        if (lastBgColor_.isValid())
            color = lastBgColor_;
    }
    auto dlg = new QColorDialog (color, this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Select Background Color"));
    dlg->open();
    connect (dlg, &QDialog::finished, this, [this, dlg] (int res) {
        if (res == QDialog::Accepted)
        {
            QColor col = dlg->selectedColor();
            if (col.isValid())
            {
                lastBgColor_ = col;
                QTextCharFormat fmt;
                fmt.setBackground (col);
                mergeFormatOnWordOrSelection (fmt);
            }
        }
    });
}
/*************************/
void FN::clearFormat()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    QTextCharFormat fmt;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (!cur.hasSelection())
    { // apply to the word only if the cusor isn't at its end
        QTextCursor tmp = cur;
        tmp.movePosition (QTextCursor::EndOfWord);
        if (tmp.position() > cur.position())
            cur.select (QTextCursor::WordUnderCursor);
    }
    if (!cur.hasSelection())
        textEdit->setCurrentCharFormat (fmt); // alows to type without format
    else
        cur.setCharFormat (fmt);
}
/*************************/
void FN::textAlign (QAction *a)
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    if (a == ui->actionLeft)
        textEdit->setAlignment (Qt::Alignment (Qt::AlignLeft | Qt::AlignAbsolute));
    else if (a == ui->actionCenter)
        textEdit->setAlignment (Qt::AlignHCenter);
    else if (a == ui->actionRight)
        textEdit->setAlignment (Qt::Alignment (Qt::AlignRight | Qt::AlignAbsolute));
    else if (a == ui->actionJust)
        textEdit->setAlignment (Qt::AlignJustify);
}
/*************************/
void FN::textDirection (QAction *a)
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    QTextBlockFormat fmt;
    if (a == ui->actionLTR)
        fmt.setLayoutDirection (Qt::LeftToRight);
    else if (a == ui->actionRTL)
        fmt.setLayoutDirection (Qt::RightToLeft);

    QTextCursor cur = qobject_cast<TextEdit*>(cw)->textCursor();
    if (!cur.hasSelection())
        cur.select (QTextCursor::BlockUnderCursor);
    cur.mergeBlockFormat (fmt);

    //alignmentChanged(); // will be called by QTextDocument::contentsChange()
}
/*************************/
void FN::makeHeader()
{
    int index = ui->stackedWidget->currentIndex();
    if (index == -1) return;

    QTextCharFormat fmt;
    if (QObject::sender() == ui->actionH3)
        fmt.setProperty (QTextFormat::FontSizeAdjustment, 1);
    else if (QObject::sender() == ui->actionH2)
        fmt.setProperty (QTextFormat::FontSizeAdjustment, 2);
    else// if (QObject::sender() == ui->actionH1)
        fmt.setProperty (QTextFormat::FontSizeAdjustment, 3);
    mergeFormatOnWordOrSelection (fmt);
}
/*************************/
void FN::expandAll()
{
    ui->treeView->expandAll();
}
/*************************/
void FN::collapseAll()
{
    ui->treeView->collapseAll();
}
/*************************/
void FN::newNode()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    if (QObject::sender() == ui->actionNewSibling)
    {
        QModelIndex pIndex = model_->parent (index);
        model_->insertRow (index.row() + 1, pIndex);
    }
    else if (QObject::sender() == ui->actionPrepSibling)
    {
        QModelIndex pIndex = model_->parent (index);
        model_->insertRow (index.row(), pIndex);
    }
    else //if (QObject::sender() == ui->actionNewChild)
    {
        model_->insertRow (model_->rowCount (index), index);
        ui->treeView->expand (index);
    }
}
/*************************/
void FN::deleteNode()
{
    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    /* don't delete a single main node */
    if (!pIndex.isValid() && model_->rowCount() <= 1)
        return;

    closeWinDialogs();

    MessageBox msgBox;
    msgBox.setIcon (QMessageBox::Question);
    msgBox.setWindowTitle (tr ("Deletion"));
    msgBox.setText (tr ("<center><b><big>Delete this node?</big></b></center>"));
    msgBox.setInformativeText (tr ("<center><b><i>Warning!</i></b></center>\n<center>This action cannot be undone.</center>"));
    msgBox.setStandardButtons (QMessageBox::Yes | QMessageBox::No);
    msgBox.changeButtonText (QMessageBox::Yes, tr ("Yes"));
    msgBox.changeButtonText (QMessageBox::No, tr ("No"));
    msgBox.setDefaultButton (QMessageBox::No);
    msgBox.setParent (this, Qt::Dialog);
    msgBox.setWindowModality (Qt::WindowModal);
    msgBox.show();
    if (!static_cast<FNSingleton*>(qApp)->isWayland())
    {
        msgBox.move (x() + width()/2 - msgBox.width()/2,
                     y() + height()/2 - msgBox.height()/ 2);
    }
    switch (msgBox.exec()) {
    case QMessageBox::Yes:
        break;
    case QMessageBox::No:
    default:
        return;
    }

    /* remove all widgets paired with
       this node or its descendants */
    QModelIndexList list = model_->allDescendants (index);
    list << index;
    for (int i = 0; i < list.count(); ++i)
    {
        QHash<DomItem*, TextEdit*>::iterator it = widgets_.find (static_cast<DomItem*>(list.at (i).internalPointer()));
        if (it != widgets_.end())
        {
            TextEdit *textEdit = it.value();
            if (saveNeeded_ > 0 && textEdit->document()->isModified())
                --saveNeeded_;
            searchEntries_.remove (textEdit);
            greenSels_.remove (textEdit);
            ui->stackedWidget->removeWidget (textEdit);
            delete textEdit;
            widgets_.remove (it.key());
        }
    }

    /* now, really remove the node */
    model_->removeRow (index.row(), pIndex);
}
/*************************/
void FN::moveUpNode()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    if (index.row() == 0) return;

    model_->moveUpRow (index.row(), pIndex);
}
/*************************/
void FN::moveLeftNode()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    if (!pIndex.isValid()) return;

    model_->moveLeftRow (index.row(), pIndex);
}
/*************************/
void FN::moveDownNode()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    if (index.row() == model_->rowCount (pIndex) - 1) return;

    model_->moveDownRow (index.row(), pIndex);
}
/*************************/
void FN::moveRightNode()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    QModelIndex pIndex = model_->parent (index);

    if (index.row() == 0) return;

    model_->moveRightRow (index.row(), pIndex);
}
/*************************/
// Add or edit tags.
void FN::handleTags()
{
    closeWinDialogs();

    QModelIndex index = ui->treeView->currentIndex();
    DomItem *item = static_cast<DomItem*>(index.internalPointer());
    QDomNode node = item->node();
    QDomNamedNodeMap attributeMap = node.attributes();
    QString tags = attributeMap.namedItem ("tag").nodeValue();

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Tags"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    LineEdit *lineEdit = new LineEdit();
    lineEdit->returnOnClear = false;
    lineEdit->setMinimumWidth (250);
    lineEdit->setText (tags);
    lineEdit->setToolTip ("<p style='white-space:pre'>"
                          + tr ("Tag(s) for this node")
                          + "</p>");
    connect (lineEdit, &QLineEdit::returnPressed, dialog, &QDialog::accept);
    QSpacerItem *spacer = new QSpacerItem (1, 5);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);

    grid->addWidget (lineEdit, 0, 0, 1, 3);
    grid->addItem (spacer, 1, 0);
    grid->addWidget (cancelButton, 2, 1, Qt::AlignRight);
    grid->addWidget (okButton, 2, 2, Qt::AlignCenter);
    grid->setColumnStretch (0, 1);
    grid->setRowStretch (1, 1);

    dialog->setLayout (grid);

    dialog->open();
    connect (dialog, &QDialog::finished, this, [this, lineEdit, node, tags] (int res) {
        if (res == QDialog::Accepted)
        {
            QString newTags = lineEdit->text();
            if (newTags != tags)
            {
                if (newTags.isEmpty())
                    node.toElement().removeAttribute ("tag");
                else
                    node.toElement().setAttribute ("tag", newTags);

                noteModified();
            }
        }
    });
}
/*************************/
void FN::renameNode()
{
    ui->treeView->edit (ui->treeView->currentIndex());
}
/*************************/
void FN::nodeIcon()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid()) return;
    DomItem *item = static_cast<DomItem*>(index.internalPointer());
    if (item == nullptr) return;

    closeWinDialogs();

    QDomNode node = item->node();
    QString curIcn = node.toElement().attribute ("icon");

    QDialog *dlg = new QDialog (this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Node Icon"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    dlg->setContentsMargins (5, 5, 5, 5);

    LineEdit *ImagePathEntry = new LineEdit();
    ImagePathEntry->returnOnClear = false;
    ImagePathEntry->setMinimumWidth (200);
    ImagePathEntry->setToolTip (tr ("Image path"));
    connect (ImagePathEntry, &QLineEdit::returnPressed, dlg, &QDialog::accept);
    QToolButton *openBtn = new QToolButton();
    openBtn->setIcon (symbolicIcon::icon (":icons/document-open.svg"));
    openBtn->setToolTip (tr ("Open image"));
    connect (openBtn, &QAbstractButton::clicked, dlg, [=] {
        QString path;
        if (!xmlPath_.isEmpty())
        {
            QDir dir = QFileInfo (xmlPath_).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
        else
        {
            QDir dir = QDir::home();
            path = dir.path();
        }
        QString file;
        FileDialog dialog (this);
        dialog.setAcceptMode (QFileDialog::AcceptOpen);
        dialog.setWindowTitle (tr ("Open Image..."));
        dialog.setFileMode (QFileDialog::ExistingFiles);
        dialog.setNameFilter (tr ("Image Files") + " (*.svg *.png *.jpg *.jpeg *.bmp *.gif);;" + tr ("All Files") + " (*)");
        dialog.setDirectory (path);
        if (dialog.exec())
        {
            QStringList files = dialog.selectedFiles();
            if (files.count() > 0)
                file = files.at (0);
        }
        ImagePathEntry->setText (file);
    });
    QSpacerItem *spacer = new QSpacerItem (1, 10, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dlg, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dlg, &QDialog::accept);

    grid->addWidget (ImagePathEntry, 0, 0, 1, 4);
    grid->addWidget (openBtn, 0, 4, Qt::AlignCenter);
    grid->addItem (spacer, 1, 0);
    grid->addWidget (cancelButton, 2, 2, Qt::AlignRight);
    grid->addWidget (okButton, 2, 3, 1, 2, Qt::AlignCenter);
    grid->setColumnStretch (1, 1);

    dlg->setLayout (grid);
    dlg->resize (dlg->sizeHint());
    dlg->open();
    connect (dlg, &QDialog::finished, this, [this, ImagePathEntry, index, node, curIcn] (int res) {
        if (res == QDialog::Accepted)
        {
            QString imagePath = ImagePathEntry->text();
            if (imagePath.isEmpty())
            {
                if (!curIcn.isEmpty())
                {
                    node.toElement().removeAttribute ("icon");
                    emit model_->dataChanged (index, index); // will update geometry and call noteModified()
                }
            }
            else
            {
                QFile file (imagePath);
                if (file.open (QIODevice::ReadOnly))
                {
                    QDataStream in (&file);
                    QByteArray rawarray;
                    QDataStream datastream (&rawarray, QIODevice::WriteOnly);
                    char a;
                    while (in.readRawData (&a, 1) != 0)
                        datastream.writeRawData (&a, 1);
                    file.close();
                    QByteArray base64array = rawarray.toBase64();
                    const QString icn = QString (base64array);

                    if (curIcn != icn)
                    {
                        node.toElement().setAttribute ("icon", icn);
                        emit model_->dataChanged (index, index);
                    }
                }
            }
        }
    });
}
/*************************/
void FN::toggleStatusBar()
{
    if (ui->statusBar->isVisible())
    {
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        ui->statusBar->removeWidget (statusLabel);
        if (statusLabel)
        {
            delete statusLabel;
            statusLabel = nullptr;
        }
        ui->statusBar->setVisible (false);
        return;
    }

    int rows = model_->rowCount();
    int allNodes = 0;
    if (rows > 0)
    {
        QModelIndex indx = model_->index (0, 0, QModelIndex());
        while ((indx = model_->adjacentIndex (indx, true)).isValid())
            ++allNodes;
        ++allNodes;
    }
    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    if (xmlPath_.isEmpty())
    {
        statusLabel->setText (tr ("<b>Main nodes:</b> <i>%1</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>All nodes:</b> <i>%2</i>")
                              .arg (rows).arg (allNodes));
    }
    else
    {
        statusLabel->setText (tr ("<b>Note:</b> <i>%1</i><br>"
                                  "<b>Main nodes:</b> <i>%2</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>All nodes:</b> <i>%3</i>")
                              .arg (xmlPath_).arg (rows).arg (allNodes));
    }
    ui->statusBar->addWidget (statusLabel);
    ui->statusBar->setVisible (true);
}
/*************************/
void FN::docProp()
{
    if (!ui->statusBar->isVisible()) return;

    QList<QLabel *> list = ui->statusBar->findChildren<QLabel*>();
    if (list.isEmpty()) return;
    QLabel *statusLabel = list.at (0);
    int rows = model_->rowCount();
    int allNodes = 0;
    if (rows > 0)
    {
        QModelIndex indx = model_->index (0, 0, QModelIndex());
        while ((indx = model_->adjacentIndex (indx, true)).isValid())
            ++allNodes;
        ++allNodes;
    }
    if (xmlPath_.isEmpty())
    {
        statusLabel->setText (tr ("<b>Main nodes:</b> <i>%1</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>All nodes:</b> <i>%2</i>")
                              .arg (rows).arg (allNodes));
    }
    else
    {
        statusLabel->setText (tr ("<b>Note:</b> <i>%1</i><br>"
                                  "<b>Main nodes:</b> <i>%2</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>All nodes:</b> <i>%3</i>")
                              .arg (xmlPath_).arg (rows).arg (allNodes));
    }
}
/*************************/
void FN::setNewFont (DomItem *item, const QTextCharFormat &fmt)
{
    QString text;
    QDomNodeList list = item->node().childNodes();
    if (!list.item (0).isText()) return;
    text = list.item (0).nodeValue();
    if (!text.startsWith ("<!DOCTYPE HTML PUBLIC"))
        return;

    TextEdit *textEdit = new TextEdit;
    /* body font */
    textEdit->document()->setDefaultFont (defaultFont_);
    textEdit->setHtml (text);

    /* paragraph font, merged with body font */
    QTextCursor cursor = textEdit->textCursor();
    cursor.select (QTextCursor::Document);
    cursor.mergeCharFormat (fmt);

    text = textEdit->toHtml();
    list.item (0).setNodeValue (text);

    delete textEdit;
}
/*************************/
void FN::textFontDialog()
{
    closeWinDialogs();

    auto dlg = new QFontDialog (defaultFont_, this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Select Document Font"));
    dlg->open();
    connect (dlg, &QDialog::finished, this, [this, dlg] (int res) {
        if (res == QDialog::Accepted)
        {
            QFont newFont = dlg->selectedFont();
            defaultFont_ = QFont (newFont.family(), newFont.pointSize());
            defaultFont_.setFamilies (newFont.families()); // to override the existing font families

            noteModified();

            /* let the dialog be closed */
            QTimer::singleShot (0, this, [this] () {
                QTextCharFormat fmt;
                fmt.setFont (defaultFont_, QTextCharFormat::FontPropertiesSpecifiedOnly);

                /* change the font for all shown nodes
                FIXME: Text zooming won't work until the document is reloaded. */
                QHash<DomItem*, TextEdit*>::iterator it;
                for (it = widgets_.begin(); it != widgets_.end(); ++it)
                {
                    it.value()->setEditorFont (defaultFont_); // needed when the application font changes
                    it.value()->document()->setDefaultFont (defaultFont_);
                    QTextCursor cursor = it.value()->textCursor();
                    cursor.select (QTextCursor::Document);
                    cursor.mergeCharFormat (fmt);
                }

                /* also, change the font for all nodes that aren't shown yet */
                for (int i = 0; i < model_->rowCount (QModelIndex()); ++i)
                {
                    QModelIndex index = model_->index (i, 0, QModelIndex());
                    DomItem *item = static_cast<DomItem*>(index.internalPointer());
                    if (!widgets_.contains (item))
                        setNewFont (item, fmt);
                    QModelIndexList list = model_->allDescendants (index);
                    for (int j = 0; j < list.count(); ++j)
                    {
                        item = static_cast<DomItem*>(list.at (j).internalPointer());
                        if (!widgets_.contains (item))
                            setNewFont (item, fmt);
                    }
                }

                /* rehighlight found matches for this node
                because the font may be smaller now */
                if (QWidget *cw = ui->stackedWidget->currentWidget())
                {
                    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
                    rehighlight (textEdit);
                }
            });
        }
    });
}
/*************************/
void FN::nodeFontDialog()
{
    closeWinDialogs();

    auto dlg = new QFontDialog (nodeFont_, this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Select Node Font"));
    dlg->open();
    connect (dlg, &QDialog::finished, this, [this, dlg] (int res) {
        if (res == QDialog::Accepted)
        {
            nodeFont_ = dlg->selectedFont();
            noteModified();

            /* let the dialog be closed */
            QTimer::singleShot (0, this, [this] () {
                ui->treeView->setFont (nodeFont_);
            });
        }
    });
}
/*************************/
void FN::docColorDialog()
{
    closeWinDialogs();

    QColor oldBgColor = bgColor_;
    QColor oldFgColor = fgColor_;

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Set Document Colors"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 10, 5, 5);

    QGridLayout *colorLayout = new QGridLayout;
    colorLayout->setSpacing (5);
    colorLayout->setContentsMargins (0, 10, 0, 0);

    QLabel *label = new QLabel ("<center><i>"
                                + tr ("These colors will be applied to new nodes.<br>They may or may not affect existing nodes<br>but document reopening is recommended.")
                                + "</i></center>");

    QLabel *bgLabel = new QLabel (tr ("Background color:"));
    ColorLabel *bgColorLabel = new ColorLabel();
    bgColorLabel->setColor (bgColor_);
    colorLayout->addWidget (bgLabel, 0, 0);
    colorLayout->addWidget (bgColorLabel, 0, 1);

    QLabel *fgLabel = new QLabel (tr ("Text color:"));
    ColorLabel *fgColorLabel = new ColorLabel();
    fgColorLabel->setColor (fgColor_);
    colorLayout->addWidget (fgLabel, 1, 0);
    colorLayout->addWidget (fgColorLabel, 1, 1);

    colorLayout->setColumnStretch (1, 1);

    QSpacerItem *spacer = new QSpacerItem (1, 10);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);
    okButton->setDefault (true);

    grid->addWidget (label, 0, 0, 1, 2);
    grid->addLayout (colorLayout, 1, 0, 1, 2);
    grid->addItem (spacer, 2, 0);
    grid->addWidget (cancelButton, 3, 0, Qt::AlignRight);
    grid->addWidget (okButton, 3, 1, Qt::AlignCenter);
    grid->setColumnStretch (0, 1);
    grid->setRowStretch (2, 1);

    dialog->setLayout (grid);

    dialog->open();
    connect (dialog, &QDialog::finished, this, [this, bgColorLabel, fgColorLabel, oldBgColor, oldFgColor] (int res) {
        if (res == QDialog::Accepted)
        {
            bgColor_ = bgColorLabel->getColor();
            fgColor_ = fgColorLabel->getColor();
            if (bgColor_ != oldBgColor || fgColor_ != oldFgColor)
            {
                noteModified();

                /* let the dialog be closed */
                QTimer::singleShot (0, this, [this] () {
                    /* apply the new colors to the existing editors
                    (the colors will be applied to future editors at FN::newWidget) */
                    QHash<DomItem*, TextEdit*>::iterator it;
                    for (it = widgets_.begin(); it != widgets_.end(); ++it)
                        setEditorStyleSheet (it.value());

                    /* remove green highlights and update the yellow ones
                    because their colors may not be suitable now */
                    QHash<TextEdit*,QList<QTextEdit::ExtraSelection> >::iterator iter;
                    for (iter = greenSels_.begin(); iter != greenSels_.end(); ++iter)
                    {
                        QList<QTextEdit::ExtraSelection> extraSelectionsIth;
                        greenSels_[iter.key()] = extraSelectionsIth;
                        iter.key()->setExtraSelections (QList<QTextEdit::ExtraSelection>());
                    }
                    hlight();
                });
            }
        }
    });
}
/*************************/
void FN::noteModified()
{
    if (model_->rowCount() == 0) // if the last node is closed
    {
        ui->actionSave->setEnabled (false);
        setWindowModified (false);
    }
    else
    {
        if (saveNeeded_ == 0)
        {
            ui->actionSave->setEnabled (true);
            setWindowModified (true);
        }
        ++saveNeeded_;
    }
}
/*************************/
void FN::nodeChanged (const QModelIndex&, const QModelIndex&)
{
    noteModified();
    closeWinDialogs(); // in the case of renaming
}
/*************************/
void FN::treeChanged()
{
    updateNodeActions();
    noteModified();
    docProp();
    closeWinDialogs();
}
/*************************/
void FN::showHideSearch()
{
    bool visibility = ui->lineEdit->isVisible();

    if (QObject::sender() == ui->actionFind
        && visibility && !ui->lineEdit->hasFocus())
    {
        ui->lineEdit->setFocus();
        ui->lineEdit->selectAll();
        return;
    }

    ui->lineEdit->setVisible (!visibility);
    ui->nextButton->setVisible (!visibility);
    ui->prevButton->setVisible (!visibility);
    ui->caseButton->setVisible (!visibility);
    ui->wholeButton->setVisible (!visibility);
    ui->everywhereButton->setVisible (!visibility);
    ui->tagsButton->setVisible (!visibility);
    ui->namesButton->setVisible (!visibility);

    if (!visibility)
        ui->lineEdit->setFocus();
    else
    {
        ui->dockReplace->setVisible (false); // no replace dock without searchbar
        if (QWidget *cw = ui->stackedWidget->currentWidget())
        {
            /* return focus to the document */
            qobject_cast<TextEdit*>(cw)->setFocus();
            /* cancel search */
            QHash<TextEdit*,QString>::iterator it;
            for (it = searchEntries_.begin(); it != searchEntries_.end(); ++it)
            {
                ui->lineEdit->setText (QString());
                it.value() = QString();
                disconnect (it.key()->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                disconnect (it.key()->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                disconnect (it.key(), &TextEdit::resized, this, &FN::hlight);
                disconnect (it.key(), &QTextEdit::textChanged, this, &FN::hlight);
                QList<QTextEdit::ExtraSelection> extraSelections;
                greenSels_[it.key()] = extraSelections;
                it.key()->setExtraSelections (extraSelections);
            }
            ui->everywhereButton->setChecked (false);
            ui->tagsButton->setChecked (false);
            ui->namesButton->setChecked (false);
        }
    }
}
/*************************/
void FN::findInNames()
{
    QString txt = ui->lineEdit->text();
    if (txt.isEmpty()) return;

    QModelIndex indx = ui->treeView->currentIndex();
    bool down = true;
    bool found = false;
    if (QObject::sender() == ui->prevButton)
        down = false;
    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (ui->caseButton->isChecked())
        cs = Qt::CaseSensitive;
    QRegularExpression regex;
    if (ui->wholeButton->isChecked())
    {
        if (cs == Qt::CaseInsensitive)
            regex.setPatternOptions (QRegularExpression::CaseInsensitiveOption);
        regex.setPattern (QString ("\\b%1\\b").arg (QRegularExpression::escape (txt)));
        while ((indx = model_->adjacentIndex (indx, down)).isValid())
        {
            if (model_->data (indx, Qt::DisplayRole).toString().indexOf (regex) != -1)
            {
                found = true;
                break;
            }
        }
    }
    else
    {
        while ((indx = model_->adjacentIndex (indx, down)).isValid())
        {
            if (model_->data (indx, Qt::DisplayRole).toString().contains (txt, cs))
            {
                found = true;
                break;
            }
        }
    }

    /* if nothing is found, search again from the first/last index to the current index */
    if (!indx.isValid())
    {
        if (down)
            indx = model_->index (0, 0);
        else
        {
            indx = model_->index (model_->rowCount() - 1, 0);
            while (model_->hasChildren (indx))
                indx = model_->index (model_->rowCount (indx) - 1, 0, indx);
        }
        if (indx == ui->treeView->currentIndex())
            return;

        if (ui->wholeButton->isChecked())
        {
            if (model_->data (indx, Qt::DisplayRole).toString().indexOf (regex) != -1)
                found = true;
            else
            {
                while ((indx = model_->adjacentIndex (indx, down)).isValid())
                {
                    if (indx == ui->treeView->currentIndex())
                        return;
                    if (model_->data (indx, Qt::DisplayRole).toString().indexOf (regex) != -1)
                    {
                        found = true;
                        break;
                    }
                }
            }
        }
        else
        {
            if (model_->data (indx, Qt::DisplayRole).toString().contains (txt, cs))
                found = true;
            else
            {
                while ((indx = model_->adjacentIndex (indx, down)).isValid())
                {
                    if (indx == ui->treeView->currentIndex())
                        return;
                    if (model_->data (indx, Qt::DisplayRole).toString().contains (txt, cs))
                    {
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (found)
        ui->treeView->setCurrentIndex (indx);
}
/*************************/
void FN::clearTagsList (int)
{
    tagsList_.clear();
}
/*************************/
void FN::selectRow (QListWidgetItem *item)
{
    QList<QDialog *> list = findChildren<QDialog*>();
    if (list.isEmpty()) return;
    QList<QListWidget *> list1;
    for (int i = 0; i < list.count(); ++i)
    {
        list1 = list.at (i)->findChildren<QListWidget *>();
        if (!list1.isEmpty()) break;
    }
    if (list1.isEmpty()) return;
    QListWidget *listWidget = list1.at (0);
    ui->treeView->setCurrentIndex (tagsList_.at (listWidget->row (item)));
}
/*************************/
void FN::chooseRow (int row)
{
    ui->treeView->setCurrentIndex (tagsList_.at (row));
}
/*************************/
void FN::findInTags()
{
    QString txt = ui->lineEdit->text();
    if (txt.isEmpty()) return;

    closeWinDialogs();

    QModelIndex nxtIndx = model_->index (0, 0, QModelIndex());
    DomItem *item = static_cast<DomItem*>(nxtIndx.internalPointer());
    QDomNode node = item->node();
    QDomNamedNodeMap attributeMap = node.attributes();
    QString tags = attributeMap.namedItem ("tag").nodeValue();

    while (nxtIndx.isValid())
    {
        item = static_cast<DomItem*>(nxtIndx.internalPointer());
        node = item->node();
        attributeMap = node.attributes();
        tags = attributeMap.namedItem ("tag").nodeValue();
        if (tags.contains (txt, Qt::CaseInsensitive))
            tagsList_.append (nxtIndx);
        nxtIndx = model_->adjacentIndex (nxtIndx, true);
    }

    int matches = tagsList_.count();

    QDialog *TagsDialog = new QDialog (this);
    TagsDialog->setAttribute (Qt::WA_DeleteOnClose, true);
    if (matches > 1)
        TagsDialog->setWindowTitle (tr ("%1 Matches").arg (matches));
    else if (matches == 1)
        TagsDialog->setWindowTitle (tr ("One Match"));
    else
        TagsDialog->setWindowTitle (tr ("No Match"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    QListWidget *listWidget = new QListWidget();
    listWidget->setSelectionMode (QAbstractItemView::SingleSelection);
    connect (listWidget, &QListWidget::itemActivated, this, &FN::selectRow);
    connect (listWidget, &QListWidget::currentRowChanged, this, &FN::chooseRow);
    QPushButton *closeButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Close"));
    connect (closeButton, &QAbstractButton::clicked, TagsDialog, &QDialog::reject);
    connect (TagsDialog, &QDialog::finished, this, &FN::clearTagsList);

    for (int i = 0; i < matches; ++i)
        new QListWidgetItem (model_->data (tagsList_.at (i), Qt::DisplayRole).toString(), listWidget);

    grid->addWidget (listWidget, 0, 0);
    grid->addWidget (closeButton, 1, 0, Qt::AlignRight);

    TagsDialog->setLayout (grid);
    TagsDialog->show();
    /*TagsDialog->move (x() + width()/2 - TagsDialog->width(),
                      y() + height()/2 - TagsDialog->height());*/
    TagsDialog->raise();
    if (!static_cast<FNSingleton*>(qApp)->isWayland())
        TagsDialog->activateWindow();
}
/*************************/
// Returns true if this window has a modal dialog or
// another window has an application-modal dialog.
bool FN::hasBlockingDialog()
{
    const auto dialogs = findChildren<QDialog*>();
    for (const auto &dlg : dialogs)
    {
        if (dlg->isModal())
            return true;
    }
    /* FNSingleton isn't used because it doesn't see a window
       with password prompt, before its file is opened. */
    const auto wins = qApp->topLevelWidgets();
    for (const auto &win : wins)
    {
        if (win->windowModality() == Qt::ApplicationModal)
            return true;
    }
    return false;
}
/*************************/
void FN::closeWarningBar()
{
    const auto warningBars = ui->centralWidget->findChildren<WarningBar*>();
    for (const auto &wb : warningBars)
    {
        if (!wb->isPermanent())
            wb->closeBar();
    }
}
/*************************/
void FN::closeNonModalDialogs()
{
    const auto dialogs = findChildren<QDialog*>();
    for (const auto &dlg : dialogs)
    {
        if (!dlg->isModal())
            dlg->done (QDialog::Rejected);
    }
    /* also close warning bar */
    closeWarningBar();
}
/*************************/
// Closes non-modal dialogs of this window and window-modal dialogs of all windows.

/* WARNING: The reason for closing all window-modal dialogs is an old Qt bug: If a
            window-modal dialog is opened in a window and is closed after another dialog
            is opened in another window, the second dialog will be seen as a child of
            the first window, so that a crash will happen if the dialog is closed after
            closing the first window.

            As a workaround, we can show a warning bar instead of opening a new dialog,
            or close the previous one. With FeatherNotes, the second way is preferable. */

void FN::closeWinDialogs()
{
    closeNonModalDialogs();
    const auto wins = qApp->topLevelWidgets();
    for (const auto &win : wins)
    {
        if (auto dlg = qobject_cast<QDialog*>(win))
        {
            if (dlg->windowModality() == Qt::WindowModal)
                dlg->done (QDialog::Rejected);
        }
    }
}
/*************************/
void FN::scrolled (int) const
{
    hlight();
}
/*************************/
void FN::allBtn (bool checked)
{
    if (checked)
    {
        ui->tagsButton->setChecked (false);
        ui->namesButton->setChecked (false);
    }
}
/*************************/
void FN::tagsAndNamesBtn (bool checked)
{
    int index = ui->stackedWidget->currentIndex();
    if (index == -1) return;

    if (checked)
    {
        /* first clear all search info except the search
           entry's text but don't do redundant operations */
        if (!ui->tagsButton->isChecked() || !ui->namesButton->isChecked())
        {
            QHash<TextEdit*,QString>::iterator it;
            for (it = searchEntries_.begin(); it != searchEntries_.end(); ++it)
            {
                it.value() = QString();
                disconnect (it.key()->verticalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                disconnect (it.key()->horizontalScrollBar(), &QAbstractSlider::valueChanged, this, &FN::scrolled);
                disconnect (it.key(), &TextEdit::resized, this, &FN::hlight);
                disconnect (it.key(), &QTextEdit::textChanged, this, &FN::hlight);
                QList<QTextEdit::ExtraSelection> extraSelections;
                greenSels_[it.key()] = extraSelections;
                it.key()->setExtraSelections (extraSelections);
            }
        }
        else // then uncheck the other radio buttons
        {
            if (QObject::sender() == ui->tagsButton)
                ui->namesButton->setChecked (false);
            else
                ui->tagsButton->setChecked (false);
        }
        ui->everywhereButton->setChecked (false);
    }
    if (QObject::sender() == ui->tagsButton)
    {
        ui->prevButton->setEnabled (!checked);
        ui->wholeButton->setEnabled (!checked);
        ui->caseButton->setEnabled (!checked);
    }
}
/*************************/
void FN::replaceDock()
{
    if (!ui->dockReplace->isVisible())
    {
        if (!ui->lineEdit->isVisible()) // replace dock needs searchbar
        {
            ui->lineEdit->setVisible (true);
            ui->nextButton->setVisible (true);
            ui->prevButton->setVisible (true);
            ui->caseButton->setVisible (true);
            ui->wholeButton->setVisible (true);
            ui->everywhereButton->setVisible (true);
            ui->tagsButton->setVisible (true);
            ui->namesButton->setVisible (true);
        }
        ui->dockReplace->setWindowTitle (tr ("Replacement"));
        ui->dockReplace->setVisible (true);
        ui->dockReplace->setTabOrder (ui->lineEditFind, ui->lineEditReplace);
        ui->dockReplace->setTabOrder (ui->lineEditReplace, ui->rplNextButton);
        ui->dockReplace->raise();
        if (!static_cast<FNSingleton*>(qApp)->isWayland())
            ui->dockReplace->activateWindow();
        if (!ui->lineEditFind->hasFocus())
            ui->lineEditFind->setFocus();
        return;
    }

    ui->dockReplace->setVisible (false);
    // closeReplaceDock(false) is automatically called here
}
/*************************/
// When the dock is closed with its titlebar button,
// clear the replacing text and remove green highlights.
void FN::closeReplaceDock (bool visible)
{
    if (visible || !isVisible() || isMinimized()) return;

    txtReplace_.clear();
    /* remove green highlights */
    QHash<TextEdit*,QList<QTextEdit::ExtraSelection> >::iterator it;
    for (it = greenSels_.begin(); it != greenSels_.end(); ++it)
    {
        QList<QTextEdit::ExtraSelection> extraSelectionsIth;
        greenSels_[it.key()] = extraSelectionsIth;
        it.key()->setExtraSelections (QList<QTextEdit::ExtraSelection>());
    }
    hlight();

    /* return focus to the document */
    if (QWidget *cw = ui->stackedWidget->currentWidget())
        cw->setFocus();
}
/*************************/
// Resize the floating dock widget to its minimum size.
void FN::resizeDock (bool topLevel)
{
    if (topLevel)
        ui->dockReplace->resize (ui->dockReplace->minimumWidth(),
                                 ui->dockReplace->minimumHeight());
}
/*************************/
void FN::replace()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);

    ui->dockReplace->setWindowTitle (tr ("Replacement"));

    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;

    if (txtReplace_ != ui->lineEditReplace->text())
    {
        txtReplace_ = ui->lineEditReplace->text();
        /* remove previous green highlights if the replacing text is changed */
        QHash<TextEdit*,QList<QTextEdit::ExtraSelection> >::iterator it;
        for (it = greenSels_.begin(); it != greenSels_.end(); ++it)
        {
            QList<QTextEdit::ExtraSelection> extraSelectionsIth;
            greenSels_[it.key()] = extraSelectionsIth;
            it.key()->setExtraSelections (QList<QTextEdit::ExtraSelection>());
        }
    }

    bool backwardSearch = false;
    QTextCursor start = textEdit->textCursor();
    if (QObject::sender() == ui->rplNextButton)
    {
        if (rplOtherNode_)
            start.movePosition (QTextCursor::Start, QTextCursor::MoveAnchor);
    }
    else// if (QObject::sender() == ui->rplPrevButton)
    {
        backwardSearch = true;
        if (rplOtherNode_)
            start.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
    }

    QTextCursor found;
    if (!backwardSearch)
        found = finding (txtFind, start, searchFlags_);
    else
        found = finding (txtFind, start, searchFlags_ | QTextDocument::FindBackward);

    QList<QTextEdit::ExtraSelection> extraSelections = greenSels_[textEdit];
    QModelIndex nxtIndx;
    if (found.isNull())
    {
        if (ui->everywhereButton->isChecked())
        {
            nxtIndx = ui->treeView->currentIndex();
            Qt::CaseSensitivity cs = Qt::CaseInsensitive;
            if (ui->caseButton->isChecked()) cs = Qt::CaseSensitive;
            QString text;
            while (!text.contains (txtFind, cs))
            {
                nxtIndx = model_->adjacentIndex (nxtIndx, !backwardSearch);
                if (!nxtIndx.isValid()) break;
                DomItem *item = static_cast<DomItem*>(nxtIndx.internalPointer());
                if (TextEdit *thisTextEdit = widgets_.value (item))
                    text = thisTextEdit->toPlainText(); // the node text may have been edited
                else
                {
                    QDomNodeList list = item->node().childNodes();
                    text = list.item (0).nodeValue();
                }
            }
        }
        rplOtherNode_ = false;
    }
    else
    {
        QColor green = qGray(fgColor_.rgb()) > 127 ? QColor (Qt::darkGreen) : QColor (Qt::green);
        int pos;
        QTextCursor tmp = start;

        start.setPosition (found.anchor());
        pos = found.anchor();
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        textEdit->setTextCursor (start);
        textEdit->insertPlainText (txtReplace_);

        if (rplOtherNode_)
        {
            /* the value of this boolean should be set to false but, in addition,
               we shake the splitter as a workaround for what seems to be a bug
               that makes ending parts of texts disappear after a text insertion */
            QList<int> sizes = ui->splitter->sizes();
            QList<int> newSizes; newSizes << sizes.first() + 1 << sizes.last() - 1;
            ui->splitter->setSizes (newSizes);
            ui->splitter->setSizes (sizes);
            rplOtherNode_ = false;
        }

        start = textEdit->textCursor(); // at the end of txtReplace_
        tmp.setPosition (pos);
        tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection extra;
        extra.format.setBackground (green);
        extra.format.setUnderlineStyle (QTextCharFormat::WaveUnderline);
        extra.format.setUnderlineColor (fgColor_);
        extra.cursor = tmp;
        extraSelections.append (extra);

        if (QObject::sender() != ui->rplNextButton)
        {
            /* With the cursor at the end of the replacing text, if the backward replacement
               is repeated and the text is matched again (which is possible when the replacing
               and replaced texts are the same, case-insensitively), the replacement won't proceed.
               So, the cursor should be moved. */
            start.setPosition (start.position() - txtReplace_.length());
            textEdit->setTextCursor (start);
        }
    }

    greenSels_[textEdit] = extraSelections;
    textEdit->setExtraSelections (extraSelections);
    hlight();

    if (nxtIndx.isValid())
    {
        rplOtherNode_ = true;
        ui->treeView->setCurrentIndex (nxtIndx);
        replace();
    }
}
/*************************/
void FN::replaceAll()
{
    QString txtFind = ui->lineEditFind->text();
    if (txtFind.isEmpty()) return;

    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (ui->caseButton->isChecked()) cs = Qt::CaseSensitive;

    QModelIndex nxtIndx;
    /* start with the first node when replacing everywhere */
    if (!rplOtherNode_ && ui->everywhereButton->isChecked())
    {
        nxtIndx = model_->index (0, 0);
        DomItem *item = static_cast<DomItem*>(nxtIndx.internalPointer());
        QString text;
        if (TextEdit *thisTextEdit = widgets_.value (item))
            text = thisTextEdit->toPlainText(); // the node text may have been edited
        else
        {
            QDomNodeList list = item->node().childNodes();
            text = list.item (0).nodeValue();
        }
        while (!text.contains (txtFind, cs))
        {
            nxtIndx = model_->adjacentIndex (nxtIndx, true);
            if (!nxtIndx.isValid())
            {
                ui->dockReplace->setWindowTitle (tr ("No Match"));
                return;
            }
            item = static_cast<DomItem*>(nxtIndx.internalPointer());
            if (TextEdit *thisTextEdit = widgets_.value (item))
                text = thisTextEdit->toPlainText();
            else
            {
                QDomNodeList list = item->node().childNodes();
                text = list.item (0).nodeValue();
            }
        }
        rplOtherNode_ = true;
        ui->treeView->setCurrentIndex (nxtIndx);
        nxtIndx = QModelIndex();
    }

    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;
    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);

    if (txtReplace_ != ui->lineEditReplace->text())
    {
        txtReplace_ = ui->lineEditReplace->text();
        QHash<TextEdit*,QList<QTextEdit::ExtraSelection> >::iterator it;
        for (it = greenSels_.begin(); it != greenSels_.end(); ++it)
        {
            QList<QTextEdit::ExtraSelection> extraSelectionsIth;
            greenSels_[it.key()] = extraSelectionsIth;
            it.key()->setExtraSelections (QList<QTextEdit::ExtraSelection>());
        }
    }

    QTextCursor orig = textEdit->textCursor();
    orig.setPosition (orig.anchor());
    textEdit->setTextCursor (orig);
    QColor green = qGray(fgColor_.rgb()) > 127 ? QColor (Qt::darkGreen) : QColor (Qt::green);
    int pos; QTextCursor found;
    QTextCursor start = orig;
    start.beginEditBlock();
    start.setPosition (0);
    QTextCursor tmp = start;
    QTextEdit::ExtraSelection extra;
    extra.format.setBackground (green);
    extra.format.setUnderlineStyle (QTextCharFormat::WaveUnderline);
    extra.format.setUnderlineColor (fgColor_);
    QList<QTextEdit::ExtraSelection> extraSelections = greenSels_[textEdit];
    while (!(found = finding (txtFind, start, searchFlags_)).isNull())
    {
        start.setPosition (found.anchor());
        pos = found.anchor();
        start.setPosition (found.position(), QTextCursor::KeepAnchor);
        start.insertText (txtReplace_);

        if (rplOtherNode_)
        {
            QList<int> sizes = ui->splitter->sizes();
            QList<int> newSizes; newSizes << sizes.first() + 1 << sizes.last() - 1;
            ui->splitter->setSizes (newSizes);
            ui->splitter->setSizes (sizes);
            rplOtherNode_ = false;
        }

        if (replCount_ < 1000)
        {
            tmp.setPosition (pos);
            tmp.setPosition (start.position(), QTextCursor::KeepAnchor);
            extra.cursor = tmp;
            extraSelections.append (extra);
        }
        start.setPosition (start.position());
        ++replCount_;
    }
    rplOtherNode_ = false;
    greenSels_[textEdit] = extraSelections;
    start.endEditBlock();
    textEdit->setExtraSelections (extraSelections);
    hlight();

    if (ui->everywhereButton->isChecked())
    {
        nxtIndx = ui->treeView->currentIndex();
        QString text;
        while (!text.contains (txtFind, cs))
        {
            nxtIndx = model_->adjacentIndex (nxtIndx, true);
            if (!nxtIndx.isValid()) break;
            DomItem *item = static_cast<DomItem*>(nxtIndx.internalPointer());
            if (TextEdit *thisTextEdit = widgets_.value (item))
                text = thisTextEdit->toPlainText();
            else
            {
                QDomNodeList list = item->node().childNodes();
                text = list.item (0).nodeValue();
            }
        }
    }

    if (nxtIndx.isValid())
    {
        rplOtherNode_ = true;
        ui->treeView->setCurrentIndex (nxtIndx);
        replaceAll();
    }
    else
    {
        if (replCount_ == 0)
            ui->dockReplace->setWindowTitle (tr ("No Replacement"));
        else if (replCount_ == 1)
            ui->dockReplace->setWindowTitle (tr ("One Replacement"));
        else
        {
            ui->dockReplace->setWindowTitle (tr ("%1 Replacements").arg (replCount_));
            if (replCount_ > 1000 && !txtReplace_.isEmpty())
            {
                closeWinDialogs();
                showWarningBar ("<center><b><big>"
                                 + tr ("The first 1000 replacements are highlighted.")
                                 + "</big></b></center>", 10);
            }
        }
        replCount_ = 0;
    }
}
/*************************/
void FN::showEvent (QShowEvent *event)
{
    /* To position the main window correctly when it's shown for
       the first time, we call setGeometry() inside showEvent(). */
    if (!shownBefore_ && !event->spontaneous())
    {
        shownBefore_ = true;
        if (remPosition_ && !static_cast<FNSingleton*>(qApp)->isWayland())
        {
            QSize theSize = (remSize_ ? winSize_ : startSize_);
            setGeometry (position_.x(), position_.y(),
                         theSize.width(), theSize.height());
            if (isFull_ && isMaxed_)
                setWindowState (Qt::WindowMaximized | Qt::WindowFullScreen);
            else if (isMaxed_)
                setWindowState (Qt::WindowMaximized);
            else if (isFull_)
                setWindowState (Qt::WindowFullScreen);
        }
    }
    QWidget::showEvent (event);
}
/*************************/
void FN::showAndFocus()
{
    show();
    raise();
    if (QWidget *cw = ui->stackedWidget->currentWidget())
        cw->setFocus();
    if (!static_cast<FNSingleton*>(qApp)->isWayland())
    { // to bypass focus stealing prevention
        activateWindow();
        QTimer::singleShot (0, this, [this] {
            if (QWindow *win = windowHandle())
                win->requestActivate();
        });
    }
}
/*************************/
void FN::activateFNWindow (bool noDelay)
{
    FNSingleton *singleton = static_cast<FNSingleton*>(qApp);
    if (noDelay || !findChildren<QDialog*>().isEmpty() || hasBlockingDialog())
    {
        showNormal();
        raise();
        if (!singleton->isWayland())
            activateWindow();
        return;
    }

    if (!isVisible())
    {
        show();
#ifdef HAS_X11
        if (singleton->isX11() && onWhichDesktop (winId()) != fromDesktop())
        {
            closeNonModalDialogs();
            moveToCurrentDesktop (winId());
        }
#endif
        showAndFocus();
    }
#ifdef HAS_X11
    else if (singleton->isX11() && onWhichDesktop (winId()) != fromDesktop())
    {
        closeNonModalDialogs();
        moveToCurrentDesktop (winId());
        if (isMinimized())
            showNormal();
        showAndFocus();
    }
#endif
    else if (!isActiveWindow())
    {
#ifdef HAS_X11
        if (singleton->isX11())
        {
            if (isMinimized())
                showNormal();
            showAndFocus();
        }
        else
#endif
        {
            closeNonModalDialogs();
            hide();
            QTimer::singleShot (0, this, &FN::showAndFocus);
        }
    }
}
/*************************/
void FN::trayActivated (QSystemTrayIcon::ActivationReason r)
{
    if (!tray_) return;
    if (r != QSystemTrayIcon::Trigger) return;

    FNSingleton *singleton = static_cast<FNSingleton*>(qApp);

    if (hasBlockingDialog())
    { // don't respond to the tray icon when there's a blocking dialog
        activateFNWindow (true);
        return;
    }

    if (!isVisible())
    {
        show();

#ifdef HAS_X11
        if (singleton->isX11() && onWhichDesktop (winId()) != fromDesktop())
        {
            closeNonModalDialogs();
            moveToCurrentDesktop (winId());
        }
#endif
        showAndFocus();
    }
#ifdef HAS_X11
    else if (!singleton->isX11())
    {
        if (!isActiveWindow())
        {
            closeNonModalDialogs();
            hide();
            QTimer::singleShot (0, this, &FN::showAndFocus);
        }
        else
        {
            closeNonModalDialogs();
            if (!isMaximized() && !isFullScreen() && !singleton->isWayland())
            {
                position_.setX (geometry().x());
                position_.setY (geometry().y());
            }
            QTimer::singleShot (0, this, &QWidget::hide);
        }
    }
    else if (onWhichDesktop (winId()) == fromDesktop())
    { // under X11 and on this desktop
        QRect sr;
        if (QWindow *win = windowHandle())
        {
            if (QScreen *sc = win->screen())
                sr = sc->virtualGeometry();
        }
        if (sr.isNull())
        {
            if (QScreen *pScreen = QApplication::primaryScreen())
                sr = pScreen->virtualGeometry();
        }
        QRect g = geometry();
        if (g.x() >= sr.left() && g.x() + g.width() <= sr.left() + sr.width()
            && g.y() >= sr.top() && g.y() + g.height() <= sr.top() + sr.height())
        { // inside the screen borders
            if (isActiveWindow())
            {
                if (!isMaximized() && !isFullScreen())
                {
                    position_.setX (g.x());
                    position_.setY (g.y());
                }
                closeNonModalDialogs();
                QTimer::singleShot (0, this, &QWidget::hide);
            }
            else
            {
                if (isMinimized())
                    showNormal();
                showAndFocus();
            }
        }
        else
        { // partially offscreen
            closeNonModalDialogs();
            hide();
            if (!singleton->isWayland())
                setGeometry (position_.x(), position_.y(), g.width(), g.height());
            QTimer::singleShot (0, this, &FN::showAndFocus);
        }
    }
    else
    { // under X11, visible and on another desktop
        closeNonModalDialogs();
        moveToCurrentDesktop (winId());
        if (isMinimized())
            showNormal();
        showAndFocus();
    }
#else
    // visible and without X11
    else if (!isActiveWindow())
    {
        closeNonModalDialogs();
        hide();
        QTimer::singleShot (0, this, &FN::showAndFocus);
    }
    else
    {
        closeNonModalDialogs();
        if (!isMaximized() && !isFullScreen() && !singleton->isWayland())
        {
            position_.setX (geometry().x());
            position_.setY (geometry().y());
        }
        QTimer::singleShot (0, this, &QWidget::hide);
    }
#endif
}
/*************************/
void FN::activateTray()
{
    if (hasBlockingDialog())
    { // don't respond to the tray icon when there's a modal dialog
        activateFNWindow (true);
        return;
    }

    trayActivated (QSystemTrayIcon::Trigger);
}
/*************************/
void FN::insertLink()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (!cur.hasSelection()) return;

    closeWinDialogs();

    /* only if the position is after the anchor,
       the format will be detected correctly */
    int pos, anch;
    if ((pos = cur.position()) < (anch = cur.anchor()))
    {
        cur.setPosition (pos);
        cur.setPosition (anch, QTextCursor::KeepAnchor);
    }
    QTextCharFormat format = cur.charFormat();
    QString href = format.anchorHref();

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Insert Link"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    /* needed widgets */
    LineEdit *linkEntry = new LineEdit();
    linkEntry->returnOnClear = false;
    linkEntry->setMinimumWidth (250);
    linkEntry->setText (href);
    connect (linkEntry, &QLineEdit::returnPressed, dialog, &QDialog::accept);
    QSpacerItem *spacer = new QSpacerItem (1, 5);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (okButton, &QAbstractButton::clicked, dialog,  &QDialog::accept);

    /* fit in the grid */
    grid->addWidget (linkEntry, 0, 0, 1, 3);
    grid->addItem (spacer, 1, 0);
    grid->addWidget (cancelButton, 2, 1, Qt::AlignRight);
    grid->addWidget (okButton, 2, 2, Qt::AlignRight);
    grid->setColumnStretch (0, 1);
    grid->setRowStretch (1, 1);

    dialog->setLayout (grid);

    /* show the dialog */
    dialog->open();
    connect (dialog, &QDialog::finished, textEdit, [this, textEdit, linkEntry] (int res) {
        if (res == QDialog::Accepted)
        {
            /* repeated calculations */
            QTextCursor cur = textEdit->textCursor();
            int pos, anch;
            QTextCursor cursor = cur;
            if ((pos = cur.position()) < (anch = cur.anchor()))
            {
                cursor.setPosition (pos);
                cursor.setPosition (anch, QTextCursor::KeepAnchor);
            }
            QTextCharFormat format = cursor.charFormat();

            QString address = linkEntry->text();
            if (!address.isEmpty())
            {
                format.setAnchor (true);
                format.setFontUnderline (true);
                format.setFontItalic (true);
                format.setForeground (qGray (bgColor_.rgb()) > 127 ? QColor (Qt::blue) : QColor (85, 227, 255));
            }
            else
            {
                format.setAnchor (false);
                format.setFontUnderline (false);
                format.setFontItalic (false);
                format.setForeground (QBrush());
            }
            format.setAnchorHref (address);
            cur.mergeCharFormat (format);
        }
    });
}
/*************************/
void FN::embedImage()
{
    int index = ui->stackedWidget->currentIndex();
    if (index == -1) return;

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Embed Image"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    /* create the needed widgets */
    imagePathEntry_ = new LineEdit();
    imagePathEntry_->returnOnClear = false;
    imagePathEntry_->setMinimumWidth (200);
    imagePathEntry_->setToolTip (tr ("Image path"));
    connect (imagePathEntry_, &QLineEdit::returnPressed, dialog, &QDialog::accept);
    QToolButton *openBtn = new QToolButton();
    openBtn->setIcon (symbolicIcon::icon (":icons/document-open.svg"));
    openBtn->setToolTip (tr ("Open image"));
    connect (openBtn, &QAbstractButton::clicked, this, &FN::setImagePath);
    QLabel *label = new QLabel();
    label->setText (tr ("Scale to"));
    SpinBox *spinBox = new SpinBox();
    spinBox->setRange (1, 200);
    spinBox->setValue (imgScale_);
    spinBox->setSuffix (tr ("%"));
    spinBox->setToolTip (tr ("Scaling percentage"));
    connect (spinBox, &SpinBox::returnPressed, dialog, &QDialog::accept);
    QSpacerItem *spacer = new QSpacerItem (1, 10);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);

    /* make the widgets fit in the grid */
    grid->addWidget (imagePathEntry_, 0, 0, 1, 4);
    grid->addWidget (openBtn, 0, 4, Qt::AlignCenter);
    grid->addWidget (label, 1, 0, Qt::AlignRight);
    grid->addWidget (spinBox, 1, 1, Qt::AlignLeft);
    grid->addItem (spacer, 2, 0);
    grid->addWidget (cancelButton, 3, 2, Qt::AlignRight);
    grid->addWidget (okButton, 3, 3, 1, 2, Qt::AlignCenter);
    grid->setColumnStretch (1, 1);
    grid->setRowStretch (2, 1);

    dialog->setLayout (grid);

    dialog->open();
    connect (dialog, &QDialog::finished, this, [this, spinBox] (int res) {
        if (imagePathEntry_ != nullptr)
            lastImgPath_ = imagePathEntry_->text();
        if (res == QDialog::Accepted)
        {
            imgScale_ = spinBox->value();
            imageEmbed (lastImgPath_);
        }
    });
}
/*************************/
void FN::imageEmbed (const QString &path)
{
    if (path.isEmpty()) return;

    QImage img = QImage (path);
    if (img.isNull()) return;
    QSize imgSize = img.size();
    if (imgSize.isEmpty()) return;

    QFile file (path);
    if (!file.open (QIODevice::ReadOnly))
        return;
    /* read the data serialized from the file */
    QDataStream in (&file);
    QByteArray rawarray;
    QDataStream datastream (&rawarray, QIODevice::WriteOnly);
    char a;
    /* copy between the two data streams */
    while (in.readRawData (&a, 1) != 0)
        datastream.writeRawData (&a, 1);
    file.close();
    QByteArray base64array = rawarray.toBase64();

    int w, h;
    if (qobject_cast<QDialog*>(QObject::sender())) // from embedImage()
    {
        w = imgSize.width() * imgScale_ / 100;
        h = imgSize.height() * imgScale_ / 100;
    }
    else
    {
        w = imgSize.width();
        h = imgSize.height();
    }
    TextEdit *textEdit = qobject_cast<TextEdit*>(ui->stackedWidget->currentWidget());
    //QString ("<img src=\"data:image/png;base64,%1\">")
    textEdit->insertHtml (QString ("<img src=\"data:image;base64,%1\" width=\"%2\" height=\"%3\" />")
                          .arg (QString (base64array))
                          .arg (w)
                          .arg (h));

    raise();
    if (!static_cast<FNSingleton*>(qApp)->isWayland())
        activateWindow();
}
/*************************/
void FN::setImagePath (bool)
{
    QString path;
    if (!lastImgPath_.isEmpty())
    {
        if (QFile::exists (lastImgPath_))
            path = lastImgPath_;
        else
        {
            QDir dir = QFileInfo (lastImgPath_).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
    }
    else
        path = QDir::home().path();

    QString imagePath;
    FileDialog dialog (this);
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (tr ("Open Image..."));
    dialog.setFileMode (QFileDialog::ExistingFiles);
    dialog.setNameFilter (tr ("Image Files") + " (*.svg *.png *.jpg *.jpeg *.bmp *.gif);;" + tr ("All Files") + " (*)");
    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else
    {
        dialog.setDirectory (path.section ("/", 0, -2)); // workaround for KDE
        dialog.selectFile (path);
        dialog.autoScroll();
    }
    if (dialog.exec())
    {
        QStringList files = dialog.selectedFiles();
        if (files.count() > 0)
            imagePath = files.at (0);
    }

    if (!imagePath.isEmpty() && imagePathEntry_ != nullptr)
        imagePathEntry_->setText (imagePath);
}
/*************************/
bool FN::isImageSelected()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return false;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (!cur.hasSelection()) return false;

    QTextDocumentFragment docFrag = cur.selection();
    QString txt = docFrag.toHtml();
    if (txt.contains (QRegularExpression (EMBEDDED_IMG)))
        return true;

    return false;
}
/*************************/
void FN::scaleImage()
{
    TextEdit *textEdit = qobject_cast<TextEdit*>(ui->stackedWidget->currentWidget());
    QTextCursor cur = textEdit->textCursor();
    QTextDocumentFragment docFrag = cur.selection();
    if (docFrag.isEmpty()) return;
    QString txt = docFrag.toHtml();

    QRegularExpression imageExp (R"((?<=\s)src\s*=\s*"data:[^<>]*;base64\s*,[a-zA-Z0-9+=/\s]+)");
    QRegularExpressionMatch match;
    QSize imageSize;
    int W = 0, H = 0;

    int startIndex = txt.indexOf (EMBEDDED_IMG, 0, &match);
    if (startIndex == -1) return;

    QString str = txt.mid (startIndex, match.capturedLength());
    int indx = str.lastIndexOf (imageExp, -1, &match);
    QString imgStr = str.mid (indx, match.capturedLength());
    imgStr.remove (QRegularExpression (R"(src\s*=\s*"data:[^<>]*;base64\s*,)"));
    QImage image;
    if (image.loadFromData (QByteArray::fromBase64 (imgStr.toUtf8())))
        imageSize = image.size();
    if (imageSize.isEmpty()) return;

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Scale Image(s)"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    QLabel *label = new QLabel();
    label->setText (tr ("Scale to"));
    SpinBox *spinBox = new SpinBox();
    spinBox->setRange (1, 200);
    spinBox->setSuffix (tr ("%"));
    spinBox->setToolTip (tr ("Scaling percentage"));
    connect (spinBox, &SpinBox::returnPressed, dialog, &QDialog::accept);
    QSpacerItem *spacer = new QSpacerItem (1, 10);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);

    grid->addWidget (label, 0, 0, Qt::AlignRight);
    grid->addWidget (spinBox, 0, 1, 1, 2, Qt::AlignLeft);
    grid->addItem (spacer, 1, 0);
    grid->addWidget (cancelButton, 2, 1, 1, 2, Qt::AlignRight);
    grid->addWidget (okButton, 2, 3, Qt::AlignCenter);
    grid->setColumnStretch (1, 1);
    grid->setRowStretch (1, 1);

    int scale = 100;

    /* first, check the (last) width */
    if ((indx = str.lastIndexOf (QRegularExpression (R"(width\s*=\s*"\s*(\+|-){0,1}[0-9]+\s*")"), -1, &match)) != -1)
    {
        bool ok;
        str = str.mid (indx, match.capturedLength());
        str.remove (QRegularExpression (R"(width\s*=\s*"\s*)"));
        str.remove (QRegularExpression (R"(\s*")"));
        W = str.toInt(&ok);
        if (!ok) W = 0;
        W = qMax (W, 0);
        scale = 100 * W / imageSize.width();
    }
    /* if there's no width, check the (last) height */
    else if ((indx = str.lastIndexOf (QRegularExpression (R"(height\s*=\s*"\s*(\+|-){0,1}[0-9]+\s*")"), -1, &match)) != -1)
    {
        bool ok;
        str = str.mid (indx, match.capturedLength());
        str.remove (QRegularExpression (R"(height\s*=\s*"\s*)"));
        str.remove (QRegularExpression (R"(\s*")"));
        H = str.toInt(&ok);
        if (!ok) H = 0;
        H = qMax (H, 0);
        scale = 100 * H / imageSize.height();
    }

    spinBox->setValue (scale);

    dialog->setLayout (grid);

    dialog->open();
    connect (dialog, &QDialog::finished, textEdit,
             [spinBox, textEdit, imageExp, txt, imgStr, startIndex, imageSize] (int res) {
        if (res == QDialog::Accepted)
        {
            int scale = spinBox->value();

            int startIndx = startIndex;
            QSize imgSize = imageSize;
            QString text = txt;
            QString imgString = imgStr;

            QRegularExpressionMatch match;
            int indx;
            while ((indx = text.indexOf (EMBEDDED_IMG, startIndx, &match)) != -1)
            {
                QString str = text.mid (indx, match.capturedLength());

                if (imgSize.isEmpty()) // already calculated for the first image
                {
                    QRegularExpressionMatch imageMatch;
                    int pos = str.lastIndexOf (imageExp, -1, &imageMatch);

                    if (pos == -1)
                    {
                        startIndx = indx + match.capturedLength();
                        continue;
                    }
                    imgString = str.mid (pos, imageMatch.capturedLength());
                    imgString.remove (QRegularExpression (R"(src\s*=\s*"data:[^<>]*;base64\s*,)"));
                    QImage image;
                    if (!image.loadFromData (QByteArray::fromBase64 (imgString.toUtf8())))
                    {
                        startIndx = indx + match.capturedLength();
                        continue;
                    }
                    imgSize = image.size();
                    if (imgSize.isEmpty()) return;
                }

                int W = imgSize.width() * scale / 100;
                int H = imgSize.height() * scale / 100;
                text.replace (indx, match.capturedLength(), "<img src=\"data:image;base64," + imgString + QString ("\" width=\"%1\" height=\"%2\">").arg (W).arg (H));
                imgSize = QSize(); // for the next image

                /* since the text is changed, startIndex should be found again */
                indx = text.indexOf (EMBEDDED_IMG, startIndx, &match);
                startIndx = indx + match.capturedLength();
            }
            QTextCursor cur = textEdit->textCursor();
            cur.insertHtml (text);
        }
    });
}
/*************************/
void FN::saveImage()
{
    TextEdit *textEdit = qobject_cast<TextEdit*>(ui->stackedWidget->currentWidget());
    QTextCursor cur = textEdit->textCursor();
    QTextDocumentFragment docFrag = cur.selection();
    if (docFrag.isEmpty()) return;
    QString txt = docFrag.toHtml();

    QString path;
    if (!xmlPath_.isEmpty())
    {
        QDir dir = QFileInfo (xmlPath_).absoluteDir();
        if (!dir.exists())
            dir = QDir::home();
        path = dir.path();

        QString shownName = QFileInfo (xmlPath_).fileName();
        if (shownName.endsWith (".fnx"))
            shownName.chop (4);
        path += "/" + shownName;
    }
    else
    {
        QDir dir = QDir::home();
        path = dir.path();
        path += "/" + tr ("untitled");
    }

    QRegularExpression imageExp (R"((?<=\s)src\s*=\s*"data:[^<>]*;base64\s*,[a-zA-Z0-9+=/\s]+)");
    int indx;
    int startIndex = 0;
    int n = 1;
    QString extension = "png";
    QRegularExpressionMatch match;
    while ((indx = txt.indexOf (EMBEDDED_IMG, startIndex, &match)) != -1)
    {
        QString str = txt.mid (indx, match.capturedLength());
        startIndex = indx + match.capturedLength();

        indx = str.lastIndexOf (imageExp, -1, &match);

        if (indx == -1) continue;
        str = str.mid (indx, match.capturedLength());
        str.remove (QRegularExpression (R"(src\s*=\s*"data:[^<>]*;base64\s*,)"));
        QImage image;
        if (!image.loadFromData (QByteArray::fromBase64 (str.toUtf8())))
            continue;

        bool retry (true);
        bool err (false);
        while (retry)
        {
            if (err)
            {
                closeWinDialogs();
                MessageBox msgBox;
                msgBox.setIcon (QMessageBox::Question);
                msgBox.setWindowTitle (tr ("Error"));
                msgBox.setText (tr ("<center><b><big>Image cannot be saved! Retry?</big></b></center>"));
                msgBox.setInformativeText (tr ("<center>Maybe you did not choose a proper extension</center>\n"\
                                               "<center>or do not have write permission.</center><p></p>"));
                msgBox.setStandardButtons (QMessageBox::Yes | QMessageBox::No);
                msgBox.changeButtonText (QMessageBox::Yes, tr ("Yes"));
                msgBox.changeButtonText (QMessageBox::No, tr ("No"));
                msgBox.setDefaultButton (QMessageBox::No);
                msgBox.setParent (this, Qt::Dialog);
                msgBox.setWindowModality (Qt::WindowModal);
                msgBox.show();
                if (!static_cast<FNSingleton*>(qApp)->isWayland())
                {
                    msgBox.move (x() + width()/2 - msgBox.width()/2,
                                 y() + height()/2 - msgBox.height()/ 2);
                }
                switch (msgBox.exec())
                {
                case QMessageBox::Yes:
                    break;
                case QMessageBox::No:
                default:
                    retry = false; // next image without saving this one
                    break;
                }
            }

            if (retry)
            {
                closeWinDialogs();
                QString fname;
                FileDialog dialog (this);
                dialog.setWindowModality (Qt::WindowModal);
                dialog.setAcceptMode (QFileDialog::AcceptSave);
                dialog.setWindowTitle (tr ("Save Image As..."));
                dialog.setFileMode (QFileDialog::AnyFile);
                dialog.setNameFilter (tr ("Image Files") + " (*.png *.jpg *.jpeg *.bmp);;" + tr ("All Files") + " (*)");
                dialog.setDirectory (path.section ("/", 0, -2)); // workaround for KDE
                dialog.selectFile (QString ("%1-%2.%3").arg (path).arg (n).arg (extension));
                dialog.autoScroll();
                if (dialog.exec())
                {
                    fname = dialog.selectedFiles().at (0);
                    if (fname.isEmpty() || QFileInfo (fname).isDir())
                    {
                        err = true;
                        continue;
                    }
                }
                else return;

                if (image.save (fname))
                {
                    lastImgPath_ = fname;
                    QFileInfo info = QFileInfo (lastImgPath_);
                    QString shownName = info.fileName();
                    extension = shownName.split (".").last();
                    shownName.chop (extension.size() + 1);
                    /* if the name ends with a number following a dash,
                       use it; otherwise, increase the number by one */
                    int m = 0;
                    QRegularExpression exp ("-[1-9]+[0-9]*");
                    indx = shownName.lastIndexOf (QRegularExpression ("-[1-9]+[0-9]*"), -1, &match);
                    if (indx > -1 && indx == shownName.size() - match.capturedLength())
                    {
                        QString number = shownName.split ("-").last();
                        shownName.chop (number.size() + 1);
                        m = number.toInt() + 1;
                    }
                    n = m > n ? m : n + 1;
                    path = info.dir().path() + "/" + shownName;
                    retry = false; // next image after saving this one
                }
                else
                    err = true;
            }
        }
    }
}
/*************************/
void FN::addTable()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle (tr ("Insert Table"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    QLabel *labelRow = new QLabel();
    labelRow->setText (tr ("Rows:"));
    SpinBox *spinBoxRow = new SpinBox();
    spinBoxRow->setRange (1, 100);
    spinBoxRow->setValue (1);
    connect (spinBoxRow, &SpinBox::returnPressed, dialog, &QDialog::accept);
    QLabel *labelCol = new QLabel();
    labelCol->setText (tr ("Columns:"));
    SpinBox *spinBoxCol = new SpinBox();
    spinBoxCol->setRange (1, 100);
    spinBoxCol->setValue (1);
    connect (spinBoxCol, &SpinBox::returnPressed, dialog, &QDialog::accept);
    QSpacerItem *spacer = new QSpacerItem (1, 10);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);

    grid->addWidget (labelRow, 0, 0, Qt::AlignRight);
    grid->addWidget (spinBoxRow, 0, 1, 1, 2);
    grid->addWidget (labelCol, 1, 0, Qt::AlignRight);
    grid->addWidget (spinBoxCol, 1, 1, 1, 2);
    grid->addItem (spacer, 2, 0);
    grid->addWidget (cancelButton, 3, 0, 1, 2, Qt::AlignRight);
    grid->addWidget (okButton, 3, 2, Qt::AlignLeft);
    grid->setColumnStretch (1, 2);
    grid->setRowStretch (2, 1);

    dialog->setLayout (grid);
    dialog->resize (dialog->size().expandedTo (QSize (300, 0)));

    dialog->open();
    connect (dialog, &QDialog::finished, cw, [this, cw, spinBoxRow, spinBoxCol] (int res) {
        if (res == QDialog::Accepted)
        {
            int rows = spinBoxRow->value();
            int columns = spinBoxCol->value();
            TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
            QTextCursor cur = textEdit->textCursor();
            QTextTableFormat tf;
            tf.setCellPadding (3);
            QTextTable *table = cur.insertTable (rows, columns);
            table->setFormat (tf);
        }
    });
}
/*************************/
void FN::tableMergeCells()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
        table->mergeCells (cur);
}
/*************************/
void FN::tablePrependRow()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->insertRows (tableCell.row(), 1);
    }
}
/*************************/
void FN::tableAppendRow()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->insertRows (tableCell.row() + 1, 1);
    }
}
/*************************/
void FN::tablePrependCol()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->insertColumns (tableCell.column(), 1);
    }
}
/*************************/
void FN::tableAppendCol()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->insertColumns (tableCell.column() + 1, 1);
    }
}
/*************************/
void FN::tableDeleteRow()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->removeRows (tableCell.row(), 1);
    }
}
/*************************/
void FN::tableDeleteCol()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    if (QTextTable *table = cur.currentTable())
    {
        QTextTableCell tableCell = table->cellAt (cur);
        table->removeColumns (tableCell.column(), 1);
    }
}
/*************************/
void FN::toggleWrapping()
{
    int count = ui->stackedWidget->count();
    if (count == 0) return;

    if (ui->actionWrap->isChecked())
    {
        for (int i = 0; i < count; ++i)
            qobject_cast<TextEdit*>(ui->stackedWidget->widget (i))->setLineWrapMode (QTextEdit::WidgetWidth);
    }
    else
    {
        for (int i = 0; i < count; ++i)
            qobject_cast<TextEdit*>(ui->stackedWidget->widget (i))->setLineWrapMode (QTextEdit::NoWrap);
    }
    hlight();
}
/*************************/
void FN::toggleIndent()
{
    int count = ui->stackedWidget->count();
    if (count == 0) return;

    if (ui->actionIndent->isChecked())
    {
        for (int i = 0; i < count; ++i)
            qobject_cast<TextEdit*>(ui->stackedWidget->widget (i))->autoIndentation = true;
    }
    else
    {
        for (int i = 0; i < count; ++i)
            qobject_cast<TextEdit*>(ui->stackedWidget->widget (i))->autoIndentation = false;
    }
}
/*************************/
void FN::prefDialog()
{
    closeWinDialogs();
    /* first, update settings because another
       FeatherNotes window may have changed them  */
    readAndApplyConfig (false);

    PrefDialog dlg (this);
    dlg.exec();
}
/*************************/
QByteArray FN::getSpltiterState() const
{
    return ui->splitter->saveState();
}
/*************************/
void FN::makeTreeTransparent (bool trans)
{
    if (trans)
    {
        if (!transparentTree_)
        {
            transparentTree_ = true;
            ui->treeView->setFrameShape (QFrame::NoFrame);
            if (ui->treeView->viewport())
            {
                QPalette p = ui->treeView->palette();
                p.setColor (QPalette::Base, QColor (Qt::transparent));
                p.setColor (QPalette::Text, p.color (QPalette::WindowText));
                ui->treeView->setPalette (p);
                ui->treeView->viewport()->setAutoFillBackground (false);
            }
        }
    }
    else
    {
        if (transparentTree_)
        {
            transparentTree_ = false;
            ui->treeView->setFrameShape (QFrame::StyledPanel);
            if (ui->treeView->viewport())
            {
                ui->treeView->setPalette (QPalette());
                ui->treeView->viewport()->setAutoFillBackground (true);
            }
        }
    }
}
/*************************/
void FN::setToolBarIconSize (bool small)
{
    smallToolbarIcons_ = small;
    int styleSize = style()->pixelMetric (QStyle::PM_ToolBarIconSize);
    QSize s = small ? QSize (16, 16) : QSize (styleSize, styleSize);
    if (s != ui->mainToolBar->iconSize())
        ui->mainToolBar->setIconSize (s);
}
/*************************/
void FN::showToolbar (bool show)
{
    ui->mainToolBar->setVisible (show);
    noToolbar_ = !show;
}
/*************************/
void FN::showMenubar (bool show)
{
    ui->menuBar->setVisible (show);
    ui->actionMenu->setVisible (!show);
    noMenubar_ = !show;
}
/*************************/
void FN::updateCustomizableShortcuts()
{
    QHash<QAction*, QKeySequence>::const_iterator iter = defaultShortcuts_.constBegin();
    QList<QString> cn = customActions_.keys();

    while (iter != defaultShortcuts_.constEnd())
    {
        const QString name = iter.key()->objectName();
        iter.key()->setShortcut (cn.contains (name)
                                 ? QKeySequence (customActions_.value (name), QKeySequence::PortableText)
                                 : iter.value());
        ++ iter;
    }
}
/*************************/
void FN::readShortcuts()
{
    /* NOTE: We don't read the custom shortcuts from global config files
             because we want the user to be able to restore their default values. */
    Settings tmp ("feathernotes", "fn");
    Settings settings (tmp.fileName(), QSettings::NativeFormat);

    settings.beginGroup ("shortcuts");
    QStringList addedShortcuts; // to prevent ambiguous shortcuts
    QStringList actions = settings.childKeys();
    for (int i = 0; i < actions.size(); ++i)
    {
        QVariant v = settings.value (actions.at (i));
        bool isValid;
        QString vs = validatedShortcut (v, addedShortcuts, &isValid);
        if (isValid)
            customActions_.insert (actions.at (i), vs);
        else // remove the key on writing config
            uncustomizedActions_ << actions.at (i);
    }
    settings.endGroup();
}
/*************************/
QString FN::validatedShortcut (const QVariant v, QStringList &added, bool *isValid)
{
    if (v.isValid())
    {
        QString str = v.toString();
        if (str.isEmpty())
        { // it means the removal of a shortcut
            *isValid = true;
            return QString();
        }

        if (!QKeySequence (str, QKeySequence::PortableText).toString().isEmpty())
        {
            if (!reservedShortcuts_.contains (str)
                // prevent ambiguous shortcuts as far as possible
                && !added.contains (str))
            {
                *isValid = true;
                added << str;
                return str;
            }
        }
    }
    *isValid = false;
    return QString();
}
/*************************/
void FN::readAndApplyConfig (bool startup)
{
    QSettings settings ("feathernotes", "fn");

    /**************
     *** Window ***
     **************/

    settings.beginGroup ("window");

    startSize_ = settings.value ("startSize", QSize (700, 500)).toSize();
    if (startSize_.isEmpty())
        startSize_ = QSize (700, 500);
    if (settings.value ("size").toString() == "none")
    {
        remSize_ = false; // true by default
        if (startup)
            resize (startSize_);
    }
    else
    {
        remSize_ = true;
        if (startup)
        {
            winSize_ = settings.value ("size", QSize (700, 500)).toSize();
            if (winSize_.isEmpty())
                winSize_ = QSize (700, 500);
            isMaxed_ = settings.value ("max", false).toBool();
            isFull_ = settings.value ("fullscreen", false).toBool();
            resize (winSize_);
            if (!remPosition_ || static_cast<FNSingleton*>(qApp)->isWayland()) // otherwise -> showEvent()
            {
                if (isFull_ && isMaxed_)
                    setWindowState (Qt::WindowMaximized | Qt::WindowFullScreen);
                else if (isMaxed_)
                    setWindowState (Qt::WindowMaximized);
                else if (isFull_)
                    setWindowState (Qt::WindowFullScreen);
            }
        }
    }

    QByteArray splitterSizes;
    QVariant v = settings.value ("splitterSizes");
    if (v.toString() == "none")
        remSplitter_ = false; // true by default
    else
    {
        remSplitter_ = true;
        if (v.isValid())
            splitterSizes = v.toByteArray();
    }
    if (startup)
    {
        if (splitterSizes.isEmpty() || !ui->splitter->restoreState (splitterSizes))
        {
            QList<int> sizes; sizes << 170 << 530;
            ui->splitter->setSizes (sizes);
        }
    }

    if (settings.value ("position").toString() == "none")
        remPosition_ = false; // true by default
    else
    {
        remPosition_ = true;
        if (startup)
            position_ = settings.value ("position", QPoint (0, 0)).toPoint();
    }

    prefSize_ = settings.value ("prefSize").toSize();

    hasTray_ = settings.value ("hasTray").toBool(); // false by default

    minToTray_ = settings.value ("minToTray").toBool(); // false by default

    if (settings.value ("transparentTree").toBool()) // false by default
        makeTreeTransparent (true);
    else if (!startup)
        makeTreeTransparent (false);

    v = settings.value ("smallToolbarIcons"); // true by default
    if (v.isValid())
        setToolBarIconSize (v.toBool());
    else
        setToolBarIconSize (true);

    noToolbar_ = settings.value ("noToolbar").toBool(); // false by default
    noMenubar_ = settings.value ("noMenubar").toBool(); // false by default
    if (noToolbar_ && noMenubar_)
    { // we don't want to hide all actions
        noToolbar_ = false;
        noMenubar_ = true;
    }
    ui->mainToolBar->setVisible (!noToolbar_);
    ui->menuBar->setVisible (!noMenubar_);
    ui->actionMenu->setVisible (noMenubar_);

    if (startup)
    {
        QIcon icn;
        icn = symbolicIcon::icon (":icons/go-down.svg");
        ui->nextButton->setIcon (icn);
        ui->rplNextButton->setIcon (icn);
        ui->actionMoveDown->setIcon (icn);
        icn = symbolicIcon::icon (":icons/go-up.svg");
        ui->prevButton->setIcon (icn);
        ui->rplPrevButton->setIcon (icn);
        ui->actionMoveUp->setIcon (icn);
        ui->allButton->setIcon (symbolicIcon::icon (":icons/arrow-down-double.svg"));
        icn = symbolicIcon::icon (":icons/document-save.svg");
        ui->actionSave->setIcon (icn);
        ui->actionImageSave->setIcon (icn);
        ui->actionOpen->setIcon (symbolicIcon::icon (":icons/document-open.svg"));
        ui->actionUndo->setIcon (symbolicIcon::icon (":icons/edit-undo.svg"));
        ui->actionRedo->setIcon (symbolicIcon::icon (":icons/edit-redo.svg"));
        ui->actionFind->setIcon (symbolicIcon::icon (":icons/edit-find.svg"));
        ui->actionClear->setIcon (symbolicIcon::icon (":icons/edit-clear.svg"));
        ui->actionClearRecent->setIcon (symbolicIcon::icon (":icons/edit-clear.svg"));
        ui->actionBold->setIcon (symbolicIcon::icon (":icons/format-text-bold.svg"));
        ui->actionItalic->setIcon (symbolicIcon::icon (":icons/format-text-italic.svg"));
        ui->actionUnderline->setIcon (symbolicIcon::icon (":icons/format-text-underline.svg"));
        ui->actionStrike->setIcon (symbolicIcon::icon (":icons/format-text-strikethrough.svg"));
        ui->actionTextColor->setIcon (symbolicIcon::icon (":icons/format-text-color.svg"));
        icn = symbolicIcon::icon (":icons/format-fill-color.svg");
        ui->actionBgColor->setIcon (icn);
        ui->actionDocColors->setIcon (icn);
        ui->actionNew->setIcon (symbolicIcon::icon (":icons/document-new.svg"));
        ui->actionSaveAs->setIcon (symbolicIcon::icon (":icons/document-save-as.svg"));
        icn = symbolicIcon::icon (":icons/document-print.svg");
        ui->actionPrint->setIcon (icn);
        ui->actionPrintNodes->setIcon (icn);
        ui->actionPrintAll->setIcon (icn);
        ui->actionPassword->setIcon (symbolicIcon::icon (":icons/document-encrypt.svg"));
        ui->actionQuit->setIcon (symbolicIcon::icon (":icons/application-exit.svg"));
        ui->actionCut->setIcon (symbolicIcon::icon (":icons/edit-cut.svg"));
        ui->actionCopy->setIcon (symbolicIcon::icon (":icons/edit-copy.svg"));
        icn = symbolicIcon::icon (":icons/edit-paste.svg");
        ui->actionPaste->setIcon (icn);
        ui->actionPasteHTML->setIcon (icn);
        ui->actionDelete->setIcon (symbolicIcon::icon (":icons/edit-delete.svg"));
        ui->actionSelectAll->setIcon (symbolicIcon::icon (":icons/edit-select-all.svg"));
        ui->actionDate->setIcon (symbolicIcon::icon (":icons/document-open-recent.svg"));
        ui->menuOpenRecently->setIcon (symbolicIcon::icon (":icons/document-open-recent.svg"));
        icn = symbolicIcon::icon (":icons/image-x-generic.svg");
        ui->actionEmbedImage->setIcon (icn);
        ui->actionImageScale->setIcon (icn);
        ui->actionNodeIcon->setIcon (icn);
        ui->actionExpandAll->setIcon (symbolicIcon::icon (":icons/expand.svg"));
        ui->actionCollapseAll->setIcon (symbolicIcon::icon (":icons/collapse.svg"));
        ui->actionDeleteNode->setIcon (symbolicIcon::icon (":icons/user-trash.svg"));
        icn = symbolicIcon::icon (":icons/edit-rename.svg");
        ui->actionRenameNode->setIcon (icn);
        ui->namesButton->setIcon (icn);
        ui->actionProp->setIcon (symbolicIcon::icon (":icons/document-properties.svg"));
        icn = symbolicIcon::icon (":icons/preferences-desktop-font.svg");
        ui->actionDocFont->setIcon (icn);
        ui->actionNodeFont->setIcon (icn);
        ui->actionPref->setIcon (symbolicIcon::icon (":icons/preferences-system.svg"));
        ui->actionReplace->setIcon (symbolicIcon::icon (":icons/edit-find-replace.svg"));
        ui->actionHelp->setIcon (symbolicIcon::icon (":icons/help-contents.svg"));
        ui->actionAbout->setIcon (symbolicIcon::icon (":icons/help-about.svg"));
        ui->actionSuper->setIcon (symbolicIcon::icon (":icons/format-text-superscript.svg"));
        ui->actionSub->setIcon (symbolicIcon::icon (":icons/format-text-subscript.svg"));
        ui->actionCenter->setIcon (symbolicIcon::icon (":icons/format-justify-center.svg"));
        ui->actionRight->setIcon (symbolicIcon::icon (":icons/format-justify-right.svg"));
        ui->actionLeft->setIcon (symbolicIcon::icon (":icons/format-justify-left.svg"));
        ui->actionJust->setIcon (symbolicIcon::icon (":icons/format-justify-fill.svg"));
        ui->actionMoveLeft->setIcon (symbolicIcon::icon (":icons/go-previous.svg"));
        ui->actionMoveRight->setIcon (symbolicIcon::icon (":icons/go-next.svg"));
        icn = symbolicIcon::icon (":icons/zoom-in.svg");
        ui->actionH1->setIcon (icn);
        ui->actionH2->setIcon (icn);
        ui->actionH3->setIcon (icn);
        icn = symbolicIcon::icon (":icons/tag.svg");
        ui->actionTags->setIcon (icn);
        ui->tagsButton->setIcon (icn);
        ui->actionLink->setIcon (symbolicIcon::icon (":icons/insert-link.svg"));
        ui->actionCopyLink->setIcon (symbolicIcon::icon (":icons/link.svg"));
        ui->actionTable->setIcon (symbolicIcon::icon (":icons/insert-table.svg"));
        ui->actionTableAppendRow->setIcon (symbolicIcon::icon (":icons/edit-table-insert-row-below.svg"));
        ui->actionTableAppendCol->setIcon (symbolicIcon::icon (":icons/edit-table-insert-column-right.svg"));
        ui->actionTableDeleteRow->setIcon (symbolicIcon::icon (":icons/edit-table-delete-row.svg"));
        ui->actionTableDeleteCol->setIcon (symbolicIcon::icon (":icons/edit-table-delete-column.svg"));
        ui->actionTableMergeCells->setIcon (symbolicIcon::icon (":icons/edit-table-cell-merge.svg"));
        ui->actionTablePrependRow->setIcon (symbolicIcon::icon (":icons/edit-table-insert-row-above.svg"));
        ui->actionTablePrependCol->setIcon (symbolicIcon::icon (":icons/edit-table-insert-column-left.svg"));
        ui->actionRTL->setIcon (symbolicIcon::icon (":icons/format-text-direction-rtl.svg"));
        ui->actionLTR->setIcon (symbolicIcon::icon (":icons/format-text-direction-ltr.svg"));
        ui->actionMenu->setIcon (symbolicIcon::icon (":icons/application-menu.svg"));

        ui->actionPrepSibling->setIcon (symbolicIcon::icon (":icons/sibling-above.svg"));
        ui->actionNewSibling->setIcon (symbolicIcon::icon (":icons/sibling-below.svg"));
        ui->actionNewChild->setIcon (symbolicIcon::icon (":icons/child.svg"));

        ui->everywhereButton->setIcon (symbolicIcon::icon (":icons/all.svg"));
        ui->wholeButton->setIcon (symbolicIcon::icon (":icons/whole.svg"));
        ui->caseButton->setIcon (symbolicIcon::icon (":icons/case.svg"));

        icn = QIcon::fromTheme ("feathernotes");
        if (icn.isNull())
            icn = QIcon (":icons/feathernotes.svg");
        setWindowIcon (icn);
    }

    settings.endGroup();

    /************
     *** Text ***
     ************/

    settings.beginGroup ("text");

    if (settings.value ("noWrap").toBool())
    {
        wrapByDefault_ = false; // true by default
        if (startup)
            ui->actionWrap->setChecked (false);
    }
    else
        wrapByDefault_ = true;

    if (settings.value ("noIndent").toBool())
    {
        indentByDefault_ = false; // true by default
        if (startup)
            ui->actionIndent->setChecked (false);
    }
    else
        indentByDefault_ = true;

    autoBracket_ = settings.value ("autoBracket").toBool(); // false by default

    autoReplace_ = settings.value ("autoReplace").toBool(); // false by default

    dateFormat_ = settings.value ("dateFormat").toString();

    int as = settings.value ("autoSave", -1).toInt();
    if (startup)
        autoSave_ = as;
    else if (autoSave_ != as)
    {
        autoSave_ = as;
        if (autoSave_ >= 1)
            timer_->start (autoSave_ * 1000 * 60);
        else if (timer_->isActive())
            timer_->stop();
    }

    openLastFile_ = settings.value ("openLastFile").toBool(); // false by default
    if (openLastFile_)
    {
        xmlPath_ = settings.value ("lastOpenedFile").toString();
        static_cast<FNSingleton*>(qApp)->setLastFile (xmlPath_);
        lastNode_ = settings.value ("lastNode", -1).toInt();
    }
    else
    {
        lastNode_ = -1;
        static_cast<FNSingleton*>(qApp)->setLastFile (QString());
    }

#ifdef HAS_HUNSPELL
    dictPath_ = settings.value ("dictionaryPath").toString();
#endif

    rememberExpanded_ = settings.value ("rememberExpanded").toBool(); // false by default

    saveOnExit_ = settings.value ("saveOnExit").toBool(); // false by default

    settings.endGroup();
}
/*************************/
void FN::writeGeometryConfig (bool withLastNodeInfo)
{
    Settings settings ("feathernotes", "fn");
    settings.beginGroup ("window");

    if (remSize_)
    {
        settings.setValue ("size", winSize_);
        settings.setValue ("max", isMaxed_);
        settings.setValue ("fullscreen", isFull_);
    }
    else
    {
        settings.setValue ("size", "none");
        settings.remove ("max");
        settings.remove ("fullscreen");
    }
    settings.setValue ("startSize", startSize_);

    if (remSplitter_)
        settings.setValue ("splitterSizes", ui->splitter->saveState());
    else
        settings.setValue ("splitterSizes", "none");

    if (remPosition_)
    {
        if (!static_cast<FNSingleton*>(qApp)->isWayland())
        {
            QPoint CurrPos;
            if (isVisible() && !isMaximized() && !isFullScreen())
            {
                CurrPos.setX (geometry().x());
                CurrPos.setY (geometry().y());
            }
            else
                CurrPos = position_;
            settings.setValue ("position", CurrPos);
        }
    }
    else
        settings.setValue ("position", "none");

    settings.setValue ("prefSize", prefSize_);

    settings.endGroup();

    if (withLastNodeInfo && openLastFile_ && !xmlPath_.isEmpty())
    {
        settings.beginGroup ("text");

        lastNode_ = 0;
        QModelIndex curIndx = ui->treeView->currentIndex();
        QModelIndex indx = model_->index (0, 0, QModelIndex());
        if (curIndx.isValid() && indx.isValid())
        {
            while (indx != curIndx
                   && (indx = model_->adjacentIndex (indx, true)).isValid())
            {
                ++lastNode_;
            }
        }
        settings.setValue ("lastNode", lastNode_);

        settings.endGroup();
    }
}
/*************************/
// Recent files are also updated.
void FN::rememberLastOpenedFile (bool recentNumIsSet)
{
    Settings settings ("feathernotes", "fn");
    settings.beginGroup ("text");

    int recentNum;
    QStringList recentFiles;
    if (recentNumIsSet) // recentNum_ is set in the Preferences dialog
        recentNum = recentNum_;
    else
        recentNum = settings.value ("recentFilesNumber", DEFAULT_RECENT_NUM).toInt();
    recentNum = qBound (0, recentNum, MAX_RECENT_NUM);
    if (recentNum > 0)
        recentFiles = settings.value ("recentFiles").toStringList();

    if (openLastFile_
        && !xmlPath_.isEmpty()) // a file has been opened or saved to
    {
        settings.setValue ("lastOpenedFile", xmlPath_);
        static_cast<FNSingleton*>(qApp)->setLastFile (xmlPath_);
    }

    if (recentNumIsSet) // called by the Preferences dialog
    {
        if (!openLastFile_)
        {
            settings.remove ("lastOpenedFile");
            static_cast<FNSingleton*>(qApp)->setLastFile (QString());
            settings.remove ("lastNode");
        }
        settings.setValue ("recentFilesNumber", recentNum);
        settings.setValue ("openReccentSeparately", openReccentSeparately_);
    }

    if (recentNum == 0)
    {
        settings.setValue ("recentFiles", ""); // don't save "@Invalid()"
        settings.endGroup();
        return;
    }

    recentFiles.removeAll ("");
    recentFiles.removeDuplicates();
    if (!recentNumIsSet
        && !xmlPath_.isEmpty()) // a file has been opened or saved to
    {
        recentFiles.removeAll (xmlPath_);
        recentFiles.prepend (xmlPath_);
    }
    while (recentFiles.count() > recentNum)
        recentFiles.removeLast();
    if (recentFiles.isEmpty())
        settings.setValue ("recentFiles", "");
    else
        settings.setValue ("recentFiles", recentFiles);

    settings.endGroup();
}
/*************************/
void FN::writeConfig()
{
    Settings settings ("feathernotes", "fn");
    if (!settings.isWritable()) return;

    /**************
     *** Window ***
     **************/

    settings.beginGroup ("window");

    settings.setValue ("hasTray", hasTray_);
    settings.setValue ("minToTray", minToTray_);
    settings.setValue ("transparentTree", transparentTree_);
    settings.setValue ("smallToolbarIcons", smallToolbarIcons_);
    settings.setValue ("noToolbar", noToolbar_);
    settings.setValue ("noMenubar", noMenubar_);

    settings.endGroup();

    /************
     *** Text ***
     ************/

    settings.beginGroup ("text");

    settings.setValue ("noWrap", !wrapByDefault_);
    settings.setValue ("noIndent", !indentByDefault_);
    settings.setValue ("autoBracket", autoBracket_);
    settings.setValue ("autoReplace", autoReplace_);

    settings.setValue ("dateFormat", dateFormat_);

    settings.setValue ("autoSave", autoSave_);
    if (autoSave_ >= 1)
        timer_->start (autoSave_ * 1000 * 60);
    else if (timer_->isActive())
        timer_->stop();

    settings.setValue ("openLastFile", openLastFile_);

#ifdef HAS_HUNSPELL
    settings.setValue ("dictionaryPath", dictPath_);
#endif

    settings.setValue ("rememberExpanded", rememberExpanded_);

    settings.setValue ("saveOnExit", saveOnExit_);

    settings.endGroup();

    /*****************
     *** Shortcuts ***
     *****************/

    settings.beginGroup ("shortcuts");

    for (int i = 0; i < uncustomizedActions_.size(); ++i)
        settings.remove (uncustomizedActions_.at (i));

    QHash<QString, QString>::const_iterator it = customActions_.constBegin();
    while (it != customActions_.constEnd())
    {
        settings.setValue (it.key(), it.value());
        ++it;
    }

    settings.endGroup();
}
/*************************/
// NOTE: Because of a bug in Qt, if printing is done in this thread, it'll interfere with how
// emojis are shown. Printing in a separate thread is also good for not blocking windows.
void FN::txtPrint()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    QApplication::setOverrideCursor (QCursor(Qt::WaitCursor));

    /* choose an appropriate name and directory */
    QDir dir = QDir::home();
    if (!xmlPath_.isEmpty())
        dir = QFileInfo (xmlPath_).absoluteDir();
    QModelIndex indx = ui->treeView->currentIndex();
    QString fname;
    if (QObject::sender() == ui->actionPrintAll)
    {
        if (xmlPath_.isEmpty()) fname = tr ("Untitled");
        else
        {
            fname = QFileInfo (xmlPath_).fileName();
            if (fname.endsWith (".fnx"))
                fname.chop (4);
        }
    }
    else if ((fname = model_->data (indx, Qt::DisplayRole).toString()).isEmpty())
            fname = tr ("Untitled");
    fname = dir.filePath (fname);
    fname.append (".pdf");

    QTextDocument *doc = nullptr;
    bool newDocCreated = false;
    if (QObject::sender() == ui->actionPrint)
    {
        if (bgColor_ == QColor (Qt::white) && fgColor_ == QColor (Qt::black))
            doc = qobject_cast<TextEdit*>(cw)->document();
        else
        {
            doc = new QTextDocument();
            doc->setDefaultStyleSheet (DOC_STYLESHEET.arg (bgColor_.name(), fgColor_.name()));
            newDocCreated = true;
            doc->setHtml (qobject_cast<TextEdit*>(cw)->toHtml());
        }
    }
    else
    {
        QString text;
        if (QObject::sender() == ui->actionPrintNodes)
        {
            indx = ui->treeView->currentIndex();
            QModelIndex sibling = model_->sibling (indx.row() + 1, 0, indx);
            while (indx != sibling)
            {
                text.append (nodeAddress (indx));
                DomItem *item = static_cast<DomItem*>(indx.internalPointer());
                if (TextEdit *thisTextEdit = widgets_.value (item))
                    text.append (thisTextEdit->toHtml()); // the node text may have been edited
                else
                {
                    QDomNodeList lst = item->node().childNodes();
                    text.append (lst.item (0).nodeValue());
                }
                indx = model_->adjacentIndex (indx, true);
            }
        }
        else// if (QObject::sender() == ui->actionPrintAll)
        {
            indx = model_->index (0, 0, QModelIndex());
            while (indx.isValid())
            {
                text.append (nodeAddress (indx));
                DomItem *item = static_cast<DomItem*>(indx.internalPointer());
                if (TextEdit *thisTextEdit = widgets_.value (item))
                    text.append (thisTextEdit->toHtml());
                else
                {
                    QDomNodeList lst = item->node().childNodes();
                    text.append (lst.item (0).nodeValue());
                }
                indx = model_->adjacentIndex (indx, true);
            }
        }
        doc = new QTextDocument();
        if (bgColor_ != QColor (Qt::white) || fgColor_ != QColor (Qt::black))
            doc->setDefaultStyleSheet (DOC_STYLESHEET.arg (bgColor_.name(), fgColor_.name()));
        newDocCreated = true;
        doc->setHtml (text);
    }

    QTimer::singleShot (0, this, []() { // wait for the dialog
        if (QGuiApplication::overrideCursor() != nullptr)
            QApplication::restoreOverrideCursor();
    });

    bool Use96Dpi = QCoreApplication::instance()->testAttribute (Qt::AA_Use96Dpi);
    QScreen *screen = QGuiApplication::primaryScreen();
    qreal sourceDpiX = Use96Dpi ? 96 : screen ? screen->logicalDotsPerInchX() : 100;
    qreal sourceDpiY = Use96Dpi ? 96 : screen ? screen->logicalDotsPerInchY() : 100;
    Printing *thread = new Printing (doc,
                                     fname,
                                     sourceDpiX,
                                     sourceDpiY,
                                     fgColor_);
    if (newDocCreated) delete doc;

    QPrintDialog dlg (thread->printer(), this);
    dlg.setWindowTitle (tr ("Print Document"));
    if (dlg.exec() == QDialog::Accepted)
    {
        connect (thread, &QThread::finished, thread, &QObject::deleteLater);
        connect (thread, &QThread::finished, this, [this] {
            // this also closes the permanent bar below
            showWarningBar ("<center><b><big>" + tr ("Printing completed.") + "</big></b></center>", 10);
        });
        showWarningBar ("<center><b><big>" + tr ("Printing in progress...") + "</big></b></center>", 0);
        thread->start();
    }
    else
        delete thread;

    if (QGuiApplication::overrideCursor() != nullptr)
        QApplication::restoreOverrideCursor();
}
/*************************/
void FN::exportHTML()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setWindowModality (Qt::WindowModal);
    dialog->setWindowTitle (tr ("Export HTML"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    QGroupBox *groupBox = new QGroupBox (tr ("Export:"));
    QRadioButton *radio1 = new QRadioButton (tr ("&Current node"));
    radio1->setChecked (true);
    QRadioButton *radio2 = new QRadioButton (tr ("With all &sub-nodes"));
    QRadioButton *radio3 = new QRadioButton (tr ("&All nodes"));
    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget (radio1);
    vbox->addWidget (radio2);
    vbox->addWidget (radio3);
    connect (radio1, &QAbstractButton::toggled, this, &FN::setHTMLName);
    connect (radio2, &QAbstractButton::toggled, this, &FN::setHTMLName);
    connect (radio3, &QAbstractButton::toggled, this, &FN::setHTMLName);
    vbox->addStretch (1);
    groupBox->setLayout (vbox);

    QLabel *label = new QLabel();
    label->setText (tr ("Output file:"));

    htmlPahEntry_ = new LineEdit();
    htmlPahEntry_->returnOnClear = false;
    htmlPahEntry_->setMinimumWidth (150);
    QModelIndex indx = ui->treeView->currentIndex();
    QDir dir = QDir::home();
    if (!xmlPath_.isEmpty())
        dir = QFileInfo (xmlPath_).absoluteDir();
    QString fname;
    if ((fname = model_->data (indx, Qt::DisplayRole).toString()).isEmpty())
        fname = tr ("Untitled");
    fname.append (".html");
    fname = dir.filePath (fname);
    htmlPahEntry_->setText (fname);
    connect (htmlPahEntry_, &QLineEdit::returnPressed, dialog, &QDialog::accept);

    QToolButton *openBtn = new QToolButton();
    openBtn->setIcon (symbolicIcon::icon (":icons/document-open.svg"));
    openBtn->setToolTip (tr ("Select path"));
    connect (openBtn, &QAbstractButton::clicked, this, &FN::setHTMLPath);
    QSpacerItem *spacer = new QSpacerItem (1, 5);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (okButton, &QAbstractButton::clicked, dialog, &QDialog::accept);


    grid->addWidget (groupBox, 0, 0, 1, 2);
    grid->addWidget (label, 1, 0);
    grid->addWidget (htmlPahEntry_, 1, 1, 1, 3);
    grid->addWidget (openBtn, 1, 4, Qt::AlignLeft);
    grid->addItem (spacer, 2, 0);
    grid->addWidget (cancelButton, 3, 1, Qt::AlignRight);
    grid->addWidget (okButton, 3, 2, 1, 3, Qt::AlignRight);
    grid->setColumnStretch (1, 1);
    grid->setRowStretch (2, 1);

    dialog->setLayout (grid);
    /*dialog->show();
    dialog->move (x() + width()/2 - dialog->width(),
                  y() + height()/2 - dialog->height());*/

    int sel = 0;
    bool prompt = true;
    while (prompt)
    {
        switch (dialog->exec()) {
        case QDialog::Accepted:
            if (radio2->isChecked())
                sel = 1;
            else if (radio3->isChecked())
                sel = 2;
            else
                sel = 0;
            if (htmlPahEntry_ != nullptr)
                fname = htmlPahEntry_->text();
            if (QFile::exists (fname))
            {
                prompt = QMessageBox::No == QMessageBox::question (this,
                                                                   tr ("Question"),
                                                                   tr ("The file already exists.\nDo you want to replace it?\n"));
                if (prompt) continue;
            }
            prompt = false;

            QApplication::setOverrideCursor (QCursor(Qt::WaitCursor));
            delete dialog;
            break;
        case QDialog::Rejected:
        default:
            if (QGuiApplication::overrideCursor() != nullptr)
                QApplication::restoreOverrideCursor();
            prompt = false;
            delete dialog;
            return;
        }
    }

    QTextDocument *doc = nullptr;
    bool newDocCreated = false;
    if (sel == 0)
    {
        if (bgColor_ == QColor (Qt::white) && fgColor_ == QColor (Qt::black))
            doc = qobject_cast<TextEdit*>(cw)->document();
        else
        {
            doc = new QTextDocument();
            doc->setDefaultStyleSheet (DOC_STYLESHEET.arg (bgColor_.name(), fgColor_.name()));
            newDocCreated = true;
            doc->setHtml (qobject_cast<TextEdit*>(cw)->toHtml());
        }
    }
    else
    {
        QString text;
        if (sel == 1)
        {
            indx = ui->treeView->currentIndex();
            QModelIndex sibling = model_->sibling (indx.row() + 1, 0, indx);
            while (indx != sibling)
            {
                text.append (nodeAddress (indx));
                DomItem *item = static_cast<DomItem*>(indx.internalPointer());
                if (TextEdit *thisTextEdit = widgets_.value (item))
                    text.append (thisTextEdit->toHtml()); // the node text may have been edited
                else
                {
                    QDomNodeList lst = item->node().childNodes();
                    text.append (lst.item (0).nodeValue());
                }
                indx = model_->adjacentIndex (indx, true);
            }
        }
        else// if (sel == 2)
        {
            indx = model_->index (0, 0, QModelIndex());
            while (indx.isValid())
            {
                text.append (nodeAddress (indx));
                DomItem *item = static_cast<DomItem*>(indx.internalPointer());
                if (TextEdit *thisTextEdit = widgets_.value (item))
                    text.append (thisTextEdit->toHtml());
                else
                {
                    QDomNodeList lst = item->node().childNodes();
                    text.append (lst.item (0).nodeValue());
                }
                indx = model_->adjacentIndex (indx, true);
            }
        }
        doc = new QTextDocument();
        if (bgColor_ != QColor (Qt::white) || fgColor_ != QColor (Qt::black))
            doc->setDefaultStyleSheet (DOC_STYLESHEET.arg (bgColor_.name(), fgColor_.name()));
        newDocCreated = true;
        doc->setHtml (text);
    }

    QTextDocumentWriter writer (fname, "html");
    bool success = false;
    success = writer.write (doc);
    if (newDocCreated) delete doc;

    if (QGuiApplication::overrideCursor() != nullptr)
        QApplication::restoreOverrideCursor();

    if (!success)
    {
        showWarningBar (tr ("<center><b><big>Cannot be saved!</big></b></center>")
                        + "\n<center>" + writer.device()->errorString() + "</center>", 15);
    }
}
/*************************/
QString FN::nodeAddress (QModelIndex index)
{
    QString res;
    if (!index.isValid()) return res;

    QStringList l;
    l.prepend (model_->data (index, Qt::DisplayRole).toString());
    QModelIndex indx = model_->parent (index);
    while (indx.isValid())
    {
        l.prepend (model_->data (indx, Qt::DisplayRole).toString());
        indx = model_->parent (indx);
    }
    bool rtl = l.join (" ").isRightToLeft();
    if (rtl)
        res = l.join (" ← ");
    else
        res = l.join (" → ");
    if (fgColor_ != QColor (Qt::black))
        res = QString ("<br><center><h2><p dir='%1' style=\"font-family:'%2'; color:%3\">%4</p></h2></center><hr>")
              .arg (rtl ? "rtl" : "ltr", nodeFont_.family(), fgColor_.name(), res);
    else
        res = QString ("<br><center><h2><p dir='%1' style=\"font-family:'%2';\">%3</p></h2></center><hr>")
              .arg (rtl ? "rtl" : "ltr", nodeFont_.family(), res);

    return res;
}
/*************************/
void FN::setHTMLName (bool checked)
{
    if (!checked || htmlPahEntry_ == nullptr)
        return;
    QList<QDialog *> list = findChildren<QDialog*>();
    if (list.isEmpty()) return;
    QList<QRadioButton *> list1;
    int i = 0;
    for (i = 0; i < list.count(); ++i)
    {
        list1 = list.at (i)->findChildren<QRadioButton *>();
        if (!list1.isEmpty()) break;
    }
    if (list1.isEmpty()) return;
    int index = list1.indexOf (qobject_cast<QRadioButton*>(QObject::sender()));

    /* choose an appropriate name */
    QModelIndex indx = ui->treeView->currentIndex();
    QString fname;
    if (index == 2)
    {
        if (xmlPath_.isEmpty()) fname = tr ("Untitled");
        else
        {
            fname = QFileInfo (xmlPath_).fileName();
            if (fname.endsWith (".fnx"))
                fname.chop (4);
        }
    }
    else if ((fname = model_->data (indx, Qt::DisplayRole).toString()).isEmpty())
        fname = tr ("Untitled");
    fname.append (".html");

    QString str = htmlPahEntry_->text();
    QStringList strList = str.split ("/");
    if (strList.count() == 1)
    {
        QDir dir = QDir::home();
        if (!xmlPath_.isEmpty())
            dir = QFileInfo (xmlPath_).absoluteDir();
        fname = dir.filePath (fname);
    }
    else
    {
        QString last = strList.last();
        int lstIndex = str.lastIndexOf (last);
        fname = str.replace (lstIndex, last.size(), fname);
    }

    htmlPahEntry_->setText (fname);
}
/*************************/
void FN::setHTMLPath (bool)
{
    if (htmlPahEntry_ == nullptr) return;
    QString path;
    if ((path = htmlPahEntry_->text()).isEmpty())
        path = QDir::home().filePath (tr ("Untitled") + ".html");

    QString HTMLPath;
    FileDialog dialog (this);
    dialog.setAcceptMode (QFileDialog::AcceptSave);
    dialog.setWindowTitle (tr ("Save HTML As..."));
    dialog.setFileMode (QFileDialog::AnyFile);
    dialog.setNameFilter (tr ("HTML Files") + " (*.html *.htm)");
    dialog.setDirectory (path.section ("/", 0, -2)); // workaround for KDE
    dialog.selectFile (path);
    dialog.autoScroll();
    if (dialog.exec())
    {
        HTMLPath = dialog.selectedFiles().at (0);
        if (HTMLPath.isEmpty() || QFileInfo (HTMLPath).isDir())
            return;
    }
    else return;

    if (htmlPahEntry_ != nullptr)
        htmlPahEntry_->setText (HTMLPath);
}
/*************************/
void FN::setPswd()
{
    int index = ui->stackedWidget->currentIndex();
    if (index == -1) return;

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setWindowTitle (tr ("Set Password"));
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    LineEdit *lineEdit1 = new LineEdit();
    lineEdit1->setMinimumWidth (200);
    lineEdit1->setEchoMode (QLineEdit::Password);
    lineEdit1->setPlaceholderText (tr ("Type password"));
    connect (lineEdit1, &QLineEdit::returnPressed, this, [this, lineEdit1] {
        if (pswrd_ != lineEdit1->text())
        {
            pswrd_ = lineEdit1->text();
            noteModified();
        }
        reallySetPswrd();
    });
    LineEdit *lineEdit2 = new LineEdit();
    lineEdit2->returnOnClear = false;
    lineEdit2->setEchoMode (QLineEdit::Password);
    lineEdit2->setPlaceholderText (tr ("Retype password"));
    connect (lineEdit2, &QLineEdit::returnPressed, this, [this, lineEdit1] {
        if (pswrd_ != lineEdit1->text())
        {
            pswrd_ = lineEdit1->text();
            noteModified();
        }
        reallySetPswrd();
    });
    QLabel *label = new QLabel();
    QSpacerItem *spacer = new QSpacerItem (1, 10);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, this, [this, lineEdit1] {
        if (pswrd_ != lineEdit1->text())
        {
            pswrd_ = lineEdit1->text();
            noteModified();
        }
        reallySetPswrd();
    });

    grid->addWidget (lineEdit1, 0, 0, 1, 3);
    grid->addWidget (lineEdit2, 1, 0, 1, 3);
    grid->addWidget (label, 2, 0, 1, 3);
    grid->addItem (spacer, 3, 0);
    grid->addWidget (cancelButton, 4, 0, 1, 2, Qt::AlignRight);
    grid->addWidget (okButton, 4, 2, Qt::AlignCenter);
    grid->setColumnStretch (1, 1);
    grid->setRowStretch (3, 1);
    label->setVisible (false);

    dialog->setLayout (grid);
    dialog->resize (dialog->size().expandedTo (QSize (300, 0)));

    dialog->open();
}
/*************************/
void FN::reallySetPswrd()
{
    QList<QDialog *> list = findChildren<QDialog*>();
    if (list.isEmpty()) return;

    QList<LineEdit *> listEdit;
    int i = 0;
    for (i = 0; i < list.count(); ++i)
    {
        listEdit = list.at (i)->findChildren<LineEdit *>();
        if (!listEdit.isEmpty()) break;
    }
    if (listEdit.isEmpty()) return;
    if (listEdit.count() < 2) return;

    QList<QLabel *> listLabel = list.at (i)->findChildren<QLabel *>();
    if (listLabel.isEmpty()) return;

    QList<QPushButton *> listBtn = list.at (i)->findChildren<QPushButton *>();
    if (listBtn.isEmpty()) return;

    listBtn.at (0)->setDefault (false); // don't interfere
    LineEdit *lineEdit1 = listEdit.at (0);
    LineEdit *lineEdit2 = listEdit.at (1);
    if (QObject::sender() == lineEdit1)
    {
        listLabel.at (0)->setVisible (false);
        lineEdit2->setFocus();
        return;
    }

    if (lineEdit1->text() != lineEdit2->text())
    {
        listLabel.at (0)->setText (tr ("<center>Passwords were different. Retry!</center>"));
        listLabel.at (0)->setVisible (true);
    }
    else
        list.at (i)->accept();
}
/*************************/
bool FN::isPswrdCorrect (const QString &file)
{
    if (!isVisible() || !isActiveWindow())
    {
        activateFNWindow (true);
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents(); // needed when starting iconified

    closeWinDialogs();

    QDialog *dialog = new QDialog (this);
    dialog->setWindowModality (Qt::WindowModal);
    dialog->setWindowTitle (tr ("Enter Password"));
    QGridLayout *grid = new QGridLayout;
    grid->setSpacing (5);
    grid->setContentsMargins (5, 5, 5, 5);

    LineEdit *lineEdit = new LineEdit();
    lineEdit->setMinimumWidth (200);
    lineEdit->setEchoMode (QLineEdit::Password);
    lineEdit->setPlaceholderText (tr ("Enter Password"));
    connect (lineEdit, &QLineEdit::returnPressed, this, &FN::checkPswrd);
    QLabel *pathLabel = new QLabel(file);
    pathLabel->setSizePolicy (QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    pathLabel->setWordWrap (true);
    pathLabel->setAlignment (Qt::AlignCenter);
    QLabel *label = new QLabel();
    QSpacerItem *spacer0 = new QSpacerItem (1, 5);
    QSpacerItem *spacer1 = new QSpacerItem (1, 5);
    QPushButton *cancelButton = new QPushButton (symbolicIcon::icon (":icons/dialog-cancel.svg"), tr ("Cancel"));
    QPushButton *okButton = new QPushButton (symbolicIcon::icon (":icons/dialog-ok.svg"), tr ("OK"));
    connect (cancelButton, &QAbstractButton::clicked, dialog, &QDialog::reject);
    connect (okButton, &QAbstractButton::clicked, this, &FN::checkPswrd);

    grid->addWidget (pathLabel, 0, 0, 1, 3);
    grid->addItem (spacer0, 1, 0);
    grid->addWidget (lineEdit, 2, 0, 1, 3);
    grid->addWidget (label, 3, 0, 1, 3);
    grid->addItem (spacer1, 4, 0);
    grid->addWidget (cancelButton, 5, 0, 1, 2, Qt::AlignRight);
    grid->addWidget (okButton, 5, 2, Qt::AlignCenter);
    grid->setColumnStretch (1, 1);
    grid->setRowStretch (2, 1);
    label->setVisible (false);

    dialog->setLayout (grid);
    dialog->resize (dialog->size().expandedTo (QSize (300, 0)));
    /*dialog->show();
    dialog->move (x() + width()/2 - dialog->width(),
                  y() + height()/2 - dialog->height());*/

    bool res = true;
    switch (dialog->exec()) {
    case QDialog::Accepted:
        if (pswrd_ != lineEdit->text())
            res = false;
        delete dialog;
        break;
    case QDialog::Rejected:
    default:
        delete dialog;
        res = false;
        break;
    }

    return res;
}
/*************************/
void FN::checkPswrd()
{
    QList<QDialog *> list = findChildren<QDialog*>();
    if (list.isEmpty()) return;

    QList<LineEdit *> listEdit;
    int i = 0;
    for (i = 0; i < list.count(); ++i)
    {
        listEdit = list.at (i)->findChildren<LineEdit *>();
        if (!listEdit.isEmpty()) break;
    }
    if (listEdit.isEmpty()) return;

    QList<QLabel *> listLabel = list.at (i)->findChildren<QLabel *>();
    if (listLabel.size() < 2) return;

    QList<QPushButton *> listBtn = list.at (i)->findChildren<QPushButton *>();
    if (listBtn.isEmpty()) return;

    listBtn.at (0)->setDefault (false); // don't interfere

    if (listEdit.at (0)->text() != pswrd_)
    {
        listLabel.at (1)->setText (tr ("<center>Wrong password. Retry!</center>"));
        listLabel.at (1)->setVisible (true);
    }
    else
        list.at (i)->accept();
}
/*************************/
void FN::aboutDialog()
{
    closeWinDialogs();

    class AboutDialog : public QDialog {
    public:
        explicit AboutDialog (QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QDialog (parent, f) {
            aboutUi.setupUi (this);
            aboutUi.textLabel->setOpenExternalLinks (true);
        }
        void setTabTexts (const QString& first, const QString& sec) {
            aboutUi.tabWidget->setTabText (0, first);
            aboutUi.tabWidget->setTabText (1, sec);
        }
        void setMainIcon (const QIcon& icn) {
            aboutUi.iconLabel->setPixmap (icn.pixmap (64, 64));
        }
        void settMainTitle (const QString& title) {
            aboutUi.titleLabel->setText (title);
        }
        void setMainText (const QString& txt) {
            aboutUi.textLabel->setText (txt);
        }
    private:
        Ui::AboutDialog aboutUi;
    };

    auto dialog = new AboutDialog (this);
    dialog->setAttribute (Qt::WA_DeleteOnClose, true);

    QIcon FPIcon = QIcon::fromTheme ("feathernotes");
    if (FPIcon.isNull())
        FPIcon = QIcon (":icons/feathernotes.svg");
    dialog->setMainIcon (FPIcon);
    dialog->settMainTitle (QString ("<center><b><big>%1 %2</big></b></center><br>").arg (qApp->applicationName()).arg (qApp->applicationVersion()));
    dialog->setMainText ("<center> " + tr ("A lightweight notes manager") + " </center>\n<center> "
                         + tr ("based on Qt") + " </center><br><center> "
                         + tr ("Author")+": <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang ("
                         + tr ("aka.") + " Tsu Jan)</a> </center><p></p>");
    dialog->setTabTexts (tr ("About FeatherNotes"), tr ("Translators"));
    dialog->setWindowTitle (tr ("About FeatherNotes"));
    dialog->open();
}
/*************************/
void FN::showHelpDialog()
{
    closeWinDialogs();

    auto dlg = new FHelp (this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->resize (ui->stackedWidget->size().expandedTo (ui->treeView->size()));
    dlg->open();
}
/*************************/
void FN::updateRecentAction()
{
    if (auto action = ui->menuOpenRecently->menuAction())
        action->setEnabled (getRecentFilesNumber() > 0);
}
/*************************/
void FN::updateRecenMenu()
{
    QList<QAction*> curActions = ui->menuOpenRecently->actions();

    /* recentNum_ is up-to-date because updateRecentAction() is already called */
    if (recentNum_ == 0)
    {
        while (curActions.size() > 1)
        {
            auto action = curActions.takeAt (curActions.size() - 2);
            ui->menuOpenRecently->removeAction (action);
            delete action;
        }
        ui->actionClearRecent->setEnabled (false);
        return;
    }

    QSettings settings ("feathernotes", "fn");
    settings.beginGroup ("text");
    QStringList recentFiles = settings.value ("recentFiles").toStringList();
    settings.endGroup();

    recentFiles.removeAll ("");
    recentFiles.removeDuplicates();
    while (recentFiles.count() > recentNum_)
        recentFiles.removeLast();
    recentNum_ = qMin (recentNum_, recentFiles.count());

    int usableActionsNum = curActions.size() - 1; // except for the clear action
    QFontMetrics metrics (ui->menuOpenRecently->font());
    int w = 150 * metrics.horizontalAdvance (' ');
    for (int i = 0; i < recentNum_; ++i)
    {
        if (i < usableActionsNum)
        {
            curActions.at (i)->setText (metrics.elidedText (recentFiles.at (i), Qt::ElideMiddle, w));
            curActions.at (i)->setData (recentFiles.at (i));
        }
        else
        {
            QAction *newAction = new QAction (this);
            connect (newAction, &QAction::triggered, this, &FN::openRecentFile);
            ui->menuOpenRecently->addAction (newAction);
            newAction->setText (metrics.elidedText (recentFiles.at (i), Qt::ElideMiddle, w));
            newAction->setData (recentFiles.at (i));
        }
    }
    ui->menuOpenRecently->addAction (ui->actionClearRecent); // put it at the end
    ui->actionClearRecent->setEnabled (recentNum_ != 0);
    while (curActions.size() - 1 > recentNum_)
    {
        auto action = curActions.takeAt (curActions.size() - 2);
        ui->menuOpenRecently->removeAction (action);
        delete action;
    }
}
/*************************/
void FN::clearRecentMenu()
{
    QList<QAction*> curActions = ui->menuOpenRecently->actions();
    while (curActions.size() > 1)
    {
        auto action = curActions.takeAt (curActions.size() - 2);
        ui->menuOpenRecently->removeAction (action);
        delete action;
    }
    ui->actionClearRecent->setEnabled (false);

    QSettings settings ("feathernotes", "fn");
    settings.beginGroup ("text");
    settings.setValue ("recentFiles", ""); // don't save "@Invalid()"
    settings.endGroup();
}
/*************************/
// Only called by the Preferences dialog to get an up-to-date number.
int FN::getRecentFilesNumber()
{
    QSettings settings ("feathernotes", "fn");
    settings.beginGroup ("text");
    recentNum_ = qBound (0, settings.value ("recentFilesNumber", DEFAULT_RECENT_NUM).toInt(), MAX_RECENT_NUM);
    openReccentSeparately_ = settings.value ("openReccentSeparately", false).toBool();
    settings.endGroup();
    return recentNum_;
}
/*************************/
#ifdef HAS_HUNSPELL
static inline void moveToWordStart (QTextCursor& cur, bool forward)
{
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l)
    {
        QChar ch = blockText.at (indx);
        while (!ch.isLetterOrNumber() && ch != '\'' && ch != '-'
               && ch != QChar (QChar::Nbsp) && ch != QChar (0x200C))
        {
            cur.movePosition (QTextCursor::NextCharacter);
            ++indx;
            if (indx == l)
            {
                if (cur.movePosition (QTextCursor::NextBlock))
                    moveToWordStart (cur, forward);
                return;
            }
            ch = blockText.at (indx);
        }
    }
    if (!forward && indx > 0)
    {
        QChar ch = blockText.at (indx - 1);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-'
               || ch == QChar (QChar::Nbsp) || ch == QChar (0x200C))
        {
            cur.movePosition (QTextCursor::PreviousCharacter);
            --indx;
            ch = blockText.at (indx);
            if (indx == 0) break;
        }
    }
}

static inline void selectWord (QTextCursor& cur)
{
    moveToWordStart (cur, true);
    const QString blockText = cur.block().text();
    const int l = blockText.length();
    int indx = cur.positionInBlock();
    if (indx < l)
    {
        QChar ch = blockText.at (indx);
        while (ch.isLetterOrNumber() || ch == '\'' || ch == '-'
               || ch == QChar (QChar::Nbsp) || ch == QChar (0x200C))
        {
            cur.movePosition (QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            ++indx;
            if (indx == l) break;
            ch = blockText.at (indx);
        }
    }

    /* no dash, single quote mark or number at the start */
    while (!cur.selectedText().isEmpty()
           && (cur.selectedText().at (0) == '-' || cur.selectedText().at (0) == '\''
               || cur.selectedText().at (0).isNumber()))
    {
        int p = cur.position();
        cur.setPosition (cur.anchor() + 1);
        cur.setPosition (p, QTextCursor::KeepAnchor);
    }
    /* no dash or single quote mark at the end */
    while (!cur.selectedText().isEmpty()
           && (cur.selectedText().endsWith ("-") || cur.selectedText().endsWith ("\'")))
    {
        cur.setPosition (cur.position() - 1, QTextCursor::KeepAnchor);
    }
}

void FN::spellingCheckingMsg (const QString &msg, bool hasInfo)
{
    if (hasInfo)
    {
        showWarningBar ("<center><b><big>" + msg + "</big></b></center>"
                        + "\n<center><i>" + tr ("See Preferences → Text → Spell Checking!") + "</i></center>", 20);
    }
    else
        showWarningBar ("<center><b><big>" + msg + "</big></b></center>", 10);
}

void FN::checkSpelling()
{
    QWidget *cw = ui->stackedWidget->currentWidget();
    if (!cw) return;

    closeWinDialogs();

    auto dictPath = dictPath_;
    if (dictPath.isEmpty())
    {
        spellingCheckingMsg (tr ("You need to add a Hunspell dictionary."), true);
        return;
    }
    if (!QFile::exists (dictPath))
    {
        spellingCheckingMsg (tr ("The Hunspell dictionary does not exist."), true);
        return;
    }
    if (dictPath.endsWith (".dic"))
        dictPath = dictPath.left (dictPath.size() - 4);
    const QString affixFile = dictPath + ".aff";
    if (!QFile::exists (affixFile))
    {
        spellingCheckingMsg (tr ("The Hunspell dictionary is not accompanied by an affix file."), true);
        return;
    }
    QString confPath = QStandardPaths::writableLocation (QStandardPaths::ConfigLocation);
    if (!QFile (confPath + "/feathernotes").exists()) // create config dir if needed
        QDir (confPath).mkpath (confPath + "/feathernotes");
    QString userDict = confPath + "/feathernotes/userDict-" + dictPath.section ("/", -1);

    TextEdit *textEdit = qobject_cast<TextEdit*>(cw);
    QTextCursor cur = textEdit->textCursor();
    cur.setPosition (cur.anchor());
    moveToWordStart (cur, false);
    selectWord (cur);
    QString word = cur.selectedText();
    while (word.isEmpty())
    {
        if (!cur.movePosition (QTextCursor::NextCharacter))
        {
            spellingCheckingMsg (tr ("No misspelling from text cursor."), false);
            return;
        }
        selectWord (cur);
        word = cur.selectedText();
    }

    SpellChecker *spellChecker = new SpellChecker (dictPath, userDict);

    while (spellChecker->spell (word))
    {
        cur.setPosition (cur.position());
        if (cur.atEnd())
        {
            delete spellChecker;
            spellingCheckingMsg (tr ("No misspelling from text cursor."), false);
            return;
        }
        if (cur.movePosition (QTextCursor::NextCharacter))
            selectWord (cur);
        word = cur.selectedText();
        while (word.isEmpty())
        {
            cur.setPosition (cur.anchor());
            if (!cur.movePosition (QTextCursor::NextCharacter))
            {
                delete spellChecker;
                spellingCheckingMsg (tr ("No misspelling from text cursor."), false);
                return;
            }
            selectWord (cur);
            word = cur.selectedText();
        }
    }
    textEdit->setTextCursor (cur);
    textEdit->ensureCursorVisible();

    auto dlg = new SpellDialog (spellChecker, word, this);
    dlg->setAttribute (Qt::WA_DeleteOnClose, true);
    dlg->setWindowTitle (tr ("Spell Checking"));

    connect (dlg, &SpellDialog::spellChecked, [dlg, textEdit] (int res) {
        QTextCursor cur = textEdit->textCursor();
        if (!cur.hasSelection()) return; // impossible
        QString word = cur.selectedText();
        QString corrected;
        switch (res) {
        case SpellDialog::CorrectOnce:
            cur.insertText (dlg->replacement());
            break;
        case SpellDialog::IgnoreOnce:
            break;
        case SpellDialog::CorrectAll:
            /* remember this corretion */
            dlg->spellChecker()->addToCorrections (word, dlg->replacement());
            cur.insertText (dlg->replacement());
            break;
        case SpellDialog::IgnoreAll:
            /* always ignore the selected word */
            dlg->spellChecker()->ignoreWord (word);
            break;
        case SpellDialog::AddToDict:
            /* not only ignore it but also add it to user dictionary */
            dlg->spellChecker()->addToUserWordlist (word);
            break;
        }

        /* check the next word */
        cur.setPosition (cur.position());
        if (cur.atEnd())
        {
            textEdit->setTextCursor (cur);
            textEdit->ensureCursorVisible();
            dlg->close();
            return;
        }
        if (cur.movePosition (QTextCursor::NextCharacter))
            selectWord (cur);
        word = cur.selectedText();

        while (word.isEmpty())
        {
            cur.setPosition (cur.anchor());
            if (!cur.movePosition (QTextCursor::NextCharacter))
            {
                textEdit->setTextCursor (cur);
                textEdit->ensureCursorVisible();
                dlg->close();
                return;
            }
            selectWord (cur);
            word = cur.selectedText();
        }
        while (dlg->spellChecker()->spell (word)
               || !(corrected = dlg->spellChecker()->correct (word)).isEmpty())
        {
            if (!corrected.isEmpty())
            {
                cur.insertText (corrected);
                corrected = QString();
            }
            else
                cur.setPosition (cur.position());
            if (cur.atEnd())
            {
                textEdit->setTextCursor (cur);
                textEdit->ensureCursorVisible();
                dlg->close();
                return;
            }
            if (cur.movePosition (QTextCursor::NextCharacter))
                selectWord (cur);
            word = cur.selectedText();
            while (word.isEmpty())
            {
                cur.setPosition (cur.anchor());
                if (!cur.movePosition (QTextCursor::NextCharacter))
                {
                    textEdit->setTextCursor (cur);
                    textEdit->ensureCursorVisible();
                    dlg->close();
                    return;
                }
                selectWord (cur);
                word = cur.selectedText();
            }
        }
        textEdit->setTextCursor (cur);
        textEdit->ensureCursorVisible();
        dlg->checkWord (word);
    });

    dlg->open();

    connect (dlg, &QDialog::finished, this, [spellChecker] {
        delete spellChecker;
    });
}
#endif
/*************************/
bool FN::event (QEvent *event)
{
    if (event->type() == QEvent::StyleChange)
    {
        /* when the style changes in runtime, the text color of a transparent side pane
           should be set to its window text color again because the latter may have changed */
        if (transparentTree_ && ui->treeView->viewport())
        {
            QPalette p = ui->treeView->palette();
            p.setColor (QPalette::Text, p.color (QPalette::WindowText));
            ui->treeView->setPalette (p);
        }
    }
    /* NOTE: This is a workaround for an old Qt bug, because of which,
             QTimer may not work after resuming from suspend or hibernation. */
    else if (event->type() == QEvent::WindowActivate
             && timer_->isActive() && timer_->remainingTime() <= 0)
    {
        if (autoSave_ >= 1)
        {
            autoSaving();
            timer_->start (autoSave_ * 1000 * 60);
        }
    }
    return QMainWindow::event (event);
}

}
