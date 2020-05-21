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

#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

#include <QString>
#include <QHash>

class Hunspell;

namespace FeatherNotes {

class SpellChecker {
public:
    SpellChecker (const QString& dictionaryPath, const QString& userDictionary);
    ~SpellChecker();

    bool spell (const QString& word);
    QStringList suggest (const QString& word);
    void ignoreWord (const QString& word);
    void addToUserWordlist (const QString& word);

    void addToCorrections (const QString& misspelled, const QString& correct) {
        corrections_.insert (misspelled, correct);
    }
    QString correct (const QString& misspelled) const {
        return corrections_.value (misspelled);
    }

private:
    Hunspell *hunspell_;
    QString userDictionary_;
    QTextCodec *codec_;
    QHash<QString, QString> corrections_;
};

}

#endif // SPELLCHECKER_H
