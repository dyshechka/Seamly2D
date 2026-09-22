/******************************************************************************
*   @file   measurements_old_format_converter.cpp
**  @brief  Converts an old-format individual measurements file to the
**          current one. See the header for the full rule set.
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
#include "measurements_old_format_converter.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDate>
#include <QDomComment>
#include <QDomElement>
#include <QDomNodeList>
#include <QFile>
#include <QIODevice>
#include <QLatin1Char>
#include <QLatin1String>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

#include "../ifc/xml/vdomdocument.h"
#include "../vmisc/projectversion.h"

namespace
{
    // Any character the qmuparser formula grammar treats as part of an identifier (see
    // QmuFormulaBase::InitCharSets() in qmuformulabase.cpp): a Unicode letter or digit, "_",
    // "@", "#" or "'". Used to make sure a rename only ever matches a *whole* identifier token,
    // never a substring of a longer one (so renaming "Р" can't accidentally touch "Рост").
    const QString IdentBoundaryExclude = QStringLiteral("[\\w@#']");
}

//---------------------------------------------------------------------------------------------------------------------
bool OldFormatMeasurementsConverter::IsIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('@') ||
           c == QLatin1Char('#') || c == QLatin1Char('\'');
}

//---------------------------------------------------------------------------------------------------------------------
int OldFormatMeasurementsConverter::FindMatchingParen(const QString &s, int openIdx)
{
    int depth = 0;
    for (int i = openIdx; i < s.size(); ++i)
    {
        if (s.at(i) == QLatin1Char('('))
        {
            ++depth;
        }
        else if (s.at(i) == QLatin1Char(')'))
        {
            --depth;
            if (depth == 0)
            {
                return i;
            }
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
QString OldFormatMeasurementsConverter::StripRedundantParens(const QString &expr)
{
    QString result = expr.trimmed();
    while (result.size() >= 2 && result.at(0) == QLatin1Char('('))
    {
        const int close = FindMatchingParen(result, 0);
        if (close == result.size() - 1)
        {
            result = result.mid(1, result.size() - 2).trimmed();
        }
        else
        {
            break;
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QString OldFormatMeasurementsConverter::CollapseAtSign(const QString &name)
{
    if (name.startsWith(QLatin1String("@@")))
    {
        return name.mid(1); // "@@k" -> "@k": keep exactly one "@"
    }
    if (name.startsWith(QLatin1Char('@')))
    {
        return name.mid(1); // "@k" -> "k": remove entirely
    }
    return name;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Converts every old-style rint((<expr>) * 10^n) / 10^n call in formula to the new
/// rint(<expr>; n) syntax, bottom-up (a nested old-style call inside another one's argument is
/// converted first). A rint(...) call that isn't immediately followed by a divide-by-the-same-
/// value is left exactly as it was.
QString OldFormatMeasurementsConverter::ConvertRintSyntax(const QString &formula)
{
    static const QRegularExpression multRe(QStringLiteral("^(.*)\\*\\s*([0-9]+)\\s*$"));
    static const QRegularExpression powerOfTenRe(QStringLiteral("^10*$"));

    QString out;
    out.reserve(formula.size());
    int i = 0;
    const int n = formula.size();
    while (i < n)
    {
        const bool looksLikeRintCall = (i + 5 <= n) && formula.at(i) == QLatin1Char('r') &&
                                        formula.at(i + 1) == QLatin1Char('i') && formula.at(i + 2) == QLatin1Char('n') &&
                                        formula.at(i + 3) == QLatin1Char('t') && formula.at(i + 4) == QLatin1Char('(');
        const bool onBoundary = (i == 0) || !IsIdentChar(formula.at(i - 1));

        if (looksLikeRintCall && onBoundary)
        {
            const int openIdx = i + 4; // index of '('
            const int closeIdx = FindMatchingParen(formula, openIdx);
            if (closeIdx == -1)
            {
                out += formula.at(i);
                ++i;
                continue;
            }

            const QString argsRaw = formula.mid(openIdx + 1, closeIdx - openIdx - 1);
            const QString argsConverted = ConvertRintSyntax(argsRaw); // nested rint() first

            bool converted = false;
            const QRegularExpressionMatch mm = multRe.match(argsConverted);
            if (mm.hasMatch())
            {
                QString innerExpr = mm.captured(1).trimmed();
                const QString multStr = mm.captured(2);
                if (powerOfTenRe.match(multStr).hasMatch()) // a power of ten: 1, 10, 100, 1000...
                {
                    const QString after = formula.mid(closeIdx + 1);
                    const QRegularExpression afterRe(QStringLiteral("^\\s*/\\s*") + QRegularExpression::escape(multStr) +
                                                      QStringLiteral("(?![0-9])"));
                    const QRegularExpressionMatch am = afterRe.match(after);
                    if (am.hasMatch())
                    {
                        const int precision = multStr.size() - 1;
                        innerExpr = StripRedundantParens(innerExpr);
                        out += QStringLiteral("rint(%1; %2)").arg(innerExpr).arg(precision);
                        i = closeIdx + 1 + am.capturedLength(0);
                        converted = true;
                    }
                }
            }

            if (!converted)
            {
                out += QLatin1String("rint(") + argsConverted + QLatin1Char(')');
                i = closeIdx + 1;
            }
            continue;
        }

        out += formula.at(i);
        ++i;
    }
    return out;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Rewrites every whole-identifier-token occurrence of a renamed measurement name inside
