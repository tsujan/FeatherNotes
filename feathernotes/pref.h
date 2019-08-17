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

#ifndef PREF_H
#define PREF_H

#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QKeySequenceEdit>
#include <QTimer>

namespace FeatherNotes {

class FNKeySequenceEdit : public QKeySequenceEdit
{
    Q_OBJECT

public:
    FNKeySequenceEdit (QWidget *parent = nullptr);

protected:
    virtual void keyPressEvent (QKeyEvent *event);
};


class Delegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    Delegate (QObject *parent = nullptr);

    virtual QWidget* createEditor (QWidget *parent,
                                   const QStyleOptionViewItem&,
                                   const QModelIndex&) const;

protected:
    virtual bool eventFilter (QObject *object, QEvent *event);
};

namespace Ui {
class PrefDialog;
}

class PrefDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrefDialog (QWidget *parent = nullptr);
    ~PrefDialog();

private slots:
    void onClosing();
    void prefSize (int value);
    void onShortcutChange (QTableWidgetItem *item);
    void restoreDefaultShortcuts();

private:
    void closeEvent (QCloseEvent *event);
    void showPrompt (const QString& str = QString(), bool temporary = false);

    Ui::PrefDialog *ui;
    QWidget * parent_;
    QHash<QString, QString> shortcuts_, newShortcuts_;
    QString prevtMsg_;
    bool hasTray_, autoBracket_;
    QTimer *promptTimer_;
};

}

#endif // PREF_H
