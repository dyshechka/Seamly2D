/******************************************************************************
**  @file   knit_measurements.h
**  @brief  Марта's personal knitting-measurement dictionary.
**
**  Unlike the built-in sewing dictionary (measurements_def.h/.cpp), this
**  list is not hard-coded into the app. It lives in a plain XML file in the
**  user's own config folder (see KnitDictionaryFilePath()) that she can
**  edit by hand, and that SeamlyMe also grows automatically as she adds new
**  custom measurements (see RegisterKnitMeasurement()).
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

#ifndef KNIT_MEASUREMENTS_H
#define KNIT_MEASUREMENTS_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief One entry in the knitting-measurement dictionary.
 */
struct KnitMeasurementInfo
{
    QString name;        // Name used in formulas, e.g. "ОГ".
    QString group;       // Display group in SeamlyMe's tree, e.g. "Обхваты".
    QString fullName;    // Human-readable full name, e.g. "(16) Обхват груди третий".
    QString description; // Optional longer description.
};

// Full dictionary, in file order. Loaded from disk on first call and
// cached afterwards.
const QVector<KnitMeasurementInfo> &KnitMeasurementDictionary();

// Flat list of every dictionary name -- treat these the same as the
// built-in sewing measurement names (no "@" prefix required).
Q_REQUIRED_RESULT QStringList AllKnitGroupNames();

// Distinct group names, in first-seen order (for building the tree in
// SeamlyMe's measurement database dialog).
Q_REQUIRED_RESULT QStringList KnitGroupNames();

// Names belonging to one group, in file order.
Q_REQUIRED_RESULT QStringList KnitNamesInGroup(const QString &group);

// True if "name" is a recognized knitting-dictionary measurement.
bool IsKnitMeasurement(const QString &name);

// Metadata lookups. Return an empty string if "name" isn't in the dictionary.
QString KnitFullName(const QString &name);
QString KnitDescription(const QString &name);

// Absolute path to the user's personal dictionary file. Creates it (seeded
// with a starting set of measurements) the first time it's called, if it
// doesn't exist yet.
QString KnitDictionaryFilePath();

// Appends one new measurement to the dictionary file and refreshes the
// cache. Does nothing (returns false) if "name" is already present.
bool RegisterKnitMeasurement(const QString &name, const QString &fullName, const QString &description,
                             const QString &group = QString());

// Forces the next KnitMeasurementDictionary() call to re-read the file from
// disk instead of using the cached copy.
void InvalidateKnitMeasurementCache();

#endif // KNIT_MEASUREMENTS_H
