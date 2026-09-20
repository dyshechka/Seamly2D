/******************************************************************************
 *   @file   tmainwindow.h
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
 **  @file   tmainwindow.h
 **  @author Roman Telezhynskyi <dismine(at)gmail.com>
 **  @date   10 7, 2015
 **
 **  @brief
 **  @copyright
 **  This source code is part of the Valentina project, a pattern making
 **  program, whose allow create and modeling patterns of clothing.
 **  Copyright (C) 2015 Valentina project
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

#ifndef TMAINWINDOW_H
#define TMAINWINDOW_H

#include <QColor>
#include <QMap>
#include <QSet>
#include <QTableWidget>

#include "../vmisc/def.h"
#include "../vmisc/vlockguard.h"
#include "../vmisc/vtablesearch.h"
#include "../vpatterndb/variables/measurement_variable.h"
#include "../vwidgets/vabstractmainwindow.h"
#include "dialogs/me_shortcuts_dialog.h"

namespace Ui
{
    class TMainWindow;
}

class QLabel;
class MeShortcutsDialog;
class MeasurementDoc;
class VContainer;

class TMainWindow : public VAbstractMainWindow
{
    Q_OBJECT

public:
    explicit            TMainWindow(QWidget *parent = nullptr);
    virtual            ~TMainWindow() override;

    QString             CurrentFile() const;

    void                RetranslateTable();

    void                SetBaseMHeight(int height);
    void                SetBaseMSize(int size);
    void                setPUnit(Unit unit);

    bool                LoadFile(const QString &path);

public slots:
    virtual void        setStatusMessage(const QString &toolTip) override;
    virtual void        zoomToSelected() override;
    virtual void        updateGroups() override;

protected:
    virtual void        closeEvent(QCloseEvent *event) override;
    virtual void        changeEvent(QEvent* event) override;
    virtual void        showEvent(QShowEvent *event) override;
    virtual bool        eventFilter(QObject *object, QEvent *event) override;
    virtual void        exportToCSVData(const QString &fileName, const DialogExportToCSV &dialog) final;
    void                handleExportToCSV();

private slots:
    void                FileNew();
    void                OpenIndividual();
    void                OpenMultisize();
    void                OpenTemplate();
    void                CreateFromExisting();
    //void                handleBodyScanner1();
    void                handleBodyScanner2();
    void                Preferences();
    void                initToolBarStyles();

    void                print();
    void                printPages(QPrinter *printer);

    bool                FileSave();
    bool                FileSaveAs();
    void                AboutToShowWindowMenu();
    void                ShowWindow() const;

#if defined(Q_OS_MAC)
    void                AboutToShowDockMenu();
    void                OpenAt(QAction *where);
#endif //defined(Q_OS_MAC)

    void                SaveGivenName();
    void                SaveFamilyName();
    void                SaveEmail();
    void                SaveGender(int index);
    void                SaveBirthDate(const QDate & date);
    void                SaveNotes();
    void                SavePMSystem(int index);

    void                Remove();
    void                MoveTop();
    void                MoveUp();
    void                MoveDown();
    void                MoveBottom();
    void                Fx();

    void                AddCustom();
    void                AddKnown();
    void                ImportFromPattern();

    void                ChangedSize(int index);
    void                ChangedHeight(int index);

    void                ShowMData();

    void                SaveMName(const QString &text);
    void                SaveMValue();
    void                CommitMValue();
    void                SaveMBaseValue(double value);
    void                SaveMSizeIncrease(double value);
    void                SaveMHeightIncrease(double value);
    void                SaveMDescription();
    void                SaveMFullName();
    void                SaveMIsSection(bool checked);

    void                patternUnitsChanged(int index);

private:
    Q_DISABLE_COPY(TMainWindow)
    Ui::TMainWindow    *ui;
    MeasurementDoc     *individualMeasurements;
    VContainer         *data;
    Unit                mUnit;
    Unit                pUnit;
    MeasurementsType    mType;
    qreal               currentSize;
    qreal               currentHeight;
    QString             curFile;
    QComboBox          *gradationHeights;
    QComboBox          *gradationSizes;
    QComboBox          *comboBoxUnits;

    std::shared_ptr<VLockGuard<char>> lock;
    QSharedPointer<VTableSearch>      m_search;
    QLabel             *labelGradationHeights;
    QLabel             *labelGradationSizes;
    QLabel             *labelPatternUnit;
    QAction            *actionDockDiagram;
    bool                dockDiagramVisible;
    bool                isInitialized;
    bool                m_isReadOnly;
    enum { MaxRecentFiles = 5 };
    QAction            *recentFileActs[MaxRecentFiles];
    QAction            *separatorAct;
    QVector<QObject *>  hackedWidgets;
    QString             m_currentSvgPath;   // Stores the current "://diagrams/..." path
    QString             m_currentNumber;    // Stores the current measurement number (e.g. "L13")
    QString             m_currentName;      // Stores the translated name text
    QString             m_currentDescription;

    // Row-highlighting support for the measurements table -- see RefreshTable().
    // m_usedByMeasurement is a reverse formula-dependency index (name -> names of measurements
    // whose formula references it), rebuilt every time the table is refreshed.
    // m_affectedMeasurements holds the names of measurements whose row should show the "check
    // me, something I depend on just changed" (pink) highlight -- populated by
    // MarkMeasurementSaved(), cleared once that row is viewed (ShowNewMData()) or its own
    // formula is (re)saved.
    // m_formulaDrafts holds unsaved, currently-invalid formula text by measurement name (orange
    // highlight) -- see CommitMValue()/ShowNewMData().
    QMap<QString, QStringList> m_usedByMeasurement;
    QSet<QString>              m_affectedMeasurements;
    QMap<QString, QString>     m_formulaDrafts;

    // Name of the measurement currently reflected in the Details panel (plainTextEditFormula
    // included), or empty when no row is shown. A name rather than a row index because rows can
    // shift (add/remove) between when this is recorded and when it's used. Needed because a
    // mouse click on a different table row can update ui->tableWidget->currentRow() to the NEW
    // row before plainTextEditFormula's FocusOut (and so CommitMValue(), see eventFilter()) is
    // actually delivered -- by then, committing against currentRow() would target the wrong row
    // entirely, silently discarding whatever was typed for the row she's leaving. See
    // ShowNewMData().
    QString                     m_editingMeasurementName;

    void                SetupMenu();
    void                InitWindow();
    void                initializeTable();
    void                RetranslateTableHeaders();
    void                SetDecimals();
    void                initUnits();
    void                setCurrentPatternUnits();
    void                InitComboBoxUnits();
    void                InitGender(QComboBox *gender);

    void                ShowNewMData(bool fresh);
    void                ShowUnits();
    void                ShowHeaderUnits(QTableWidget *table, int column, const QString &unit);
    void                UpdateRecentFileActions();

    void                MeasurementsWasSaved(bool saved);
    void                SetCurrentFile(const QString &fileName);
    bool                SaveMeasurements(const QString &fileName, QString &error);

    bool                CanReplaceCurrentWindow() const;
    bool                MaybeSave();

    QTableWidgetItem   *AddCell(const QString &text, int row, int column, int aligment, bool ok = true);

    void                SetRowHighlight(int row, const QColor &color);
    void                MarkMeasurementSaved(const QString &name);
    void                CommitMValueFor(const QString &measurementName, bool restoreSelection);

    Q_REQUIRED_RESULT QComboBox *SetGradationList(QLabel *label, const QStringList &list);

    void                SetDefaultHeight(int value);
    void                SetDefaultSize(int value);

    void                RefreshData(bool freshCall = false);
    void                RefreshTable(bool freshCall = false);

    QString             GetCustomName() const;
    QString             ClearCustomName(const QString &name) const;
    void                RegisterNewKnitMeasurements();

    bool                EvalFormula(const QString &formula, bool fromUser, VContainer *data, QLabel *label);
    // Does `internalFormula` (a formula in internal/untranslated form, e.g. meash->GetFormula())
    // refer to a measurement that's marked as a section divider (checkBoxIsSection/IsSection())?
    // A divider has no real value -- it's purely organizational -- so using it in a calculation
    // is always a mistake, even though the Calculator has no trouble evaluating it (a divider
    // row is still stored as a variable with a numeric value -- see
    // MeasurementDoc::readMeasurements()). On a true result, *sectionName (if given) is set to
    // the divider's user-facing (translated) name for use in an error message. See RefreshTable().
    bool                FormulaReferencesSection(const QString &internalFormula, QString *sectionName = nullptr) const;
    QString             getMeasurementNumber(const QString &name);
    void                ShowMDiagram(QSharedPointer<MeasurementVariable> meash);

    void                Open(const QString &pathTo, const QString &filter);
    void                UpdatePadlock(bool ro);
    void                MeasurementGUI();
    void                Controls();
    void                MFields(bool enabled);
    void                UpdateWindowTitle();

    void                ReadSettings();
    void                WriteSettings();

    QStringList         FilterMeasurements(const QStringList &mNew, const QStringList &mFilter);

    void                updatePatternUnit();

    bool                LoadFromExistingFile(const QString &path);

    void                CreateWindowMenu(QMenu *menu);

    bool                IgnoreLocking(int error, const QString &path);

    template <class T>
    void                HackWidget(T **widget);
    void                copyToClipboard();
    void                renderScaledDiagram();
};

#endif // TMAINWINDOW_H
