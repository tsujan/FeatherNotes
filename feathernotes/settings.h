/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016 <tsujan2000@gmail.com>
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>

namespace FeatherNotes {

// Prevent redundant writings!
class Settings : public QSettings
{
    Q_OBJECT
public:
    Settings (const QString &organization, const QString &application = QString(), QObject *parent = nullptr)
             : QSettings (organization, application, parent) {}

    void setValue (const QString &key, const QVariant &v) {
        if (value (key) == v)
            return;
        QSettings::setValue (key, v);
    }
};

}

#endif // SETTINGS_H
