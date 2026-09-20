/******************************************************************************
 *   @file   tmainwindow.cpp
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
 **  @file   tmainwindow.cpp
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

#include "tmainwindow.h"
#include "ui_tmainwindow.h"
#include "dialogs/dialogaboutseamlyme.h"
#include "dialogs/database_dialog.h"
#include "dialogs/dialogseamlymepreferences.h"
#include "dialogs/dialogexporttocsv.h"
#include "dialogs/me_shortcuts_dialog.h"
#include "../vpatterndb/calculator.h"
#include "../vpatterndb/measurements_def.h"
#include "../vpatterndb/knit_measurements.h"
#include "../vpatterndb/pmsystems.h"
#include "../ifc/ifcdef.h"
#include "../ifc/xml/individual_size_converter.h"
#include "../ifc/xml/multi_size_converter.h"
#include "../ifc/xml/vpatternconverter.h"
#include "../vmisc/def.h"
#include "../vmisc/vlockguard.h"
#include "../vmisc/vsysexits.h"
#include "../vmisc/qxtcsvmodel.h"
#include "vlitepattern.h"
#include "../qmuparser/qmudef.h"
#include "../vtools/dialogs/support/edit_formula_dialog.h"
#include "version.h"
#include "../vformat/measurements.h"
#include "application_me.h" // Should be last because of definning qApp

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QStyleFactory>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QProcess>
#include <QStringConverter>
#include <QSvgRenderer>
#include <QtNumeric>

#if defined(Q_OS_MAC)
#include <QMimeData>
#include <QDrag>
#endif //defined(Q_OS_MAC)

#define DIALOG_MAX_FORMULA_HEIGHT 64

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wmissing-prototypes")
QT_WARNING_DISABLE_INTEL(1418)

Q_LOGGING_CATEGORY(tMainWindow, "t.mainwindow")

QT_WARNING_POP

// We need this enum in case we will add or delete a column. And also make code more readable.
enum {ColumnName = 0, ColumnNumber, ColumnFullName, ColumnCalcValue, ColumnFormula, ColumnBaseValue, ColumnInSizes, ColumnInHeights, ColumnDescription};

// Keeps the Formula column narrow enough that a long, complex formula wraps
// onto multiple lines instead of stretching the column to fit one very long
// line (see initializeTable() and RefreshTable()).
static const int maxFormulaColumnWidth = 260;

// Row-highlight colors for the measurements table (Individual/single-size files only -- see
// RefreshTable(), SetRowHighlight()). Priority when more than one might apply to the same row
// is problem, then reminder, then blue (a section-divider row is exclusively blue and never
// reaches the problem/reminder checks at all -- see meash->IsSection() in
// RefreshTable()/ShowNewMData()). Colors follow the usual warning-color association -- orange
// reads as "worth a look", pink/red reads as "something's actually wrong" -- rather than the
// color names matching the QColor variable names, which is why these are named by role, not by
// hue:
//   blue     -- this row is a section divider (checkBoxIsSection), not a real measurement -- it
//               has no formula and is never used in calculations. Purely organizational, so a
//               long measurement list can be split into logical groups.
//   reminder -- (orange) a measurement this formula uses was just edited elsewhere; the result
//               here may have quietly changed. Worth a look, not urgent -- the formula itself is
//               still fine. Also gets a small warning icon in front of the Formula column's text
//               (display only -- see ReminderRowIcon() -- never written into the formula itself
//               or the edit field, so it can't end up saved or interfere with comparing against
//               what's currently typed). Cleared once the row is viewed or its own formula is
//               (re)saved -- see MarkMeasurementSaved()/ShowNewMData().
//   problem  -- (pink) the formula currently sitting in this row is unsaved and invalid (a
//               draft -- see CommitMValue()), or it references a section divider, which has no
//               real value to use. Needs fixing.
static const QColor rowHighlightBlue(0xca, 0xda, 0xf8);
static const QColor rowHighlightReminder(0xfc, 0xe5, 0xcd); // orange
static const QColor rowHighlightProblem(0xf4, 0xcc, 0xcc);  // pink

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief ReminderRowIcon a small warning-triangle icon shown in front of the Formula column's
 * text for a "reminder" (rowHighlightReminder) row -- see the comment above rowHighlightBlue.
 * Built once into a static QIcon and reused -- RefreshTable() rebuilds every row on every
 * change, so this runs often enough to be worth not re-rendering a glyph-to-pixmap every time.
 * Deliberately an icon (QTableWidgetItem::setIcon()), not text prepended to the cell's own
 * string: that string doubles as the "what's actually saved" reference CommitMValueFor() and
 * Fx() compare the edit field against, so anything added there would end up either stuck in the
 * saved formula or breaking that comparison.
 */
static const QIcon &ReminderRowIcon()
{
	static const QIcon icon = []()
	{
		// Drawn as plain vector shapes (a filled triangle plus an exclamation mark made of a
		// small rectangle and a dot) -- deliberately NOT emoji text ("\u26A0\uFE0F" via
		// QPainter::drawText(), which is what this used to do). That emoji version crashed:
		// on this Mac's macOS/Qt combination, Qt renders that particular glyph through
		// CoreText, which loads its color bitmap ("sbix" table, used for full-color emoji)
		// via ImageIO's PNG decoder -- and that decoder crashed with SIGBUS the first time
		// this icon was built (see the crash report Марта sent: SIGBUS inside
		// IIOReadPlugin::callInitialize(), reached from QPainter::drawText() ->
		// CTFontDrawGlyphs() -> TCGImageData::TCGImageData(..., TsbixContext const&, ...),
		// called from right here via RefreshTable()). A hand-drawn shape needs no font or
		// color-glyph rendering at all, so it can't hit that code path again.
		QPixmap pixmap(16, 16);
		pixmap.fill(Qt::transparent);

		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing, true);

		QPolygonF triangle;
		triangle << QPointF(8.0, 1.5) << QPointF(15.0, 14.5) << QPointF(1.0, 14.5);

		painter.setPen(QPen(QColor(0x8a, 0x5a, 0x00), 1.2));
		painter.setBrush(QColor(0xf5, 0xa6, 0x23));
		painter.drawPolygon(triangle);

		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(0x3a, 0x2a, 0x00));
		painter.drawRect(QRectF(7.3, 5.5, 1.4, 5.0));     // the exclamation mark's stroke
		painter.drawEllipse(QRectF(7.3, 11.5, 1.4, 1.4)); // its dot

		painter.end();
		return QIcon(pixmap);
	}();
	return icon;
}

//---------------------------------------------------------------------------------------------------------------------
TMainWindow::TMainWindow(QWidget *parent)
	: VAbstractMainWindow(parent),
	  ui(new Ui::TMainWindow),
	  individualMeasurements(nullptr),
	  data(nullptr),
	  mUnit(Unit::Cm),
	  pUnit(Unit::Cm),
	  mType(MeasurementsType::Individual),
	  currentSize(0),
	  currentHeight(0),
	  curFile(),
	  gradationHeights(nullptr),
	  gradationSizes(nullptr),
	  comboBoxUnits(nullptr),
	  lock(nullptr),
	  m_search(),
	  labelGradationHeights(nullptr),
	  labelGradationSizes(nullptr),
	  labelPatternUnit(nullptr),
	  actionDockDiagram(nullptr),
	  dockDiagramVisible(true),
	  isInitialized(false),
	  m_isReadOnly(false),
	  recentFileActs(),
	  separatorAct(nullptr),
	  hackedWidgets(),
      m_currentSvgPath(),
      m_currentNumber(),
      m_currentName(),
      m_currentDescription()
{
	ui->setupUi(this);

	qApp->Settings()->getOsSeparator() ? setLocale(QLocale()) : setLocale(QLocale::c());

	ui->find_LineEdit->setClearButtonEnabled(true);
	ui->lineEditName->setClearButtonEnabled(true);
	ui->lineEditFullName->setClearButtonEnabled(true);
	ui->lineEditGivenName->setClearButtonEnabled(true);
	ui->lineEditFamilyName->setClearButtonEnabled(true);
	ui->lineEditEmail->setClearButtonEnabled(true);

	ui->find_LineEdit->installEventFilter(this);
	ui->plainTextEditFormula->installEventFilter(this);

	m_search = QSharedPointer<VTableSearch>(new VTableSearch(ui->tableWidget));
	ui->tabWidget->setVisible(false);

    // Разрешаем перенос текста по словам в ячейках таблицы
    ui->tableWidget->setWordWrap(true);

    // Заставляем строки автоматически расширяться по высоте под объем текста
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Force this one table onto the cross-platform Fusion style instead of the native macOS
    // one it would otherwise inherit from the app. On this Mac, the native style paints each
    // row's background itself (including its own zebra-striping) and doesn't reliably respect a
    // per-item Qt::BackgroundRole -- confirmed directly: turning tableWidget's own
    // "alternatingRowColors" property off (see the .ui file and SetRowHighlight()) made no
    // difference, a "reminder" row still came out plain gray instead of orange even though its
    // warning icon (a separate paint step) showed up fine. Fusion paints entirely through Qt's
    // own item-view delegate rather than native row drawing, so item->setBackground() (used for
    // the blue/reminder/problem row highlighting, see SetRowHighlight()) is respected reliably.
    // Scoped to just this widget -- setStyle() here doesn't touch the look of the rest of the
    // window.
    ui->tableWidget->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

	ui->mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
	ui->toolBarGradation->setContextMenuPolicy(Qt::PreventContextMenu);

	//MSVC doesn't support int arrays in initializer list
	for (int i = 0; i < MaxRecentFiles; ++i)
	{
		recentFileActs[i] = nullptr;
	}

	SetupMenu();
	UpdateWindowTitle();
	ReadSettings();

#if defined(Q_OS_MAC)
	// On Mac default icon size is 32x32.
	ui->toolBarGradation->setIconSize(QSize(24, 24));

	ui->pushButtonShowInExplorer->setText(tr("Show in Finder"));

	// Mac OS Dock Menu
	QMenu *menu = new QMenu(this);
	connect(menu, &QMenu::aboutToShow, this, &TMainWindow::AboutToShowDockMenu);
	AboutToShowDockMenu();
	menu->setAsDockMenu();
#endif //defined(Q_OS_MAC)
}

//---------------------------------------------------------------------------------------------------------------------
TMainWindow::~TMainWindow()
{
	delete data;
	delete individualMeasurements;
	delete ui;
}

