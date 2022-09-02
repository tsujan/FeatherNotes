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

#ifndef SINGLETON_H
#define SINGLETON_H

#include <QApplication>
#include "fn.h"
//#include "config.h"

namespace FeatherNotes {

// A single-instance approach based on QSharedMemory.
class FNSingleton : public QApplication
{
    Q_OBJECT
public:
    FNSingleton (int &argc, char **argv);
    ~FNSingleton();

    QList<FN*> Wins; // All FeatherNotes windows.

    void sendInfo (const QStringList &info);
    void openFile (const QStringList &info);
    void removeWin (FN *win);

    bool isPrimaryInstance() const {
        return isPrimaryInstance_;
    }
    bool isX11() const {
        return isX11_;
    }
    bool isWayland() const {
        return isWayland_;
    }

    bool isTrayChecked() const {
        return trayChecked_;
    }
    void setTrayChecked() {
        trayChecked_ = true;
    }
    bool isQuitSignalReceived() const {
        return quitSignalReceived_;
    }

    void setLastFile (const QString &lastFile) {
        lastFile_ = lastFile;
    }

public slots:
    void quitSignalReceived();
    void quitting();

private:
    bool quitSignalReceived_;
    bool isPrimaryInstance_;
    bool isX11_;
    bool isWayland_;
    bool trayChecked_;
    QString lastFile_;
};

}

#endif // SINGLETON_H
