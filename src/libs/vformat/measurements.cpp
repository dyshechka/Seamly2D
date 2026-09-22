/******************************************************************************
*   @file   measurements.cpp
**  @author Douglas S Caskey
**  @date   25 Jan, 2024
**
**  @brief
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

/************************************************************************
 **
 **  @file   vmeasurements.cpp
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   14 7, 2015
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentina project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2013-2015 Valentina project
 **  <https://bitbucket.org/dismine/valentina> All Rights Reserved.
 **
 **  Valentina is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Valentina is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Valentina.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "measurements.h"

#include <qnumeric.h>
#include <QDate>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QLatin1Char>
#include <QMessageLogger>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QtDebug>

#include "../ifc/exception/vexceptionemptyparameter.h"
#include "../ifc/xml/individual_size_converter.h"
#include "../ifc/xml/multi_size_converter.h"
#include "../ifc/ifcdef.h"
#include "../qmuparser/qmutokenparser.h"
#include "../qmuparser/qmuparsererror.h"
#include "../vpatterndb/calculator.h"
#include "../vpatterndb/variables/measurement_variable.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/measurements_def.h"
#include "../vpatterndb/knit_measurements.h"
#include "../vpatterndb/pmsystems.h"
#include "../vmisc/projectversion.h"

const QString MeasurementDoc::TagVST              = QStringLiteral("vst");
const QString MeasurementDoc::TagVIT              = QStringLiteral("vit");
const QString MeasurementDoc::TagSMMS             = QStringLiteral("smms");
const QString MeasurementDoc::TagSMIS             = QStringLiteral("smis");
const QString MeasurementDoc::TagBodyMeasurements = QStringLiteral("body-measurements");
const QString MeasurementDoc::TagNotes            = QStringLiteral("notes");
const QString MeasurementDoc::TagSize             = QStringLiteral("size");
const QString MeasurementDoc::TagHeight           = QStringLiteral("height");
const QString MeasurementDoc::TagPersonal         = QStringLiteral("personal");
const QString MeasurementDoc::TagFamilyName       = QStringLiteral("family-name");
const QString MeasurementDoc::TagGivenName        = QStringLiteral("given-name");
const QString MeasurementDoc::TagBirthDate        = QStringLiteral("birth-date");
const QString MeasurementDoc::TagGender           = QStringLiteral("gender");
const QString MeasurementDoc::TagPMSystem         = QStringLiteral("pm_system");
const QString MeasurementDoc::TagEmail            = QStringLiteral("email");
const QString MeasurementDoc::TagReadOnly         = QStringLiteral("read-only");
const QString MeasurementDoc::TagMeasurement      = QStringLiteral("m");

const QString MeasurementDoc::AttrBase            = QStringLiteral("base");
const QString MeasurementDoc::AttrValue           = QStringLiteral("value");
const QString MeasurementDoc::AttrSizeIncrease    = QStringLiteral("size_increase");
const QString MeasurementDoc::AttrHeightIncrease  = QStringLiteral("height_increase");
const QString MeasurementDoc::AttrDescription     = QStringLiteral("description");
const QString MeasurementDoc::AttrName            = QStringLiteral("name");
const QString MeasurementDoc::AttrFullName        = QStringLiteral("full_name");
const QString MeasurementDoc::AttrIsSection       = QStringLiteral("isSection");

const QString MeasurementDoc::GenderMale          = QStringLiteral("male");
const QString MeasurementDoc::GenderFemale        = QStringLiteral("female");
const QString MeasurementDoc::GenderUnknown       = QStringLiteral("unknown");

const QString defBirthDate                        = QStringLiteral("1800-01-01");

namespace
{
//---------------------------------------------------------------------------------------------------------------------
QString FileComment()
{
    return QString("Measurements created with SeamlyMe v%1 (https://seamly.io/).").arg(APP_VERSION_STR);
}
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementDoc::MeasurementDoc(VContainer *data)
    :VDomDocument()
    , data(data)
    , type(MeasurementsType::Unknown)
    , m_currentSize(nullptr)
    , m_currentHeight(nullptr)
{
    SCASSERT(data != nullptr)
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementDoc::MeasurementDoc(Unit unit, VContainer *data)
    :VDomDocument()
    , data(data)
    , type(MeasurementsType::Individual)
    , m_currentSize(nullptr)
    , m_currentHeight(nullptr)
{
    SCASSERT(data != nullptr)

    CreateEmptyIndividualFile(unit);
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementDoc::MeasurementDoc(Unit unit, int baseSize, int baseHeight, VContainer *data)
    :VDomDocument()
    , data(data)
    , type(MeasurementsType::Multisize)
    , m_currentSize(nullptr)
    , m_currentHeight(nullptr)
{
    SCASSERT(data != nullptr)

    CreateEmptyMultisizeFile(unit, baseSize, baseHeight);
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::setXMLContent(const QString &fileName)
{
    VDomDocument::setXMLContent(fileName);
    type = ReadType();
}

//---------------------------------------------------------------------------------------------------------------------
bool MeasurementDoc::SaveDocument(const QString &fileName, QString &error)
{
    // Update comment with Seamly2D version
    QDomNode commentNode = documentElement().firstChild();
    if (commentNode.isComment())
    {
        QDomComment comment = commentNode.toComment();
        comment.setData(FileComment());
    }

    return VDomDocument::SaveDocument(fileName, error);
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::addEmpty(const QString &name, const QString &formula)
{
    const QDomElement element = MakeEmpty(name, formula);

    const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
    list.at(0).appendChild(element);
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::AddEmptyAfter(const QString &after, const QString &name, const QString &formula)
{
    const QDomElement element = MakeEmpty(name, formula);
    const QDomElement sibling = FindM(after);

    const QDomNodeList list = elementsByTagName(TagBodyMeasurements);

    if (sibling.isNull())
    {
        list.at(0).appendChild(element);
    }
    else
    {
        list.at(0).insertAfter(element, sibling);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::Remove(const QString &name)
{
    const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
    list.at(0).removeChild(FindM(name));
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::MoveTop(const QString &name)
{
    const QDomElement node = FindM(name);
    if (not node.isNull())
    {
        const QDomNodeList mList = elementsByTagName(TagMeasurement);
        if (mList.size() >= 2)
        {
            const QDomNode top = mList.at(0);
            if (not top.isNull())
            {
                const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
                list.at(0).insertBefore(node, top);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::MoveUp(const QString &name)
{
    const QDomElement node = FindM(name);
    if (not node.isNull())
    {
        const QDomElement prSibling = node.previousSiblingElement(TagMeasurement);
        if (not prSibling.isNull())
        {
            const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
            list.at(0).insertBefore(node, prSibling);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::MoveDown(const QString &name)
{
    const QDomElement node = FindM(name);
    if (not node.isNull())
    {
        const QDomElement nextSibling = node.nextSiblingElement(TagMeasurement);
        if (not nextSibling.isNull())
        {
            const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
            list.at(0).insertAfter(node, nextSibling);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::MoveBottom(const QString &name)
{
    const QDomElement node = FindM(name);
    if (not node.isNull())
    {
        const QDomNodeList mList = elementsByTagName(TagMeasurement);
        if (mList.size() >= 2)
        {
            const QDomNode bottom = mList.at(mList.size()-1);
            if (not bottom.isNull())
            {
                const QDomNodeList list = elementsByTagName(TagBodyMeasurements);
                list.at(0).insertAfter(node, bottom);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::readMeasurements() const
{
    // For conversion values we must first calculate all data in measurement file's unit.
    // That's why we need two containers: one for converted values, second for real data.

    // Container for values in measurement file's unit
    QScopedPointer<VContainer> tempData(new VContainer(data->getTranslateVariables(), data->GetPatternUnit()));

    const QDomNodeList list = elementsByTagName(TagMeasurement);
    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();

        const QString name = GetParametrString(dom, AttrName);

        QString description;
        try
        {
            description = GetParametrString(dom, AttrDescription);
        }
        catch (VExceptionEmptyParameter &error)
        {
            Q_UNUSED(error)
        }

        QString fullName;
        try
        {
            fullName = GetParametrString(dom, AttrFullName);
        }
        catch (VExceptionEmptyParameter &error)
        {
            Q_UNUSED(error)
        }

        // Files normally don't repeat a knitting measurement's full name and
        // description (that's the point of the dictionary) -- fill them in
        // here so every display path shows the same text regardless of
        // whether this measurement counts as "custom". An explicit value
        // already in the file always wins.
        if (IsKnitMeasurement(name))
        {
            if (fullName.isEmpty())
            {
                fullName = KnitFullName(name);
            }
            if (description.isEmpty())
            {
                description = KnitDescription(name);
            }
        }

        QSharedPointer<MeasurementVariable> meash;
        QSharedPointer<MeasurementVariable> tempMeash;
        if (type == MeasurementsType::Multisize)
        {
            qreal base = GetParametrDouble(dom, AttrBase, "0");
            qreal ksize = GetParametrDouble(dom, AttrSizeIncrease, "0");
            qreal kheight = GetParametrDouble(dom, AttrHeightIncrease, "0");

            tempMeash = QSharedPointer<MeasurementVariable>(new MeasurementVariable(static_cast<quint32>(i), name, BaseSize(),
                                                                      BaseHeight(), base, ksize, kheight, fullName, description));
            tempMeash->setSize(m_currentSize);
            tempMeash->setHeight(m_currentHeight);
            tempMeash->SetUnit(data->GetPatternUnit());

            base = UnitConvertor(base, measurementUnits(), *data->GetPatternUnit());
            ksize = UnitConvertor(ksize, measurementUnits(), *data->GetPatternUnit());
            kheight = UnitConvertor(kheight, measurementUnits(), *data->GetPatternUnit());

            const qreal baseSize = UnitConvertor(BaseSize(), measurementUnits(), *data->GetPatternUnit());
            const qreal baseHeight = UnitConvertor(BaseHeight(), measurementUnits(), *data->GetPatternUnit());

            meash = QSharedPointer<MeasurementVariable>(new MeasurementVariable(static_cast<quint32>(i), name, baseSize, baseHeight,
                                                                  base, ksize, kheight, fullName, description));
            meash->setSize(m_currentSize);
            meash->setHeight(m_currentHeight);
            meash->SetUnit(data->GetPatternUnit());
        }
        else
        {
            const QString formula = GetParametrString(dom, AttrValue, "0");
            bool ok = false;
            qreal value = EvalFormula(tempData.data(), formula, &ok);

            // Section-divider rows (see AttrIsSection/TMainWindow::checkBoxIsSection) are purely
            // organizational -- absent on older files, which is exactly "not a section" (false).
            const bool isSection = getParameterBool(dom, AttrIsSection, falseStr);

            if (isSection)
            {
                // A divider has no real value to hold, so force it to an obviously-wrong
                // sentinel instead of leaving whatever its formula/value happened to be before
                // it was turned into a divider (which could easily look like a plausible real
                // number). This is a last-resort floor: TMainWindow::FormulaReferencesSection()
                // is what actually stops a formula from using a divider, but if one ever slips
                // through anyway, -100000 is impossible to mistake for a legitimate result.
                value = -100000;
            }

            tempMeash = QSharedPointer<MeasurementVariable>(new MeasurementVariable(tempData.data(), static_cast<quint32>(i), name,
                                                                      value, formula, ok, fullName, description, QString(),
                                                                      isSection));

            value = isSection ? -100000 : UnitConvertor(value, measurementUnits(), *data->GetPatternUnit());
            meash = QSharedPointer<MeasurementVariable>(new MeasurementVariable(data, static_cast<quint32>(i), name, value, formula,
                                                                  ok, fullName, description, QString(), isSection));
        }
        tempData->AddVariable(name, tempMeash);
        data->AddVariable(name, meash);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::ClearForExport()
{
    const QDomNodeList list = elementsByTagName(TagMeasurement);

    for (int i=0; i < list.size(); ++i)
    {
        QDomElement domElement = list.at(i).toElement();
        if (domElement.isNull() == false)
        {
            if (qmu::QmuTokenParser::IsSingle(domElement.attribute(AttrValue)))
            {
                SetAttribute(domElement, AttrValue, QString("0"));
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementsType MeasurementDoc::Type() const
{
    return type;
}

//---------------------------------------------------------------------------------------------------------------------
int MeasurementDoc::BaseSize() const
{
    if (type == MeasurementsType::Multisize)
    {
        return static_cast<int>(UniqueTagAttr(TagSize, AttrBase, 50));
    }
    else
    {
        return 0;
    }
}

//---------------------------------------------------------------------------------------------------------------------
int MeasurementDoc::BaseHeight() const
{
    if (type == MeasurementsType::Multisize)
    {
        return static_cast<int>(UniqueTagAttr(TagHeight, AttrBase, 176));
    }
    else
    {
        return 0;
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::Notes() const
{
    return UniqueTagText(TagNotes, "");
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetNotes(const QString &text)
{
    if (not isReadOnly())
    {
        setTagText(TagNotes, text);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::FamilyName() const
{
    return UniqueTagText(TagFamilyName, "");
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetFamilyName(const QString &text)
{
    if (not isReadOnly())
    {
        setTagText(TagFamilyName, text);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::GivenName() const
{
    return UniqueTagText(TagGivenName, "");
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetGivenName(const QString &text)
{
    if (not isReadOnly())
    {
        setTagText(TagGivenName, text);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QDate MeasurementDoc::BirthDate() const
{
    return QDate::fromString(UniqueTagText(TagBirthDate, defBirthDate), "yyyy-MM-dd");
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetBirthDate(const QDate &date)
{
    if (not isReadOnly())
    {
        setTagText(TagBirthDate, date.toString("yyyy-MM-dd"));
    }
}

//---------------------------------------------------------------------------------------------------------------------
GenderType MeasurementDoc::Gender() const
{
    return StrToGender(UniqueTagText(TagGender, GenderUnknown));
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetGender(const GenderType &gender)
{
    if (not isReadOnly())
    {
        setTagText(TagGender, GenderToStr(gender));
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::PMSystem() const
{
    return UniqueTagText(TagPMSystem, ClearPMCode(p998_S));
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetPMSystem(const QString &system)
{
    if (not isReadOnly())
    {
        setTagText(TagPMSystem, ClearPMCode(system));
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::Email() const
{
    return UniqueTagText(TagEmail, "");
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetEmail(const QString &text)
{
    if (not isReadOnly())
    {
        setTagText(TagEmail, text);
    }
}

//---------------------------------------------------------------------------------------------------------------------
bool MeasurementDoc::isReadOnly() const
{
    return UniqueTagText(TagReadOnly, falseStr) == trueStr;
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetReadOnly(bool ro)
{
    if (ro)
    {
        setTagText(TagReadOnly, trueStr);
    }
    else
    {
        setTagText(TagReadOnly, falseStr);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::setSize(qreal *size)
{
    m_currentSize = size;
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::setHeight(qreal *height)
{
    m_currentHeight = height;
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMName(const QString &name, const QString &text)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrName, text);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMValue(const QString &name, const QString &text)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrValue, text);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMBaseValue(const QString &name, double value)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrBase, value);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMSizeIncrease(const QString &name, double value)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrSizeIncrease, value);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMHeightIncrease(const QString &name, double value)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrHeightIncrease, value);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMDescription(const QString &name, const QString &text)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrDescription, text);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMFullName(const QString &name, const QString &text)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute(node, AttrFullName, text);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::SetMIsSection(const QString &name, bool value)
{
    QDomElement node = FindM(name);
    if (not node.isNull())
    {
        SetAttribute<bool>(node, AttrIsSection, value);
    }
    else
    {
        qWarning() << tr("Can't find measurement '%1'").arg(name);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::GenderToStr(const GenderType &sex)
{
    switch (sex)
    {
        case GenderType::Male:
            return GenderMale;
        case GenderType::Female:
            return GenderFemale;
        case GenderType::Unknown:
        default:
            return GenderUnknown;
    }
}

//---------------------------------------------------------------------------------------------------------------------
GenderType MeasurementDoc::StrToGender(const QString &sex)
{
    const QStringList genders = QStringList() << GenderMale << GenderFemale << GenderUnknown;
    switch (genders.indexOf(sex))
    {
        case 0: // GenderMale
            return GenderType::Male;
        case 1: // GenderFemale
            return GenderType::Female;
        case 2: // GenderUnknown
        default:
            return GenderType::Unknown;
    }
}

//---------------------------------------------------------------------------------------------------------------------
QStringList MeasurementDoc::ListAll() const
{
    QStringList listNames;
    const QDomNodeList list = elementsByTagName(TagMeasurement);

    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement domElement = list.at(i).toElement();
        if (domElement.isNull() == false)
        {
            listNames.append(domElement.attribute(AttrName));
        }
    }

    return listNames;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList MeasurementDoc::listKnown() const
{
    QStringList listNames;
    const QStringList list = ListAll();
    for (int i=0; i < list.size(); ++i)
    {
        if (list.at(i).indexOf(CustomMSign) != 0)
        {
            listNames.append(list.at(i));
        }
    }

    return listNames;
}

//---------------------------------------------------------------------------------------------------------------------
bool MeasurementDoc::eachKnownNameIsValid() const
{
    QStringList names = AllGroupNames() + AllKnitGroupNames();

    QSet<QString> set;
    foreach (const QString &var, names)
    {
        set.insert(var);
    }

    names = listKnown();
    foreach (const QString &var, names)
    {
        if (not set.contains(var))
        {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Returns the name (the "name" attribute, not the human-readable full_name) of the first
/// known (non-"@") measurement in this file that eachKnownNameIsValid() would reject, or an empty
/// string if every known name is valid. Lets callers report *which* measurement is the problem
/// instead of the generic "file contains invalid measurement(s)" message.
QString MeasurementDoc::FirstUnknownName() const
{
    QStringList names = AllGroupNames() + AllKnitGroupNames();

    QSet<QString> set;
    foreach (const QString &var, names)
    {
        set.insert(var);
    }

    names = listKnown();
    foreach (const QString &var, names)
    {
        if (not set.contains(var))
        {
            return var;
        }
    }

    return QString();
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds every "known" (non-"@") measurement name in this file that isn't a built-in
/// sewing measurement and isn't already in the local knitting-measurement dictionary to that
/// dictionary, using this file's own full_name/description text for it.
///
/// This is the open-time counterpart to TMainWindow::RegisterNewKnitMeasurements() (which does
/// the same job but only runs when *saving*, reading full_name/description off the already-built
/// VContainer). This one reads straight off the file's own XML, so it works before the file has
/// been loaded into a container -- specifically, right before eachKnownNameIsValid() is checked
/// on open. Without it, a plain (no "@") measurement name that was never saved through this exact
/// dictionary before -- an older file, one authored on another machine, one typed by hand -- would
/// make eachKnownNameIsValid() reject the file outright, with no way to open it and save it once to
/// register the name normally. That defeats the point of dropping the "@" requirement (see
/// tz_slovar_merok): a plain custom measurement name is supposed to just work, the first time it's
/// seen, whether that's on save or on open.
void MeasurementDoc::RegisterUnknownKnitMeasurements() const
{
    if (type != MeasurementsType::Individual)
    {
        return; // the knitting dictionary is scoped to individual measurement files (tz_slovar_merok)
    }

    const QStringList sewingNames = AllGroupNames();
    const QDomNodeList list = elementsByTagName(TagMeasurement);

    for (int i = 0; i < list.size(); ++i)
    {
        const QDomElement dom = list.at(i).toElement();
        if (dom.isNull())
        {
            continue;
        }

        const QString name = dom.attribute(AttrName);
        if (name.isEmpty() || name.indexOf(CustomMSign) == 0 || sewingNames.contains(name) || IsKnitMeasurement(name))
        {
            continue; // empty, "@"-custom, a sewing measurement, or already registered
        }

        RegisterKnitMeasurement(name, dom.attribute(AttrFullName), dom.attribute(AttrDescription));
    }
}

//---------------------------------------------------------------------------------------------------------------------
VContainer *MeasurementDoc::GetData() const
{
    return data;
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::CreateEmptyMultisizeFile(Unit unit, int baseSize, int baseHeight)
{
    this->clear();
    QDomElement mElement = this->createElement(TagSMMS);

    mElement.appendChild(createComment(FileComment()));

    QDomElement version = createElement(TagVersion);
    const QDomText newNodeText = createTextNode(MultiSizeConverter::MeasurementMaxVerStr);
    version.appendChild(newNodeText);
    mElement.appendChild(version);

    QDomElement ro = createElement(TagReadOnly);
    const QDomText roNodeText = createTextNode("false");
    ro.appendChild(roNodeText);
    mElement.appendChild(ro);

    mElement.appendChild(createElement(TagNotes));

    QDomElement mUnit = createElement(TagUnit);
    const QDomText unitText = createTextNode(UnitsToStr(unit));
    mUnit.appendChild(unitText);
    mElement.appendChild(mUnit);

    QDomElement system = createElement(TagPMSystem);
    system.appendChild(createTextNode(ClearPMCode(p998_S)));
    mElement.appendChild(system);

    QDomElement size = createElement(TagSize);
    SetAttribute(size, AttrBase, QString().setNum(baseSize));
    mElement.appendChild(size);

    QDomElement height = createElement(TagHeight);
    SetAttribute(height, AttrBase, QString().setNum(baseHeight));
    mElement.appendChild(height);

    mElement.appendChild(createElement(TagBodyMeasurements));

    this->appendChild(mElement);
    insertBefore(createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""), this->firstChild());
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementDoc::CreateEmptyIndividualFile(Unit unit)
{
    this->clear();
    QDomElement mElement = this->createElement(TagSMIS);

    mElement.appendChild(createComment(FileComment()));

    QDomElement version = createElement(TagVersion);
    const QDomText newNodeText = createTextNode(IndividualSizeConverter::MeasurementMaxVerStr);
    version.appendChild(newNodeText);
    mElement.appendChild(version);

    QDomElement ro = createElement(TagReadOnly);
    const QDomText roNodeText = createTextNode("false");
    ro.appendChild(roNodeText);
    mElement.appendChild(ro);

    mElement.appendChild(createElement(TagNotes));

    QDomElement mUnit = createElement(TagUnit);
    mUnit.appendChild(createTextNode(UnitsToStr(unit)));
    mElement.appendChild(mUnit);

    QDomElement system = createElement(TagPMSystem);
    system.appendChild(createTextNode(ClearPMCode(p998_S)));
    mElement.appendChild(system);

    QDomElement personal = createElement(TagPersonal);
    personal.appendChild(createElement(TagFamilyName));
    personal.appendChild(createElement(TagGivenName));

    QDomElement date = createElement(TagBirthDate);
    date.appendChild(createTextNode(defBirthDate));
    personal.appendChild(date);

    QDomElement gender = createElement(TagGender);
    gender.appendChild(createTextNode(GenderToStr(GenderType::Unknown)));
    personal.appendChild(gender);

    personal.appendChild(createElement(TagEmail));
    mElement.appendChild(personal);

    mElement.appendChild(createElement(TagBodyMeasurements));

    this->appendChild(mElement);
    insertBefore(createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""), this->firstChild());
}

//---------------------------------------------------------------------------------------------------------------------
qreal MeasurementDoc::UniqueTagAttr(const QString &tag, const QString &attr, qreal defValue) const
{
    const qreal defVal = UnitConvertor(defValue, Unit::Cm, measurementUnits());

    const QDomNodeList nodeList = this->elementsByTagName(tag);
    if (nodeList.isEmpty())
    {
        return defVal;
    }
    else
    {
        const QDomNode domNode = nodeList.at(0);
        if (domNode.isNull() == false && domNode.isElement())
        {
            const QDomElement domElement = domNode.toElement();
            if (domElement.isNull() == false)
            {
                return GetParametrDouble(domElement, attr, QString("%1").arg(defVal));
            }
        }
    }
    return defVal;
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement MeasurementDoc::MakeEmpty(const QString &name, const QString &formula)
{
    QDomElement element = createElement(TagMeasurement);

    SetAttribute(element, AttrName, name);

    if (type == MeasurementsType::Multisize)
    {
        SetAttribute(element, AttrBase, QString("0"));
        SetAttribute(element, AttrSizeIncrease, QString("0"));
        SetAttribute(element, AttrHeightIncrease, QString("0"));
    }
    else
    {
        if (formula.isEmpty())
        {
            SetAttribute(element, AttrValue, QString("0"));
        }
        else
        {
            SetAttribute(element, AttrValue, formula);
        }
    }

    return element;
}

//---------------------------------------------------------------------------------------------------------------------
QDomElement MeasurementDoc::FindM(const QString &name) const
{
    if (name.isEmpty())
    {
        qWarning() << tr("The measurement name is empty!");
        return QDomElement();
    }

    QDomNodeList list = elementsByTagName(TagMeasurement);

    for (int i=0; i < list.size(); ++i)
    {
        const QDomElement domElement = list.at(i).toElement();
        if (domElement.isNull() == false)
        {
            const QString parameter = domElement.attribute(AttrName);
            if (parameter == name)
            {
                return domElement;
            }
        }
    }

    return QDomElement();
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementsType MeasurementDoc::ReadType() const
{
    QDomElement root = documentElement();
    if ((root.tagName() == TagVST) || (root.tagName() == TagSMMS))
    {
        return MeasurementsType::Multisize;
    }
    else if ((root.tagName() == TagVIT) || (root.tagName() == TagSMIS))
    {
        return MeasurementsType::Individual;
    }
    else
    {
        return MeasurementsType::Unknown;
    }
}

//---------------------------------------------------------------------------------------------------------------------
qreal MeasurementDoc::EvalFormula(VContainer *data, const QString &formula, bool *ok) const
{
    if (formula.isEmpty())
    {
        *ok = true;
        return 0;
    }
    else
    {
        try
        {
            // Replace line return character with spaces for calc if exist
            QString f = formula;
            f.replace("\n", " ");
            QScopedPointer<Calculator> cal(new Calculator());
            const qreal result = cal->EvalFormula(data->DataVariables(), f);

            (qIsInf(result) || qIsNaN(result)) ? *ok = false : *ok = true;
            return result;
        }
        catch (qmu::QmuParserError &error)
        {
            Q_UNUSED(error)
            *ok = false;
            return 0;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString MeasurementDoc::ClearPMCode(const QString &code) const
{
    QString clear = code;
    const int index = clear.indexOf(QLatin1Char('p'));
    if (index == 0)
    {
        clear.remove(0, 1);
    }
    return clear;
}