//---------------------------------------------------------------------------------------------------------------------
QString TMainWindow::CurrentFile() const
{
	return curFile;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::RetranslateTable()
{
	if (individualMeasurements != nullptr)
	{
		const int row = ui->tableWidget->currentRow();
		RefreshTable();
		ui->tableWidget->selectRow(row);
		m_search->refreshList(ui->find_LineEdit->text());
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetBaseMHeight(int height)
{
	if (individualMeasurements != nullptr)
	{
		if (mType == MeasurementsType::Multisize)
		{
			const int row = ui->tableWidget->currentRow();
			currentHeight = UnitConvertor(height, Unit::Cm, mUnit);
			RefreshData();
			ui->tableWidget->selectRow(row);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetBaseMSize(int size)
{
	if (individualMeasurements != nullptr)
	{
		if (mType == MeasurementsType::Multisize)
		{
			const int row = ui->tableWidget->currentRow();
			currentSize = UnitConvertor(size, Unit::Cm, mUnit);
			RefreshData();
			ui->tableWidget->selectRow(row);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::setPUnit(Unit unit)
{
	pUnit = unit;
    setCurrentPatternUnits();
	updatePatternUnit();
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::LoadFile(const QString &path)
{
    QString filename = path;

	if (individualMeasurements != nullptr && CanReplaceCurrentWindow())
	{
		// See the identical reset in FileNew() -- this window has nothing worth keeping, so load
		// the file here instead of opening a separate window for it.
		delete individualMeasurements;
		individualMeasurements = nullptr;
		delete data;
		data = nullptr;
	}

	if (individualMeasurements == nullptr)
	{
		if (!QFileInfo(filename).exists())
		{
			qCCritical(tMainWindow, "%s", qUtf8Printable(tr("File '%1' doesn't exist!").arg(filename)));
			if (qApp->isTestMode())
			{
				qApp->exit(V_EX_NOINPUT);
			}
			return false;
		}

		// Check if file already opened
		QList<TMainWindow*>list = qApp->mainWindows();
		for (int i = 0; i < list.size(); ++i)
		{
			if (list.at(i)->CurrentFile() == filename)
			{
				list.at(i)->activateWindow();
				close();
				return false;
			}
		}

		VlpCreateLock(lock, filename);

		if (!lock->IsLocked())
		{
			if (!IgnoreLocking(lock->GetLockError(), filename))
			{
				return false;
			}
		}

		try
		{
			data = new VContainer(qApp->translateVariables(), &mUnit);

			individualMeasurements = new MeasurementDoc(data);
			individualMeasurements->setSize(&currentSize);
			individualMeasurements->setHeight(&currentHeight);
			individualMeasurements->setXMLContent(filename);

			mType = individualMeasurements->Type();

			if (mType == MeasurementsType::Unknown)
			{
				VException e(tr("File has unknown format."));
				throw e;
			}

			if (mType == MeasurementsType::Multisize)
			{
				MultiSizeConverter converter(filename);
				m_curFileFormatVersion = converter.getCurrentFormatVersion();
				m_curFileFormatVersionStr = converter.getVersionStr();
				individualMeasurements->setXMLContent(converter.Convert());// Read again after conversion
                filename.replace(QLatin1String(".vst"), QLatin1String(".smms"));
			}
			else
			{
				IndividualSizeConverter converter(filename);
				m_curFileFormatVersion = converter.getCurrentFormatVersion();
				m_curFileFormatVersionStr = converter.getVersionStr();
				individualMeasurements->setXMLContent(converter.Convert());// Read again after conversion
                filename.replace(QLatin1String(".vit"), QLatin1String(".smis"));
			}

			if (!individualMeasurements->eachKnownNameIsValid())
			{
				VException e(tr("File contains invalid known measurement(s)."));
				throw e;
			}

			mUnit = individualMeasurements->measurementUnits();
			pUnit = mUnit;

			currentSize = individualMeasurements->BaseSize();
			currentHeight = individualMeasurements->BaseHeight();

			ui->labelToolTip->setVisible(false);
			ui->tabWidget->setVisible(true);

			m_isReadOnly = individualMeasurements->isReadOnly();
			UpdatePadlock(m_isReadOnly);

			SetCurrentFile(filename);

			InitWindow();

			const bool freshCall = true;
			RefreshData(freshCall);

			if (ui->tableWidget->rowCount() > 0)
			{
				ui->tableWidget->selectRow(0);
			}

			MeasurementGUI();
		}
		catch (VException &exception)
		{
			qCCritical(tMainWindow, "%s\n\n%s\n\n%s", qUtf8Printable(tr("File error.")),
					   qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));

			ui->labelToolTip->setVisible(true);
			ui->tabWidget->setVisible(false);
			delete individualMeasurements;
			individualMeasurements = nullptr;
			delete data;
			data = nullptr;
			lock.reset();

			if (qApp->isTestMode())
			{
				qApp->exit(V_EX_NOINPUT);
			}
			return false;
		}
	}
	else
	{
		qApp->newMainWindow();
		return qApp->mainWindow()->LoadFile(filename);
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::setStatusMessage(const QString &toolTip)
{
	Q_UNUSED(toolTip)
	// do nothing
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::updateGroups()
{
    // do nothing
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::FileNew()
{
	if (individualMeasurements != nullptr && CanReplaceCurrentWindow())
	{
		// This window is still showing a brand new, completely untouched file (nothing typed,
		// nothing saved) -- reset it and start over here instead of opening yet another window
		// for what would otherwise be an identical blank file.
		delete individualMeasurements;
		individualMeasurements = nullptr;
		delete data;
		data = nullptr;
	}

	if (individualMeasurements == nullptr)
	{
		// The old "New measurement file" dialog (type/unit/base size/base
		// height) has been removed -- new files are always Individual, in
		// centimeters. Multisize files can still be opened via "Open
		// multisize" even though they're no longer offered when creating a
		// new file.
		mUnit = Unit::Cm;
		pUnit = mUnit;
		mType = MeasurementsType::Individual;

		data = new VContainer(qApp->translateVariables(), &mUnit);

		individualMeasurements = new MeasurementDoc(mUnit, data);
		m_curFileFormatVersion = IndividualSizeConverter::MeasurementMaxVer;
		m_curFileFormatVersionStr = IndividualSizeConverter::MeasurementMaxVerStr;

		m_isReadOnly = individualMeasurements->isReadOnly();
		UpdatePadlock(m_isReadOnly);

		SetCurrentFile("");
		MeasurementsWasSaved(false);

		InitWindow();

		MeasurementGUI();

		// A brand new file used to come up with an empty table -- nothing to click on, nothing
		// to show in the details panel below. Give her a first row right away, the same as
		// pressing the "Custom" add button herself, so "New" always produces something she can
		// immediately start filling in instead of a blank screen.
		AddCustom();
	}
	else
	{
		qApp->newMainWindow();
		qApp->mainWindow()->FileNew();
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::OpenIndividual()
{
    const QString filter = tr("Individual measurements") + QLatin1String(" (*.") + smisExt +
                                                           QLatin1String(" *.") + vitExt  + QLatin1String(");;") +
                           tr("All files") + QLatin1String(" (*.*)");

	//Use standard path to individual measurements
	const QString dir = qApp->seamlyMeSettings()->getIndividualSizePath();

	bool usedNotExistedDir = false;
	QDir directory(dir);
	if (!directory.exists())
	{
		usedNotExistedDir = directory.mkpath(".");
	}

	Open(dir, filter);

	if (usedNotExistedDir)
	{
		QDir directory(dir);
		directory.rmpath(".");
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::OpenMultisize()
{
    const QString filter = tr("Multisize measurements")  + QLatin1String(" (*.") + smmsExt +
                                                           QLatin1String(" *.") + vstExt  + QLatin1String(");;") +
                           tr("All files") + QLatin1String(" (*.*)");

	//Use standard path to multisize measurements
	QString dir = qApp->seamlyMeSettings()->getMultisizePath();
	dir = VCommonSettings::prepareMultisizeTables(dir);

	Open(dir, filter);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::OpenTemplate()
{
    const QString filter = tr("Measurements") + QLatin1String(" (*.") + smisExt + QLatin1String(" *.") + smmsExt +
                                                QLatin1String(" *.") + vitExt + QLatin1String(" *.") + vstExt  +
                                                QLatin1String(");;") +
                           tr("All files")    + QLatin1String(" (*.*)");

	//Use standard path to template files
	QString dir = qApp->seamlyMeSettings()->getTemplatePath();
	dir = VCommonSettings::PrepareStandardTemplates(dir);
	Open(dir, filter);

	if (individualMeasurements != nullptr)
	{// The file was opened.
		SetCurrentFile(""); // Force user to to save new file
		lock.reset();// remove lock from template
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::CreateFromExisting()
{
    const QString filter = tr("Individual measurements") + QLatin1String(" (*.") + smisExt +
                                                           QLatin1String(" *.") + vitExt + QLatin1String(")");

	//Use standard path to individual measurements
	const QString dir = qApp->seamlyMeSettings()->getIndividualSizePath();

	bool usedNotExistedDir = false;
	QDir directory(dir);
	if (!directory.exists())
	{
		usedNotExistedDir = directory.mkpath(".");
	}

    const QString filename = fileDialog(this, tr("Select file"), dir, filter, nullptr,
                                        qApp->seamlyMeSettings()->getUseNativeFileDialogs(),
                                        QFileDialog::ExistingFile, QFileDialog::AcceptOpen);

	if (!filename.isEmpty())
	{
		if (individualMeasurements == nullptr)
		{
			LoadFromExistingFile(filename);
		}
		else
		{
			qApp->newMainWindow()->CreateFromExisting();
		}
	}

	if (usedNotExistedDir)
	{
		QDir directory(dir);
		directory.rmpath(".");
	}
}

/*
//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::handleBodyScanner1()
{
    const QString filter = QString("3D Measure Up") + QLatin1String(" (*.txt)");

    //Use standard path to template files
    QString dir = qApp->seamlyMeSettings()->getBodyScansPath();

    bool usedNotExistedDir = false;
    QDir directory(dir);
    if (!directory.exists())
    {
        usedNotExistedDir = directory.mkpath(".");
    }

    const QString filename = fileDialog(this, tr("Import body scan"), dir, filter, nullptr,
                                        qApp->seamlyMeSettings()->getUseNativeFileDialogs(),
                                        QFileDialog::ExistingFile, QFileDialog::AcceptOpen);

    QMessageBox messageBox(this);
    messageBox.setMaximumWidth(600);
    messageBox.setText("3D Measure Up file:");
    messageBox.setInformativeText(filename);
    messageBox.setStandardButtons(QMessageBox::Ok);
    messageBox.setDefaultButton(QMessageBox::Ok);
    messageBox.exec();
}
*/

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::handleBodyScanner2()
{
    QString msg = tr("To utilize a 3DLook body scan the file needs to be converted to SeamlyME format.\n") +
                  tr("Attach your 3DLook file to an email and send to convert@seamly.io.\n\n") +
                  tr("You will receive an email with the converted file, which you can then\nload in SeamlyME as usual.\n\n");

    QMessageBox messageBox(this);
    messageBox.setIconPixmap(QPixmap(":/icon/body_scan.png"));
    messageBox.setText(tr("Convert 3DLook file:"));
    messageBox.setInformativeText(msg);
    messageBox.setStandardButtons(QMessageBox::Ok);
    messageBox.setDefaultButton(QMessageBox::Ok);
    messageBox.exec();

/*
    VSettings *settings = new VSettings(QSettings::IniFormat, QSettings::UserScope, "Seamly2DTeam", "Seamly2D", this);

    QString to = "convert@seamly.io";
    QString name = settings->getCompanyName();
    QString email = settings->getEmail();
    QString subject = "3DLook file conversion";
    QString body = "Dear Seamly Team,\n\n Please convert the following 3DLook files to SeamlyME format.\n";

    QDesktopServices::openUrl(QUrl("mailto:" + to + "?subject=" + subject + "&body=" + body, QUrl::TolerantMode));
*/
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::Preferences()
{
	// Calling constructor of the dialog take some time. Because of this user have time to call the dialog twice.
	static QPointer<DialogSeamlyMePreferences> guard;// Prevent any second run
	if (guard.isNull())
	{
		QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		DialogSeamlyMePreferences *preferences = new DialogSeamlyMePreferences(this);
		// QScopedPointer needs to be sure any exception will never block guard
		QScopedPointer<DialogSeamlyMePreferences> dlg(preferences);
		guard = preferences;
		// Must be first
		connect(dlg.data(), &DialogSeamlyMePreferences::updateProperties, this, &TMainWindow::setWindowsLocale);
		connect(dlg.data(), &DialogSeamlyMePreferences::updateProperties, this, &TMainWindow::initToolBarStyles);
		QGuiApplication::restoreOverrideCursor();
		dlg->exec();
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::initToolBarStyles()
{
	initToolBarStyle(ui->toolBarGradation);
	initToolBarStyle(ui->mainToolBar);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::closeEvent(QCloseEvent *event)
{
	if (MaybeSave())
	{
		WriteSettings();
		event->accept();
		deleteLater();
	}
	else
	{
		event->ignore();
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		qApp->Settings()->getOsSeparator() ? setLocale(QLocale()) : setLocale(QLocale::c());

		// retranslate designer form (single inheritance approach)
		ui->retranslateUi(this);
		RetranslateTableHeaders();

		if (mType == MeasurementsType::Multisize)
		{
			ui->labelMType->setText(tr("Multisize measurements"));
			ui->labelBaseSizeValue->setText(QString().setNum(individualMeasurements->BaseSize()) + QLatin1String(" ") +
											UnitsToStr(individualMeasurements->measurementUnits(), true));
			ui->labelBaseHeightValue->setText(QString().setNum(individualMeasurements->BaseHeight()) + QLatin1String(" ") +
											  UnitsToStr(individualMeasurements->measurementUnits(), true));

			labelGradationHeights = new QLabel(tr("Height:"));
			labelGradationSizes = new QLabel(tr("Size:"));
		}
		else
		{
			ui->labelMType->setText(tr("Individual measurements"));

			const qint32 index = ui->comboBoxGender->currentIndex();
			ui->comboBoxGender->blockSignals(true);
			ui->comboBoxGender->clear();
			InitGender(ui->comboBoxGender);
			ui->comboBoxGender->setCurrentIndex(index);
			ui->comboBoxGender->blockSignals(false);
		}

		{
			const qint32 index = ui->comboBoxPMSystem->currentIndex();
			ui->comboBoxPMSystem->blockSignals(true);
			ui->comboBoxPMSystem->clear();
			InitPMSystems(ui->comboBoxPMSystem);
			ui->comboBoxPMSystem->setCurrentIndex(index);
			ui->comboBoxPMSystem->blockSignals(false);
		}

		{
			labelPatternUnit = new QLabel(tr("Pattern unit:"));

			if (comboBoxUnits != nullptr)
			{
				const qint32 index = comboBoxUnits->currentIndex();
				comboBoxUnits->blockSignals(true);
				comboBoxUnits->clear();
				InitComboBoxUnits();
				comboBoxUnits->setCurrentIndex(index);
				comboBoxUnits->blockSignals(false);
			}
		}
	}

	// remember to call base class implementation
	QMainWindow::changeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent( event );
	if ( event->spontaneous() )
	{
		return;
	}

	if (isInitialized)
	{
		return;
	}
	// do your init stuff here

	dockDiagramVisible = ui->dockWidgetDiagram->isVisible();
	ui->dockWidgetDiagram->setVisible(false);

	isInitialized = true;//first show windows are held
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::eventFilter(QObject *object, QEvent *event)
{
	if (object == ui->plainTextEditFormula && event->type() == QEvent::FocusOut)
	{
		if (ui->plainTextEditFormula->toPlainText().isEmpty())
		{
			// Nothing was typed after the placeholder zero was cleared on focus-in (or
			// everything was deleted by hand) -- put the safe default back instead of leaving
			// the field empty or showing an "Empty field" error.
			ui->plainTextEditFormula->blockSignals(true);
			ui->plainTextEditFormula->setPlainText(QStringLiteral("0"));
			ui->plainTextEditFormula->blockSignals(false);
		}

		// Formula editing is considered "finished" once the field loses focus (the user
		// clicked/tabbed away, selected another measurement, etc.) -- commit the pending
		// edit and refresh the table now. See SaveMValue()/CommitMValue() for why this is
		// deferred instead of happening on every keystroke.
		CommitMValue();
		// Fall through to QMainWindow::eventFilter() below so normal focus handling still
		// happens for this widget.
	}
	else if (object == ui->plainTextEditFormula && event->type() == QEvent::FocusIn)
	{
		// A brand new/custom measurement starts with a placeholder formula of just "0". Clicking
		// in to type a real formula used to leave that "0" in place, so the first keystrokes were
		// silently glued onto it (e.g. "0acos(...)") instead of replacing it -- same issue as the
		// Fx dialog already solves for via EditFormulaDialog::SetFormula(). Reuse the same
		// "autoClearFx" setting here so the two stay consistent.
		if (qApp->Settings()->autoClearFx())
		{
			const QString text = ui->plainTextEditFormula->toPlainText();
			bool isDouble = false;
			const double value = text.toDouble(&isDouble);
			// Only the placeholder zero gets cleared here -- any other value already sitting in
			// this field (including one just typed a moment ago) is left completely alone.
			if (isDouble && qFuzzyIsNull(value))
			{
				// Silence textChanged for this programmatic clear -- otherwise SaveMValue() sees the
				// now-empty field and immediately shows "Error. Empty field.", even though the user
				// hasn't done anything yet. CommitMValue() shows that same message later, but only if
				// they actually leave the field still empty -- see the comment there.
				ui->plainTextEditFormula->blockSignals(true);
				ui->plainTextEditFormula->clear();
				ui->plainTextEditFormula->blockSignals(false);
			}
		}
	}

	if (QPlainTextEdit *plainTextEdit = qobject_cast<QPlainTextEdit *>(object))
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
			// Enter/Return is intentionally left unhandled here so it inserts a line
			// break, the same way it already does in the Description field. Formula
			// text is sanitized (newlines replaced with spaces) before it is evaluated
			// or saved, see TMainWindow::SaveMValue().
			if ((keyEvent->key() == Qt::Key_Period) && (keyEvent->modifiers() & Qt::KeypadModifier))
			{
				if (qApp->Settings()->getOsSeparator())
				{
					plainTextEdit->insertPlainText(localeDecimalPoint(QLocale()));
				}
				else
				{
					plainTextEdit->insertPlainText(localeDecimalPoint(QLocale::c()));
				}
				return true;
			}
		}
	}
	else if (QLineEdit *textEdit = qobject_cast<QLineEdit *>(object))
	{
		if (event->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
			if ((keyEvent->key() == Qt::Key_Period) && (keyEvent->modifiers() & Qt::KeypadModifier))
			{
				if (qApp->Settings()->getOsSeparator())
				{
					textEdit->insert(localeDecimalPoint(QLocale()));
				}
				else
				{
					textEdit->insert(localeDecimalPoint(QLocale::c()));
				}
				return true;
			}
		}
	}
    else if (object == ui->dockWidgetDiagram && event->type() == QEvent::Resize)
    {
        renderScaledDiagram();
    }
	else
	{
		// pass the event on to the parent class
		return QMainWindow::eventFilter(object, event);
	}
	return false;// pass the event to the widget
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::exportToCSVData(const QString &fileName, const DialogExportToCSV &dialog)
{
	QxtCsvModel csv;
    int columns;
    if (mType == MeasurementsType::Multisize)
    {
        columns = ui->tableWidget->columnCount();
    }
    else
    {
        columns = 5;
    }
	{
		int colCount = 0;
		for (int column = 0; column < columns; ++column)
		{
			if (!ui->tableWidget->isColumnHidden(column))
			{
				csv.insertColumn(colCount++);
			}
		}
	}

	if (dialog.WithHeader())
	{
		int colCount = 0;
		for (int column = 0; column < columns; ++column)
		{
			if (!ui->tableWidget->isColumnHidden(column))
			{
				QTableWidgetItem *header = ui->tableWidget->horizontalHeaderItem(colCount);
				csv.setHeaderText(colCount, header->text());
				++colCount;
			}
		}
	}

	const int rows = ui->tableWidget->rowCount();
	for (int row = 0; row < rows; ++row)
	{
		csv.insertRow(row);
		int colCount = 0;
		for (int column = 0; column < columns; ++column)
		{
			if (!ui->tableWidget->isColumnHidden(column))
			{
				QTableWidgetItem *item = ui->tableWidget->item(row, column);
				csv.setText(row, colCount, item->text());
				++colCount;
			}
		}
	}

	csv.toCSV(fileName, dialog.WithHeader(), dialog.Separator(), dialog.SelectedEncoding());
}

void TMainWindow::handleExportToCSV()
{
    QString file = tr("untitled");
    if(!curFile.isEmpty())
    {
        file = QFileInfo(curFile).baseName();
    }
    exportToCSV(file);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::print()
{
    int width = 0;
    int height = 0;
    int columns;
    if (mType == MeasurementsType::Multisize)
    {
        columns = ui->tableWidget->columnCount();
    }
    else
    {
        columns = 5;
    }
    int rows = ui->tableWidget->rowCount();

    for( int i = 0; i < columns; ++i ) {
            width += ui->tableWidget->columnWidth(i);
    }

    for( int i = 0; i < rows; ++i ) {
        height += ui->tableWidget->rowHeight(i);
    }

    QPrintPreviewDialog  *dialog = new QPrintPreviewDialog(this);
    connect(dialog, &QPrintPreviewDialog::paintRequested, this, &TMainWindow::printPages);

    dialog->showMaximized();
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->exec();
}

void TMainWindow::printPages(QPrinter *printer)
{
    int columns;
    if (mType == MeasurementsType::Multisize)
    {
        columns = ui->tableWidget->columnCount();
    }
    else
    {
        columns = 5;
    }

    QTextDocument doc;

    QString text("<h2>" + CurrentFile() + "</h2>");
    text.append("<table>");
    if (mType == MeasurementsType::Multisize)
    {
        text.append("<tr><td align = left><b>Base Size:</b></td><td>" + ui->labelBaseSizeValue->text()     + "</td></tr>");
        text.append("<tr><td align = left><b>Base Height:</b></td><td>" + ui->labelBaseHeightValue->text() + "</td></tr>");
    }
    else
    {
        text.append("<tr><td align = left><b>Units:</b></td><td>"      + UnitsToStr(mUnit)                 + "</td></tr>");
        text.append("<tr><td align = left><b>First Name:</b></td><td>" + ui->lineEditGivenName->text()     + "</td></tr>");
        text.append("<tr><td align = left><b>Last Name:</b></td><td>"  + ui->lineEditFamilyName->text()    + "</td></tr>");
        text.append("<tr><td align = left><b>Gender:</b></td><td>"     + ui->comboBoxGender->currentText() + "</td></tr>");
        text.append("<tr><td align = left><b>Email:</b></td><td>"      + ui->lineEditEmail->text()         + "</td></tr>");
    }
    text.append("<tr><td align = left><b>Notes:</b></td><td>"      + ui->plainTextEditNotes->toPlainText() + "</td></tr></table>");
    text.append("<p>");

    text.append("<table><thead>");
    text.append("<tr>");
    for (int i = 0; i < columns; i++)
    {
        text.append("<th>").append(ui->tableWidget->horizontalHeaderItem(i)->data(Qt::DisplayRole).toString()).append("</th>");
    }
    text.append("</tr></thead>");
    text.append("<tbody>");
    for (int i = 0; i < ui->tableWidget->rowCount(); i++)
    {
        text.append("<tr>");
        for (int j = 0; j < columns; j++)
        {
            QTableWidgetItem *item = ui->tableWidget->item(i, j);
            if (!item || item->text().isEmpty())
            {

                if (j > 1)
                {
                    ui->tableWidget->setItem(i, j, new QTableWidgetItem("0"));
                }
                else
                {
                    ui->tableWidget->setItem(i, j, new QTableWidgetItem(""));
                }
            }
            if (j == 1 || j > 2)
            {
                text.append("<td align = center>").append(ui->tableWidget->item(i, j)->text()).append("</td>");
            }
            else
            {
                text.append("<td align = left>").append(ui->tableWidget->item(i, j)->text()).append("</td>");
            }
        }
        text.append("</tr>");
    }
    text.append("</tbody></table>");

        printer->setPageMargins(QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter);
        doc.setHtml(text);
        doc.setPageSize(printer->pageLayout().paintRectPixels(static_cast<int>(PrintDPI)).size());
        doc.print(printer);
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::FileSave()
{
	// Make sure a formula edit still in progress (cursor never left the field, e.g. Ctrl+S
	// pressed mid-edit) isn't silently dropped -- SaveMeasurements() below only writes what's
	// already committed to the model.
	CommitMValue();

	if (curFile.isEmpty() || m_isReadOnly)
	{
		return FileSaveAs();
	}
	else
	{
		if (mType == MeasurementsType::Multisize
				&& m_curFileFormatVersion < MultiSizeConverter::MeasurementMaxVer
				&& !ContinueFormatRewrite(m_curFileFormatVersionStr, MultiSizeConverter::MeasurementMaxVerStr))
		{
			return false;
		}
		else if (mType == MeasurementsType::Individual
				 && m_curFileFormatVersion < IndividualSizeConverter::MeasurementMaxVer
				 && !ContinueFormatRewrite(m_curFileFormatVersionStr, IndividualSizeConverter::MeasurementMaxVerStr))
		{
			return false;
		}

#ifdef Q_OS_WIN32
		qt_ntfs_permission_lookup++; // turn checking on
#endif /*Q_OS_WIN32*/
		const bool isFileWritable = QFileInfo(curFile).isWritable();
#ifdef Q_OS_WIN32
		qt_ntfs_permission_lookup--; // turn it off again
#endif /*Q_OS_WIN32*/

		if (!isFileWritable)
		{
			QMessageBox messageBox(this);
			messageBox.setIcon(QMessageBox::Question);
			messageBox.setText(tr("The measurements document has no write permissions."));
			messageBox.setInformativeText("Do you want to change the premissions?");
			messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
			messageBox.setDefaultButton(QMessageBox::Yes);

			if (messageBox.exec() == QMessageBox::Yes)
			{
#ifdef Q_OS_WIN32
				qt_ntfs_permission_lookup++; // turn checking on
#endif /*Q_OS_WIN32*/
				bool changed = QFile::setPermissions(curFile,
													 QFileInfo(curFile).permissions() | QFileDevice::WriteUser);
#ifdef Q_OS_WIN32
				qt_ntfs_permission_lookup--; // turn it off again
#endif /*Q_OS_WIN32*/

				if (!changed)
				{
					QMessageBox messageBox(this);
					messageBox.setIcon(QMessageBox::Warning);
					messageBox.setText(tr("Cannot set permissions for %1 to writable.").arg(curFile));
					messageBox.setInformativeText(tr("Could not save the file."));
					messageBox.setDefaultButton(QMessageBox::Ok);
					messageBox.setStandardButtons(QMessageBox::Ok);
					messageBox.exec();
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		QString error;
		if (!SaveMeasurements(curFile, error))
		{
			QMessageBox messageBox;
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Could not save the file"));
			messageBox.setDefaultButton(QMessageBox::Ok);
			messageBox.setDetailedText(error);
			messageBox.setStandardButtons(QMessageBox::Ok);
			messageBox.exec();
			return false;
		}
		else
		{
			if (mType == MeasurementsType::Multisize)
			{
				m_curFileFormatVersion = MultiSizeConverter::MeasurementMaxVer;
				m_curFileFormatVersionStr = MultiSizeConverter::MeasurementMaxVerStr;
			}
			else
			{
				m_curFileFormatVersion = IndividualSizeConverter::MeasurementMaxVer;
				m_curFileFormatVersionStr = IndividualSizeConverter::MeasurementMaxVerStr;
			}
		}
	}
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::FileSaveAs()
{
	// See the identical call in FileSave() -- flush any formula edit still in progress
	// before writing the file.
	CommitMValue();

	QString dir;
    QString filters;
    QString suffix;
    QString filePath = CurrentFile();
    QString fileName = QLatin1String("measurements");
    if (!filePath.isEmpty())
    {
        dir = QFileInfo(filePath).path();
        fileName = QFileInfo(filePath).baseName();
        if (mType == MeasurementsType::Individual)
        {

            filters = tr("Individual measurements") + QLatin1String(" (*.") + smisExt +
                                                      QLatin1String(" *.") + vitExt + QLatin1String(")");
            suffix = smisExt;
        }
        else
        {
            filters = tr("Multisize measurements") + QLatin1String(" (*.") + smmsExt +
                                                     QLatin1String(" *.") + vstExt + QLatin1String(")");
            suffix = smmsExt;
        }
    }
    else
    {
        if (mType == MeasurementsType::Individual)
        {
            dir = qApp->seamlyMeSettings()->getDefaultIndividualSizePath();
            filters = tr("Individual measurements") + QLatin1String(" (*.") + smisExt +
                                                      QLatin1String(" *.") + vitExt + QLatin1String(")");
            suffix = smisExt;
        }
        else
        {
            dir = qApp->seamlyMeSettings()->getDefaultMultisizePath();
            filters = tr("Multisize measurements") + QLatin1String(" (*.") + smmsExt +
                                                     QLatin1String(" *.") + vstExt + QLatin1String(")");
            suffix = smmsExt;
        }
    }

	if (mType == MeasurementsType::Individual)
	{
        filters = tr("Individual measurements") + QLatin1String(" (*.") + smisExt +
                                                  QLatin1String(" *.") + vitExt + QLatin1String(")");
		suffix = smisExt;
	}
	else
	{
		filters = tr("Multisize measurements") + QLatin1String(" (*.") + smmsExt +
                                                 QLatin1String(" *.") + vstExt + QLatin1String(")");
		suffix = smmsExt;
	}

    fileName += QLatin1String(".") + suffix;

	if (curFile.isEmpty())
	{
		if (mType == MeasurementsType::Individual)
		{
			dir = qApp->seamlyMeSettings()->getIndividualSizePath();
		}
		else
		{
			dir = qApp->seamlyMeSettings()->getMultisizePath();
			dir = VCommonSettings::prepareMultisizeTables(dir);
		}
	}
	else
	{
		dir = QFileInfo(curFile).absolutePath();
	}

	bool usedNotExistedDir = false;
	QDir directory(dir);
	if (!directory.exists())
	{
		usedNotExistedDir = directory.mkpath(".");
	}

    fileName = fileDialog(this, tr("Save as"), dir + QLatin1String("/") + fileName,
                                        filters, nullptr, qApp->seamlyMeSettings()->getUseNativeFileDialogs(),
                                        QFileDialog::AnyFile, QFileDialog::AcceptSave);

	auto RemoveTempDir = [usedNotExistedDir, dir]()
	{
		if (usedNotExistedDir)
		{
			QDir directory(dir);
			directory.rmpath(".");
		}
	};

	if (fileName.isEmpty())
	{
		RemoveTempDir();
		return false;
	}

	QFileInfo fileInfo(fileName);
	if (fileInfo.suffix().isEmpty() && fileInfo.suffix() != suffix)
	{
		fileName += QLatin1String(".") + suffix;
	}

	if (fileInfo.exists() && fileName != filePath)
	{
		// Temporarily try to lock the file before saving
		VLockGuard<char> lock(fileName);
		if (!lock.IsLocked())
		{
			qCWarning(tMainWindow, "%s",
					   qUtf8Printable(tr("Failed to lock. This file already opened in another window.")));
			RemoveTempDir();
			return false;
		}
	}

	// Need for restoring previous state in case of failure
	const bool readOnly = individualMeasurements->isReadOnly();

	individualMeasurements->SetReadOnly(false);
	m_isReadOnly = false;

	QString error;
	bool result = SaveMeasurements(fileName, error);
	if (result == false)
	{
		QMessageBox messageBox;
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setInformativeText(tr("Could not save file"));
		messageBox.setDefaultButton(QMessageBox::Ok);
		messageBox.setDetailedText(error);
		messageBox.setStandardButtons(QMessageBox::Ok);
		messageBox.exec();

		// Restore previous state
		individualMeasurements->SetReadOnly(readOnly);
		m_isReadOnly = readOnly;
		RemoveTempDir();
		return false;
	}

	UpdatePadlock(false);
	UpdateWindowTitle();

    if (fileName != filePath)
    {
        VlpCreateLock(lock, fileName);
	    if (!lock->IsLocked())
        {
            qCCritical(tMainWindow, "%s", qUtf8Printable(tr("Failed to lock. This file already opened in another window. "
														    "Expect collisions when running 2 copies of the program.")));
		    RemoveTempDir();
	        return false;
	    }
    }

	RemoveTempDir();
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::AboutToShowWindowMenu()
{
	ui->window_Menu->clear();
	CreateWindowMenu(ui->window_Menu);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowWindow() const
{
	if (QAction *action = qobject_cast<QAction*>(sender()))
	{
		const QVariant v = action->data();
		if (v.canConvert<int>())
		{
			const int offset = qvariant_cast<int>(v);
			const QList<TMainWindow*> windows = qApp->mainWindows();
			windows.at(offset)->raise();
			windows.at(offset)->activateWindow();
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
#if defined(Q_OS_MAC)
void TMainWindow::AboutToShowDockMenu()
{
	if (QMenu *menu = qobject_cast<QMenu *>(sender()))
	{
		menu->clear();
		CreateWindowMenu(menu);

		menu->addSeparator();

		menu->addAction(ui->actionOpenIndividual);
		menu->addAction(ui->actionOpenMultisize);
		menu->addAction(ui->actionOpenTemplate);

		menu->addSeparator();

		QAction *actionPreferences = menu->addAction(tr("Preferences"));
		actionPreferences->setMenuRole(QAction::NoRole);
		connect(actionPreferences, &QAction::triggered, this, &TMainWindow::Preferences);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::OpenAt(QAction *where)
{
	const QString path = curFile.left(curFile.indexOf(where->text())) + where->text();
	if (path == curFile)
	{
		return;
	}
	QProcess process;
	process.start(QStringLiteral("/usr/bin/open"), QStringList() << path, QIODevice::ReadOnly);
	process.waitForFinished();
}
#endif //defined(Q_OS_MAC)

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveGivenName()
{
	if (individualMeasurements->GivenName() != ui->lineEditGivenName->text())
	{
		individualMeasurements->SetGivenName(ui->lineEditGivenName->text());
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveFamilyName()
{
	if (individualMeasurements->FamilyName() != ui->lineEditFamilyName->text())
	{
		individualMeasurements->SetFamilyName(ui->lineEditFamilyName->text());
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveEmail()
{
	if (individualMeasurements->Email() != ui->lineEditEmail->text())
	{
		individualMeasurements->SetEmail(ui->lineEditEmail->text());
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveGender(int index)
{
	const GenderType type = static_cast<GenderType>(ui->comboBoxGender->itemData(index).toInt());
	if (individualMeasurements->Gender() != type)
	{
		individualMeasurements->SetGender(type);
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveBirthDate(const QDate &date)
{
	if (individualMeasurements->BirthDate() != date)
	{
		individualMeasurements->SetBirthDate(date);
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveNotes()
{
	if (individualMeasurements->Notes() != ui->plainTextEditNotes->toPlainText())
	{
		individualMeasurements->SetNotes(ui->plainTextEditNotes->toPlainText());
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SavePMSystem(int index)
{
	QString system = ui->comboBoxPMSystem->itemData(index).toString();
	system.remove(0, 1);// clear p

	if (individualMeasurements->PMSystem() != system)
	{
		individualMeasurements->SetPMSystem(system);
		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::Remove()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), 0);
	const QString removedName = nameField->data(Qt::UserRole).toString();
	individualMeasurements->Remove(removedName);

	// A deleted measurement's leftover draft/reminder-flag bookkeeping would otherwise sit in these
	// maps forever (they're only ever cleared by name, on resave/view -- see MarkMeasurementSaved()
	// and ShowNewMData()), ready to wrongly apply to some future, unrelated measurement that
	// happens to get the same name later. m_usedByMeasurement doesn't need this: RefreshTable()
	// rebuilds it from scratch on every call.
	m_formulaDrafts.remove(removedName);
	m_affectedMeasurements.remove(removedName);

	MeasurementsWasSaved(false);

	m_search->removeRow(row);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	if (ui->tableWidget->rowCount() > 0)
	{
		// row is the just-removed measurement's old position. If it was the last row, that
		// index no longer exists after removal -- selectRow() would silently do nothing, the
		// Details panel would keep showing the deleted row's now-stale data, and nothing would
		// end up selected at all. Clamp to the new last row in that case (effectively landing on
		// the previous row, which is what's expected when deleting the last one).
		ui->tableWidget->selectRow(qMin(row, ui->tableWidget->rowCount() - 1));
	}
	else
	{
		MFields(false);

		ui->actionExportToCSV->setEnabled(false);

		ui->lineEditName->blockSignals(true);
		ui->lineEditName->setText("");
		ui->lineEditName->blockSignals(false);

		ui->plainTextEditDescription->blockSignals(true);
		ui->plainTextEditDescription->setPlainText("");
		ui->plainTextEditDescription->blockSignals(false);

		ui->lineEditFullName->blockSignals(true);
		ui->lineEditFullName->setText("");
		ui->lineEditFullName->blockSignals(false);

		if (mType == MeasurementsType::Multisize)
		{
			ui->labelCalculatedValue->blockSignals(true);
			ui->doubleSpinBoxBaseValue->blockSignals(true);
			ui->doubleSpinBoxInSizes->blockSignals(true);
			ui->doubleSpinBoxInHeights->blockSignals(true);

			ui->labelCalculatedValue->setText("");
			ui->doubleSpinBoxBaseValue->setValue(0);
			ui->doubleSpinBoxInSizes->setValue(0);
			ui->doubleSpinBoxInHeights->setValue(0);

			ui->labelCalculatedValue->blockSignals(false);
			ui->doubleSpinBoxBaseValue->blockSignals(false);
			ui->doubleSpinBoxInSizes->blockSignals(false);
			ui->doubleSpinBoxInHeights->blockSignals(false);
		}
		else
		{
			ui->labelCalculatedValue->blockSignals(true);
			ui->labelCalculatedValue->setText("");
			ui->labelCalculatedValue->blockSignals(false);

			ui->plainTextEditFormula->blockSignals(true);
			ui->plainTextEditFormula->setPlainText("");
			ui->plainTextEditFormula->blockSignals(false);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MoveTop()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	individualMeasurements->MoveTop(nameField->data(Qt::UserRole).toString());
	MeasurementsWasSaved(false);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(0);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MoveUp()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	individualMeasurements->MoveUp(nameField->data(Qt::UserRole).toString());
	MeasurementsWasSaved(false);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(row-1);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MoveDown()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	individualMeasurements->MoveDown(nameField->data(Qt::UserRole).toString());
	MeasurementsWasSaved(false);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(row+1);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MoveBottom()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	individualMeasurements->MoveBottom(nameField->data(Qt::UserRole).toString());
	MeasurementsWasSaved(false);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(ui->tableWidget->rowCount()-1);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::Fx()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);

	QSharedPointer<MeasurementVariable> meash;

	try
	{
	   // Translate to internal look.
	   meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
	}

	catch(const VExceptionBadId &exception)
	{
		qCCritical(tMainWindow, "%s\n\n%s\n\n%s",
				   qUtf8Printable(tr("Can't find measurement '%1'.").arg(nameField->text())),
				   qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	// The "Функция" dialog's Measurements list should only offer rows that make sense to pick:
	// this measurement's private snapshot (see readMeasurements()) already excludes anything
	// defined after it in the file, but section-divider rows -- organizational only, never a
	// real value, see checkBoxIsSection/IsSection() -- are still in there and would be a
	// meaningless, broken choice to insert into a formula. Strip them out before handing the
	// snapshot to the dialog. Safe to mutate: GetData() returns this one measurement's own
	// private copy (taken when the file was last read/refreshed), not shared with anyone else.
	{
		const QMap<QString, QSharedPointer<MeasurementVariable>> snapshotMeasurements =
				meash->GetData()->DataMeasurements();
		QMapIterator<QString, QSharedPointer<MeasurementVariable>> iSnap(snapshotMeasurements);
		while (iSnap.hasNext())
		{
			iSnap.next();
			if (iSnap.value()->IsSection())
			{
				meash->GetData()->RemoveVariable(iSnap.key());
			}
		}
	}

	EditFormulaDialog *dialog = new EditFormulaDialog(meash->GetData(), NULL_ID, MeasurementDialog, this);
	dialog->setWindowTitle(tr("Edit measurement"));
	// Keep line breaks as typed when opening the dialog -- flattening them here used to throw
	// away her multi-line formatting the moment "Функция" was opened, before she'd even touched
	// anything. The parser treats a line break as insignificant whitespace either way (same as
	// TranslateVariables()/EvalFormula() elsewhere in this file), so nothing about evaluation
	// depends on flattening it first.
	dialog->SetFormula(qApp->translateVariables()->TryFormulaFromUser(ui->plainTextEditFormula->toPlainText(), true));
	const QString postfix = UnitsToStr(mUnit, true);//Show unit in dialog label (cm, mm or inch)
	dialog->setPostfix(postfix);

	if (dialog->exec() == QDialog::Accepted)
	{
		// Because of the bug need to take QTableWidgetItem twice time. Previous update "killed" the pointer.
		const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
		const QString newFormula = dialog->GetFormula();

		// The list above already strips section-divider rows out of what this dialog offers to
		// pick from, but nothing stops her from typing a name by hand instead -- and the result
		// (e.g. "Тест + Тест") is otherwise perfectly valid math, so the dialog's own validation
		// has no reason to refuse it. Same check as CommitMValueFor()/RefreshTable(): refuse to
		// commit it here too, keep it as a problem (pink) draft, and explain why, instead of silently
		// accepting a meaningless result.
		QString sectionRefName;
		if (FormulaReferencesSection(newFormula, &sectionRefName))
		{
			const QString errPostfix = UnitsToStr(mUnit);
			const QString message = tr("This formula uses \"%1\", which is a section divider "
										"and can't be used in calculations.").arg(sectionRefName);
			ui->labelCalculatedValue->setText(tr("Error") + " (" + errPostfix + "). " + message);
			ui->labelCalculatedValue->setToolTip(message);

			QString userFormula;
			try
			{
				userFormula = qApp->translateVariables()->FormulaToUser(newFormula, qApp->Settings()->getOsSeparator());
			}
			catch (qmu::QmuParserError &error)
			{
				Q_UNUSED(error)
				userFormula = newFormula;
			}
			m_formulaDrafts.insert(nameField->data(Qt::UserRole).toString(), userFormula);
			SetRowHighlight(row, rowHighlightProblem);

			delete dialog;
			return;
		}

		individualMeasurements->SetMValue(nameField->data(Qt::UserRole).toString(), newFormula);
		MarkMeasurementSaved(nameField->data(Qt::UserRole).toString());

		MeasurementsWasSaved(false);

		RefreshData();

		m_search->refreshList(ui->find_LineEdit->text());

		ui->tableWidget->selectRow(row);
	}
	delete dialog;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::AddCustom()
{
	const QString name = GetCustomName();
	qint32 currentRow = -1;

	if (ui->tableWidget->currentRow() == -1)
	{
		currentRow  = ui->tableWidget->rowCount();
		individualMeasurements->addEmpty(name);
	}
	else
	{
		currentRow  = ui->tableWidget->currentRow()+1;
		const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
		individualMeasurements->AddEmptyAfter(nameField->data(Qt::UserRole).toString(), name);
	}

	m_search->addRow(currentRow);
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->selectRow(currentRow);

	ui->actionExportToCSV->setEnabled(true);

	MeasurementsWasSaved(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::AddKnown()
{
	QScopedPointer<MeasurementDatabaseDialog> dialog (new MeasurementDatabaseDialog(individualMeasurements->listKnown(), this));
	if (dialog->exec() == QDialog::Accepted)
	{
		qint32 currentRow;

		const QStringList list = dialog->getNewMeasurementNames();
		if (ui->tableWidget->currentRow() == -1)
		{
			currentRow  = ui->tableWidget->rowCount() + list.size() - 1;
			for (int i = 0; i < list.size(); ++i)
			{
				if (mType == MeasurementsType::Individual)
				{
					individualMeasurements->addEmpty(list.at(i), qApp->translateVariables()->MFormula(list.at(i)));
				}
				else
				{
					individualMeasurements->addEmpty(list.at(i));
				}

				m_search->addRow(currentRow);
			}
		}
		else
		{
			currentRow  = ui->tableWidget->currentRow() + list.size();
			const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
			QString after = nameField->data(Qt::UserRole).toString();
			for (int i = 0; i < list.size(); ++i)
			{
				if (mType == MeasurementsType::Individual)
				{
					individualMeasurements->AddEmptyAfter(after, list.at(i), qApp->translateVariables()->MFormula(list.at(i)));
				}
				else
				{
					individualMeasurements->AddEmptyAfter(after, list.at(i));
				}
				m_search->addRow(currentRow);
				after = list.at(i);
			}
		}

		RefreshData();
		m_search->refreshList(ui->find_LineEdit->text());

		ui->tableWidget->selectRow(currentRow);

		ui->actionExportToCSV->setEnabled(true);

		MeasurementsWasSaved(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ImportFromPattern()
{
	if (individualMeasurements == nullptr)
	{
		return;
	}

    const QString filter = tr("Pattern files") + QLatin1String(" (*.") + valExt +
                           QLatin1String(" *.") + sm2dExt + QLatin1String(")");
	//Use standard path to individual measurements
	QString dir = qApp->seamlyMeSettings()->getTemplatePath();
	dir = VCommonSettings::PrepareStandardTemplates(dir);

    const QString filename = fileDialog(this, tr("Import from a pattern"), dir, filter, nullptr,
                                        qApp->seamlyMeSettings()->getUseNativeFileDialogs(),
                                        QFileDialog::ExistingFile,
                                        QFileDialog::AcceptOpen);

	if (filename.isEmpty())
	{
		return;
	}

    QMessageBox::StandardButton answer = QMessageBox::Abort;
	VLockGuard<char> lock(filename);
    if (!lock.IsLocked())
    {
        answer = QMessageBox::warning(this, tr("Locking file"),
                                      tr("This file already opened in another window. Ignore if you want "
                                      "to continue (not recommended, can cause a data corruption)."),
                                      QMessageBox::Abort|QMessageBox::Ignore, QMessageBox::Abort);
        if (answer == QMessageBox::Abort)
        {
            return;
        }
    }

	QStringList measurements;
	try
	{
		VPatternConverter converter(filename);
		QScopedPointer<VLitePattern> doc(new VLitePattern());
		doc->setXMLContent(converter.Convert());
		measurements = doc->ListMeasurements();
	}

	catch (VException &exception)
	{
		qCCritical(tMainWindow, "%s\n\n%s\n\n%s", qUtf8Printable(tr("File error.")),
				   qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	measurements = FilterMeasurements(measurements, individualMeasurements->ListAll());

	qint32 currentRow;

	if (ui->tableWidget->currentRow() == -1)
	{
		currentRow  = ui->tableWidget->rowCount() + measurements.size() - 1;
		for (int i = 0; i < measurements.size(); ++i)
		{
			individualMeasurements->addEmpty(measurements.at(i));
		}
	}
	else
	{
		currentRow  = ui->tableWidget->currentRow() + measurements.size();
		const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
		QString after = nameField->data(Qt::UserRole).toString();
		for (int i = 0; i < measurements.size(); ++i)
		{
			individualMeasurements->AddEmptyAfter(after, measurements.at(i));
			after = measurements.at(i);
		}
	}

	RefreshData();

	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->selectRow(currentRow);

	MeasurementsWasSaved(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ChangedSize(int index)
{
	const int row = ui->tableWidget->currentRow();
    currentSize = gradationSizes->itemText(index).toInt();
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(row);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ChangedHeight(int index)
{
	const int row = ui->tableWidget->currentRow();
    currentHeight = gradationHeights->itemText(index).toInt();
	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());
	ui->tableWidget->selectRow(row);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowMData()
{
	ShowNewMData(true);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowNewMData(bool fresh)
{
	if (fresh && !m_editingMeasurementName.isEmpty())
	{
		const QTableWidgetItem *targetNameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
		const QString targetMeasurementName = targetNameField ? targetNameField->data(Qt::UserRole).toString()
															   : QString();

		if (targetMeasurementName != m_editingMeasurementName)
		{
			// The row selection already changed (e.g. a mouse click on another row can move
			// focus and so deliver plainTextEditFormula's FocusOut -- see eventFilter() --
			// only after the table's own selection, and this function's own repaint of that
			// field, already happened; by then CommitMValue() would read the field's
			// now-overwritten text against the WRONG row and silently discard whatever she'd
			// typed for the row she's leaving). Flush that pending edit first, explicitly by
			// name rather than trusting currentRow(), which by this point already points at
			// the new row.
			CommitMValueFor(m_editingMeasurementName, false);

			// A successful commit above calls RefreshData(), which rebuilds the whole table and
			// can leave a different row selected (or none) -- put the selection back on the row
			// she actually clicked/navigated to. Look it up by name again since a full rebuild
			// can also shift row indexes.
			if (!targetMeasurementName.isEmpty())
			{
				for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
				{
					const QTableWidgetItem *item = ui->tableWidget->item(row, ColumnName);
					if (item && item->data(Qt::UserRole).toString() == targetMeasurementName)
					{
						ui->tableWidget->blockSignals(true);
						ui->tableWidget->selectRow(row);
						ui->tableWidget->blockSignals(false);
						break;
					}
				}
			}
		}
	}

	if (ui->tableWidget->rowCount() > 0)
	{
		MFields(true);

		const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName); // name
		QSharedPointer<MeasurementVariable> meash;

		try
		{
			// Translate to internal look.
			meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
		}

		catch(const VExceptionBadId &exception)
		{
			Q_UNUSED(exception)
			m_editingMeasurementName.clear();
			MFields(false);
			return;
		}

		ShowMDiagram(meash);

		// Don't block all signal for QLineEdit. Need for correct handle with clear button.
		disconnect(ui->lineEditName, &QLineEdit::textEdited, this, &TMainWindow::SaveMName);
		ui->plainTextEditDescription->blockSignals(true);
		if (meash->isCustom())
		{
			ui->plainTextEditDescription->setPlainText(meash->GetDescription());
			ui->lineEditFullName->setText(meash->getGuiText());
			ui->lineEditName->setText(ClearCustomName(meash->GetName()));
		}
		else
		{
			//Show known
			ui->plainTextEditDescription->setPlainText(qApp->translateVariables()->Description(meash->GetName()));
			ui->lineEditFullName->setText(qApp->translateVariables()->guiText(meash->GetName()));
			ui->lineEditName->setText(nameField->text());
		}
		connect(ui->lineEditName, &QLineEdit::textEdited, this, &TMainWindow::SaveMName);
		ui->plainTextEditDescription->blockSignals(false);

		if (mType == MeasurementsType::Multisize)
		{
			// Formula-edit tracking (m_editingMeasurementName, see the flush-on-row-switch logic
			// at the top of this function) only applies to the Individual-file formula field
			// below -- make sure a stale name from an earlier selection can't linger here.
			m_editingMeasurementName.clear();

			ui->labelCalculatedValue->blockSignals(true);
			ui->doubleSpinBoxBaseValue->blockSignals(true);
			ui->doubleSpinBoxInSizes->blockSignals(true);
			ui->doubleSpinBoxInHeights->blockSignals(true);

			const QString postfix = UnitsToStr(pUnit);//Show unit in dialog label (cm, mm or inch)
			const qreal value = UnitConvertor(*data->DataVariables()->value(meash->GetName())->GetValue(), mUnit,
											  pUnit);
			ui->labelCalculatedValue->setText(qApp->LocaleToString(value) + " " +postfix);

			if (fresh)
			{
				ui->doubleSpinBoxBaseValue->setValue(meash->GetBase());
				ui->doubleSpinBoxInSizes->setValue(meash->GetKsize());
				ui->doubleSpinBoxInHeights->setValue(meash->GetKheight());
			}

			ui->labelCalculatedValue->blockSignals(false);
			ui->doubleSpinBoxBaseValue->blockSignals(false);
			ui->doubleSpinBoxInSizes->blockSignals(false);
			ui->doubleSpinBoxInHeights->blockSignals(false);
		}
		else
		{
			const QString measurementName = nameField->data(Qt::UserRole).toString();

			ui->checkBoxIsSection->blockSignals(true);
			ui->checkBoxIsSection->setChecked(meash->IsSection());
			ui->checkBoxIsSection->blockSignals(false);

			if (meash->IsSection())
			{
				// A section divider has no formula and nothing to calculate -- don't show or
				// let her edit one here. MFields(true) above enabled these widgets generically;
				// override that for this one row.
				ui->plainTextEditFormula->blockSignals(true);
				ui->plainTextEditFormula->clear();
				ui->plainTextEditFormula->blockSignals(false);
				ui->plainTextEditFormula->setEnabled(false);
				ui->toolButtonExpr->setEnabled(false);

				ui->labelCalculatedValue->setText(QString());
				ui->labelCalculatedValue->setToolTip(QString());

				// Nothing here for CommitMValue()/CommitMValueFor() to ever commit -- see the
				// flush-on-row-switch logic at the top of this function.
				m_editingMeasurementName.clear();
			}
			else
			{
				// This row's formula field is now what's on screen and editable -- record it so
				// switching to another row (see the flush logic at the top of this function)
				// knows whose pending edit to commit before showing anything else.
				m_editingMeasurementName = measurementName;

				// A formula that's currently invalid or hasn't been saved (see CommitMValue())
				// is kept separately from the model -- check it first so switching away and
				// back to this row shows what she was actually typing, not the last value that
				// saved successfully.
				const bool hasDraft = m_formulaDrafts.contains(measurementName);
				const QString draftFormula = hasDraft ? m_formulaDrafts.value(measurementName) : QString();

				if (hasDraft)
				{
					EvalFormula(draftFormula, true, meash->GetData(), ui->labelCalculatedValue);
				}
				else
				{
					EvalFormula(meash->GetFormula(), false, meash->GetData(), ui->labelCalculatedValue);

					// EvalFormula() above only catches parser failures and infinite/NaN results --
					// a formula that refers to a section divider (checkBoxIsSection) parses and
					// computes just fine on its own (the divider row still holds a numeric value,
					// see MeasurementDoc::readMeasurements()), so it needs this separate check, the
					// same one RefreshTable() uses for the row's problem highlight/red value below.
					// Without this, the Value field here would silently show a normal-looking
					// number while the table row is flagged as an error, with nothing to explain
					// why.
					QString sectionRefName;
					if (FormulaReferencesSection(meash->GetFormula(), &sectionRefName))
					{
						const QString postfix = UnitsToStr(pUnit);
						const QString message = tr("This formula uses \"%1\", which is a section "
													"divider and can't be used in calculations.")
													.arg(sectionRefName);
						ui->labelCalculatedValue->setText(tr("Error") + " (" + postfix + "). " + message);
						ui->labelCalculatedValue->setToolTip(message);
					}
				}

				ui->plainTextEditFormula->blockSignals(true);

				QString formula;
				if (hasDraft)
				{
					// Already in the same as-typed form the field holds -- no FormulaToUser()
					// translation needed (or wanted, it was never saved in internal form).
					formula = draftFormula;
				}
				else
				{
					try
					{
						formula = qApp->translateVariables()->FormulaToUser(meash->GetFormula(), qApp->Settings()->getOsSeparator());
					}
					catch (qmu::QmuParserError &error)
					{
						Q_UNUSED(error)
						formula = meash->GetFormula();
					}
				}

				ui->plainTextEditFormula->setPlainText(formula);
				ui->plainTextEditFormula->blockSignals(false);

				// Viewing this row counts as having checked it -- clear any pending "something
				// this depends on just changed" flag now instead of waiting for the next full
				// refresh. See the priority comment above rowHighlightBlue: a still-unsaved draft
				// keeps its problem highlight regardless. Otherwise, if the formula is still
				// broken after this look (e.g. it references a name that hasn't been fixed up
				// yet), upgrade the row from reminder to problem -- she's seen it, but it still
				// needs attention -- rather than clearing the highlight as if everything were
				// fine.
				if (m_affectedMeasurements.remove(measurementName))
				{
					QColor rowColor;
					if (hasDraft || !meash->IsFormulaOk())
					{
						rowColor = rowHighlightProblem;
					}
					SetRowHighlight(ui->tableWidget->currentRow(), rowColor);

					// The reminder icon (see ReminderRowIcon()) lives on the Formula item as its
					// own Qt::DecorationRole, separate from the row's background color set just
					// above -- clearing the highlight doesn't clear it, so without this line it
					// would keep showing even once the row stops being flagged, whether it
					// settles back to plain (rowColor left invalid) or turns into a problem row
					// (rowColor = rowHighlightProblem, which gets its own red error indicator
					// elsewhere and was never given this icon to begin with -- see RefreshTable()).
					if (QTableWidgetItem *formulaItem = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnFormula))
					{
						formulaItem->setIcon(QIcon());
					}
				}
			}
		}

		MeasurementGUI();
	}
	else
	{
		m_editingMeasurementName.clear();
		MFields(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
QString TMainWindow::getMeasurementNumber(const QString &name)
{
	return  qApp->translateVariables()->MNumber(name);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowMDiagram(QSharedPointer<MeasurementVariable> meash)
{
    //const QString description = meash->GetDescription();
    const QString name        = meash->GetName();
	const VTranslateVars *trv = qApp->translateVariables();
	const QString number      = trv->MNumber(name);
    const QString description = trv->Description(name);

    // Clear variables so resizes don't draw a stale image
    m_currentSvgPath.clear();
    m_currentNumber.clear();
    m_currentName.clear();
    m_currentDescription.clear();

    if (number.isEmpty())
    {
        // Save the current states for the resize event to look at later
        m_currentSvgPath     = QString("://diagrams/custom.svg");
        m_currentNumber      = tr("Custom measurement");
        m_currentName        = name;
        m_currentDescription = QStringLiteral("");
    }
    else
    {
        // Save the current states for the resize event to look at later
        m_currentSvgPath     = QString("://diagrams/%1.svg").arg(MapDiagrams(trv, number));
        m_currentNumber      = number;
        m_currentName        = trv->guiText(name);
        m_currentDescription = description;
    }

    // Execute the initial draw pass using the live dock size
    renderScaledDiagram();
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMName(const QString &text)
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);

	QSharedPointer<MeasurementVariable> meash;

	try
	{
		// Translate to internal look.
		meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
	}

	catch(const VExceptionBadId &exception)
	{
		qCWarning(tMainWindow, "%s\n\n%s\n\n%s",
				  qUtf8Printable(tr("Can't find measurement '%1'.").arg(nameField->text())),
				  qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	QString newName = text;

	if (meash->isCustom())
	{
		if (newName.isEmpty())
		{
			newName = GetCustomName();
		}

		if (!data->IsUnique(newName))
		{
			qint32 num = 2;
			QString name = newName;
			do
			{
				name = name + QLatin1String("_") + QString().number(num);
				num++;
			} while (!data->IsUnique(name));
			newName = name;
		}

		// The measurement keeps its formula/value -- nothing about IT changed -- but any other
		// row whose formula refers to it BY THIS OLD NAME just had that reference silently
		// broken (SetMName() below only renames this one measurement; it doesn't rewrite
		// formulas elsewhere that mention the old name -- see MeasurementDoc::SetMName()). Flag
		// those rows the same way an edited formula does (see MarkMeasurementSaved()), so she
		// notices and can update them, and drop any stale draft/affected bookkeeping under the
		// name that's about to stop existing so it can't leak onto some future, unrelated
		// measurement that reuses it (see the same concern in Remove()).
		const QString oldName = nameField->data(Qt::UserRole).toString();
		const QStringList affected = m_usedByMeasurement.value(oldName);
		for (const QString &affectedName : affected)
		{
			m_affectedMeasurements.insert(affectedName);
		}
		m_affectedMeasurements.remove(oldName);
		m_formulaDrafts.remove(oldName);

		individualMeasurements->SetMName(nameField->text(), newName);
		MeasurementsWasSaved(false);
		RefreshData();
		m_search->refreshList(ui->find_LineEdit->text());

		ui->tableWidget->blockSignals(true);
		ui->tableWidget->selectRow(row);
		ui->tableWidget->blockSignals(false);
	}
	else
	{
		qCWarning(tMainWindow, "%s", qUtf8Printable(tr("The name of known measurement forbidden to change.")));
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMValue()
{
	// Live preview only: update the "calculated value" label as the user types, without
	// touching the measurements model or rebuilding the whole table. On a large file with
	// many cross-referencing formulas, doing the full commit (see CommitMValue()) on every
	// keystroke made typing a formula lag by several seconds per letter. The actual commit
	// now happens once editing is finished -- see CommitMValue(), called from an eventFilter()
	// FocusOut on this field, and also flushed explicitly before saving the file (FileSave()/
	// FileSaveAs()) so Ctrl+S can never miss an edit still in progress.
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);

	// Keep line breaks as typed -- they are stored as-is (the parser treats them as
	// insignificant whitespace, same as a space, and EvalFormula() below still
	// flattens its own working copy before evaluating) so the formula keeps its
	// multi-line formatting the next time this measurement is selected, instead of
	// collapsing back to one line.
	QString text = ui->plainTextEditFormula->toPlainText();

	QTableWidgetItem *formulaField = ui->tableWidget->item(row, ColumnFormula);
	if (formulaField->text() == text)
	{
		QTableWidgetItem *result = ui->tableWidget->item(row, ColumnCalcValue);
		const QString postfix = UnitsToStr(mUnit);//Show unit in dialog label (cm, mm or inch)
		ui->labelCalculatedValue->setText(result->text() + " " +postfix);
		return;
	}

	if (text.isEmpty())
	{
		const QString postfix = UnitsToStr(mUnit);//Show unit in dialog label (cm, mm or inch)
		ui->labelCalculatedValue->setText(tr("Error") + " (" + postfix + "). " + tr("Empty field."));
		return;
	}

	QSharedPointer<MeasurementVariable> meash;
	try
	{
		// Translate to internal look.
		meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
	}

	catch(const VExceptionBadId &exception)
	{
		qCWarning(tMainWindow, "%s\n\n%s\n\n%s",
				  qUtf8Printable(tr("Can't find measurement '%1'.").arg(nameField->text())),
				  qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	// Just refresh the live preview label -- EvalFormula() only evaluates this one formula,
	// it does not touch the rest of the table, so this stays cheap even on a big file.
	EvalFormula(text, true, meash->GetData(), ui->labelCalculatedValue);
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Commit the formula currently in the editor to the measurements model and refresh the
 * whole table, for whichever measurement is named by measurementName -- not necessarily
 * ui->tableWidget->currentRow(), which may already have moved on to a different row by the
 * time this runs (see ShowNewMData()'s flush-on-row-switch logic; CommitMValue() below is the
 * normal FocusOut-triggered path and always targets the row that's still current). This is the
 * expensive half of what used to be SaveMValue() -- it recalculates every measurement
 * (readMeasurements(), via RefreshData()) and rebuilds every row of the table (RefreshTable()),
 * which is why it only runs once editing is finished rather than on every keystroke. Safe to
 * call when nothing changed -- it compares against the stored formula first and does nothing
 * if they already match.
 * @param restoreSelection re-select the committed row afterward (blockSignals'd, so it won't
 * re-trigger ShowNewMData()) and restore the text cursor. Pass false when the caller is about
 * to select a different row itself right after (the row-switch flush), so this doesn't fight
 * that -- the row being flushed is being left, not shown.
 */
void TMainWindow::CommitMValueFor(const QString &measurementName, bool restoreSelection)
{
	if (measurementName.isEmpty())
	{
		return;
	}

	int row = -1;
	for (int candidate = 0; candidate < ui->tableWidget->rowCount(); ++candidate)
	{
		const QTableWidgetItem *item = ui->tableWidget->item(candidate, ColumnName);
		if (item && item->data(Qt::UserRole).toString() == measurementName)
		{
			row = candidate;
			break;
		}
	}

	if (row == -1)
	{
		// The row is gone (e.g. deleted from under this pending edit) -- nothing to commit to.
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);

	QString text = ui->plainTextEditFormula->toPlainText();

	QTableWidgetItem *formulaField = ui->tableWidget->item(row, ColumnFormula);
	if (formulaField->text() == text)
	{
		return;
	}

	if (text.isEmpty())
	{
		// Only surface the "Empty field" error once the user actually leaves the field still
		// empty -- not the moment it becomes empty (e.g. right after the auto-clear-on-focus in
		// eventFilter(), which silences this same message on purpose so it doesn't flash up
		// before they've had a chance to type anything).
		const QString postfix = UnitsToStr(mUnit);//Show unit in dialog label (cm, mm or inch)
		ui->labelCalculatedValue->setText(tr("Error") + " (" + postfix + "). " + tr("Empty field."));
		return;
	}

	QSharedPointer<MeasurementVariable> meash;
	try
	{
		// Translate to internal look.
		meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
	}
	catch(const VExceptionBadId &exception)
	{
		qCWarning(tMainWindow, "%s\n\n%s\n\n%s",
				  qUtf8Printable(tr("Can't find measurement '%1'.").arg(nameField->text())),
				  qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	if (!EvalFormula(text, true, meash->GetData(), ui->labelCalculatedValue))
	{
		// Doesn't parse/evaluate right now, so it can't be committed to the model (that would
		// corrupt the pattern) -- but don't just drop it either. Keep it as a draft so switching
		// to another row and back doesn't silently replace what she typed with the last value
		// that did save -- see ShowNewMData(), which checks this map before falling back to
		// meash->GetFormula(). Recolor this row (problem/pink) right away rather than waiting for a
		// full refresh, which would also undo the point of the CommitMValue/RefreshTable split.
		m_formulaDrafts.insert(nameField->data(Qt::UserRole).toString(), text);
		SetRowHighlight(row, rowHighlightProblem);
		return;
	}

	QString internalFormula;
	try
	{
		internalFormula = qApp->translateVariables()->FormulaFromUser(text, qApp->Settings()->getOsSeparator());
	}
	catch (qmu::QmuParserError &error) // Just in case something bad will happen
	{
		Q_UNUSED(error)
		return;
	}

	// A formula that parses and computes fine (EvalFormula() above passed) can still be wrong in
	// a way EvalFormula() has no way to see: it may refer to a measurement that's a section
	// divider (checkBoxIsSection), which has no real value to use -- see
	// FormulaReferencesSection(). Catch that here too, before it's accepted as a committed
	// result, with the same "keep as a problem draft, don't commit" treatment as an outright
	// EvalFormula() failure above -- otherwise a formula like "Тест + Тест" (both operands the
	// same section-divider measurement) sails through as a normal-looking committed value.
	QString sectionRefName;
	if (FormulaReferencesSection(internalFormula, &sectionRefName))
	{
		const QString postfix = UnitsToStr(mUnit);
		const QString message = tr("This formula uses \"%1\", which is a section divider and "
									"can't be used in calculations.").arg(sectionRefName);
		ui->labelCalculatedValue->setText(tr("Error") + " (" + postfix + "). " + message);
		ui->labelCalculatedValue->setToolTip(message);

		m_formulaDrafts.insert(nameField->data(Qt::UserRole).toString(), text);
		SetRowHighlight(row, rowHighlightProblem);
		return;
	}

	try
	{
		individualMeasurements->SetMValue(nameField->data(Qt::UserRole).toString(), internalFormula);
		MarkMeasurementSaved(nameField->data(Qt::UserRole).toString());
	}
	catch (qmu::QmuParserError &error) // Just in case something bad will happen
	{
		Q_UNUSED(error)
		return;
	}

	MeasurementsWasSaved(false);

	const QTextCursor cursor = ui->plainTextEditFormula->textCursor();

	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	if (restoreSelection)
	{
		// Find this row again by name -- RefreshData() just rebuilt the whole table, so the old
		// row index may no longer point at the same measurement.
		int newRow = -1;
		for (int candidate = 0; candidate < ui->tableWidget->rowCount(); ++candidate)
		{
			const QTableWidgetItem *item = ui->tableWidget->item(candidate, ColumnName);
			if (item && item->data(Qt::UserRole).toString() == measurementName)
			{
				newRow = candidate;
				break;
			}
		}

		if (newRow != -1)
		{
			ui->tableWidget->blockSignals(true);
			ui->tableWidget->selectRow(newRow);
			ui->tableWidget->blockSignals(false);
		}

		ui->plainTextEditFormula->setTextCursor(cursor);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Commit whatever's in the formula editor right now to the row it's currently showing.
 * The normal path: called on FocusOut (see eventFilter()) once editing finishes, and also
 * flushed explicitly before saving the file (FileSave()/FileSaveAs()) so Ctrl+S can never miss
 * an edit still in progress. See CommitMValueFor() for the actual work; ShowNewMData() is the
 * other caller, and targets a specific (possibly no-longer-current) row by name instead.
 */
void TMainWindow::CommitMValue()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	if (!nameField)
	{
		return;
	}

	CommitMValueFor(nameField->data(Qt::UserRole).toString(), true);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMBaseValue(double value)
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
	individualMeasurements->SetMBaseValue(nameField->data(Qt::UserRole).toString(), value);

	MeasurementsWasSaved(false);

	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->blockSignals(true);
	ui->tableWidget->selectRow(row);
	ui->tableWidget->blockSignals(false);

	ShowNewMData(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMSizeIncrease(double value)
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
	individualMeasurements->SetMSizeIncrease(nameField->data(Qt::UserRole).toString(), value);

	MeasurementsWasSaved(false);

	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->blockSignals(true);
	ui->tableWidget->selectRow(row);
	ui->tableWidget->blockSignals(false);

	ShowNewMData(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMHeightIncrease(double value)
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
	individualMeasurements->SetMHeightIncrease(nameField->data(Qt::UserRole).toString(), value);

	MeasurementsWasSaved(false);

	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->blockSignals(true);
	ui->tableWidget->selectRow(row);
	ui->tableWidget->blockSignals(false);

	ShowNewMData(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMDescription()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);
	individualMeasurements->SetMDescription(nameField->data(Qt::UserRole).toString(), ui->plainTextEditDescription->toPlainText());

	MeasurementsWasSaved(false);

	const QTextCursor cursor = ui->plainTextEditDescription->textCursor();

	RefreshData();

	ui->tableWidget->blockSignals(true);
	ui->tableWidget->selectRow(row);
	ui->tableWidget->blockSignals(false);

	ui->plainTextEditDescription->setTextCursor(cursor);
}


//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SaveMFullName()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName);

	QSharedPointer<MeasurementVariable> meash;

	try
	{
		// Translate to internal look.
		meash = data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
	}

	catch(const VExceptionBadId &exception)
	{
		qCWarning(tMainWindow, "%s\n\n%s\n\n%s",
				  qUtf8Printable(tr("Can't find measurement '%1'.").arg(nameField->text())),
				  qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));
		return;
	}

	if (meash->isCustom())
	{
		individualMeasurements->SetMFullName(nameField->data(Qt::UserRole).toString(), ui->lineEditFullName->text());

		MeasurementsWasSaved(false);

		RefreshData();

		ui->tableWidget->blockSignals(true);
		ui->tableWidget->selectRow(row);
		ui->tableWidget->blockSignals(false);
	}
	else
	{
		qCWarning(tMainWindow, "%s", qUtf8Printable(tr("The full name of known measurement forbidden to change.")));
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Toggle whether the selected row is a section divider -- see checkBoxIsSection in
 * tmainwindow.ui, MeasurementDoc::AttrIsSection, and the blue row highlight in RefreshTable().
 * Not restricted to custom measurements (unlike SaveMFullName() above) -- there's no reason a
 * known/library measurement couldn't also be repurposed as a divider.
 */
void TMainWindow::SaveMIsSection(bool checked)
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	const QTableWidgetItem *nameField = ui->tableWidget->item(row, ColumnName);
	individualMeasurements->SetMIsSection(nameField->data(Qt::UserRole).toString(), checked);

	MeasurementsWasSaved(false);

	RefreshData();
	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->blockSignals(true);
	ui->tableWidget->selectRow(row);
	ui->tableWidget->blockSignals(false);

	ShowNewMData(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::patternUnitsChanged(int index)
{
	pUnit = static_cast<Unit>(comboBoxUnits->itemData(index).toInt());

	updatePatternUnit();
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetupMenu()
{
	// File
	connect(ui->actionNew, &QAction::triggered, this, &TMainWindow::FileNew);
	connect(ui->actionOpenIndividual, &QAction::triggered, this, &TMainWindow::OpenIndividual);
	connect(ui->actionOpenMultisize, &QAction::triggered, this, &TMainWindow::OpenMultisize);
	connect(ui->actionOpenTemplate, &QAction::triggered, this, &TMainWindow::OpenTemplate);
	connect(ui->actionCreateFromExisting, &QAction::triggered, this, &TMainWindow::CreateFromExisting);

    //connect(ui->bodyScanner1_Action, &QAction::triggered, this, &TMainWindow::handleBodyScanner1);
	connect(ui->bodyScanner2_Action, &QAction::triggered, this, &TMainWindow::handleBodyScanner2);

	connect(ui->print_Action, &QAction::triggered, this, &TMainWindow::print);
    connect(ui->actionSave, &QAction::triggered, this, &TMainWindow::FileSave);
	connect(ui->actionSaveAs, &QAction::triggered, this, &TMainWindow::FileSaveAs);
	connect(ui->actionExportToCSV, &QAction::triggered, this, &TMainWindow::handleExportToCSV);
	connect(ui->actionReadOnly, &QAction::triggered, this, [this](bool ro)
	{
		if (!m_isReadOnly)
		{
			individualMeasurements->SetReadOnly(ro);
			MeasurementsWasSaved(false);
			UpdatePadlock(ro);
			UpdateWindowTitle();
		}
		else
		{
			if (QAction *action = qobject_cast< QAction * >(this->sender()))
			{
				action->setChecked(true);
			}
		}
	});
	connect(ui->actionPreferences, &QAction::triggered, this, &TMainWindow::Preferences);

	for (int i = 0; i < MaxRecentFiles; ++i)
	{
		QAction *action = new QAction(this);
		recentFileActs[i] = action;
		connect(action, &QAction::triggered, this, [this]()
		{
			QAction *action = qobject_cast<QAction *>(sender());
			if (action)
			{
				const QString filePath = action->data().toString();
				if (!filePath.isEmpty())
				{
					LoadFile(filePath);
				}
			}
		});
		ui->menuFile->insertAction(ui->actionPreferences, recentFileActs[i]);
		recentFileActs[i]->setVisible(false);
	}

	separatorAct = new QAction(this);
	separatorAct->setSeparator(true);
	separatorAct->setVisible(false);
	ui->menuFile->insertAction(ui->actionPreferences, separatorAct );


	connect(ui->actionQuit, &QAction::triggered, this, &TMainWindow::close);

	// Measurements
	connect(ui->actionAddCustom, &QAction::triggered, this, &TMainWindow::AddCustom);
	connect(ui->actionAddKnown, &QAction::triggered, this, &TMainWindow::AddKnown);
	connect(ui->actionDatabase, &QAction::triggered, qApp, &ApplicationME::showDataBase);
	connect(ui->actionImportFromPattern, &QAction::triggered, this, &TMainWindow::ImportFromPattern);
	actionDockDiagram = ui->dockWidgetDiagram->toggleViewAction();
	actionDockDiagram->setMenuRole(QAction::NoRole);
	ui->measurements_Menu->addAction(actionDockDiagram);
	actionDockDiagram->setEnabled(false);
	actionDockDiagram->setIcon(QIcon("://seamlymeicon/24x24/mannequin.png"));

	// Window
	connect(ui->window_Menu, &QMenu::aboutToShow, this, &TMainWindow::AboutToShowWindowMenu);
	AboutToShowWindowMenu();

	// Help
    connect(ui->shortcuts_Action, &QAction::triggered, this, [this]()
    {
        MeShortcutsDialog *shortcutsDialog = new MeShortcutsDialog(this);
        shortcutsDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        shortcutsDialog->show();
    });
	connect(ui->actionAboutQt, &QAction::triggered, this, [this]()
	{
		QMessageBox::aboutQt(this, tr("About Qt"));
	});
	connect(ui->actionAboutSeamlyMe, &QAction::triggered, this, [this]()
	{
		DialogAboutSeamlyMe *aboutDialog = new DialogAboutSeamlyMe(this);
		aboutDialog->setAttribute(Qt::WA_DeleteOnClose, true);
		aboutDialog->show();
	});

	//Actions for recent files loaded by a seamlyme window application.
	UpdateRecentFileActions();

}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::InitWindow()
{
	SCASSERT(individualMeasurements != nullptr)
	ui->labelToolTip->setVisible(false);
	ui->tabWidget->setVisible(true);
	ui->dockWidgetDiagram->setVisible(dockDiagramVisible);
	actionDockDiagram->setEnabled(true);
	ui->tabWidget->setCurrentIndex(0);

	ui->plainTextEditNotes->setEnabled(true);
	ui->toolBarGradation->setVisible(true);

	if (mType == MeasurementsType::Multisize)
	{
		ui->labelMType->setText(tr("Multisize measurements"));

		// Base value/In sizes/In heights are only meaningful for multisize measurements.
		ui->widgetBaseValue->setVisible(true);
		ui->widgetInSizes->setVisible(true);
		ui->widgetInHeights->setVisible(true);

		ui->labelBaseSizeValue->setText(QString().setNum(individualMeasurements->BaseSize()) + " " +
										UnitsToStr(individualMeasurements->measurementUnits(), true));
		ui->labelBaseHeightValue->setText(QString().setNum(individualMeasurements->BaseHeight()) + " " +
										  UnitsToStr(individualMeasurements->measurementUnits(), true));

		// Because Qt Designer doesn't know about our deleting we will create empty objects for correct
		// working the retranslation UI
		// Tab Measurements
		HackWidget(&ui->plainTextEditFormula);
		HackWidget(&ui->toolButtonExpr);
		HackWidget(&ui->labelFormula);

		// Tab Information
		HackWidget(&ui->lineEditGivenName);
		HackWidget(&ui->lineEditFamilyName);
		HackWidget(&ui->comboBoxGender);
		HackWidget(&ui->lineEditEmail);
		HackWidget(&ui->labelGivenName);
		HackWidget(&ui->labelFamilyName);
		HackWidget(&ui->labelBirthDate);
		HackWidget(&ui->dateEditBirthDate);
		HackWidget(&ui->labelGender);
		HackWidget(&ui->labelEmail);

		const QStringList listHeights = MeasurementVariable::WholeListHeights(mUnit);
		const QStringList listSizes = MeasurementVariable::WholeListSizes(mUnit);

		labelGradationHeights = new QLabel(tr("Height:"));
		gradationHeights = SetGradationList(labelGradationHeights, listHeights);
		SetDefaultHeight(static_cast<int>(VContainer::height()));
		connect(gradationHeights, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &TMainWindow::ChangedHeight);

		labelGradationSizes = new QLabel(tr("Size:"));
		gradationSizes = SetGradationList(labelGradationSizes, listSizes);
		SetDefaultSize(static_cast<int>(VContainer::size()));
		connect(gradationSizes, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &TMainWindow::ChangedSize);

		connect(ui->doubleSpinBoxBaseValue,
				static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
				this, &TMainWindow::SaveMBaseValue);
		connect(ui->doubleSpinBoxInSizes,
				static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
				this, &TMainWindow::SaveMSizeIncrease);
		connect(ui->doubleSpinBoxInHeights,
				static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
				this, &TMainWindow::SaveMHeightIncrease);

		SetDecimals();
	}
	else
	{
		ui->labelMType->setText(tr("Individual measurements"));

		ui->lineEditGivenName->setEnabled(true);
		ui->lineEditFamilyName->setEnabled(true);
		ui->dateEditBirthDate->setEnabled(true);
		ui->comboBoxGender->setEnabled(true);
		ui->lineEditEmail->setEnabled(true);

		// Tab Measurements
		HackWidget(&ui->doubleSpinBoxBaseValue);
		HackWidget(&ui->doubleSpinBoxInSizes);
		HackWidget(&ui->doubleSpinBoxInHeights);
		HackWidget(&ui->labelBaseValue);
		HackWidget(&ui->labelInSizes);
		HackWidget(&ui->labelInHeights);

		// Individual measurements don't use Base value/In sizes/In heights.
		// Hide the whole row so it doesn't keep reserving empty space on the form.
		ui->widgetBaseValue->setVisible(false);
		ui->widgetInSizes->setVisible(false);
		ui->widgetInHeights->setVisible(false);

		// Tab Information
		HackWidget(&ui->labelBaseSize);
		HackWidget(&ui->labelBaseSizeValue);
		HackWidget(&ui->labelBaseHeight);
		HackWidget(&ui->labelBaseHeightValue);

		ui->lineEditGivenName->setText(individualMeasurements->GivenName());
		ui->lineEditFamilyName->setText(individualMeasurements->FamilyName());

		ui->comboBoxGender->clear();
		InitGender(ui->comboBoxGender);
		const qint32 index = ui->comboBoxGender->findData(static_cast<int>(individualMeasurements->Gender()));
		ui->comboBoxGender->setCurrentIndex(index);

		{
			const QLocale dateLocale = QLocale(qApp->Settings()->getLocale());
			ui->dateEditBirthDate->setLocale(dateLocale);
			ui->dateEditBirthDate->setDisplayFormat(dateLocale.dateFormat());
			ui->dateEditBirthDate->setDate(individualMeasurements->BirthDate());
		}

		ui->lineEditEmail->setText(individualMeasurements->Email());

		connect(ui->lineEditGivenName, &QLineEdit::editingFinished, this, &TMainWindow::SaveGivenName);
		connect(ui->lineEditFamilyName, &QLineEdit::editingFinished, this, &TMainWindow::SaveFamilyName);
		connect(ui->lineEditEmail, &QLineEdit::editingFinished, this, &TMainWindow::SaveEmail);
		connect(ui->comboBoxGender, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
				&TMainWindow::SaveGender);
		connect(ui->dateEditBirthDate, &QDateEdit::dateChanged, this, &TMainWindow::SaveBirthDate);

		connect(ui->plainTextEditFormula, &QPlainTextEdit::textChanged, this, &TMainWindow::SaveMValue,
				Qt::UniqueConnection);

		connect(ui->toolButtonExpr, &QToolButton::clicked, this, &TMainWindow::Fx);
	}

	ui->comboBoxPMSystem->setEnabled(true);
	ui->comboBoxPMSystem->clear();
	InitPMSystems(ui->comboBoxPMSystem);
	const qint32 index = ui->comboBoxPMSystem->findData(QLatin1Char('p')+individualMeasurements->PMSystem());
	ui->comboBoxPMSystem->setCurrentIndex(index);
	connect(ui->comboBoxPMSystem, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
			&TMainWindow::SavePMSystem);

	connect(ui->find_LineEdit, &QLineEdit::textChanged, [this] (const QString &text){m_search->find(text);});
	connect(ui->toolButtonFindPrevious, &QToolButton::clicked, [this] (){m_search->findPrevious();});
	connect(ui->toolButtonFindNext, &QToolButton::clicked, [this] (){m_search->findNext();});
    connect(ui->regex_ToolButton, &QToolButton::toggled, [this] (const bool &checked)
    {
        if (checked)
        {
            ui->case_ToolButton->blockSignals(true);
            ui->case_ToolButton->setChecked(false);
            ui->case_ToolButton->blockSignals(false);
            m_search->setMatchCase(false);

            ui->word_ToolButton->blockSignals(true);
            ui->word_ToolButton->setChecked(false);
            ui->word_ToolButton->blockSignals(false);
            m_search->setMatchWord(false);
        }

        m_search->setMatchRegEx(checked);
        m_search->find(ui->find_LineEdit->text());
    });

    connect(ui->case_ToolButton,  &QToolButton::toggled, [this] (const bool &checked)
    {
        if (checked)
        {
            ui->regex_ToolButton->blockSignals(true);
            ui->regex_ToolButton->setChecked(false);
            ui->regex_ToolButton->blockSignals(false);
            m_search->setMatchRegEx(false);
        }

        m_search->setMatchCase(checked);
        m_search->find(ui->find_LineEdit->text());
    });

    connect(ui->word_ToolButton,  &QToolButton::toggled, [this] (const bool &checked)
    {
        if (checked)
        {
            ui->regex_ToolButton->blockSignals(true);
            ui->regex_ToolButton->setChecked(false);
            ui->regex_ToolButton->blockSignals(false);
            m_search->setMatchRegEx(false);
        }

        m_search->setMatchWord(checked);
        m_search->find(ui->find_LineEdit->text());
    });

	connect(m_search.data(), &VTableSearch::hasResult, this, [this] (bool state)
	{
		ui->toolButtonFindPrevious->setEnabled(state);
	});
    connect(ui->clipboard_ToolButton, &QToolButton::clicked, this, &TMainWindow::copyToClipboard);
	connect(m_search.data(), &VTableSearch::hasResult, this, [this] (bool state)
	{
		ui->toolButtonFindNext->setEnabled(state);
	});

	ui->plainTextEditNotes->setPlainText(individualMeasurements->Notes());
	connect(ui->plainTextEditNotes, &QPlainTextEdit::textChanged, this, &TMainWindow::SaveNotes);

	ui->actionAddCustom->setEnabled(true);
	ui->actionAddKnown->setEnabled(true);
	ui->actionImportFromPattern->setEnabled(true);
	ui->actionSaveAs->setEnabled(true);

	ui->lineEditName->setValidator(new QRegularExpressionValidator(QRegularExpression(
																	   QLatin1String("^$|")+NameRegExp()),
																   this));

	connect(ui->toolButtonRemove, &QToolButton::clicked, this, &TMainWindow::Remove);
	connect(ui->toolButtonTop, &QToolButton::clicked, this, &TMainWindow::MoveTop);
	connect(ui->toolButtonUp, &QToolButton::clicked, this, &TMainWindow::MoveUp);
	connect(ui->toolButtonDown, &QToolButton::clicked, this, &TMainWindow::MoveDown);
	connect(ui->toolButtonBottom, &QToolButton::clicked, this, &TMainWindow::MoveBottom);

	connect(ui->lineEditName, &QLineEdit::textEdited, this, &TMainWindow::SaveMName);
	connect(ui->plainTextEditDescription, &QPlainTextEdit::textChanged, this, &TMainWindow::SaveMDescription);
	connect(ui->lineEditFullName, &QLineEdit::textEdited, this, &TMainWindow::SaveMFullName);
	connect(ui->checkBoxIsSection, &QCheckBox::toggled, this, &TMainWindow::SaveMIsSection);

	connect(ui->pushButtonShowInExplorer, &QPushButton::clicked, this, [this]()
	{
		ShowInGraphicalShell(curFile);
	});

	initUnits();

	initializeTable();
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::RetranslateTableHeaders()
{
	// The Description column reuses the same translatable source text as the
	// Description field's own label ("Description:"), so it picks up the
	// translation already set up for that field instead of needing a new one.
	// The trailing colon fits a form label but not a table header, so it is
	// stripped after translation.
	QString descriptionHeader = tr("Description:");
	if (descriptionHeader.endsWith(QLatin1Char(':')))
	{
		descriptionHeader.chop(1);
	}

	QTableWidgetItem *headerItem = ui->tableWidget->horizontalHeaderItem(ColumnDescription);
	if (headerItem != nullptr)
	{
		headerItem->setText(descriptionHeader);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::initializeTable()
{
	if (mType == MeasurementsType::Multisize)
	{
		ui->tableWidget->setColumnHidden( ColumnFormula, true );// formula
	}

	// These columns duplicate what the details panel already shows below the table,
	// so keep the table itself compact by always hiding them.
	ui->tableWidget->setColumnHidden( ColumnNumber, true );// number
	ui->tableWidget->setColumnHidden( ColumnBaseValue, true );// base value
	ui->tableWidget->setColumnHidden( ColumnInSizes, true );// in sizes
	ui->tableWidget->setColumnHidden( ColumnInHeights, true );// in heights

	RetranslateTableHeaders();

	connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &TMainWindow::ShowMData);

	ShowUnits();

	ui->tableWidget->resizeColumnsToContents();
	// Cap the Formula column's width so a long, complex formula wraps onto
	// multiple lines (matching the multi-line editing already supported in
	// the dedicated Formula field below) instead of stretching the column
	// to fit one very long, hard-to-read line.
	if (ui->tableWidget->columnWidth(ColumnFormula) > maxFormulaColumnWidth)
	{
		ui->tableWidget->horizontalHeader()->resizeSection(ColumnFormula, maxFormulaColumnWidth);
	}
	ui->tableWidget->resizeRowsToContents();
	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowUnits()
{
	const QString unit = UnitsToStr(mUnit);

	ShowHeaderUnits(ui->tableWidget, ColumnCalcValue, UnitsToStr(pUnit));// calculated value
	ShowHeaderUnits(ui->tableWidget, ColumnFormula, unit);// formula
	ShowHeaderUnits(ui->tableWidget, ColumnBaseValue, unit);// base value
	ShowHeaderUnits(ui->tableWidget, ColumnInSizes, unit);// in sizes
	ShowHeaderUnits(ui->tableWidget, ColumnInHeights, unit);// in heights
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ShowHeaderUnits(QTableWidget *table, int column, const QString &unit)
{
	SCASSERT(table != nullptr)

	QString header = table->horizontalHeaderItem(column)->text();
	const int index = header.indexOf(QLatin1String("("));
	if (index != -1)
	{
		header.remove(index-1, 100);
	}
	const QString unitHeader = QString("%1 (%2)").arg(header, unit);
	table->horizontalHeaderItem(column)->setText(unitHeader);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MeasurementsWasSaved(bool saved)
{
	setWindowModified(!saved);
	!m_isReadOnly ? ui->actionSave->setEnabled(!saved): ui->actionSave->setEnabled(false);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetCurrentFile(const QString &fileName)
{
	curFile = fileName;
	if (curFile.isEmpty())
	{
		ui->lineEditPathToFile->setText(QLatin1String("<") + tr("Empty") + QLatin1String(">"));
		ui->lineEditPathToFile->setToolTip(tr("File was not saved yet."));
		ui->lineEditPathToFile->setCursorPosition(0);
		ui->pushButtonShowInExplorer->setEnabled(false);
	}
	else
	{
		ui->lineEditPathToFile->setText(QDir::toNativeSeparators(curFile));
		ui->lineEditPathToFile->setToolTip(QDir::toNativeSeparators(curFile));
		ui->lineEditPathToFile->setCursorPosition(0);
		ui->pushButtonShowInExplorer->setEnabled(true);
		auto settings = qApp->seamlyMeSettings();
		QStringList files = settings->GetRecentFileList();
		files.removeAll(fileName);
		files.prepend(fileName);
		while (files.size() > MaxRecentFiles)
		{
			files.removeLast();
		}
		settings->SetRecentFileList(files);
		UpdateRecentFileActions();
	}

	UpdateWindowTitle();
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::SaveMeasurements(const QString &fileName, QString &error)
{
	RegisterNewKnitMeasurements();

	const bool result = individualMeasurements->SaveDocument(fileName, error);
	if (result)
	{
		SetCurrentFile(fileName);
		MeasurementsWasSaved(result);
	}
	return result;
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief Adds every measurement in this file that isn't a built-in sewing
/// measurement and isn't already in Марта's knitting-measurement dictionary
/// to that dictionary, using whatever full name and description she's
/// typed for it so far. Called right before saving, so a plain name she
/// just created (no "@") is "known" the next time this or any other file
/// uses it -- she never has to think about the "@" prefix at all.
void TMainWindow::RegisterNewKnitMeasurements()
{
	if (individualMeasurements == nullptr || mType != MeasurementsType::Individual)
	{
		return;
	}

	const QStringList sewingNames = AllGroupNames();
	const QStringList allNames = individualMeasurements->ListAll();

	for (const QString &name : allNames)
	{
		if (name.indexOf(CustomMSign) == 0 || sewingNames.contains(name) || IsKnitMeasurement(name))
		{
			continue; // legacy "@" custom name, a sewing measurement, or already registered
		}

		try
		{
			const QSharedPointer<MeasurementVariable> meash = data->getVariable<MeasurementVariable>(name);
			RegisterKnitMeasurement(name, meash->getGuiText(), meash->GetDescription());
		}
		catch (const VExceptionBadId &exception)
		{
			Q_UNUSED(exception)
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::CanReplaceCurrentWindow() const
{
	// Safe to silently reuse this window for a different file -- no prompt, no risk of losing
	// anything -- only when it's still showing a brand new, completely untouched file: no name
	// on disk yet, nothing in the table. Deliberately mirrors MaybeSave()'s own "don't ask if
	// file was created without modifications" freebie rather than checking isWindowModified()
	// directly -- FileNew() marks a fresh file as modified immediately (it has never been saved
	// to disk), so that flag is already true right after pressing "New" even though there is
	// nothing here actually worth keeping. Anything else (a loaded or edited file, even one
	// that's already fully saved) keeps the existing, safer behavior of opening a separate
	// window instead, same as it always has.
	return individualMeasurements != nullptr && curFile.isEmpty() && ui->tableWidget->rowCount() == 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::MaybeSave()
{
	if (this->isWindowModified())
	{
		if (curFile.isEmpty() && ui->tableWidget->rowCount() == 0)
		{
			return true;// Don't ask if file was created without modifications.
		}

		QScopedPointer<QMessageBox> messageBox(new QMessageBox(tr("Unsaved changes"),
															   tr("Measurements have been modified.\n"
																  "Do you want to save your changes?"),
															   QMessageBox::Warning, QMessageBox::Yes, QMessageBox::No,
															   QMessageBox::Cancel, this, Qt::Sheet));

		messageBox->setDefaultButton(QMessageBox::Yes);
		messageBox->setEscapeButton(QMessageBox::Cancel);

		messageBox->setButtonText(QMessageBox::Yes, curFile.isEmpty() || m_isReadOnly ? tr("Save...") : tr("Save"));
		messageBox->setButtonText(QMessageBox::No, tr("Don't Save"));

		messageBox->setWindowModality(Qt::ApplicationModal);
        messageBox->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint
                                                 & ~Qt::WindowMaximizeButtonHint
                                                 & ~Qt::WindowMinimizeButtonHint);
		const QMessageBox::StandardButton ret = static_cast<QMessageBox::StandardButton>(messageBox->exec());

		switch (ret)
		{
			case QMessageBox::Yes:
				if (m_isReadOnly)
				{
					return FileSaveAs();
				}
				else
				{
					return FileSave();
				}
			case QMessageBox::No:
				return true;
			case QMessageBox::Cancel:
				return false;
			default:
				break;
		}
	}
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
QTableWidgetItem *TMainWindow::AddCell(const QString &text, int row, int column, int aligment, bool ok)
{
	QTableWidgetItem *item = new QTableWidgetItem(text);
	item->setTextAlignment(aligment);
	item->setToolTip(text);

	// set the item non-editable (view only), and non-selectable
	Qt::ItemFlags flags = item->flags();
	flags &= ~(Qt::ItemIsEditable); // reset/clear the flag
	item->setFlags(flags);

	if (!ok)
	{
		QBrush brush = item->foreground();
		brush.setColor(Qt::red);
		item->setForeground(brush);
	}

	ui->tableWidget->setItem(row, column, item);

	return item;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Paint every cell in a table row with the same background color, or, with an invalid
 * QColor, with the plain alternating-row shading a row would normally get on its own. Used for
 * the row-level highlighting described above AddCell() -- setForeground() there colors one
 * cell's text on a value error, this colors a whole row's background for the "has a formula" /
 * "check this" / "unsaved draft" states instead.
 *
 * tableWidget's own "alternatingRowColors" property is OFF (see the .ui file) and every row's
 * shading is applied by hand here instead, including the plain, unhighlighted rows -- on this
 * Mac's native style, the built-in alternating shading is painted directly by the view and
 * doesn't reliably respect a per-item Qt::BackgroundRole set below it: a "reminder" row that
 * landed on what would have been an alternate row kept its plain alternate-row gray instead of
 * turning orange (the warning icon still showed, since that's a separate, unaffected paint
 * step). Painting every row's background here, always, sidesteps that rather than depending on
 * two different systems agreeing on top of each other.
 */
void TMainWindow::SetRowHighlight(int row, const QColor &color)
{
	QBrush brush;
	if (color.isValid())
	{
		brush = QBrush(color);
	}
	else
	{
		// Same odd/even shading alternatingRowColors would have given this row, taken from the
		// palette (so it still follows the system's light/dark appearance) rather than a
		// hard-coded gray.
		const QPalette::ColorRole role = (row % 2 != 0) ? QPalette::AlternateBase : QPalette::Base;
		brush = QBrush(ui->tableWidget->palette().color(role));
	}

	for (int column = 0; column < ui->tableWidget->columnCount(); ++column)
	{
		if (QTableWidgetItem *cellItem = ui->tableWidget->item(row, column))
		{
			cellItem->setBackground(brush);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Update row-highlight bookkeeping after a measurement's formula was actually committed
 * to the model (individualMeasurements->SetMValue() succeeded) -- see CommitMValue() and Fx().
 * Anyone whose formula references this measurement's name may now compute to a different
 * result, so their rows get flagged (reminder/orange, via m_affectedMeasurements) until she
 * looks at them or resaves them -- see ShowNewMData() and the problem/reminder priority comment
 * above rowHighlightBlue. This measurement's own reminder flag and any leftover invalid-formula draft are
 * cleared, since what's now in the model is exactly what was just typed and it evaluated fine.
 */
void TMainWindow::MarkMeasurementSaved(const QString &name)
{
	const QStringList affected = m_usedByMeasurement.value(name);
	for (const QString &affectedName : affected)
	{
		m_affectedMeasurements.insert(affectedName);
	}

	m_affectedMeasurements.remove(name);
	m_formulaDrafts.remove(name);
}

//---------------------------------------------------------------------------------------------------------------------
QComboBox *TMainWindow::SetGradationList(QLabel *label, const QStringList &list)
{
	ui->toolBarGradation->addWidget(label);

	QComboBox *comboBox = new QComboBox;
	comboBox->addItems(list);
	ui->toolBarGradation->addWidget(comboBox);

	return comboBox;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetDefaultHeight(int value)
{
	const qint32 index = gradationHeights->findText(QString("%1").arg(value));
	if (index != -1)
	{
		gradationHeights->setCurrentIndex(index);
	}
	else
	{
		currentHeight = gradationHeights->currentText().toInt();
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetDefaultSize(int value)
{
	const qint32 index = gradationSizes->findText(QString("%1").arg(value));
	if (index != -1)
	{
		gradationSizes->setCurrentIndex(index);
	}
	else
	{
		currentSize = gradationSizes->currentText().toInt();
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::RefreshData(bool freshCall)
{
	VContainer::ClearUniqueNames();
	data->ClearVariables(VarType::Measurement);
	individualMeasurements->readMeasurements();

	RefreshTable(freshCall);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::RefreshTable(bool freshCall)
{
	ui->tableWidget->blockSignals(true);
	ui->tableWidget->clearContents();

	// The vertical header normally stays in ResizeToContents mode so a row's height always
	// matches its (word-wrapped) content. But that mode makes every single setItem() call below
	// trigger its own synchronous row-height recompute -- unnoticeable on a small file, but on
	// Марта's 150-row file (2026-09-12) that alone added several seconds to editing a single
	// formula, and made the window unresponsive to clicks for as long as it took. Switch to
	// Fixed for the duration of the populate loop and do one resizeRowsToContents() call after
	// it instead -- same final row heights, one pass instead of ~900 (150 rows x ~6 columns).
	ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	// Rebuilt fresh on every refresh below (Individual/single-size files only -- see the
	// Individual branch further down) so it always reflects the current formula text, including
	// any reference just added or removed. See MarkMeasurementSaved() for how it's used.
	m_usedByMeasurement.clear();

	ShowUnits();

	const QMap<QString, QSharedPointer<MeasurementVariable> > table = data->DataMeasurements();
	QMap<int, QSharedPointer<MeasurementVariable> > orderedTable;
	QMap<QString, QSharedPointer<MeasurementVariable> >::const_iterator iterMap;
	for (iterMap = table.constBegin(); iterMap != table.constEnd(); ++iterMap)
	{
		QSharedPointer<MeasurementVariable> meash = iterMap.value();
		orderedTable.insert(meash->Index(), meash);
	}

	qint32 currentRow = -1;
	QMap<int, QSharedPointer<MeasurementVariable> >::const_iterator iMap;
	ui->tableWidget->setRowCount ( orderedTable.size() );
	for (iMap = orderedTable.constBegin(); iMap != orderedTable.constEnd(); ++iMap)
	{
		QSharedPointer<MeasurementVariable> meash = iMap.value();
		currentRow++;

		if (mType == MeasurementsType::Individual)
		{
			QTableWidgetItem *item = AddCell(qApp->translateVariables()->MToUser(meash->GetName()), currentRow, ColumnName,
											 Qt::AlignVCenter); // name
			item->setData(Qt::UserRole, meash->GetName());

			if (meash->isCustom())
			{
                AddCell(QStringLiteral("na"), currentRow, ColumnNumber, Qt::AlignVCenter);
				AddCell(meash->getGuiText(), currentRow, ColumnFullName, Qt::AlignVCenter);
			}
			else
			{

				AddCell(getMeasurementNumber(meash->GetName()), currentRow, ColumnNumber, Qt::AlignVCenter);
                AddCell(qApp->translateVariables()->guiText(meash->GetName()), currentRow, ColumnFullName, Qt::AlignVCenter);
			}

			const qreal value = UnitConvertor(*meash->GetValue(), mUnit, pUnit);
			AddCell(locale().toString(value), currentRow, ColumnCalcValue, Qt::AlignHCenter | Qt::AlignVCenter,
					meash->IsFormulaOk()); // calculated value

			QString formula;
			try
			{
				formula = qApp->translateVariables()->FormulaToUser(meash->GetFormula(), qApp->Settings()->getOsSeparator());
			}
			catch (qmu::QmuParserError &error)
			{
				Q_UNUSED(error)
				formula = meash->GetFormula();
			}

			AddCell(formula, currentRow, ColumnFormula, Qt::AlignVCenter); // formula

			const QString description = meash->isCustom() ? meash->GetDescription()
												: qApp->translateVariables()->Description(meash->GetName());
			AddCell(description, currentRow, ColumnDescription, Qt::AlignVCenter); // description

			// Row highlighting: figure out which of blue/reminder/problem (if any) this row gets --
			// see the comment above rowHighlightBlue for what each means and the priority order.
			// A section-divider row (meash->IsSection()) is always blue and skips everything
			// else below -- it has no real formula, so it's never a dependency of anything and
			// never carries a draft. The internal (untranslated) formula is used for dependency
			// extraction so the names line up with meash->GetName()/Qt::UserRole, which is what
			// m_usedByMeasurement/m_affectedMeasurements are keyed on.
			if (meash->IsSection())
			{
				SetRowHighlight(currentRow, rowHighlightBlue);
			}
			else
			{
				const QString internalFormula = meash->GetFormula();

				// A formula that refers to a section-divider measurement (checkBoxIsSection) is
				// always wrong -- a divider has no real value, it's purely organizational -- even
				// though the Calculator itself has no trouble evaluating it (see
				// FormulaReferencesSection()). Detected in the same usedNames pass used just
				// below to build m_usedByMeasurement, so the formula is only parsed once.
				bool referencesSection = false;
				QString sectionRefName;

				// Only re-parse a formula that the data layer itself already parsed
				// successfully (meash->IsFormulaOk(), set by readMeasurements()/EvalFormula() in
				// measurements.cpp). A formula that's currently incomplete or malformed on disk
				// (e.g. something left mid-edit) already gets its own red error indicator further
				// down and doesn't need to show up in anyone else's dependency list -- and asking
				// the parser to tokenize a broken formula a second time here, for every table
				// refresh, is worth avoiding rather than relying on this being harmless.
				if (!internalFormula.isEmpty() && meash->IsFormulaOk())
				{
					try
					{
						QScopedPointer<Calculator> depCal(new Calculator());
						const QStringList usedNames = depCal->GetUsedVariables(internalFormula);
						for (const QString &usedName : usedNames)
						{
							m_usedByMeasurement[usedName].append(meash->GetName());

							if (!referencesSection)
							{
								const QSharedPointer<MeasurementVariable> used = table.value(usedName);
								if (!used.isNull() && used->IsSection())
								{
									referencesSection = true;
									sectionRefName = qApp->translateVariables()->MToUser(usedName);
								}
							}
						}
					}
					catch (qmu::QmuParserError &error)
					{
						// Can't even tokenize this formula right now -- nothing to record. The
						// existing red error highlight on the calculated-value cell already
						// flags that something is wrong with this row.
						Q_UNUSED(error)
					}
					catch (...)
					{
						// Belt and suspenders: this is a table-refresh convenience scan, not
						// essential to showing the row itself, so whatever this formula did to
						// get here, it must not be allowed to take the whole table refresh (and
						// the app) down with it.
					}
				}

				QColor rowColor;
				if (m_formulaDrafts.contains(meash->GetName()) || referencesSection)
				{
					rowColor = rowHighlightProblem;
				}
				else if (m_affectedMeasurements.contains(meash->GetName()))
				{
					rowColor = rowHighlightReminder;
				}
				SetRowHighlight(currentRow, rowColor);

				if (referencesSection)
				{
					const QString message = tr("This formula uses \"%1\", which is a section "
												"divider and can't be used in calculations.")
												.arg(sectionRefName);
					if (QTableWidgetItem *calcItem = ui->tableWidget->item(currentRow, ColumnCalcValue))
					{
						QBrush brush = calcItem->foreground();
						brush.setColor(Qt::red);
						calcItem->setForeground(brush);
						calcItem->setToolTip(message);
					}
					if (QTableWidgetItem *formulaItem = ui->tableWidget->item(currentRow, ColumnFormula))
					{
						formulaItem->setToolTip(message);
					}
				}
				else if (rowColor == rowHighlightReminder)
				{
					// Orange here isn't an error -- the value is still correct -- it's a "you
					// may want to take another look" reminder because a measurement this formula
					// uses was just changed elsewhere. Say so explicitly (tooltip) and mark it
					// visually (a small warning icon in front of the formula text, see
					// ReminderRowIcon()) so a colored row doesn't read as broken. Clicking into
					// the row clears it (see ShowNewMData()).
					const QString message = tr("A measurement this formula uses was just changed. "
												"Open this row to confirm the result is still what "
												"you expect -- doing so clears this reminder.");
					if (QTableWidgetItem *nameItem = ui->tableWidget->item(currentRow, ColumnName))
					{
						nameItem->setToolTip(message);
					}
					if (QTableWidgetItem *formulaItem = ui->tableWidget->item(currentRow, ColumnFormula))
					{
						formulaItem->setToolTip(message);
						formulaItem->setIcon(ReminderRowIcon());
					}
				}
			}
		}
		else
		{
			QTableWidgetItem *item = AddCell(qApp->translateVariables()->MToUser(meash->GetName()), currentRow, 0,
											 Qt::AlignVCenter); // name
			item->setData(Qt::UserRole, meash->GetName());

			if (meash->isCustom())
			{
				AddCell(QStringLiteral("na"), currentRow, ColumnNumber, Qt::AlignVCenter);
                AddCell(meash->getGuiText(), currentRow, ColumnFullName, Qt::AlignVCenter);
			}
			else
			{
				AddCell(getMeasurementNumber(meash->GetName()), currentRow, ColumnNumber, Qt::AlignVCenter);
                AddCell(qApp->translateVariables()->guiText(meash->GetName()), currentRow, ColumnFullName, Qt::AlignVCenter);
			}

			const qreal value = UnitConvertor(*data->DataVariables()->value(meash->GetName())->GetValue(), mUnit,
											  pUnit);
			AddCell(locale().toString(value), currentRow, ColumnCalcValue,
					Qt::AlignHCenter | Qt::AlignVCenter, meash->IsFormulaOk()); // calculated value

			AddCell(locale().toString(meash->GetBase()), currentRow, ColumnBaseValue,
					Qt::AlignHCenter | Qt::AlignVCenter); // base value

			AddCell(locale().toString(meash->GetKsize()), currentRow, ColumnInSizes,
					Qt::AlignHCenter | Qt::AlignVCenter); // in sizes

			AddCell(locale().toString(meash->GetKheight()), currentRow, ColumnInHeights,
					Qt::AlignHCenter | Qt::AlignVCenter); // in heights

			const QString description = meash->isCustom() ? meash->GetDescription()
												: qApp->translateVariables()->Description(meash->GetName());
			AddCell(description, currentRow, ColumnDescription, Qt::AlignVCenter); // description

			// Multisize rows don't get the reminder/problem/section highlighting above (that's
			// Individual-file-only, see the comment near rowHighlightBlue), but they still need
			// SOME background -- alternatingRowColors is off for this whole table now (see
			// SetRowHighlight()), so without this every row here would come out plain instead of
			// alternating.
			SetRowHighlight(currentRow, QColor());
		}
	}

	if (freshCall)
	{
		ui->tableWidget->resizeColumnsToContents();
		// Cap the Formula column's width so a long, complex formula wraps onto
		// multiple lines (matching the multi-line editing already supported in
		// the dedicated Formula field below) instead of stretching the column
		// to fit one very long, hard-to-read line.
		if (ui->tableWidget->columnWidth(ColumnFormula) > maxFormulaColumnWidth)
		{
			ui->tableWidget->horizontalHeader()->resizeSection(ColumnFormula, maxFormulaColumnWidth);
		}
	}

	// Restore the normal auto-sizing mode now that the table is fully populated, and size every
	// row in one batched pass -- see the comment above the Fixed switch at the top of this
	// function for why this is done once here instead of implicitly on every setItem() above.
	ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->tableWidget->resizeRowsToContents();

	ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
	ui->tableWidget->blockSignals(false);

	if (ui->tableWidget->rowCount() > 0)
	{
		ui->actionExportToCSV->setEnabled(true);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief Generate a default name ("M_1", "M_2", ...; "М_1", "М_2", ... on a Russian-locale
 * build) for a brand new custom measurement. Deliberately kept locale-translated -- an earlier
 * version of this fix forced it to the untranslated Latin prefix instead, to close a real trap
 * (a Cyrillic "М_" auto-name looks identical to a Latin "M_" typed by hand, but is a different
 * character to the parser, producing a silent "Neozhidanny token" error) -- but that traded away
 * matching the rest of a Russian-locale interface, which she'd rather keep. So this stays
 * translated; the trap it reopens just means a reference to an auto-named measurement has to be
 * typed/pasted in the same script it was created in (Cyrillic М for a Cyrillic auto-name) --
 * using the Измерение/Функция picker to insert the reference, rather than typing it by hand,
 * sidesteps that entirely.
 */
QString TMainWindow::GetCustomName() const
{
	qint32 num = 1;
	QString name;
	do
	{
		name = qApp->translateVariables()->InternalVarToUser(measurement_) + QString().number(num);
		num++;
	} while (data->IsUnique(name) == false);

	return name;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::Controls()
{
	if (ui->tableWidget->rowCount() > 0)
	{
		ui->toolButtonRemove->setEnabled(true);
	}
	else
	{
		ui->toolButtonRemove->setEnabled(false);
	}

	if (ui->tableWidget->rowCount() >= 2)
	{
		if (ui->tableWidget->currentRow() == 0)
		{
			ui->toolButtonTop->setEnabled(false);
			ui->toolButtonUp->setEnabled(false);
			ui->toolButtonDown->setEnabled(true);
			ui->toolButtonBottom->setEnabled(true);
		}
		else if (ui->tableWidget->currentRow() == ui->tableWidget->rowCount()-1)
		{
			ui->toolButtonTop->setEnabled(true);
			ui->toolButtonUp->setEnabled(true);
			ui->toolButtonDown->setEnabled(false);
			ui->toolButtonBottom->setEnabled(false);
		}
		else
		{
			ui->toolButtonTop->setEnabled(true);
			ui->toolButtonUp->setEnabled(true);
			ui->toolButtonDown->setEnabled(true);
			ui->toolButtonBottom->setEnabled(true);
		}
	}
	else
	{
		ui->toolButtonTop->setEnabled(false);
		ui->toolButtonUp->setEnabled(false);
		ui->toolButtonDown->setEnabled(false);
		ui->toolButtonBottom->setEnabled(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MFields(bool enabled)
{
	ui->lineEditName->setEnabled(enabled);
	ui->plainTextEditDescription->setEnabled(enabled);
	ui->lineEditFullName->setEnabled(enabled);

	if (mType == MeasurementsType::Multisize)
	{
		ui->doubleSpinBoxBaseValue->setEnabled(enabled);
		ui->doubleSpinBoxInSizes->setEnabled(enabled);
		ui->doubleSpinBoxInHeights->setEnabled(enabled);
	}
	else
	{
		ui->plainTextEditFormula->setEnabled(enabled);
		ui->toolButtonExpr->setEnabled(enabled);
		ui->checkBoxIsSection->setEnabled(enabled);
	}

	ui->find_LineEdit->setEnabled(enabled);
	if (enabled && !ui->find_LineEdit->text().isEmpty())
	{
		ui->toolButtonFindPrevious->setEnabled(enabled);
		ui->toolButtonFindNext->setEnabled(enabled);
	}
	else
	{
		ui->toolButtonFindPrevious->setEnabled(false);
		ui->toolButtonFindNext->setEnabled(false);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::UpdateWindowTitle()
{
	QString fileName;
	bool isFileWritable = true;
	if (!curFile.isEmpty())
	{
#ifdef Q_OS_WIN32
		qt_ntfs_permission_lookup++; // turn checking on
#endif /*Q_OS_WIN32*/
		isFileWritable = QFileInfo(curFile).isWritable();
#ifdef Q_OS_WIN32
		qt_ntfs_permission_lookup--; // turn it off again
#endif /*Q_OS_WIN32*/
		fileName = curFile;
	}
	else
	{
		fileName = tr("untitled %1").arg(qApp->mainWindows().size() + 1);
		mType == MeasurementsType::Multisize ? fileName += QLatin1String(".") + smmsExt :
                                               fileName += QLatin1String(".") + smisExt;
	}

	fileName += QLatin1String("[*]");

	if (m_isReadOnly || !isFileWritable)
	{
		fileName += QLatin1String(" (") + tr("read only") + QLatin1String(")");
	}

	setWindowTitle( VER_INTERNALNAME_ME_STR + QString(" - ") + fileName);
	setWindowFilePath(curFile);

#if defined(Q_OS_MAC)
	static QIcon fileIcon = QIcon(QCoreApplication::applicationDirPath() +
								  QLatin1String("/../Resources/measurements.icns"));
	QIcon icon;
	if (!curFile.isEmpty())
	{
		if (!isWindowModified())
		{
			icon = fileIcon;
		}
		else
		{
			static QIcon darkIcon;

			if (darkIcon.isNull())
			{
				darkIcon = QIcon(darkenPixmap(fileIcon.pixmap(16, 16)));
			}
			icon = darkIcon;
		}
	}
	setWindowIcon(icon);
#endif //defined(Q_OS_MAC)
}

//---------------------------------------------------------------------------------------------------------------------
QString TMainWindow::ClearCustomName(const QString &name) const
{
	QString clear = name;
	const int index = clear.indexOf(CustomMSign);
	if (index == 0)
	{
		clear.remove(0, 1);
	}
	return clear;
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::EvalFormula(const QString &formula, bool fromUser, VContainer *data, QLabel *label)
{
	const QString postfix = UnitsToStr(pUnit);//Show unit in dialog label (cm, mm or inch)
	if (formula.isEmpty())
	{
		label->setText(tr("Error") + " (" + postfix + "). " + tr("Empty field."));
		label->setToolTip(tr("Empty field"));
		return false;
	}
	else
	{
		try
		{
			// Replace line return character with spaces for calc if exist
			QString f;
			if (fromUser)
			{
				f = qApp->translateVariables()->FormulaFromUser(formula, qApp->Settings()->getOsSeparator());
			}
			else
			{
				f = formula;
			}
			f.replace("\n", " ");
			QScopedPointer<Calculator> cal(new Calculator());
			qreal result = cal->EvalFormula(data->DataVariables(), f);

			if (qIsInf(result) || qIsNaN(result))
			{
				label->setText(tr("Error") + " (" + postfix + ").");
				label->setToolTip(tr("Invalid result. Value is infinite or NaN. Please, check your calculations."));
				return false;
			}

			result = UnitConvertor(result, mUnit, pUnit);

			label->setText(qApp->LocaleToString(result) + " " +postfix);
			label->setToolTip(tr("Value"));
			return true;
		}
		catch (qmu::QmuParserError &error)
		{
			label->setText(tr("Error") + " (" + postfix + "). " + tr("Parser error: %1").arg(error.GetMsg()));
			label->setToolTip(tr("Parser error: %1").arg(error.GetMsg()));
			return false;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::FormulaReferencesSection(const QString &internalFormula, QString *sectionName) const
{
	if (internalFormula.isEmpty())
	{
		return false;
	}

	try
	{
		QScopedPointer<Calculator> depCal(new Calculator());
		const QStringList usedNames = depCal->GetUsedVariables(internalFormula);
		const QMap<QString, QSharedPointer<MeasurementVariable> > table = data->DataMeasurements();
		for (const QString &usedName : usedNames)
		{
			const QSharedPointer<MeasurementVariable> used = table.value(usedName);
			if (!used.isNull() && used->IsSection())
			{
				if (sectionName != nullptr)
				{
					*sectionName = qApp->translateVariables()->MToUser(usedName);
				}
				return true;
			}
		}
	}
	catch (qmu::QmuParserError &error)
	{
		// Can't even tokenize this formula right now -- not our concern here, the existing
		// red error highlight on the calculated-value cell already flags it.
		Q_UNUSED(error)
	}
	catch (...)
	{
		// Belt and suspenders, same reasoning as the identical catch in RefreshTable(): this is
		// a supplementary check, not essential to showing or committing the row, so it must not
		// bring the app down no matter what this formula does to the parser.
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::Open(const QString &dir, const QString &filter)
{
    const QString filename = fileDialog(this, tr("Open file"), dir, filter, nullptr,
                                        qApp->seamlyMeSettings()->getUseNativeFileDialogs(),
                                        QFileDialog::ExistingFile, QFileDialog::AcceptOpen);

	if (!filename.isEmpty())
	{
		// LoadFile() already decides for itself whether it's safe to reuse this window or whether
		// it needs to open a separate one (see CanReplaceCurrentWindow()) -- doing that same check
		// again here, ahead of it, used to short-circuit straight to a brand new window even when
		// this one was empty and safe to reuse.
		LoadFile(filename);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::UpdatePadlock(bool ro)
{
	ui->actionReadOnly->setChecked(ro);
	if (ro)
	{
		ui->actionReadOnly->setIcon(QIcon("://seamlymeicon/24x24/padlock_locked.png"));
	}
	else
	{
		ui->actionReadOnly->setIcon(QIcon("://seamlymeicon/24x24/padlock_opened.png"));
	}

	ui->actionReadOnly->setDisabled(m_isReadOnly);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::MeasurementGUI()
{
	if (const QTableWidgetItem *nameField = ui->tableWidget->item(ui->tableWidget->currentRow(), ColumnName))
	{
		// Read-only unless we can confirm this measurement is the user's own
		// to edit (MeasurementVariable::isCustom() -- no longer just an "@"
		// check, see measurement_variable.cpp).
		bool isKnown = true;
		try
		{
			const QSharedPointer<MeasurementVariable> meash =
				data->getVariable<MeasurementVariable>(nameField->data(Qt::UserRole).toString());
			isKnown = !meash->isCustom();
		}
		catch (const VExceptionBadId &exception)
		{
			Q_UNUSED(exception)
		}

		ui->lineEditName->setReadOnly(isKnown);
		ui->plainTextEditDescription->setReadOnly(isKnown);
		ui->lineEditFullName->setReadOnly(isKnown);

		// Need to block signals for QLineEdit in readonly mode because it still emits
		// QLineEdit::editingFinished signal.
		ui->lineEditName->blockSignals(isKnown);
		ui->lineEditFullName->blockSignals(isKnown);

		Controls(); // Buttons remove, up, down
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::ReadSettings()
{
	const VSeamlyMeSettings *settings = qApp->seamlyMeSettings();
	restoreGeometry(settings->GetGeometry());
	restoreState(settings->GetWindowState());
	restoreState(settings->GetToolbarsState(), APP_VERSION);

	// Text under tool button icon
	initToolBarStyles();

	// Stack limit
	//qApp->getUndoStack()->setUndoLimit(settings->GetUndoCount());
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::WriteSettings()
{
	VSeamlyMeSettings *settings = qApp->seamlyMeSettings();
	settings->SetGeometry(saveGeometry());
	settings->SetWindowState(saveState());
	settings->SetToolbarsState(saveState(APP_VERSION));
}

//---------------------------------------------------------------------------------------------------------------------
QStringList TMainWindow::FilterMeasurements(const QStringList &mNew, const QStringList &mFilter)
{
    return convertToList(convertToSet<QString>(mNew).subtract(convertToSet<QString>(mFilter)));
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::updatePatternUnit()
{
	const int row = ui->tableWidget->currentRow();

	if (row == -1)
	{
		return;
	}

	RefreshTable();

	m_search->refreshList(ui->find_LineEdit->text());

	ui->tableWidget->selectRow(row);
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::LoadFromExistingFile(const QString &path)
{
	if (individualMeasurements == nullptr)
	{
		if (!QFileInfo(path).exists())
		{
			qCCritical(tMainWindow, "%s", qUtf8Printable(tr("File '%1' doesn't exist!").arg(path)));
			if (qApp->isTestMode())
			{
				qApp->exit(V_EX_NOINPUT);
			}
			return false;
		}

		// Check if file already opened
		QList<TMainWindow*>list = qApp->mainWindows();
		for (int i = 0; i < list.size(); ++i)
		{
			if (list.at(i)->CurrentFile() == path)
			{
				list.at(i)->activateWindow();
				close();
				return false;
			}
		}

		VlpCreateLock(lock, path);

		if (!lock->IsLocked())
		{
			if (!IgnoreLocking(lock->GetLockError(), path))
			{
				return false;
			}
		}

		try
		{
			data = new VContainer(qApp->translateVariables(), &mUnit);

			individualMeasurements = new MeasurementDoc(data);
			individualMeasurements->setSize(&currentSize);
			individualMeasurements->setHeight(&currentHeight);
			individualMeasurements->setXMLContent(path);

			mType = individualMeasurements->Type();

			if (mType == MeasurementsType::Unknown)
			{
				VException e(tr("File has unknown format."));
				throw e;
			}

			if (mType == MeasurementsType::Multisize)
			{
				VException e(tr("Export from multisize measurements is not supported."));
				throw e;
			}
			else
			{
				IndividualSizeConverter converter(path);
				m_curFileFormatVersion = converter.getCurrentFormatVersion();
				m_curFileFormatVersionStr = converter.getVersionStr();
				individualMeasurements->setXMLContent(converter.Convert());// Read again after conversion
			}

			if (!individualMeasurements->eachKnownNameIsValid())
			{
				VException e(tr("File contains invalid known measurement(s)."));
				throw e;
			}

			mUnit = individualMeasurements->measurementUnits();
			pUnit = mUnit;

			currentHeight = individualMeasurements->BaseHeight();
			currentSize = individualMeasurements->BaseSize();

			ui->labelToolTip->setVisible(false);
			ui->tabWidget->setVisible(true);

			InitWindow();

			individualMeasurements->ClearForExport();
			const bool freshCall = true;
			RefreshData(freshCall);

			if (ui->tableWidget->rowCount() > 0)
			{
				ui->tableWidget->selectRow(0);
			}

			lock.reset();// Now we can unlock the file

			m_isReadOnly = individualMeasurements->isReadOnly();
			UpdatePadlock(m_isReadOnly);
			MeasurementGUI();
		}

		catch (VException &exception)
		{
			qCCritical(tMainWindow, "%s\n\n%s\n\n%s", qUtf8Printable(tr("File error.")),
					   qUtf8Printable(exception.ErrorMessage()), qUtf8Printable(exception.DetailedInformation()));

			ui->labelToolTip->setVisible(true);
			ui->tabWidget->setVisible(false);
			delete individualMeasurements;
			individualMeasurements = nullptr;
			delete data;
			data = nullptr;
			lock.reset();

			if (qApp->isTestMode())
			{
				qApp->exit(V_EX_NOINPUT);
			}
			return false;
		}
	}
	else
	{
		qApp->newMainWindow();
		return qApp->mainWindow()->LoadFile(path);
	}

	return true;
}
//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::UpdateRecentFileActions()
{
	qCDebug(tMainWindow, "Updating recent file actions.");
	const QStringList files = qApp->seamlyMeSettings()->GetRecentFileList();
	const int numRecentFiles = qMin(files.size(), static_cast<int>(MaxRecentFiles));
	qCDebug(tMainWindow, "Updating recent file actions = %i ",numRecentFiles);

	for (int i = 0; i < numRecentFiles; ++i)
	{
		const QString text = QString("&%1. %2").arg(i + 1).arg(strippedName(files.at(i)));
		qCDebug(tMainWindow, "file %i = %s", numRecentFiles, qUtf8Printable(text));
		recentFileActs[i]->setText(text);
		recentFileActs[i]->setData(files.at(i));
		recentFileActs[i]->setVisible(true);
	}

	for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
	{
		recentFileActs[j]->setVisible(false);
	}

	separatorAct->setVisible(numRecentFiles>0);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::CreateWindowMenu(QMenu *menu)
{
	SCASSERT(menu != nullptr)

	QAction *action = menu->addAction(tr("&New Window"));
	connect(action, &QAction::triggered, this, []()
	{
		qApp->newMainWindow();
		qApp->mainWindow()->activateWindow();
	});
	action->setMenuRole(QAction::NoRole);
	menu->addSeparator();

	const QList<TMainWindow*> windows = qApp->mainWindows();
	for (int i = 0; i < windows.count(); ++i)
	{
		TMainWindow *window = windows.at(i);

		QString title = QString("%1. %2").arg(i+1).arg(window->windowTitle());
		const int index = title.lastIndexOf("[*]");
		if (index != -1)
		{
			window->isWindowModified() ? title.replace(index, 3, "*") : title.replace(index, 3, "");
		}

		QAction *action = menu->addAction(title, this, SLOT(ShowWindow()));
		action->setData(i);
		action->setCheckable(true);
		action->setMenuRole(QAction::NoRole);
		if (window->isActiveWindow())
		{
			action->setChecked(true);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool TMainWindow::IgnoreLocking(int error, const QString &path)
{
	QMessageBox::StandardButton answer = QMessageBox::Abort;
	if (!qApp->isTestMode())
	{
		switch(error)
		{
			case QLockFile::LockFailedError:
				answer = QMessageBox::warning(this, tr("Locking file"),
											  tr("This file already opened in another window. Ignore if you want "
												 "to continue (not recommended, can cause a data corruption)."),
											  QMessageBox::Abort|QMessageBox::Ignore, QMessageBox::Abort);
				break;
			case QLockFile::PermissionError:
				answer = QMessageBox::question(this, tr("Locking file"),
											   tr("The lock file could not be created, for lack of permissions. "
												  "Ignore if you want to continue (not recommended, can cause "
												  "a data corruption)."),
											   QMessageBox::Abort|QMessageBox::Ignore, QMessageBox::Abort);
				break;
			case QLockFile::UnknownError:
				answer = QMessageBox::question(this, tr("Locking file"),
											   tr("Unknown error happened, for instance a full partition "
												  "prevented writing out the lock file. Ignore if you want to "
												  "continue (not recommended, can cause a data corruption)."),
											   QMessageBox::Abort|QMessageBox::Ignore, QMessageBox::Abort);
				break;
			default:
				answer = QMessageBox::Abort;
				break;
		}
	}

	if (answer == QMessageBox::Abort)
	{
		qCWarning(tMainWindow, "Failed to lock %s", qUtf8Printable(path));
		qCWarning(tMainWindow, "Error type: %d", error);
		if (qApp->isTestMode())
		{
			switch(error)
			{
				case QLockFile::LockFailedError:
					qCCritical(tMainWindow, "%s",
							   qUtf8Printable(tr("This file already opened in another window.")));
					break;
				case QLockFile::PermissionError:
					qCCritical(tMainWindow, "%s",
							   qUtf8Printable(tr("The lock file could not be created, for lack of permissions.")));
					break;
				case QLockFile::UnknownError:
					qCCritical(tMainWindow, "%s",
							   qUtf8Printable(tr("Unknown error happened, for instance a full partition "
												 "prevented writing out the lock file.")));
					break;
				default:
					break;
			}

			qApp->exit(V_EX_NOINPUT);
		}
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::SetDecimals()
{
	switch (mUnit)
	{
		case Unit::Cm:
			ui->doubleSpinBoxBaseValue->setDecimals(2);
			ui->doubleSpinBoxBaseValue->setSingleStep(0.01);

			ui->doubleSpinBoxInSizes->setDecimals(2);
			ui->doubleSpinBoxInSizes->setSingleStep(0.01);

			ui->doubleSpinBoxInHeights->setDecimals(2);
			ui->doubleSpinBoxInHeights->setSingleStep(0.01);
			break;
		case Unit::Mm:
			ui->doubleSpinBoxBaseValue->setDecimals(1);
			ui->doubleSpinBoxBaseValue->setSingleStep(0.1);

			ui->doubleSpinBoxInSizes->setDecimals(1);
			ui->doubleSpinBoxInSizes->setSingleStep(0.1);

			ui->doubleSpinBoxInHeights->setDecimals(1);
			ui->doubleSpinBoxInHeights->setSingleStep(0.1);
			break;
		case Unit::Inch:
			ui->doubleSpinBoxBaseValue->setDecimals(5);
			ui->doubleSpinBoxBaseValue->setSingleStep(0.00001);

			ui->doubleSpinBoxInSizes->setDecimals(5);
			ui->doubleSpinBoxInSizes->setSingleStep(0.00001);

			ui->doubleSpinBoxInHeights->setDecimals(5);
			ui->doubleSpinBoxInHeights->setSingleStep(0.00001);
			break;
		default:
			break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::initUnits()
{
	labelPatternUnit = new QLabel(tr("Pattern unit:"));
	ui->toolBarGradation->addWidget(labelPatternUnit);

	comboBoxUnits = new QComboBox(this);
	InitComboBoxUnits();
    setCurrentPatternUnits();

	// set default unit
	const qint32 indexUnit = comboBoxUnits->findData(static_cast<int>(pUnit));
	if (indexUnit != -1)
	{
		comboBoxUnits->setCurrentIndex(indexUnit);
	}

	connect(comboBoxUnits, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
			&TMainWindow::patternUnitsChanged);

	ui->toolBarGradation->addWidget(comboBoxUnits);
}

void TMainWindow::setCurrentPatternUnits()
{
    if (comboBoxUnits)
    {
        comboBoxUnits->blockSignals(true);
        const qint32 indexUnit = comboBoxUnits->findData(static_cast<int>(pUnit));
        if (indexUnit != -1)
        {
            comboBoxUnits->setCurrentIndex(indexUnit);
        }
        comboBoxUnits->blockSignals(false);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::InitComboBoxUnits()
{
	SCASSERT(comboBoxUnits != nullptr)
	comboBoxUnits->addItem(UnitsToStr(Unit::Cm, true), QVariant(static_cast<int>(Unit::Cm)));
	comboBoxUnits->addItem(UnitsToStr(Unit::Mm, true), QVariant(static_cast<int>(Unit::Mm)));
	comboBoxUnits->addItem(UnitsToStr(Unit::Inch, true), QVariant(static_cast<int>(Unit::Inch)));
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::InitGender(QComboBox *gender)
{
	SCASSERT(gender != nullptr)
	gender->addItem(tr("unknown", "gender"), QVariant(static_cast<int>(GenderType::Unknown)));
	gender->addItem(tr("male", "gender"), QVariant(static_cast<int>(GenderType::Male)));
	gender->addItem(tr("female", "gender"), QVariant(static_cast<int>(GenderType::Female)));
}

//---------------------------------------------------------------------------------------------------------------------
template <class T>
void TMainWindow::HackWidget(T **widget)
{
	delete *widget;
	*widget = new T();
	hackedWidgets.append(*widget);
}

//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::zoomToSelected()
{
    // do nothing
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief copyToClipboard copy dialog selection to clipboard as comma separated values.
 */
void TMainWindow::copyToClipboard()
{
    QItemSelectionModel *model = ui->tableWidget->selectionModel();
    QModelIndexList selectedIndexes = model->selectedIndexes();

    QString clipboardString;

    for (int i = 0; i < selectedIndexes.count(); ++i)
    {
        QModelIndex current = selectedIndexes[i];
        QString displayText = current.data(Qt::DisplayRole).toString();

        // Check if another column exists beyond this one.
        if (i + 1 < selectedIndexes.count())
        {
            QModelIndex next = selectedIndexes[i+1];

            // If the column is on different row, the clipboard should take note.
            if (next.row() != current.row())
            {
                displayText.append("\n");
            }
            else
            {
                // Otherwise append a comma separator.
                displayText.append(" , ");
            }
        }
        clipboardString.append(displayText);
    }

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(clipboardString);
}

//---------------------------------------------------------------------------------------------------------------------
/// @brief renderScaledDiagram Generates a pixel-perfect, crisp vector diagram to fit the current dock size.
///
/// Loads the active SVG file path from memory and dynamically reads the real-time dimensions of the
/// main window's dock container. It compares horizontal and vertical scaling factors to determine the
/// maximum aspect-ratio fit, preventing layout bleeding. The mathematical vector paths are then rendered
/// onto a sharp, transparent canvas layout, updating the display widget alongside its text caption label.
//---------------------------------------------------------------------------------------------------------------------
void TMainWindow::renderScaledDiagram()
{
    if (m_currentSvgPath.isEmpty()) return;

    QSvgRenderer renderer(m_currentSvgPath);
    QSize nativeSize = renderer.defaultSize();

    if (nativeSize.isValid() && nativeSize.width() > 0 && nativeSize.height() > 0)
    {
        // 1. Read live dimensions from the dock container layout
        double dockW = ui->dockWidgetDiagram->width();
        double dockH = ui->dockWidgetDiagram->height();

        // 2. Enforce fallback defaults BEFORE doing math if hidden or unpainted (<= 0)
        double maxW = (dockW > 10.0)  ? (dockW - 10.0)  : 290.0;
        double maxH = (dockH > 120.0) ? (dockH - 120.0) : 500.0;

        double ratioW = maxW / nativeSize.width();
        double ratioH = maxH / nativeSize.height();
        double scaleFactor = (ratioW < ratioH) ? ratioW : ratioH;

        int targetWidth = static_cast<int>(nativeSize.width() * scaleFactor);
        int targetHeight = static_cast<int>(nativeSize.height() * scaleFactor);

        // 3. Safety Check: Guarantee pixmap has valid dimensions to prevent painter warnings
        if (targetWidth <= 0 || targetHeight <= 0)
        {
            targetWidth = 290;
            targetHeight = 500;
        }

        QPixmap pixmap(targetWidth, targetHeight);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        if (painter.isActive()) // Extra safety check for active painting context
        {
            renderer.render(&painter);
            painter.end();
        }

        ui->diagram_Label->setPixmap(pixmap);
        ui->diagram_Label->setText(QString(""));
    }

    ui->caption_Label->setText(QString("<b>%1</b>. <i>%2</i>").arg(m_currentNumber, m_currentName));
    ui->description_Label->setText(m_currentDescription);
}
