/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2019 <tsujan2000@gmail.com>
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

#ifndef FN_H
#define FN_H

//#include <QtGui> // too much
#include <QListWidgetItem>
#include <QSystemTrayIcon>
#include <QMainWindow>
#include "textedit.h"
#include "domitem.h"
#include "lineedit.h"

namespace FeatherNotes {

namespace Ui {
class FN;
}

class DomModel;

class FN : public QMainWindow
{
    Q_OBJECT

public:
    explicit FN (const QString& message, QWidget *parent = nullptr);
    ~FN();

    void writeGeometryConfig();
    void writeConfig();

    bool isSizeRem() const {
        return remSize_;
    }
    void remSize (bool rem) {
        remSize_ = rem;
    }

    QSize getWinSize() const {
        return winSize_;
    }
    void setWinSize (const QSize &size) {
        winSize_ = size;
    }

    QSize getStartSize() const {
        return startSize_;
    }
    void setStartSize (const QSize &size) {
        startSize_ = size;
    }

    bool isSplitterRem() const {
        return  remSplitter_;
    }
    void remSplitter (bool rem) {
        remSplitter_ = rem;
    }
    QByteArray getSpltiterState() const;
    void setSplitterSizes (const QByteArray &sizes) {
        splitterSizes_ = sizes;
    }

    QSize getPrefSize() const {
        return prefSize_;
    }
    void setPrefSize (const QSize &s) {
        prefSize_ = s;
    }

    bool isPositionRem() const {
        return remPosition_;
    }
    void remPosition (bool rem) {
        remPosition_ = rem;
    }
    void setPosition (const QPoint &pos) {
        position_ = pos;
    }

    bool isUnderE() const {
        return underE_;
    }
    void setUnderE (bool yes);
    QSize EShift() const {
        return EShift_;
    }
    void setEShift (QSize shift) {
        EShift_ = shift;
    }

    bool hasTray() const {
        return hasTray_;
    }
    void useTray (bool use) {
        hasTray_ = use;
    }

    bool doesMinToTray() const {
        return minToTray_;
    }
    void minToTray (bool yes) {
        minToTray_ = yes;
    }

    bool hasTransparentTree() const {
        return transparentTree_;
    }
    void makeTreeTransparent (bool trans);

    bool hasSmallToolbarIcons() const {
        return smallToolbarIcons_;
    }
    void setToolBarIconSize (bool small);

    bool withoutToolbar() const {
        return noToolbar_;
    }
    bool withoutMenubar() const {
        return noMenubar_;
    }
    void showToolbar (bool show);
    void showMenubar (bool show);

    bool isWrappedByDefault() const {
        return wrapByDefault_;
    }
    void wrapByDefault (bool wrap) {
        wrapByDefault_ = wrap;
    }

    bool isIndentedByDefault() const {
        return indentByDefault_;
    }
    void indentByDefault (bool indent) {
        indentByDefault_ = indent;
    }

    bool hasAutoBracket() const {
        return autoBracket_;
    }
    void autoBracket (bool yes) {
        autoBracket_ = yes;
    }

    bool hasAutoReplace() const {
        return autoReplace_;
    }
    void autoReplace (bool yes) {
        autoReplace_ = yes;
    }

    int getAutoSave() const {
        return autoSave_;
    }
    void setAutoSave (int interval) {
        autoSave_ = interval;
    }

    bool isScrollJumpWorkaroundEnabled() const {
        return scrollJumpWorkaround_;
    }
    void enableScrollJumpWorkaround (bool enable);

    void updateCustomizableShortcuts();

    QHash<QString, QString> customShortcutActions() const {
        return customActions_;
    }
    void setActionShortcut (const QString &action, const QString &shortcut) {
        customActions_.insert (action, shortcut);
    }
    void removeShortcut (const QString &action) {
        customActions_.remove (action);
        uncustomizedActions_ << action;
    }

    QHash<QAction*, QKeySequence> defaultShortcuts() const {
        return defaultShortcuts_;
    }

