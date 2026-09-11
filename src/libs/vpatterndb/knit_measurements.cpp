/******************************************************************************
**  @file   knit_measurements.cpp
**  @brief  Марта's personal knitting-measurement dictionary.
**
**  This source code is part of the Seamly2D project, a pattern making
**  program to create and model patterns of clothing.
**  Copyright (C) 2017-2026 Seamly2D project
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
*************************************************************************/

#include "knit_measurements.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace
{
// Seed content written to the user's dictionary file the first time it's
// needed and no file exists there yet. After that, this constant is never
// consulted again -- the file on disk (which the user can edit, and which
// SeamlyMe appends to via RegisterKnitMeasurement()) is the only source of
// truth from then on.
const char *KnitDictionarySeedXml()
{
    return R"XMLDICT(<?xml version="1.0" encoding="UTF-8"?>
<knitMeasurements version="1.0.0">
 <group name="Общие">
  <m name="Рост" full_name="(1) Рост"/>
  <m name="ЦГ" full_name="(46) Центр груди"/>
  <m name="угол" full_name="(2 способ) Угол наклона плеча"/>
  <m name="ДТО" full_name="(!) (44) Сумма (43) ДС и (61) ДП + 1" description="По замерам 93"/>
 </group>
 <group name="Обхваты">
  <m name="ОШ" full_name="(13) Обхват шеи"/>
  <m name="ОГ1" full_name="(14) Обхват груди первый"/>
  <m name="ОГ" full_name="(16) Обхват груди третий"/>
  <m name="ОТ" full_name="(18) Обхват талии"/>
  <m name="ОБ" full_name="(19) Обхват бедер"/>
  <m name="ОР" full_name="(28) Обхват руки"/>
  <m name="ОЗ" full_name="(29) Обхват запястья"/>
  <m name="ОК" full_name="(30) Обхват кисти"/>
 </group>
 <group name="Длины">
  <m name="Дпл" full_name="(31) Длина плеча"/>
  <m name="ДРпл" full_name="(33) Длина руки с плечом до запястья"/>
  <m name="ДСТ7" full_name="(40)"/>
  <m name="ДС" full_name="(43) Длина спины"/>
  <m name="ДП" full_name="(61) Длина переда"/>
 </group>
 <group name="Ширины">
  <m name="ШГ" full_name="(45) Ширина груди"/>
  <m name="ШС" full_name="(47) Ширина спины"/>
 </group>
 <group name="Высоты">
  <m name="ВГ" full_name="(35) Высота груди"/>
  <m name="ВПК" full_name="(41) Высота плеча касая"/>
 </group>
</knitMeasurements>
)XMLDICT";
}

QVector<KnitMeasurementInfo> ParseDictionaryXml(const QString &xmlText)
{
    QVector<KnitMeasurementInfo> result;
    QString currentGroup;

    QXmlStreamReader xml(xmlText);
    while (!xml.atEnd() && !xml.hasError())
    {
        xml.readNext();
        if (!xml.isStartElement())
        {
            continue;
        }

        if (xml.name() == QLatin1String("group"))
        {
            currentGroup = xml.attributes().value(QLatin1String("name")).toString();
        }
        else if (xml.name() == QLatin1String("m"))
        {
            KnitMeasurementInfo info;
            info.name        = xml.attributes().value(QLatin1String("name")).toString();
            info.fullName    = xml.attributes().value(QLatin1String("full_name")).toString();
            info.description = xml.attributes().value(QLatin1String("description")).toString();
            info.group       = currentGroup;

            if (!info.name.isEmpty())
            {
                result.append(info);
            }
        }
    }

    if (xml.hasError())
    {
        qWarning("Knit measurement dictionary: XML parse error: %s", qUtf8Printable(xml.errorString()));
    }

    return result;
}

