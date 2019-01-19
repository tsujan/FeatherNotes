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

namespace FeatherNotes {

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
    void prefSize (int value);


private:
    void closeEvent (QCloseEvent *event);

    Ui::PrefDialog *ui;
    QWidget * parent_;
};

}

#endif // PREF_H