/// value (a formula) to its new name, in a single pass so a freshly-inserted replacement can
/// never be re-matched by a later rename in the same formula.
QString OldFormatMeasurementsConverter::ApplyRenames(const QString &value, const QHash<QString, QString> &renameMap)
{
    if (renameMap.isEmpty())
    {
        return value;
    }

    QStringList names = renameMap.keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b)
    {
        return a.size() > b.size();
    });

    QStringList escaped;
    escaped.reserve(names.size());
    for (const QString &nm : qAsConst(names))
    {
        escaped.append(QRegularExpression::escape(nm));
    }

    const QString pattern = QStringLiteral("(?<!%1)(?:%2)(?!%1)").arg(IdentBoundaryExclude, escaped.join(QLatin1Char('|')));
    const QRegularExpression re(pattern, QRegularExpression::UseUnicodePropertiesOption);

    QString result;
    result.reserve(value.size());
    int lastEnd = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(value);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        result += value.mid(lastEnd, m.capturedStart() - lastEnd);
        result += renameMap.value(m.captured(0));
        lastEnd = m.capturedEnd();
    }
    result += value.mid(lastEnd);
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Decides the final name for every measurement: "@"/"@@" collapsed per CollapseAtSign(),
/// except that any measurement whose collapsed name would collide with another measurement's
/// (collapsed or already-plain) name keeps its original name untouched, with a warning. Runs to
/// a fixed point, since reverting one collision can in rare cases create another.
void OldFormatMeasurementsConverter::ComputeRenameMap(const QStringList &names, QStringList &finalNames,
                                                        QStringList &warnings)
{
    const int n = names.size();
    QVector<bool> reverted(n, false);

    bool changed = true;
    while (changed)
    {
        changed = false;

        QStringList candidate;
        candidate.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            candidate.append(reverted.at(i) ? names.at(i) : CollapseAtSign(names.at(i)));
        }

        QHash<QString, int> counts;
        for (const QString &c : qAsConst(candidate))
        {
            ++counts[c];
        }

        for (int i = 0; i < n; ++i)
        {
            if (!reverted.at(i) && counts.value(candidate.at(i)) > 1)
            {
                reverted[i] = true;
                changed = true;
            }
        }
    }

    finalNames.clear();
    finalNames.reserve(n);
    warnings.clear();
    for (int i = 0; i < n; ++i)
    {
        const QString collapsed = CollapseAtSign(names.at(i));
        if (reverted.at(i))
        {
            finalNames.append(names.at(i));
            if (names.at(i) != collapsed)
            {
                warnings.append(QCoreApplication::translate("OldFormatMeasurementsConverter",
                    "«%1»: после удаления «@» это имя совпало бы с другой меркой в этом же файле "
                    "-- оставлено без изменений.").arg(names.at(i)));
            }
        }
        else
        {
            finalNames.append(collapsed);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
OldFormatMeasurementsConverter::Report OldFormatMeasurementsConverter::Convert(const QString &sourcePath,
                                                                                const QString &destPath)
{
    Report report;

    if (sourcePath == destPath)
    {
        report.errorMessage = QCoreApplication::translate("OldFormatMeasurementsConverter",
            "Файл результата должен отличаться от исходного файла.");
        return report;
    }

    QFile inFile(sourcePath);
    if (!inFile.open(QIODevice::ReadOnly))
    {
        report.errorMessage = QCoreApplication::translate("OldFormatMeasurementsConverter",
            "Не удалось открыть исходный файл для чтения.");
        return report;
    }

    VDomDocument doc;
    QString parseError;
    int errorLine = 0;
    int errorColumn = 0;
    const bool parsed = doc.setContent(&inFile, false, &parseError, &errorLine, &errorColumn);
    inFile.close();
    if (!parsed)
    {
        report.errorMessage = QCoreApplication::translate("OldFormatMeasurementsConverter",
            "Файл не похож на файл мерок (%1, строка %2).").arg(parseError).arg(errorLine);
        return report;
    }

    QDomElement root = doc.documentElement();
    const QDomNodeList mList = root.elementsByTagName(QStringLiteral("m"));
    const int n = mList.size();

    QStringList names;
    names.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        names.append(mList.at(i).toElement().attribute(QStringLiteral("name")));
    }

    QStringList finalNames;
    ComputeRenameMap(names, finalNames, report.collisionWarnings);

    QHash<QString, QString> renameMap;
    for (int i = 0; i < n; ++i)
    {
        if (names.at(i) != finalNames.at(i))
        {
            renameMap.insert(names.at(i), finalNames.at(i));
        }
    }
    report.renamedCount = renameMap.size();

    for (int i = 0; i < n; ++i)
    {
        QDomElement m = mList.at(i).toElement();
        m.setAttribute(QStringLiteral("name"), finalNames.at(i));

        const QString oldValue = m.attribute(QStringLiteral("value"));
        if (oldValue.isEmpty())
        {
            continue;
        }

        const QString renamedValue = ApplyRenames(oldValue, renameMap);
        const QString finalValue = ConvertRintSyntax(renamedValue);

        if (finalValue != renamedValue)
        {
            ++report.rintConvertedCount;
        }
        if (finalValue != oldValue)
        {
            m.setAttribute(QStringLiteral("value"), finalValue);
        }
    }

    // Record that this file went through the converter, for future comparison. This has to be an
    // XML *comment*, not an element: opening a .smis file runs it through IndividualSizeConverter,
    // which validates it against a strict XSD (no wildcard/"any" content) via Xerces -- any element
    // the schema doesn't declare (we tried a <conversion-info> element first) makes the file fail to
    // open at all, with "no declaration found for element ...". A comment is invisible to that
    // validation, same as the pre-existing "Measurements created with SeamlyMe v..." comment already
    // at the top of the file. We don't touch that first comment or <version> itself -- <version>
    // belongs to IndividualSizeConverter's own, unrelated format-migration logic, and the first
    // comment is what MeasurementDoc::SaveDocument() rewrites on every save from inside the app.
    const QString conversionComment = QStringLiteral(" Converted from the old format (\"@\" + old rint() syntax) "
                                                       "by SeamlyMe v%1 on %2. ")
                                           .arg(APP_VERSION_STR, QDate::currentDate().toString(Qt::ISODate));
    const QDomComment conversionInfo = doc.createComment(conversionComment);

    const QDomElement versionEl = root.firstChildElement(VDomDocument::TagVersion);
    if (!versionEl.isNull())
    {
        root.insertAfter(conversionInfo, versionEl);
    }
    else
    {
        root.insertBefore(conversionInfo, root.firstChild());
    }

    QString saveError;
    if (!doc.SaveDocument(destPath, saveError))
    {
        report.errorMessage = saveError.isEmpty()
            ? QCoreApplication::translate("OldFormatMeasurementsConverter", "Не удалось сохранить итоговый файл.")
            : saveError;
        return report;
    }

    return report;
}