bool WriteDictionaryFile(const QString &path, const QVector<KnitMeasurementInfo> &entries)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning("Knit measurement dictionary: could not open '%s' for writing.", qUtf8Printable(path));
        return false;
    }

    // Preserve group order as first-seen among the entries being written.
    QStringList groups;
    for (const KnitMeasurementInfo &info : entries)
    {
        if (!groups.contains(info.group))
        {
            groups.append(info.group);
        }
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(1);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("knitMeasurements"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0.0"));

    for (const QString &group : groups)
    {
        xml.writeStartElement(QStringLiteral("group"));
        xml.writeAttribute(QStringLiteral("name"), group);

        for (const KnitMeasurementInfo &info : entries)
        {
            if (info.group != group)
            {
                continue;
            }

            xml.writeStartElement(QStringLiteral("m"));
            xml.writeAttribute(QStringLiteral("name"), info.name);
            xml.writeAttribute(QStringLiteral("full_name"), info.fullName);
            if (!info.description.isEmpty())
            {
                xml.writeAttribute(QStringLiteral("description"), info.description);
            }
            xml.writeEndElement(); // m
        }

        xml.writeEndElement(); // group
    }

    xml.writeEndElement(); // knitMeasurements
    xml.writeEndDocument();

    return !xml.hasError();
}

QVector<KnitMeasurementInfo> &KnitCache()
{
    static QVector<KnitMeasurementInfo> cache;
    return cache;
}

bool &KnitCacheLoaded()
{
    static bool loaded = false;
    return loaded;
}

void EnsureKnitCacheLoaded()
{
    if (KnitCacheLoaded())
    {
        return;
    }

    QFile file(KnitDictionaryFilePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        KnitCache() = ParseDictionaryXml(QString::fromUtf8(file.readAll()));
    }
    else
    {
        qWarning("Knit measurement dictionary: could not open '%s' for reading.",
                qUtf8Printable(file.fileName()));
    }

    KnitCacheLoaded() = true;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QString KnitDictionaryFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/knit_measurements.xml");

    if (!QFile::exists(path))
    {
        QFile seedFile(path);
        if (seedFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            seedFile.write(QString::fromUtf8(KnitDictionarySeedXml()).toUtf8());
        }
        else
        {
            qWarning("Knit measurement dictionary: could not create '%s'.", qUtf8Printable(path));
        }
    }

    return path;
}

//---------------------------------------------------------------------------------------------------------------------
const QVector<KnitMeasurementInfo> &KnitMeasurementDictionary()
{
    EnsureKnitCacheLoaded();
    return KnitCache();
}

//---------------------------------------------------------------------------------------------------------------------
QStringList AllKnitGroupNames()
{
    QStringList names;
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        names.append(info.name);
    }
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList KnitGroupNames()
{
    QStringList groups;
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        if (!groups.contains(info.group))
        {
            groups.append(info.group);
        }
    }
    return groups;
}

//---------------------------------------------------------------------------------------------------------------------
QStringList KnitNamesInGroup(const QString &group)
{
    QStringList names;
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        if (info.group == group)
        {
            names.append(info.name);
        }
    }
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
bool IsKnitMeasurement(const QString &name)
{
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        if (info.name == name)
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
QString KnitFullName(const QString &name)
{
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        if (info.name == name)
        {
            return info.fullName;
        }
    }
    return QString();
}

//---------------------------------------------------------------------------------------------------------------------
QString KnitDescription(const QString &name)
{
    for (const KnitMeasurementInfo &info : KnitMeasurementDictionary())
    {
        if (info.name == name)
        {
            return info.description;
        }
    }
    return QString();
}

//---------------------------------------------------------------------------------------------------------------------
bool RegisterKnitMeasurement(const QString &name, const QString &fullName, const QString &description,
                             const QString &group)
{
    if (name.isEmpty() || IsKnitMeasurement(name))
    {
        return false;
    }

    QVector<KnitMeasurementInfo> entries = KnitMeasurementDictionary();

    KnitMeasurementInfo info;
    info.name        = name;
    info.fullName     = fullName;
    info.description = description;
    info.group        = group.isEmpty() ? QStringLiteral("Новые") : group;
    entries.append(info);

    if (!WriteDictionaryFile(KnitDictionaryFilePath(), entries))
    {
        return false;
    }

    InvalidateKnitMeasurementCache();
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void InvalidateKnitMeasurementCache()
{
    KnitCacheLoaded() = false;
    KnitCache().clear();
}