    QStringList reservedShortcuts() const {
        return reservedShortcuts_;
    }

private slots:
    bool close();
    void checkTray();
    void showContextMenu (const QPoint &p);
    void fullScreening();
    void defaultSize();
    void rehighlight (TextEdit *textEdit);
    void zoomingIn();
    void zoomingOut();
    void unZooming();
    void newNote();
    void openFile();
    void openFNDoc (const QString &filePath);
    void autoSaving();
    bool saveFile();
    void undoing();
    void redoing();
    void cutText();
    void copyText();
    void pasteText();
    void pasteHTML();
    void deleteText();
    void selectAllText();
    void setCursorInsideSelection (bool sel);
    void txtContextMenu (const QPoint &p);
    void copyLink();
    void selChanged (const QItemSelection &selected, const QItemSelection&);
    void setSaveEnabled (bool modified);
    void setUndoEnabled (bool enabled);
    void setRedoEnabled (bool enabled);
    void formatChanged (const QTextCharFormat &format);
    void alignmentChanged();
    void directionChanged();
    void makeBold();
    void makeItalic();
    void makeUnderlined();
    void makeStriked();
    void makeSuperscript();
    void makeSubscript();
    void textColor();
    void bgColor();
    void clearFormat();
    void textAlign (QAction *a);
    void textDirection (QAction *a);
    void makeHeader();
    void insertLink();
    void embedImage();
    void imageEmbed (const QString &path);
    void setImagePath (bool);
    void scaleImage();
    void saveImage();
    void addTable();
    void tableMergeCells();
    void tablePrependRow();
    void tableAppendRow();
    void tablePrependCol();
    void tableAppendCol();
    void tableDeleteRow();
    void tableDeleteCol();
    void expandAll();
    void collapseAll();
    void newNode();
    void deleteNode();
    void moveUpNode();
    void moveLeftNode();
    void moveDownNode();
    void moveRightNode();
    void handleTags();
    void renameNode();
    void nodeIcon();
    void toggleStatusBar();
    void textFontDialog();
    void nodeFontDialog();
    void toggleWrapping();
    void toggleIndent();
    void prefDialog();
    void noteModified();
    void docProp();
    void nodeChanged (const QModelIndex&, const QModelIndex&);
    void showHideSearch();
    void clearTagsList (int);
    void selectRow (QListWidgetItem *item);
    void chooseRow (int row);
    void find();
    void hlight() const;
    void scrolled (int) const;
    void setSearchFlags();
    void allBtn (bool checked);
    void tagsAndNamesBtn (bool checked);
    void replaceDock();
    void closeReplaceDock (bool visible);
    void resizeDock (bool topLevel);
    void replace();
    void replaceAll();
    void showAndFocus();
    void stealFocus();
    void trayActivated (QSystemTrayIcon::ActivationReason r);
    void activateTray();
    void txtPrint();
    void exportHTML();
    void setHTMLName (bool checked);
    void setHTMLPath (bool);
    void setPswd();
    void reallySetPswrd();
    void checkPswrd();
    void aboutDialog();
    void showHelpDialog();
    void closeTagsDialog();

private:
    void enableActions (bool enable);
    void fileOpen (const QString &filePath);
    bool fileSave (const QString &filePath);
    void createTrayIcon();
    void closeEvent (QCloseEvent *event);
    void resizeEvent (QResizeEvent *event);
    void showEvent (QShowEvent *event);
    void showDoc (QDomDocument &doc);
    void setTitle (const QString& fname);
    void notSaved();
    void setNodesTexts();
    bool unSaved (bool modified);
    TextEdit *newWidget();
    void mergeFormatOnWordOrSelection (const QTextCharFormat &format);
    void setNewFont (DomItem *item, QTextCharFormat &fmt);
    QTextCursor finding (const QString& str,
                         const QTextCursor& start,
                         QTextDocument::FindFlags flags) const;
    void findInTags();
    void reallySetSearchFlags (bool h);
    void findInNames();
    bool isImageSelected();
    void readShortcuts();
    QString validatedShortcut (const QVariant v);
    void readAndApplyConfig (bool startup = true);
    QString nodeAddress (QModelIndex index);
    bool isPswrdCorrect();
    void dragMoveEvent (QDragMoveEvent *event);
    void dragEnterEvent (QDragEnterEvent *event);
    void dropEvent (QDropEvent *event);

    Ui::FN *ui;
    bool isX11_;
    //QWidget *dummyWidget; // For hiding the main window while keeping all its state info.
    QSystemTrayIcon *tray_;
    bool quitting_;
    int trayCounter_; // Used when waiting for the system tray to be created at startup.
    int saveNeeded_;
    QFont defaultFont_, nodeFont_;
    QColor lastTxtColor_, lastBgColor_;
    DomModel *model_;
    QString xmlPath_;
    LineEdit *ImagePathEntry_, *htmlPahEntry_;
    /* By pairing each widget with a DOM item, we won't need to worry
       about keeping the correspondence between widgets and nodes: */
    QHash<DomItem*,TextEdit*> widgets_;
    QTextDocument::FindFlags searchFlags_; // Whole word and case sensitivity flags.
    QHash<TextEdit*,QString> searchEntries_; // Search entries, one for each QTextEdit.
    bool searchingOtherNode_; // Needed when jumping to another node during search.
    bool rplOtherNode_; // Like above but for replacement.
    int replCount_; // Needed for counting replacements in all nodes.
    QHash<TextEdit*,QList<QTextEdit::ExtraSelection> > greenSels_; // For replaced matches.
    QString txtReplace_; // The replacing text.
    QModelIndexList tagsList_;
    QString linkAtPos_; // Text hyperlink at the right-click position.
    QTextTable *txtTable_; // Text table at the right-click position.
    int imgScale_; QString lastImgPath_;
    bool shownBefore_,
         remSize_, remSplitter_,
         remPosition_,
         hasTray_,
         minToTray_,
         wrapByDefault_,
         indentByDefault_,
         transparentTree_,
         smallToolbarIcons_,
         noToolbar_,
         noMenubar_,
         autoBracket_,
         autoReplace_,
         treeViewDND_;
    int autoSave_;
    QPoint position_; // Excluding the window frame.
    QSize winSize_, startSize_, prefSize_;
    //QList<int> splitterSizes_;
    QByteArray splitterSizes_;
    QTimer *timer_;
    QString pswrd_;
    bool scrollJumpWorkaround_; // Should a workaround for Qt5's "scroll jump" bug be applied?
    bool underE_; // Is FeatherNotes running under Enlightenment?
    QSize EShift_; // The shift Enlightenment's panel creates (a bug?).
    QHash<QString, QString> customActions_;
    QHash<QAction*, QKeySequence> defaultShortcuts_;
    QStringList uncustomizedActions_, reservedShortcuts_;
};

}

#endif // FN_H
