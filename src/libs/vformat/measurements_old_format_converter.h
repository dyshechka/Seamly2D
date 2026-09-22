/******************************************************************************
*   @file   measurements_old_format_converter.h
**  @brief  Converts an old-format individual measurements file to the
**          current one (see tz_slovar_merok): "@" is no longer required on
**          custom measurement names, and rint() takes its rounding
**          precision as a second argument instead of being wrapped in an
**          outer multiply/divide.
**  @copyright
**  This source code is part of the Seamly2D project, a pattern making
**  program to create and model patterns of clothing.
**  Copyright (C) 2017-2024 Seamly2D project
**  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
**
**  Seamly2D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Seamly2D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/
#ifndef MEASUREMENTS_OLD_FORMAT_CONVERTER_H
#define MEASUREMENTS_OLD_FORMAT_CONVERTER_H

#include <QChar>
#include <QHash>
#include <QString>
#include <QStringList>

/// @brief One-shot conversion of an old-format ("@" required, old rint() idiom) individual
/// measurements file to a new file in the current format. Never touches the source file --
/// Convert() always writes to a different destination path.
///
/// The rules (agreed with Марта -- see tz_slovar_merok and her "Сконвертировать" request):
///  - A single leading "@" is removed from every measurement name; "@@" collapses to a single
///    "@" instead of being removed entirely (so "@@k" -> "@k", "@k" -> "k").
///  - Every reference to a renamed name inside every OTHER measurement's formula is updated to
///    match, using whole-identifier-token matching (never a naive text replace) so a rename of
///    "Р" can never accidentally touch "Рост".
///  - If stripping "@" from a name would collide with another measurement's name in the same
///    file, that specific measurement is left untouched (its "@" stays) and a warning is
///    reported -- the conversion itself is never aborted over a collision.
///  - Old-style "rint((<expr>) * 10^n) / 10^n" (decimal-place truncation done by hand, via an
///    outer multiply/divide by a matching power of ten) becomes "rint(<expr>; n)". This is
///    applied bottom-up, so a nested old-style rint() call inside another one's argument is
///    converted first. A look-alike that does not divide back by the same value right after the
///    rint(...) call -- e.g. rounding a percentage for a comparison, "rint(x * 100) > 15" -- is
///    left exactly as it was, since it isn't decimal-place truncation.
class OldFormatMeasurementsConverter
{
public:
    struct Report
    {
        int renamedCount = 0;
        int rintConvertedCount = 0;
        QStringList collisionWarnings; ///< human-readable, one line per measurement left with its "@"
        QString errorMessage;          ///< non-empty on failure; nothing else in Report is meaningful then
    };

    static Report Convert(const QString &sourcePath, const QString &destPath);

    // Exposed for reuse/testing.
    static QString CollapseAtSign(const QString &name);
    static QString ConvertRintSyntax(const QString &formula);
    static QString ApplyRenames(const QString &value, const QHash<QString, QString> &renameMap);

private:
    static void ComputeRenameMap(const QStringList &names, QStringList &finalNames, QStringList &warnings);
    static QString StripRedundantParens(const QString &expr);
    static int FindMatchingParen(const QString &s, int openIdx);
    static bool IsIdentChar(QChar c);
};

#endif // MEASUREMENTS_OLD_FORMAT_CONVERTER_H
