/* OSCAR MainWindow Implementation
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include "test_macros.h"

#include <QHostInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QResource>
#include <QProgressBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QPixmap>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  #include <QDesktopWidget>
#endif
#include <QListView>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QProcess>
#include <QFontMetrics>
#include <QTextDocument>
#include <QTranslator>
#include <QPushButton>
#include <QCalendarWidget>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QScreen>
#include <QStorageInfo>
#include <cmath>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
  #include <QRandomGenerator>
#endif

#ifdef Q_OS_WIN
  #include "windows.h"
#endif
#include "common_gui.h"
#include "version.h"
#include "SleepLib/appsettings.h"       // defines for REMSTAR_M_SUPPORT
#include "SleepLib/journal.h"
#include "SleepLib/common.h"
#include "notifyMessageBox.h"
#include "staticQMessageBox.h"


// Custom loaders that don't autoscan..
#include <SleepLib/loader_plugins/zeo_loader.h>
#include <SleepLib/loader_plugins/dreem_loader.h>
#include <SleepLib/loader_plugins/somnopose_loader.h>
#include <SleepLib/loader_plugins/viatom_loader.h>

#ifdef REMSTAR_M_SUPPORT
#include <SleepLib/loader_plugins/mseries_loader.h>
#endif

#include "logger.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "aboutdialog.h"
#include "newprofile.h"
#include "exportcsv.h"
#include "SleepLib/schema.h"
#include "Graphs/glcommon.h"
#include "checkupdates.h"
#include "SleepLib/calcs.h"
#include "SleepLib/progressdialog.h"
#include "SleepLib/importcontext.h"

#include "reports.h"
#include "statistics.h"
#include "zip.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
#include <QOpenGLFunctions>
#endif

CheckUpdates *updateChecker;
bool openOk;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    if (logger) {
        connect(logger, SIGNAL(outputLog(QString)), this, SLOT(logMessage(QString)));
        logger->connectionReady();
    }

    // Bring window to top (useful when language is changed)
    this->activateWindow();
    this->raise();

    // Initialise oscar app registry stuff
    QSettings settings;

    // Load previous Window geometry
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());


    // Nifty Notification popups in System Tray (uses Growl on Mac)
    if (QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages()) {
        qDebug() << "Using System Tray for Menu";
        systray = new QSystemTrayIcon(QIcon(":/icons/logo-sm.png"), this);
        systray->show();
        // seems to need the systray menu for notifications to work
        systraymenu = new QMenu(this);
        systray->setContextMenu(systraymenu);
        QAction *a = systraymenu->addAction(STR_TR_OSCAR + " " + getVersion().displayString());
        a->setEnabled(false);
        systraymenu->addSeparator();
        systraymenu->addAction(tr("About"), this, SLOT(on_action_About_triggered()));
//        systraymenu->addAction(tr("Check for &Updates"), this, SLOT(on_actionCheck_for_Updates_triggered()));
        systraymenu->addSeparator();
        systraymenu->addAction(tr("Exit"), this, SLOT(close()));

//      systraymenu = nullptr;
    } else { // if not available, the messages will popup in the taskbar
        qDebug() << "No System Tray menues";
        systray = nullptr;
        systraymenu = nullptr;
    }

}

bool setupRunning = false;

// Get a unique identifier for this computer or VM
// We use this to determine whether machine has changed and openGL test needs to be repeated
// This is a Windows-only requirement (at this time)
QString getFingerprint() {
    QString machid = "n/a";
    QString glversion = getOpenGLVersionString();
    QString graphEngine = getGraphicsEngine();
    QString version = getVersion();
#ifdef Q_OS_WIN
    QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
                       QSettings::NativeFormat);
    machid = settings.value("MachineGuid").toString();
#endif
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(machid.toUtf8());
    hash.addData(glversion.toUtf8());
    hash.addData(graphEngine.toUtf8());
    hash.addData(version.toUtf8());
    return hash.result().toHex();
}

QString MainWindow::getMainWindowTitle()
{
    QString title = STR_TR_OSCAR + " " + getVersion().displayString();
#ifdef BROKEN_OPENGL_BUILD
    title += " ["+CSTR_GFX_BrokenGL+"]";
#endif
    return title;
}

// Prevents long repaint of entire window when creating first new gGraphView
void prepOpenGL (QWidget * widg) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0) && !defined(BROKEN_OPENGL_BUILD)
    #ifdef Q_OS_WIN
        widg->setAttribute(Qt::WA_NativeWindow);
    #endif
        QOpenGLWidget* gl;
        gl = new QOpenGLWidget(widg);
        gl->hide();
#else
    Q_UNUSED (widg)
#endif
}

void MainWindow::SetupGUI()
{
    setupRunning = true;

    setWindowTitle(getMainWindowTitle());

    prepOpenGL(ui->tabWidget);

#ifdef Q_OS_MAC
    ui->action_About->setMenuRole(QAction::AboutRole);
    ui->action_Preferences->setMenuRole(QAction::PreferencesRole);
#endif
    ui->actionPrint_Report->setShortcuts(QKeySequence::Print);
    ui->action_Fullscreen->setShortcuts(QKeySequence::FullScreen);

    ui->actionLine_Cursor->setChecked(AppSetting->lineCursorMode());
    ui->actionPie_Chart->setChecked(AppSetting->showPieChart());
    ui->actionDebug->setChecked(AppSetting->showDebug());
    ui->actionShow_Performance_Counters->setChecked(AppSetting->showPerformance());

    overview = nullptr;
    daily = nullptr;
    prefdialog = nullptr;
    profileSelector = nullptr;
    welcome = nullptr;

#ifdef NO_CHECKUPDATES
    ui->action_Check_for_Updates->setVisible(false);
#endif
    ui->oximetryButton->setDisabled(true);
    ui->dailyButton->setDisabled(true);
    ui->overviewButton->setDisabled(true);
    ui->statisticsButton->setDisabled(true);
    ui->importButton->setDisabled(true);
#ifdef helpless
    ui->helpButton->setVisible(false);
#endif

    m_inRecalculation = false;
    m_restartRequired = false;
    // Initialize Status Bar objects

    init_reportModeUi() ;

#ifdef Q_OS_MAC
    //p_profile->appearance->setAntiAliasing(false);
#endif
    //ui->action_Link_Graph_Groups->setChecked(p_profile->general->linkGroups());

    first_load = true;

    profileSelector = new ProfileSelector(ui->tabWidget);
    ui->tabWidget->insertTab(0, profileSelector,  STR_TR_Profile);

    // Profiles haven't been loaded here...
    profileSelector->updateProfileList();

    // Start with the new Profile Tab
    ui->tabWidget->setCurrentWidget(profileSelector);   // setting this to daily shows the cube during loading..
    ui->tabWidget->setTabEnabled(1, false);             // this should be the Statistics tab

    // toolbox is the right sidebar that contain the Navigation, bookmark , and Records tabs.
    // Navigation has offset 0
    // Bookmarks  has offset 1
    // Records    has offset 2
    ui->toolBox->setCurrentIndex(2);
    bool b = AppSetting->rightSidebarVisible();
    ui->action_Sidebar_Toggle->setChecked(b);
    ui->toolBox->setVisible(b);

    bool oldState = ui->actionShowPersonalData->blockSignals(true);
    ui->actionShowPersonalData->setChecked(AppSetting->showPersonalData());
    ui->actionShowPersonalData->blockSignals(oldState);

    oldState = ui->actionPie_Chart->blockSignals(true);
    ui->actionPie_Chart->setChecked(AppSetting->showPieChart());
    ui->actionPie_Chart->blockSignals(oldState);

    oldState = ui->actionDaily_Calendar->blockSignals(true);
    ui->actionDaily_Calendar->setChecked(AppSetting->calendarVisible());
    ui->actionDaily_Calendar->blockSignals(oldState);

    on_tabWidget_currentChanged(0);

#ifndef REMSTAR_M_SUPPORT
    ui->actionImport_RemStar_MSeries_Data->setVisible(false);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QRandomGenerator(QDateTime::currentDateTime().toSecsSinceEpoch());
#else
    qsrand(QDateTime::currentDateTime().toSecsSinceEpoch());
#endif

    QList<int> a;

    int panel_width = AppSetting->rightPanelWidth();
    a.push_back(this->width() - panel_width);
    a.push_back(panel_width);
    ui->mainsplitter->setSizes(a);
    ui->mainsplitter->setStretchFactor(0,1);
    ui->mainsplitter->setStretchFactor(1,0);

    QTimer::singleShot(50, this, SLOT(Startup()));

    ui->actionChange_Data_Folder->setVisible(false);
    ui->action_Frequently_Asked_Questions->setVisible(false);
    ui->actionReport_a_Bug->setVisible(false);  // remove this once we actually implement it
    ui->actionExport_Review->setVisible(false);  // remove this once we actually implement it

    reset_reportModeUi() ;
    if (!AppSetting->showDebug()) {
        ui->logText->hide();
    }


#ifndef helpless
    help = new Help(this);
    ui->tabWidget->addTab(help, tr("Help Browser"));
#endif
    setupRunning = false;
}

void MainWindow::logMessage(QString msg)
{
    ui->logText->appendPlainText(msg);
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    Q_UNUSED(event);
    static bool runonce = false;
    if (!runonce) {
        if (AppSetting->removeCardReminder()) {
            Notify(QObject::tr("Don't forget to place your datacard back in your CPAP device"), QObject::tr("OSCAR Reminder"));
            QThread::msleep(1000);
            QApplication::processEvents();
        }
        // Profile/User/chanels.xml is not read so it does not need to be saved//schema::channel.Save();
        if (p_profile) {
            CloseProfile();
        }

        // Shutdown and Save the current User profile
        Profiles::Done();

        // Save current window position
        QSettings settings;
        settings.setValue("MainWindow/geometry", saveGeometry());

        settings.setValue("Fingerprint", getFingerprint());

        // Trash anything allocated by the Graph objects
        DestroyGraphGlobals();

        if (systraymenu) {
            delete systraymenu;
            systraymenu = nullptr;
        }
        if (systray) {
            delete systray;
            systray = nullptr;
        }

        disconnect(logger, SIGNAL(outputLog(QString)), this, SLOT(logMessage(QString)));
        shutdownLogger();

        runonce = true;
    } else {
        qDebug() << "Qt is still calling closevent multiple times";
        QApplication::processEvents();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    QCoreApplication::quit();
}

void MainWindow::log(QString text)
{
    logger->appendClean(text);
}

void MainWindow::EnableTabs(bool b)
{
    ui->tabWidget->setTabEnabled(2, b);
    ui->tabWidget->setTabEnabled(3, b);
    ui->tabWidget->setTabEnabled(4, b);
}

void MainWindow::Notify(QString s, QString title, int ms)
{
    QString msg = s;
    if (title.isEmpty()) {
        title = STR_TR_OSCAR + " " + getVersion().displayString();
    }
    if (systray && !AppSetting->notifyMessagBoxOption()) {

#ifdef Q_OS_UNIX
        // GNOME3's systray hides the last line of the displayed Qt message.
        // As a workaround, add an extra line to bump the message back
        // into the visible area.
        char *desktop = getenv("DESKTOP_SESSION");

        if (desktop && !strncmp(desktop, "gnome", 5)) {
            msg += "\n";
        }
#endif

        systray->showMessage(title, msg, QSystemTrayIcon::Information, ms);
    } else {
        createNotifyMessageBox(this, title , msg, (ms+999)/1000);
    }
}

QString getCPAPPixmap(QString mach_class)
{
    QString cpapimage;
    if (mach_class == STR_MACH_ResMed) cpapimage = ":/icons/rms9.png";
    else if (mach_class == STR_MACH_PRS1) cpapimage = ":/icons/prs1.png";
    else if (mach_class == STR_MACH_Intellipap) cpapimage = ":/icons/intellipap.png";
    return cpapimage;
}

//QIcon getCPAPIcon(QString mach_class)
//{
//    QString cpapimage = getCPAPPixmap(mach_class);

//    return QIcon(cpapimage);
//}

void MainWindow::PopulatePurgeMenu()
{
    ui->menu_Rebuild_CPAP_Data->disconnect(ui->menu_Rebuild_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionRebuildCPAP(QAction *)));
    ui->menu_Rebuild_CPAP_Data->clear();

    ui->menuPurge_CPAP_Data->disconnect(ui->menuPurge_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionPurgeMachine(QAction *)));
    ui->menuPurge_CPAP_Data->clear();

    // Only allow rebuilding for CPAP for now, since that's the only thing that makes backups.
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    for (int i=0; i < machines.size(); ++i) {
        Machine *mach = machines.at(i);
        addMachineToMenu(mach, ui->menu_Rebuild_CPAP_Data);
    }

    // Add any imported device (except the built-in journal) to the purge menu.
    machines = p_profile->GetMachines();
    for (int i=0; i < machines.size(); ++i) {
        Machine *mach = machines.at(i);
        if (mach->type() == MT_JOURNAL) {
            continue;
        }
        addMachineToMenu(mach, ui->menuPurge_CPAP_Data);
    }

    ui->menu_Rebuild_CPAP_Data->connect(ui->menu_Rebuild_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionRebuildCPAP(QAction*)));
    ui->menuPurge_CPAP_Data->connect(ui->menuPurge_CPAP_Data, SIGNAL(triggered(QAction*)), this, SLOT(on_actionPurgeMachine(QAction*)));
}

void MainWindow::addMachineToMenu(Machine* mach, QMenu* menu)
{
    QString name = mach->brand();
    if (name.isEmpty()) {
        name = mach->loaderName();
    }
    name += " " + mach->model() + " " + mach->serial();

    QAction * action = new QAction(name.replace("&","&&"), menu);
    action->setIconVisibleInMenu(true);
    action->setIcon(mach->getPixmap());
    action->setData(mach->loaderName()+":"+mach->serial());
    menu->addAction(action);
}

void MainWindow::firstRunMessage()
{
    if (AppSetting->showAboutDialog() > 0) {
        AboutDialog * about = new AboutDialog(this);

        about->exec();

        AppSetting->setShowAboutDialog(-1);

        about->deleteLater();
    }
}

// QString GenerateWelcomeHTML();

bool MainWindow::OpenProfile(QString profileName, bool skippassword)
{
    qDebug() << "Opening profile" << profileName;

    auto pit = Profiles::profiles.find(profileName);
    if (pit == Profiles::profiles.end())
        return false;

    Profile * prof = pit.value();
    if (p_profile) {
        if ((prof != p_profile)) {
            CloseProfile();
        } else {
            // Already open
            return false;
        }
    }

    prof = profileSelector->SelectProfile(profileName, skippassword);  // asks for the password and updates stuff in profileSelector tab
    if (!prof) {
        return false;
    }
    // TODO: Check profile password

    // Check Lockfile
    QString lockhost = prof->checkLock();


    if (!lockhost.isEmpty()) {
        if (lockhost.compare(QHostInfo::localHostName()) != 0) {
            if (staticQMessageBox::warning(this, STR_MessageBox_Warning,
                    QObject::tr("There is a lockfile already present for this profile '%1', claimed on '%2'.").arg(prof->user->userName()).arg(lockhost)+"\n\n"+
                    QObject::tr("You can only work with one instance of an individual OSCAR profile at a time.")+"\n\n"+
                    QObject::tr("If you are using cloud storage, make sure OSCAR is closed and syncing has completed first on the other computer before proceeding."),
                    QMessageBox::Cancel |QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel) {
                return false;
            }
        } // not worried about localhost locks anymore, just silently drop it.

        prof->removeLock();
    }

    p_profile = prof;
    ProgressDialog * progress = new ProgressDialog(this);

    progress->setMessage(QObject::tr("Loading profile \"%1\"...").arg(profileName));
    progress->open();

    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    if (machines.isEmpty()) {
        qDebug() << "No devices in profile";
    } else {
#ifdef LOCK_RESMED_SESSIONS
    for (QList<Machine *>::iterator it = machines.begin(); it != machines.end(); ++it) {
        QString mclass=(*it)->loaderName();
        if (mclass == STR_MACH_ResMed) {
            qDebug() << "ResMed device found.. locking OSCAR preferences to suit it's summary system";

            // Have to sacrifice these features to get access to summary data.
            p_profile->session->setCombineCloseSessions(0);
            p_profile->session->setDaySplitTime(QTime(12,0,0));
            p_profile->session->setIgnoreShortSessions(false);
            p_profile->session->setLockSummarySessions(true);
            p_profile->general->setPrefCalcPercentile(95.0);    // 95%
            p_profile->general->setPrefCalcMiddle(0);           // Median (50%)
            p_profile->general->setPrefCalcMax(1);              // Dodgy max

            break;
        }
    }
#endif
    }

    // Todo: move this to AHIWIndow check to profile Load function...
    if (p_profile->cpap->AHIWindow() < 30.0) {
        p_profile->cpap->setAHIWindow(60.0);
    }


    if (p_profile->p_preferences[STR_PREF_ReimportBackup].toBool()) {
        importCPAPBackups();
        p_profile->p_preferences[STR_PREF_ReimportBackup]=false;
    }

    p_profile->LoadMachineData(progress);

    if (!p_profile->LastDay(MT_CPAP).isValid() ) { // quick test if new profile or not.
        // Override default value of clinicalMode if new profile.
        // Allows permissiveMode (not clinicalMode) to be the default value for existing profiles.
        p_profile->cpap->setClinicalMode(true);
    }
    m_clinicalMode = p_profile->cpap->clinicalMode();

    progress->setMessage(tr("Finishing profile \"%1\"").arg(profileName));

    // Show the logo?
//    QPixmap logo=QPixmap(":/icons/logo-md.png").scaled(64,64);
//    progress->setPixmap(logo);

    QApplication::processEvents();


    // Reload everything profile related
    if (daily) {
        qCritical() << "OpenProfile called with active Daily object!";
        qDebug() << "Abandon opening Profile";
        return false;
    }
//    qDebug() << "creating Welcome page";
    welcome = new Welcome(ui->tabWidget);
    ui->tabWidget->insertTab(1, welcome, tr("Welcome"));

//    qDebug() << "Creating Daily page";
    daily = new Daily(ui->tabWidget, nullptr);
    ui->tabWidget->insertTab(2, daily, STR_TR_Daily);
//    qDebug() << "reloading daily graphs";
    daily->ReloadGraphs();

    if (overview) {
        qCritical() << "OpenProfile called with active Overview object!";
        qDebug() << "Abandon opening Profile";
        return false;
    }
//    qDebug() << "Creating Overview page";
    overview = new Overview(ui->tabWidget, daily->graphView());
    ui->tabWidget->insertTab(3, overview, STR_TR_Overview);
//    qDebug() << "reloading overview graphs";
    overview->ReloadGraphs();

    // Should really create welcome and statistics here, but they need redoing later anyway to kill off webkit
    ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());

    // always use last user setting - so don't reset. // p_profile->general->setStatReportMode(STAT_MODE_STANDARD);
//    qDebug() << "Creating Statistics page";
    GenerateStatistics();
//    qDebug() << "Creating Purge menu";
    PopulatePurgeMenu();

    AppSetting->setProfileName(p_profile->user->userName());
    setWindowTitle(tr("%1 (Profile: %2)").arg(getMainWindowTitle()).arg(AppSetting->profileName()));

    QList<Machine *> oximachines = p_profile->GetMachines(MT_OXIMETER);                // Machines of any type except Journal
    QList<Machine *> posmachines = p_profile->GetMachines(MT_POSITION);
    QList<Machine *> stgmachines = p_profile->GetMachines(MT_SLEEPSTAGE);
    bool noMachines = machines.isEmpty() && posmachines.isEmpty() && oximachines.isEmpty() && stgmachines.isEmpty();

    ui->importButton->setDisabled(false);
    ui->oximetryButton->setDisabled(false);
    ui->dailyButton->setDisabled(noMachines);
    ui->overviewButton->setDisabled(noMachines);
    ui->statisticsButton->setDisabled(noMachines);
    ui->tabWidget->setTabEnabled(2, !noMachines);       // daily, STR_TR_Daily);
    ui->tabWidget->setTabEnabled(3, !noMachines);       // overview, STR_TR_Overview);
    ui->tabWidget->setTabEnabled(4, !noMachines);       // statistics, STR_TR_Statistics);

    progress->close();
    delete progress;
    qDebug() << "Finished opening Profile";

    if (updateChecker != nullptr)
        updateChecker->showMessage();

    return true;
}

void MainWindow::CloseProfile()
{
    if (updateChecker != nullptr)
        updateChecker->showMessage();

    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        delete daily;
        daily = nullptr;
    }
    if (welcome) {
        delete welcome;
        welcome = nullptr;
    }
    if (overview) {
        delete overview;
        overview = nullptr;
    }

    if (p_profile) {
        p_profile->StoreMachines();
        p_profile->UnloadMachineData();
        p_profile->saveChannels();
        p_profile->Save();
        p_profile->removeLock();
        p_profile = nullptr;
    }
}


#ifdef Q_OS_WIN
void MainWindow::TestWindowsOpenGL()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,4,0)) && !defined(BROKEN_OPENGL_BUILD)
    // 1. Set OpenGLCompatibilityCheck=1 in registry.
    QSettings settings;
    settings.setValue("OpenGLCompatibilityCheck", true);

    // 2. See if OpenGL crashes the application:
    QOpenGLWidget* gl;
    gl = new QOpenGLWidget(ui->tabWidget);
    ui->tabWidget->insertTab(2, gl, "");
    //qDebug() << __LINE__;
    QCoreApplication::processEvents();  // this triggers the SIGSEGV
    //qDebug() << __LINE__;
    // If we get here, OpenGL won't crash the application.
    ui->tabWidget->removeTab(2);
    delete gl;

    // 3. Remove OpenGLCompatibilityCheck from the registry upon success.
    settings.remove("OpenGLCompatibilityCheck");
#endif
}
#endif


void MainWindow::Startup()
{
#ifdef Q_OS_WIN
    QSettings settings;
    if (getFingerprint() != settings.value("Fingerprint"))
        TestWindowsOpenGL();
#endif

    for (auto & loader : GetLoaders()) {
        loader->setParent(this);
    }
    QString lastProfile = AppSetting->profileName();

    firstRunMessage();

    if (Profiles::profiles.contains(lastProfile) && AppSetting->autoOpenLastUsed()) {
        if (OpenProfile(lastProfile)) {
            ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());

            if (AppSetting->autoLaunchImport()) {
                on_importButton_clicked();
            }
        }
    } else {
        ui->tabWidget->setCurrentWidget(profileSelector);
    }
}

int MainWindow::importCPAP(ImportPath import, const QString &message)
{
    if (!import.loader) {
        return 0;
    }

    ui->tabWidget->setCurrentWidget(welcome);
    QApplication::processEvents();
    ProgressDialog * progdlg = new ProgressDialog(this);

    QPixmap image = import.loader->getPixmap(import.loader->PeekInfo(import.path).series);
    image = image.scaled(64,64);
    progdlg->setPixmap(image);

    progdlg->addAbortButton();

    progdlg->setWindowModality(Qt::ApplicationModal);
    progdlg->open();
    progdlg->setMessage(message);


    connect(import.loader, SIGNAL(updateMessage(QString)), progdlg, SLOT(setMessage(QString)));
    connect(import.loader, SIGNAL(setProgressMax(int)), progdlg, SLOT(setProgressMax(int)));
    connect(import.loader, SIGNAL(setProgressValue(int)), progdlg, SLOT(setProgressValue(int)));
    connect(progdlg, SIGNAL(abortClicked()), import.loader, SLOT(abortImport()));

    ImportUI importui(p_profile);
    ImportContext* ctx = new ProfileImportContext(p_profile);
    import.loader->SetContext(ctx);
    connect(ctx, &ImportContext::importEncounteredUnexpectedData, &importui, &ImportUI::onUnexpectedData);
    connect(import.loader, &MachineLoader::deviceReportsUsageOnly, &importui, &ImportUI::onDeviceReportsUsageOnly);
    connect(import.loader, &MachineLoader::deviceIsUntested, &importui, &ImportUI::onDeviceIsUntested);
    connect(import.loader, &MachineLoader::deviceIsUnsupported, &importui, &ImportUI::onDeviceIsUnsupported);

    int c = import.loader->Open(import.path);

    progdlg->setMessage(QObject::tr("Finishing up..."));
    QCoreApplication::processEvents();
    ctx->Commit();

    import.loader->SetContext(nullptr);
    delete ctx;

    if (c > 0) {
        Notify(tr("Imported %1 CPAP session(s) from\n\n%2").arg(c).arg(import.path), tr("Import Success"));
    } else if (c == 0) {
        Notify(tr("Already up to date with CPAP data at\n\n%1").arg(import.path), tr("Up to date"));
    } else {
        Notify(tr("Couldn't find any valid Device Data at\n\n%1").arg(import.path),tr("Import Problem"));
    }
    disconnect(progdlg, SIGNAL(abortClicked()), import.loader, SLOT(abortImport()));
    disconnect(import.loader, SIGNAL(setProgressMax(int)), progdlg, SLOT(setProgressMax(int)));
    disconnect(import.loader, SIGNAL(setProgressValue(int)), progdlg, SLOT(setProgressValue(int)));
    disconnect(import.loader, SIGNAL(updateMessage(QString)), progdlg, SLOT(setMessage(QString)));

    progdlg->close();

    delete progdlg;

    if (AppSetting->openTabAfterImport()>0) {
        ui->tabWidget->setCurrentIndex(AppSetting->openTabAfterImport());
    }

    return c;
}

void MainWindow::updateOverview()
{
    if (overview)
        overview->ReloadGraphs();
}
void MainWindow::finishCPAPImport()
{
    if (daily)
        daily->Unload(daily->getDate());

    p_profile->StoreMachines();
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    for (Machine * mach : machines) {
        mach->saveSessionInfo();
        mach->SaveSummaryCache();
    }

    GenerateStatistics();
    profileSelector->updateProfileList();

    if (welcome)
        welcome->refreshPage();

    if (overview) { overview->ReloadGraphs(); }
    if (daily) {
//        daily->populateSessionWidget();
        daily->ReloadGraphs();
    }
    if (AppSetting->openTabAfterImport()>0) {
        ui->tabWidget->setCurrentIndex(AppSetting->openTabAfterImport());
    }

}

void MainWindow::importCPAPBackups()
{
    // Get BackupPaths for all CPAP devices
    QList<Machine *> machlist = p_profile->GetMachines(MT_CPAP);
    QList<ImportPath> paths;
    Q_FOREACH(Machine *m, machlist) {
        paths.append(ImportPath(m->getBackupPath(), lookupLoader(m)));
    }

    if (paths.size() > 0) {
        int c=0;
        Q_FOREACH(ImportPath path, paths) {
            c+=importCPAP(path, tr("Please wait, importing from backup folder(s)..."));
        }
        if (c>0) {
            finishCPAPImport();
        }
    }
}

#ifdef Q_OS_UNIX
# include <stdio.h>
# include <unistd.h>

# if defined(Q_OS_MAC) || defined(Q_OS_BSD4)
#  include <sys/mount.h>
# elif defined(Q_OS_HAIKU)
// nothing needed
# else
#  include <sys/statfs.h>
#  include <mntent.h>
# endif // Q_OS_MAC/BSD

#endif // Q_OS_UNIX

//! \brief Returns a list of drive mountpoints
QStringList getDriveList()
{
    QStringList drivelist;
    bool crostini_detected = false;

#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
#if defined(Q_OS_LINUX)
    #define VFAT "vfat"
#elif defined(Q_OS_WIN)
    #define VFAT "FAT32"
    Q_UNUSED(crostini_detected)
#elif defined(Q_OS_MAC)
    #define VFAT "msdos"
    Q_UNUSED(crostini_detected)
#endif
    foreach (const QStorageInfo &storage, QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
#ifdef DEBUG_SDCARD
            if (storage.fileSystemType() != "tmpfs") {      // Don't show all the Linux tmpfs mount points!
                qDebug() << "Device:" << storage.device();
                qDebug() << "    Path:" << storage.rootPath();
                qDebug() << "    Name:" << storage.name();     // ...
                qDebug() << "    FS Type:" << storage.fileSystemType();
            }
#endif
            if (storage.fileSystemType() == VFAT) {
                qDebug() << "Adding" << storage.name() << "on" << storage.rootPath() << "to drivelist";
                drivelist.append(storage.rootPath());
            } else if (storage.fileSystemType() == "9p") {
                qDebug() << "Crostini filesystem 9p found";
                crostini_detected = true;
            }
        }
    }
#endif
#if defined(Q_OS_LINUX)
    if (crostini_detected) {
	    QString mntName("/mnt/chromeos/removable");
	    QDir mnt(mntName);
	    qDebug() << "Checking for" << mntName;
	    if ( mnt.exists() ) {
	        qDebug() << "Checking Crostini removable folders";
	        QFileInfoList mntPts = mnt.entryInfoList();
	        foreach ( const auto dir, mntPts ) {
	            qDebug() << "Adding" << dir.filePath() << "to drivelist";
	            drivelist.append(dir.filePath() );
	        }
	    } else {
            drivelist.clear();
            drivelist.append("CROSTINI");
        }
    }
#endif
    return drivelist;
}

extern MainWindow * mainwin;
void ImportDialogScan::cancelbutton()
{
    mainwin->importScanCancelled = true;
    hide();
}

QList<ImportPath> MainWindow::detectCPAPCards()
{
    const int timeout = 20000;  // twenty seconds

    QList<ImportPath> detectedCards;
    importScanCancelled = false;
    QString lastpath = (*p_profile)[STR_PREF_LastCPAPPath].toString();

    QList<MachineLoader *>loaders = GetLoaders(MT_CPAP);

    // See if last import location still has CPAP data. If so, consider it selected.
    Q_FOREACH(MachineLoader * loader, loaders) {
        if (loader->Detect(lastpath)) {
            detectedCards.append(ImportPath(lastpath, loader));
            qDebug() << "Found existing" << loader->loaderName() << "datacard at" << lastpath;
            return detectedCards;
        }
    }

    // Don't have existing import location. Search for others.
    QElapsedTimer time;
    time.start();

    // Create dialog
    ImportDialogScan popup(this) ;//, Qt::SplashScreen);
    QLabel waitmsg(tr("Please insert your CPAP data card..."));
    QProgressBar progress;
    QVBoxLayout waitlayout;
    popup.setLayout(&waitlayout);

    QHBoxLayout layout2;
    QIcon icon("://icons/sdcard.png");
    QPushButton skipbtn(icon, tr("Choose a folder"));
    QPushButton cancelbtn(STR_MessageBox_Cancel);
    skipbtn.setMinimumHeight(40);
    cancelbtn.setMinimumHeight(40);
    waitlayout.addWidget(&waitmsg,1,Qt::AlignCenter);
    waitlayout.addWidget(&progress,1);
    waitlayout.addLayout(&layout2,1);
    layout2.addWidget(&skipbtn);
    layout2.addWidget(&cancelbtn);
    popup.connect(&skipbtn, SIGNAL(clicked()), &popup, SLOT(hide()));
    popup.connect(&cancelbtn, SIGNAL(clicked()), &popup, SLOT(cancelbutton()));
    progress.setValue(0);
    progress.setMaximum(timeout);
    progress.setVisible(true);
//    importScanCancelled = false;
    popup.show();
    QApplication::processEvents();

    // Look for a mount point that contains CPAP data
    do {
        // Rescan for mount points in case user inserted a card
        QStringList AutoScannerPaths = getDriveList();
        if (AutoScannerPaths.contains("CROSTINI")) {  // no Crostini removable drives found!
            if (( lastpath.size() > 0) && ( ! AutoScannerPaths.contains(lastpath))) {
                if (QFile(lastpath).exists() ) {
                    AutoScannerPaths.insert(0, lastpath);
                }
            }
            else {
                staticQMessageBox::warning(this, STR_MessageBox_Warning,
                    QObject::tr("Chromebook file system detected, but no removable device found\n") +
                    QObject::tr("You must share your SD card with Linux using the ChromeOS Files program"));
                break;                                    // break out of the 20 second wait loop
            }
        }

        qDebug() << "Drive list size:" << AutoScannerPaths.size();

        if ( (lastpath.size()>0) && ( ! AutoScannerPaths.contains(lastpath))) {
            if (QFile(lastpath).exists())
                AutoScannerPaths.insert(0, lastpath);
        }

        Q_FOREACH(const QString &path, AutoScannerPaths) {
            // Scan through available device loaders and test if this folder contains valid folder structure
            Q_FOREACH(MachineLoader * loader, loaders) {
                if (loader->Detect(path)) {
                    detectedCards.append(ImportPath(path, loader));
                    qDebug() << "Found" << loader->loaderName() << "datacard at" << path;
                }
                QApplication::processEvents();
            }
        }
        int el=time.elapsed();
        progress.setValue(el);
        if (el > timeout)
            break;
        if ( ! popup.isVisible())
            break;
        // needs a slight delay here
        for (int i=0; i<20; ++i) {
            QThread::msleep(50);
            QApplication::processEvents();
        }
    } while (detectedCards.size() == 0);

    if (detectedCards.size() == 0) {
        qDebug() << "Did not find a CPAP datacard in 20 seconds";
    }

    popup.hide();
    popup.disconnect(&skipbtn, SIGNAL(clicked()), &popup, SLOT(hide()));
    popup.disconnect(&cancelbtn, SIGNAL(clicked()), &popup, SLOT(hide()));

    return detectedCards;
}


void MainWindow::on_action_Import_Data_triggered()
{
    static bool in_import = false;
    if ( p_profile == nullptr ) {
        Notify(tr("No profile has been selected for Import."), STR_MessageBox_Busy);
        return;
    }
    if (m_inRecalculation) {
        Notify(tr("Access to Import has been blocked while recalculations are in progress."),STR_MessageBox_Busy);
        return;
    }
    if (in_import) {
        Notify(tr("Import is already running in the background."),STR_MessageBox_Busy);
        return;
    }
    in_import=true;

    ui->tabWidget->setCurrentWidget(welcome);
    QApplication::processEvents();

    QList<ImportPath> datacards = selectCPAPDataCards(tr("Would you like to import from this location?"));
    if (datacards.size() > 0) {
        importCPAPDataCards(datacards);
    }

    in_import=false;
}


QList<ImportPath> MainWindow::selectCPAPDataCards(const QString & prompt, bool alwaysPrompt)
{
    QList<ImportPath> datacards = detectCPAPCards();

    if (importScanCancelled) {
        datacards.clear();
        return datacards;
    }

    QList<MachineLoader *>loaders = GetLoaders(MT_CPAP);

    QElapsedTimer time;
    time.start();


    bool asknew = false;

    // TODO: This should either iterate over all detected cards and prompt for each, or it should only
    // provide the one confirmed card in the list.
    if (datacards.size() > 0) {
        while (datacards.size() > 1) {datacards.removeLast();} // Process only the confirmed card
        MachineInfo info = datacards[0].loader->PeekInfo(datacards[0].path);
        QString infostr;
        if (!info.model.isEmpty()) {
            QString infostr2 = info.model+" ("+info.serial+")";
            infostr = tr("A %1 file structure for a %2 was located at:").arg(info.brand).arg(infostr2);
        } else {
            infostr = tr("A %1 file structure was located at:").arg(datacards[0].loader->loaderName());
        }

        if (alwaysPrompt || !p_profile->cpap->autoImport()) {
            QMessageBox mbox(QMessageBox::NoIcon,
                tr("CPAP Data Located"), infostr+"\n\n"+QDir::toNativeSeparators(datacards[0].path)+"\n\n"+
                prompt,
                QMessageBox::Yes | QMessageBox::Cancel, this);
            mbox.setDefaultButton(QMessageBox::Yes);
            mbox.addButton(tr("Specify"), QMessageBox::NoRole);

            QPixmap pixmap = datacards[0].loader->getPixmap(datacards[0].loader->PeekInfo(datacards[0].path).series).scaled(64,64);

            //QPixmap pixmap = QPixmap(getCPAPPixmap(datacards[0].loader->loaderName())).scaled(64,64);
            mbox.setIconPixmap(pixmap);
            int res = mbox.exec();
            if (res == QMessageBox::Cancel) {
                // Give the communal progress bar back
                datacards.clear();
                return datacards;
            } else if (res == QMessageBox::Yes) {
                // let it fall through.
            } else {
                //waitmsg->setText(tr("Please wait, launching file dialog..."));
                datacards.clear();
                asknew = true;
            }
        }
    } else {
        //waitmsg->setText(tr("No CPAP data card detected, launching file dialog..."));
        asknew = true;
    }

    // TODO: Get rid of the reminder and instead validate the user's selection (using the loader detection
    // below) and loop until the user either cancels or selects a valid folder.
    //
    // It doesn't look like there's any way to implement such a programmatic filter within the file
    // selection dialog without resorting to a non-native dialog.
    if (asknew) {
       // popup.show();
        mainwin->Notify(tr("Please remember to select the root folder or drive letter of your data card, and not a folder inside it."),
                        tr("Import Reminder"),8000);

        QFileDialog w(this);

        QString folder;
        if (p_profile->contains(STR_PREF_LastCPAPPath)) {
            folder = (*p_profile)[STR_PREF_LastCPAPPath].toString();
        } else {
            // TODO: Is a writable path really the best place to direct the user to find their SD card data?
            folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        }

        w.setDirectory(folder);
        w.setFileMode(QFileDialog::Directory);
        w.setOption(QFileDialog::ShowDirsOnly, true);
        w.setWindowTitle(tr("Find your CPAP data card"));

        if (w.exec() != QDialog::Accepted) {
            datacards.clear();
            return datacards;
        }


        bool found=false;
        for (int i = 0; i < w.selectedFiles().size(); i++) {
            Q_FOREACH(MachineLoader * loader, loaders) {
                if (loader->Detect(w.selectedFiles().at(i))) {
                    found=true;
                    datacards.append(ImportPath(w.selectedFiles().at(i), loader));
                    break;
                }
            }
        }
        if (!found) {
            QMessageBox msgBox ( QMessageBox::Information , tr("OSCAR Information") , tr("No supported data was found") , QMessageBox::Ok ) ;
            msgBox.setInformativeText(w.selectedFiles().at(0));
            msgBox.exec();
        }
    }

    return datacards;
}


void MainWindow::importCPAPDataCards(const QList<ImportPath> & datacards)
{
    bool newdata = false;

    int c = -1;
    for (int i = 0; i < datacards.size(); i++) {
        QString dir = datacards[i].path;
        MachineLoader * loader = datacards[i].loader;
        if (!loader) continue;

        if (!dir.isEmpty()) {
            c = importCPAP(datacards[i], tr("Importing Data"));
            qDebug() << "Finished Importing data" << c;

            if (c >= 0) {
                QDir d(dir.section("/",0,-1));
                (*p_profile)[STR_PREF_LastCPAPPath] = d.absolutePath();
            }

            if (c > 0) {
                newdata = true;
            }
        }
    }

    if (newdata)  {
        finishCPAPImport();
        PopulatePurgeMenu();
    }
}


QMenu *MainWindow::CreateMenu(QString title)
{
    QMenu *menu = new QMenu(title, ui->menubar);
    ui->menubar->insertMenu(ui->menu_Help->menuAction(), menu);
    return menu;
}

void MainWindow::on_action_Fullscreen_triggered()
{
    if (ui->action_Fullscreen->isChecked()) {
        this->showMaximized();
    } else {
        this->showNormal();
    }
}

void MainWindow::setRecBoxHTML(QString html)
{
    ui->recordsBox->setHtml(html);
}

void MainWindow::setStatsHTML(QString html)
{
    ui->statisticsView->setHtml(html);
}


void MainWindow::updateFavourites()
{
    QDate date = p_profile->LastDay(MT_JOURNAL);

    if (!date.isValid()) {
        return;
    }

    QString html = "<html><head><style type='text/css'>"
                   "p,a,td,body { font-family: '" + QApplication::font().family() + "'; }"
                   "p,a,td,body { font-size: " + QString::number(QApplication::font().pointSize() + 2) + "px; }"
                   "a:link,a:visited { color: inherit; text-decoration: none; }" //font-weight: normal;
                   "a:hover { background-color: inherit; color: white; text-decoration:none; font-weight: bold; }"
                   "</style></head><body>"
                   "<table width=100% cellpadding=2 cellspacing=0>";

    do {
        Day *journal = p_profile->GetDay(date, MT_JOURNAL);

        if (journal) {
            int numBookmarks = 0;
            if (journal->size() > 0) {
                Session *sess = journal->firstSession(MT_JOURNAL);
                if (!sess) {
                    qWarning() << "null session for MT_JOURNAL first session";
                } else {
                    QString tmp;
                    bool filtered = !bookmarkFilter.isEmpty();
                    bool found = !filtered;

                    if (sess->settings.contains(Bookmark_Start)) {
                        //QVariantList start=sess->settings[Bookmark_Start].toList();
                        //QVariantList end=sess->settings[Bookmark_End].toList();
                        QStringList notes = sess->settings[Bookmark_Notes].toStringList();

                        if (notes.size() > 0) {
                            if (numBookmarks++ > 1) {
                                tmp += "<br/>";
                            }
                            tmp += QString("<tr><td><b><a href='daily=%1'>%2</a></b>")
                                    .arg(date.toString(Qt::ISODate))
                                    .arg(date.toString(MedDateFormat));

                            tmp += "<list>";

                            for (int i = 0; i < notes.size(); i++) {
                                //QDate d=start[i].toDate();
                                QString note = notes[i];

                                if (filtered && note.contains(bookmarkFilter, Qt::CaseInsensitive)) {
                                    found = true;
                                }

                                tmp += "<li>" + note + "</li>";
                            }

                            tmp += "</list></td>";
                        }
                    }

                    if (found) { html += tmp; }
                }
            }
        }

        date = date.addDays(-1);
    } while (date >= p_profile->FirstDay(MT_JOURNAL));

    html += "</table></body></html>";
    ui->bookmarkView->setHtml(html);
}

void MainWindow::on_dailyButton_clicked()
{
    if (daily) {
        ui->tabWidget->setCurrentWidget(daily);
        daily->RedrawGraphs();
    }
}
void MainWindow::on_overviewButton_clicked()
{
    if (overview) {
        ui->tabWidget->setCurrentWidget(overview);
    }
}

void MainWindow::aboutBoxLinkClicked(const QUrl &url)
{
    QDesktopServices::openUrl(url);
}

void MainWindow::on_action_About_triggered()
{
    AboutDialog * about = new AboutDialog(this);

    about->exec();

    about->deleteLater();
}

void MainWindow::on_actionDebug_toggled(bool checked)
{
    AppSetting->setShowDebug(checked);

    logger->strlock.lock();
    if (checked) {
        ui->logText->show();
    } else {
        ui->logText->hide();
    }
  //  QApplication::processEvents();
    logger->strlock.unlock();
}

void MainWindow::on_action_Reset_Graph_Layout_triggered()
{
    if (daily && (ui->tabWidget->currentWidget() == daily)) { daily->ResetGraphLayout(); }

    if (overview && (ui->tabWidget->currentWidget() == overview)) { overview->ResetGraphLayout(); }
}

/*
void MainWindow::on_action_Reset_Graph_Order_triggered()
{
    if (daily && (ui->tabWidget->currentWidget() == daily)) { daily->ResetGraphOrder(0); }

    if (overview && (ui->tabWidget->currentWidget() == overview)) { overview->ResetGraphOrder(0); }
}
*/

void MainWindow::on_action_Standard_Graph_Order_triggered()
{
    if (daily && (ui->tabWidget->currentWidget() == daily)) { daily->ResetGraphOrder(1); }

    if (overview && (ui->tabWidget->currentWidget() == overview)) { overview->ResetGraphOrder(1); }
}

void MainWindow::on_action_Advanced_Graph_Order_triggered()
{
    if (daily && (ui->tabWidget->currentWidget() == daily)) { daily->ResetGraphOrder(2); }

    if (overview && (ui->tabWidget->currentWidget() == overview)) { overview->ResetGraphOrder(2); }
}

void MainWindow::on_action_Preferences_triggered()
{
    if (!p_profile) {
        mainwin->Notify(tr("Please open a profile first."));
        return;
    }

    if (m_inRecalculation) {
        mainwin->Notify(tr("Access to Preferences has been blocked until recalculation completes."));
        return;
    }

    PreferencesDialog pd(this, p_profile);
    prefdialog = &pd;

    if (pd.exec() == PreferencesDialog::Accepted) {
        // Apply any updates from preference changes (notably fonts)

        setApplicationFont();


        if (m_clinicalMode != p_profile->cpap->clinicalMode() ) {
            m_clinicalMode = p_profile->cpap->clinicalMode(); ;
            reloadProfile();
        };

        if (daily) {
            daily->RedrawGraphs();
        }

        if (overview) {
            overview->ResetFont();
            overview->RebuildGraphs(true);
        }

        if (welcome)
            welcome->refreshPage();

        if (profileSelector)
            profileSelector->updateProfileList();

        GenerateStatistics();

// These attempts to update calendar after a change to application font do NOT work, and I can find no QT documentation suggesting
// that changing the font after Calendar is created is even possible.
//        qDebug() << "application font family set to" << QApplication::font().family() << "and font" << QApplication::font();
//        ui->statStartDate->calendarWidget()->setFont(QApplication::font());
//        ui->statStartDate->calendarWidget()->repaint();
    }

    prefdialog = nullptr;
}

#include "oximeterimport.h"
QDateTime datetimeDialog(QDateTime datetime, QString message);

void MainWindow::on_oximetryButton_clicked()
{
    if (p_profile) {
        OximeterImport oxiimp(this);
        oxiimp.exec();
        PopulatePurgeMenu();
        if (overview) overview->ReloadGraphs();
        if (welcome) welcome->refreshPage();
    }
}

// Called for automatic check for updates
void MainWindow::CheckForUpdates(bool showWhenCurrent)
{
    updateChecker = new CheckUpdates(this);
#ifdef NO_CHECKUPDATES
    if (showWhenCurrent)
        staticQMessageBox::information(this, STR_MessageBox_Information, tr("Check for updates not implemented"));
#else
    updateChecker->checkForUpdates(showWhenCurrent);
#endif
}

// Called for manual check for updates
void MainWindow::on_action_Check_for_Updates_triggered()
{
    CheckForUpdates(true);
}

bool toolbox_visible = false;
void MainWindow::on_action_Screenshot_triggered()
{
    setUpdatesEnabled(false);
    if (daily)
        daily->hideSpaceHogs();
    toolbox_visible = ui->toolBox->isVisible();
    ui->toolBox->hide();
    setUpdatesEnabled(true);
    QTimer::singleShot(250, this, SLOT(DelayedScreenshot()));
}

void MainWindow::DelayedScreenshot()
{
    // Make sure to scale for high resolution displays (like Retina)
   // qreal pr = devicePixelRatio();

    auto screenshotRect = geometry();
    auto titleBarHeight = QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight);

    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto pixmap = QApplication::primaryScreen()->grabWindow(QDesktopWidget().winId(),
                                                            screenshotRect.left(),
                                                            screenshotRect.top() - titleBarHeight,
                                                            screenshotRect.width(),
                                                            screenshotRect.height() + titleBarHeight);
    #else
    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap;
    if (screen) {
        pixmap = screen->grabWindow(0,  // 0 means grab the entire screen
                                            screenshotRect.left(),
                                            screenshotRect.top() - titleBarHeight,
                                            screenshotRect.width(),
                                            screenshotRect.height() + titleBarHeight);
        // Use the pixmap as needed
    }
    #endif

    QString default_filename = "/screenshot-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".png";

    QString png_filepath;
    if (AppSetting->dontAskWhenSavingScreenshots()) {
        png_filepath = p_pref->Get("{home}/Screenshots");
        QDir dir(png_filepath);
        if (!dir.exists()) {
            dir.mkdir(png_filepath);
        }
        png_filepath += default_filename;
    } else {
        QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + default_filename;
        png_filepath = QFileDialog::getSaveFileName(this, tr("Choose where to save screenshot"), folder, tr("Image files (*.png)"));
        if (png_filepath.isEmpty() == false && png_filepath.toLower().endsWith(".png") == false) {
            png_filepath += ".png";
        }
    }

    // png_filepath will be empty if the user canceled the file selection above.
    if (png_filepath.isEmpty() == false) {
        qDebug() << "Saving screenshot to" << png_filepath;
        if (!pixmap.save(png_filepath)) {
            Notify(tr("There was an error saving screenshot to file \"%1\"").arg(QDir::toNativeSeparators(png_filepath)));
        } else {
            Notify(tr("Screenshot saved to file \"%1\"").arg(QDir::toNativeSeparators(png_filepath)));
        }
    }

    setUpdatesEnabled(false);
    if (daily)
        daily->showSpaceHogs();
    ui->toolBox->setVisible(toolbox_visible);
    setUpdatesEnabled(true);
}

void MainWindow::on_actionView_Oximetry_triggered()
{
    on_oximetryButton_clicked();
}

void MainWindow::on_actionPrint_Report_triggered()
{
    Report report;

    if (ui->tabWidget->currentWidget() == overview) {
        Report::PrintReport(overview->graphView(), STR_TR_Overview);
    } else if (ui->tabWidget->currentWidget() == daily) {
        Report::PrintReport(daily->graphView(), STR_TR_Daily, daily->getDate());
    } else if (ui->tabWidget->currentWidget() == ui->statisticsTab) {
        Statistics::printReport(this);
#ifndef helpless
    } else if (ui->tabWidget->currentWidget() == help) {
        help->print(&printer);  // **** THIS DID NOT SURVIVE REFACTORING STATISTICS PRINT
#endif
    }
}

void MainWindow::on_action_Edit_Profile_triggered()
{
    NewProfile *newprof = new NewProfile(this);
    QString name = AppSetting->profileName();
    newprof->edit(name);
    newprof->setWindowModality(Qt::ApplicationModal);
    newprof->setModal(true);
    newprof->exec();
    qDebug()  << newprof;
    delete newprof;
}

void MainWindow::on_action_CycleTabs_triggered()
{
    int i;
    qDebug() << "Switching Tabs";
    i = ui->tabWidget->currentIndex() + 1;

    if (i >= ui->tabWidget->count()) {
        i = 0;
    }

    ui->tabWidget->setCurrentIndex(i);
}

void MainWindow::on_actionOnline_Users_Guide_triggered()
{
    if (staticQMessageBox::question(this, STR_MessageBox_Question, tr("The User's Guide will open in your default browser"),
            QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok )
        QDesktopServices::openUrl(QUrl("https://www.apneaboard.com/wiki/index.php?title=OSCAR_Help"));
}

void MainWindow::on_action_Frequently_Asked_Questions_triggered()
{
    staticQMessageBox::information(this, STR_MessageBox_Information, tr("The FAQ is not yet implemented"));
}

void MainWindow::on_action_Rebuild_Oximetry_Index_triggered()
{
    QVector<ChannelID> invalid;

    QList<Machine *> machines = p_profile->GetMachines(MT_OXIMETER);

    qint64 f = 0, l = 0;

    int discard_threshold = p_profile->oxi->oxiDiscardThreshold();
    Machine *m;

    for (int z = 0; z < machines.size(); z++) {
        m = machines.at(z);
        //m->sessionlist.erase(m->sessionlist.find(0));
        QList<ChannelID> valid = m->availableChannels(schema::ChanType::ALL);
        valid.removeAll(OXI_PulseChange);  // Delete only these and recalculate.
        valid.removeAll(OXI_SPO2Drop);

        // For each Session
        for (QHash<SessionID, Session *>::iterator s = m->sessionlist.begin(); s != m->sessionlist.end();
                s++) {
            Session *sess = s.value();

            if (!sess) { continue; }

            sess->OpenEvents();

            // For each EventList contained in session
            invalid.clear();
            f = 0;
            l = 0;

            for (QHash<ChannelID, QVector<EventList *> >::iterator e = sess->eventlist.begin();
                    e != sess->eventlist.end(); e++) {

                // Discard any non data events.
                if (!valid.contains(e.key())) {
                    // delete and push aside for later to clean up
                    for (int i = 0; i < e.value().size(); i++)  {
                        delete e.value()[i];
                    }

                    e.value().clear();
                    invalid.push_back(e.key());
                } else {
                    QVector<EventList *> newlist;

                    for (int i = 0; i < e.value().size(); i++)  {
                        if (e.value()[i]->count() > (unsigned)discard_threshold) {
                            newlist.push_back(e.value()[i]);
                        } else {
                            delete e.value()[i];
                        }
                    }

                    for (int i = 0; i < newlist.size(); i++) {
                        EventList *el = newlist[i];

                        if (!f || f > el->first()) { f = el->first(); }

                        if (!l || l < el->last()) { l = el->last(); }
                    }

                    e.value() = newlist;
                }
            }

            for (int i = 0; i < invalid.size(); i++) {
                sess->eventlist.erase(sess->eventlist.find(invalid[i]));
            }

            if (f) { sess->really_set_first(f); }

            if (l) { sess->really_set_last(l); }

            sess->m_cnt.clear();
            sess->m_sum.clear();
            sess->m_min.clear();
            sess->m_max.clear();
            sess->m_cph.clear();
            sess->m_sph.clear();
            sess->m_avg.clear();
            sess->m_wavg.clear();
            sess->m_valuesummary.clear();
            sess->m_timesummary.clear();
            sess->m_firstchan.clear();
            sess->m_lastchan.clear();
            sess->SetChanged(true);
        }

    }

    for (int i = 0; i < machines.size(); i++) {
        Machine *m = machines[i];
        m->Save();
        m->SaveSummaryCache();
    }

    daily->LoadDate(daily->getDate());
    overview->ReloadGraphs();
}
void MainWindow::reloadProfile()
{
    QString username = p_profile->user->userName();
    int tabidx = ui->tabWidget->currentIndex();
    CloseProfile();
    OpenProfile(username);
    ui->tabWidget->setCurrentIndex(tabidx);
}

void MainWindow::RestartApplication(bool force_login, QString cmdline)
{
    qDebug() << "Restarting OSCAR";
    CloseProfile();
    p_pref->Save();

    QString apppath;
#ifdef Q_OS_MAC
    // In Mac OS the full path of aplication binary is:
    //    <base-path>/myApp.app/Contents/MacOS/myApp

    // prune the extra bits to just get the app bundle path
    apppath = QApplication::instance()->applicationDirPath().section("/", 0, -3);

    QStringList args;
    args << "-n"; // -n option is important, as it opens a new process
    args << apppath;

    args << "--args"; // OSCAR binary options after this
    args << "-p"; // -p starts with 1 second delay, to give this process time to save..


    if (force_login) { args << "-l"; }

    args << cmdline;

    if (QProcess::startDetached("/usr/bin/open", args)) {
        QApplication::instance()->exit();
    } else {
        staticQMessageBox::warning(this, STR_MessageBox_Error,
            tr("If you can read this, the restart command didn't work. You will have to do it yourself manually."), QMessageBox::Ok);
    }

#else
    apppath = QApplication::instance()->applicationFilePath();

    // If this doesn't work on windoze, try uncommenting this method
    // Technically should be the same thing..

    //if (QDesktopServices::openUrl(apppath)) {
    //    QApplication::instance()->exit();
    //} else
    QStringList args;
    args << "-p";

    if (force_login) { args << "-l"; }

    args << cmdline;

    //if (change_datafolder) { args << "-d"; }

    if (QProcess::startDetached(apppath, args)) {
        QApplication::instance()->exit();

//        ::exit(0);
    } else {
        staticQMessageBox::warning(this,  STR_MessageBox_Error,
            tr("If you can read this, the restart command didn't work. You will have to do it yourself manually."), QMessageBox::Ok);
    }

#endif
}

void MainWindow::on_actionPurge_Current_Day_triggered()
{
    this->purgeDay(MT_CPAP);
}

void MainWindow::on_actionPurgeCurrentDayOximetry_triggered()
{
    this->purgeDay(MT_OXIMETER);
}

void MainWindow::on_actionPurgeCurrentDaySleepStage_triggered()
{
    this->purgeDay(MT_SLEEPSTAGE);
}

void MainWindow::on_actionPurgeCurrentDayPosition_triggered()
{
    this->purgeDay(MT_POSITION);
}

void MainWindow::on_actionPurgeCurrentDayAllExceptNotes_triggered()
{
    this->purgeDay(MT_UNKNOWN);
}

void MainWindow::on_actionPurgeCurrentDayAll_triggered()
{
    this->purgeDay(MT_JOURNAL);
}

// Purge data for a given device type.
// Special handling: MT_JOURNAL == All data. MT_UNKNOWN == All except journal
void MainWindow::purgeDay(MachineType type)
{
    if (!daily)
        return;
    QDate date = daily->getDate();
    qDebug() << "Purging data from" << date;
    daily->Unload(date);
    Day *day = p_profile->GetDay(date, MT_UNKNOWN);
    Machine *cpap = nullptr;
    if (!day)
        return;

    QList<Session *>::iterator s;

    QList<Session *> list;
    for (s = day->begin(); s != day->end(); ++s) {
        Session *sess = *s;
        if (type == MT_JOURNAL || (type == MT_UNKNOWN && sess->type() != MT_JOURNAL) ||
                sess->type() == type) {
            list.append(*s);
            qDebug() << "Purging session from " << (*s)->machine()->loaderName() << " ID:" << (*s)->session() << "["+QDateTime::fromSecsSinceEpoch((*s)->session()).toString()+"]";
            qDebug() << "First Time:" << QDateTime::fromMSecsSinceEpoch((*s)->realFirst()).toString();
            qDebug() << "Last Time:" << QDateTime::fromMSecsSinceEpoch((*s)->realLast()).toString();
            if (sess->type() == MT_CPAP) {
                cpap = day->machine(MT_CPAP);
            }
        } else {
            qDebug() << "Skipping session from " << (*s)->machine()->loaderName() << " ID:" << (*s)->session() << "["+QDateTime::fromSecsSinceEpoch((*s)->session()).toString()+"]";
        }
    }

    if (list.size() > 0) {
        if (cpap) {
            QFile rxcache(p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" ));
            rxcache.remove();

            QFile sumfile(cpap->getDataPath()+"Summaries.xml.gz");
            sumfile.remove();
        }

//        m->day.erase(m->day.find(date));
        QSet<Machine *> machines;
        for (int i = 0; i < list.size(); i++) {
            Session *sess = list.at(i);
            machines += sess->machine();
            sess->Destroy();    // remove the summary and event files
            delete sess;
        }

        for (auto & mach : machines) {
            mach->SaveSummaryCache();
        }

        if (cpap) {
            // save purge date where later import should start
            QDate pd = cpap->purgeDate();
            if (pd.isNull() || day->date() < pd)
                cpap->setPurgeDate(day->date());
        }
    } else {
        // No data purged... could notify user?
        return;
    }
    day = p_profile->GetDay(date, MT_UNKNOWN);
    Q_UNUSED(day);

    daily->clearLastDay();
    daily->LoadDate(date);
    if (overview)
        overview->ReloadGraphs();
    if (welcome)
        welcome->refreshPage();
    GenerateStatistics();
}

void MainWindow::on_actionRebuildCPAP(QAction *action)
{
    ui->tabWidget->setCurrentWidget(welcome); // Daily view can't run during rebuild
    QApplication::processEvents();

    QString data = action->data().toString();
    QString cls = data.section(":",0,0);
    QString serial = data.section(":", 1);
    QList<Machine *> machines = p_profile->GetMachines(MT_CPAP);
    Machine * mach = nullptr;
    for (int i=0; i < machines.size(); ++i) {
        Machine * m = machines.at(i);
        if ((m->loaderName() == cls) && (m->serial() == serial)) {
            mach = m;
            break;
        }
    }
    if (!mach) return;
    QString bpath = mach->getBackupPath();
    bool backups = (dirCount(bpath) > 0) ? true : false;

    if (backups) {
        if (staticQMessageBox::question(this, STR_MessageBox_Question,
                tr("Are you sure you want to rebuild all CPAP data for the following device:\n\n") +
                mach->brand() + " " + mach->model() + " " +
                mach->modelnumber() + " (" + mach->serial() + ")\n\n" +
                tr("Please note, that this could result in loss of data if OSCAR's backups have been disabled."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
            return;
        }
    } else {
        if (staticQMessageBox::question(this,
                STR_MessageBox_Warning,
                "<p><b>"+STR_MessageBox_Warning+": </b>"+tr("For some reason, OSCAR does not have any backups for the following device:")+ "</p>" +
                "<p>"+mach->brand() + " " + mach->model() + " " + mach->modelnumber() + " (" + mach->serial() + ")</p>"+
                "<p>"+tr("Provided you have made <i>your <b>own</b> backups for ALL of your CPAP data</i>, you can still complete this operation, but you will have to restore from your backups manually.")+"</p>" +
                "<p><b>"+tr("Are you really sure you want to do this?")+"</b></p>",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
            return;
        }
    }

    QString path = mach->getBackupPath();
    MachineLoader *loader = lookupLoader(mach);

    purgeMachine(mach); // purge destroys device record

    if (backups) {
        importCPAP(ImportPath(path, loader), tr("Please wait, importing from backup folder(s)..."));
    } else {
        if (staticQMessageBox::information(this, STR_MessageBox_Warning,
                tr("Because there are no internal backups to rebuild from, you will have to restore from your own.")+"\n\n"+
                tr("Would you like to import from your own backups now? (you will have no data visible for this device until you do)"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
            on_action_Import_Data_triggered();
        } else {
        }
    }
    if (overview)
        overview->ReloadGraphs();
    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        daily->ReloadGraphs();
    }
    if (welcome)
        welcome->refreshPage();
    PopulatePurgeMenu();
    GenerateStatistics();
    p_profile->StoreMachines();
}

void MainWindow::on_actionPurgeMachine(QAction *action)
{
    QString data = action->data().toString();
    QString cls = data.section(":",0,0);
    QString serial = data.section(":", 1);
    QList<Machine *> machines = p_profile->GetMachines();
    Machine * mach = nullptr;
    for (int i=0; i < machines.size(); ++i) {
        Machine * m = machines.at(i);
        if ((m->loaderName() == cls) && (m->serial() == serial)) {
            mach = m;
            break;
        }
    }
    if (!mach) return;

    QString machname = mach->brand();
    if (machname.isEmpty()) {
        machname = mach->loaderName();
    }
    machname += " " + mach->model() + " " + mach->modelnumber();
    if (!mach->serial().isEmpty()) {
        machname += QString(" (%1)").arg(mach->serial());
    }

    QString backupnotice;
    QString bpath = mach->getBackupPath();
    bool backups = (dirCount(bpath) > 0) ? true : false;
    if (backups) {
        backupnotice = "<p>" + tr("Note as a precaution, the backup folder will be left in place.") + "</p>";
    } else {
        backupnotice = "<p>" + tr("OSCAR does not have any backups for this device!") + "</p>" +
                       "<p>" + tr("Unless you have made <i>your <b>own</b> backups for ALL of your data for this device</i>, "
                                  "<font size=+2>you will lose this device's data <b>permanently</b>!</font>") + "</p>";
    }

    if (staticQMessageBox::question(this, STR_MessageBox_Warning,
            "<p><b>"+STR_MessageBox_Warning+":</b> "  +
            tr("You are about to <font size=+2>obliterate</font> OSCAR's device database for the following device:</p>") +
            "<p><font size=+2>" + machname + "</font></p>" +
            backupnotice+
            "<p>"+tr("Are you <b>absolutely sure</b> you want to proceed?")+"</p>",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
        qDebug() << "Purging" << machname;
        purgeMachine(mach);
    }

}

void MainWindow::purgeMachine(Machine * mach)
{
    // detect backups
    daily->Unload(daily->getDate());

    // Technicially the above won't sessions under short session limit.. Using Purge to clean up the rest.
    if (mach->Purge(3478216)) {
        mach->sessionlist.clear();
        mach->day.clear();
        QDir dir;
        QString path = mach->getDataPath();
        path.chop(1);
        qDebug() << "path to device" << path;

        p_profile->DelMachine(mach);
        delete mach;
        // remove the directory unless it's got unexpected crap in it..
        bool deleted = false;
        if ( ! dir.rmdir(path)) {
#ifdef Q_OS_WIN
            wchar_t* directoryPtr = (wchar_t*)path.utf16();
            SetFileAttributes(directoryPtr, GetFileAttributes(directoryPtr) & ~FILE_ATTRIBUTE_READONLY);
            if (!::RemoveDirectory(directoryPtr)) {
               DWORD lastError = ::GetLastError();
               qDebug() << "RemoveDirectory" << path << "GetLastError: " << lastError << "(Error 145 is expected)";
               if (lastError == 145) {
                   qDebug() << path << "remaining directory contents are" << QDir(path).entryList();
               }

            } else {
               qDebug() << "Success on second attempt deleting folder with windows API " << path;
               deleted = true;
            }
#else
            qWarning() << "Couldn't remove directory" << path;
#endif
        } else {
            deleted = true;
        }
        if ( ! deleted) {
            qWarning() << "Leaving backup folder intact";
        }

        PopulatePurgeMenu();
        p_profile->StoreMachines();
    } else {
        staticQMessageBox::warning(this, STR_MessageBox_Error,
            tr("A file permission error caused the purge process to fail; you will have to delete the following folder manually:") +
            "\n\n" + QDir::toNativeSeparators(mach->getDataPath()), QMessageBox::Ok, QMessageBox::Ok);

        if (overview)
            overview->ReloadGraphs();

        if (daily) {
            daily->clearLastDay(); // otherwise Daily will crash
            daily->ReloadGraphs();
        }
        if (welcome)
            welcome->refreshPage();

        //GenerateStatistics();
        return;
    }


    if (overview)
        overview->ReloadGraphs();
    QFile rxcache(p_profile->Get("{" + STR_GEN_DataFolder + "}/RXChanges.cache" ));
    rxcache.remove();

    if (daily) {
        daily->clearLastDay(); // otherwise Daily will crash
        daily->ReloadGraphs();
    }
    if (welcome)
        welcome->refreshPage();

    QApplication::processEvents();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    //qDebug() << "Keypress:" << event->key();
}

void MainWindow::on_action_Sidebar_Toggle_toggled(bool visible)
{
    ui->toolBox->setVisible(visible);
    AppSetting->setRightSidebarVisible(visible);
}

void MainWindow::on_helpButton_clicked()
{
#ifndef helpless
    ui->tabWidget->setCurrentWidget(help);
#else
    staticQMessageBox::information(this, STR_MessageBox_Error, tr("No help is available."));
#endif
}

void MainWindow::on_actionView_Statistics_triggered()
{
    ui->tabWidget->setCurrentWidget(ui->statisticsTab);
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index);
   // QWidget *widget = ui->tabWidget->currentWidget();
}


void MainWindow::on_filterBookmarks_editingFinished()
{
    bookmarkFilter = ui->filterBookmarks->text();
    updateFavourites();
}

void MainWindow::on_filterBookmarksButton_clicked()
{
    if (!bookmarkFilter.isEmpty()) {
        ui->filterBookmarks->setText("");
        bookmarkFilter = "";
        updateFavourites();
    }
}

void MainWindow::recompressEvents()
{
    QTimer::singleShot(0, this, SLOT(doRecompressEvents()));
}
void MainWindow::reprocessEvents(bool restart)
{
    m_restartRequired = restart;
    QTimer::singleShot(0, this, SLOT(doReprocessEvents()));
}


void MainWindow::FreeSessions()
{
    QDate first = p_profile->FirstDay();
    QDate date = p_profile->LastDay();
    Day *day;
    QDate current = daily->getDate();

    do {
        day = p_profile->GetDay(date, MT_CPAP);

        if (day) {
            if (date != current) {
                day->CloseEvents();
            }
        }

        date = date.addDays(-1);
    }  while (date >= first);
}

void MainWindow::doRecompressEvents()
{
    if (!p_profile) return;
    ProgressDialog progress(this);
    progress.setMessage(QObject::tr("Recompressing Session Files"));
    progress.setProgressMax(p_profile->daylist.size());
    QPixmap icon = QPixmap(":/icons/logo-md.png").scaled(64,64);
    progress.setPixmap(icon);
    progress.open();

    bool isopen;
    int idx = 0;
    for (Day * day : p_profile->daylist) {
        for (Session * sess : day->sessions) {
            isopen = sess->eventsLoaded();
            // Load the events and summary if they aren't loaded already
            sess->LoadSummary();
            sess->OpenEvents();
            sess->SetChanged(true);
            sess->machine()->SaveSession(sess);

            if (!isopen) {
                sess->TrashEvents();
            }
        }
        progress.setProgressValue(++idx);
        QApplication::processEvents();
    }
    progress.close();
}
void MainWindow::doReprocessEvents()
{
    if (!p_profile) return;

    ProgressDialog progress(this);
    progress.setMessage("Recalculating summaries");
    progress.setProgressMax(p_profile->daylist.size());
    QPixmap icon = QPixmap(":/icons/logo-md.png").scaled(64,64);
    progress.setPixmap(icon);
    progress.open();

    if (daily) {
        daily->Unload();
        daily->clearLastDay(); // otherwise Daily will crash
        delete daily;
        daily = nullptr;
    }
    if (welcome) {
        delete welcome;
        welcome = nullptr;
    }
    if (overview) {
        delete overview;
        overview = nullptr;
    }

    for (Day * day : p_profile->daylist) {
        for (Session * sess : day->sessions) {
            bool isopen = sess->eventsLoaded();

            // Load the events if they aren't loaded already
            sess->LoadSummary();
            sess->OpenEvents();

            // Destroy any current user flags..
            sess->destroyEvent(CPAP_UserFlag1);
            sess->destroyEvent(CPAP_UserFlag2);
            sess->destroyEvent(CPAP_UserFlag3);

            // AHI flags
            sess->destroyEvent(CPAP_AHI);
            sess->destroyEvent(CPAP_RDI);

            if (sess->machine()->loaderName() != STR_MACH_PRS1) {
                sess->destroyEvent(CPAP_LargeLeak);
            } else {
                sess->destroyEvent(CPAP_Leak);
            }

            sess->SetChanged(true);

            sess->UpdateSummaries();
            sess->machine()->SaveSession(sess);

            if (!isopen) {
                sess->TrashEvents();
            }
        }
    }
    progress.close();

    welcome = new Welcome(ui->tabWidget);
    ui->tabWidget->insertTab(1, welcome, tr("Welcome"));

    daily = new Daily(ui->tabWidget, nullptr);
    ui->tabWidget->insertTab(2, daily, STR_TR_Daily);
    daily->ReloadGraphs();

    overview = new Overview(ui->tabWidget, daily->graphView());
    ui->tabWidget->insertTab(3, overview, STR_TR_Overview);
    overview->ReloadGraphs();

    // Should really create welcome and statistics here, but they need redoing later anyway to kill off webkit
    ui->tabWidget->setCurrentIndex(AppSetting->openTabAtStart());
    GenerateStatistics();
    PopulatePurgeMenu();

}

void MainWindow::on_actionImport_ZEO_Data_triggered()
{
    ZEOLoader zeo;
    importNonCPAP(zeo);
}

void MainWindow::on_actionImport_Dreem_Data_triggered()
{
    DreemLoader dreem;
    importNonCPAP(dreem);
}

void MainWindow::on_actionImport_RemStar_MSeries_Data_triggered()
{
#ifdef REMSTAR_M_SUPPORT
    QFileDialog w;
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setOption(QFileDialog::ShowDirsOnly, false);
    w.setOption(QFileDialog::DontUseNativeDialog, true);
    w.setNameFilters(QStringList("M-Series data file (*.bin)"));

    MSeriesLoader mseries;

    if (w.exec() == QFileDialog::Accepted) {
        QString filename = w.selectedFiles()[0];

        if (!mseries.Open(filename, p_profile)) {
            Notify(tr("There was a problem opening MSeries block File: ") + filename);
            return;
        }

        Notify(tr("MSeries Import complete"));
        daily->LoadDate(daily->getDate());
    }

#endif
}

void MainWindow::on_actionSleep_Disorder_Terms_Glossary_triggered()
{
    if (staticQMessageBox::question(this, STR_MessageBox_Question, tr("The Glossary will open in your default browser"),
            QMessageBox::Ok|QMessageBox::Cancel, QMessageBox::Ok) == QMessageBox::Ok )
        QDesktopServices::openUrl(QUrl("https://www.apneaboard.com/wiki/index.php?title=Definitions"));
}

/*
void MainWindow::on_actionHelp_Support_OSCAR_Development_triggered()
{
    staticQMessageBox::information(this, STR_MessageBox_Information, tr("Donations are not implemented"));
}
*/

void MainWindow::on_actionChange_Language_triggered()
{
//    if (staticQMessageBox::question(this,STR_MessageBox_Warning,tr("Changing the language will reset custom Event and Waveform names/labels/descriptions.")+"\n\n"+tr("Are you sure you want to do this?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
//        return;
//    }

    RestartApplication(true, "--language");
}

void MainWindow::on_actionChange_Data_Folder_triggered()
{
    if (p_profile) {
        p_profile->Save();
        p_profile->removeLock();
    }
    p_pref->Save();

    RestartApplication(false, "-d");
}

QString MainWindow::profilePath(QString folderProfileName ) {
    QString folderName;
    if (p_profile->contains(folderProfileName)) {
        folderName = (*p_profile)[folderProfileName].toString();
        QFileInfo fi(folderName);
        if (fi.exists() && fi.isDir()) {
            return folderName;
        }
    }
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    if (paths.size()>1) folderName = paths[0];
    return folderName;
}

void MainWindow::saveProfilePath(QString folderProfileName , QString pathName) {
        (*p_profile)[folderProfileName] = pathName;
}

void MainWindow::importNonCPAP(MachineLoader &loader)
{
    // get save location from profile.
    QDir folder = QDir(profilePath(STR_PREF_LastOximetryPath));

    QFileDialog w;

    if (folder.exists() ) {
        w.setDirectory(folder);
    }
    w.setFileMode(QFileDialog::ExistingFiles);
    w.setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    w.setOption(QFileDialog::ShowDirsOnly, false);
#if defined(Q_OS_WIN)
    // Windows can't handle Viatom name filter - use non-native for all non-CPAP loaders.
    w.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    w.setNameFilters(loader.getNameFilter());

    // Display progress if we have more than 1 file to load...
    ProgressDialog progress(this);

    if (w.exec() == QFileDialog::Accepted) {
        saveProfilePath(STR_PREF_LastOximetryPath,w.directory().absolutePath());
        QStringList files = w.selectedFiles();
        int size = files.size();
        if (size > 1) {
            progress.setMessage(QObject::tr("Importing Sessions..."));
            progress.setProgressMax(size);
            progress.setProgressValue(0);
            progress.addAbortButton();
            progress.setWindowModality(Qt::ApplicationModal);
            connect(&loader, SIGNAL(setProgressValue(int)), &progress, SLOT(setProgressValue(int)));
            connect(&progress, SIGNAL(abortClicked()), &loader, SLOT(abortImport()));
            progress.open();
            QCoreApplication::processEvents();
        }
        QString name = loader.loaderName();

        ImportUI importui(p_profile);
        ImportContext* ctx = new ProfileImportContext(p_profile);
        loader.SetContext(ctx);
        connect(ctx, &ImportContext::importEncounteredUnexpectedData, &importui, &ImportUI::onUnexpectedData);
        connect(&loader, &MachineLoader::deviceReportsUsageOnly, &importui, &ImportUI::onDeviceReportsUsageOnly);
        connect(&loader, &MachineLoader::deviceIsUntested, &importui, &ImportUI::onDeviceIsUntested);
        connect(&loader, &MachineLoader::deviceIsUnsupported, &importui, &ImportUI::onDeviceIsUnsupported);

        int res = loader.Open(files);

        progress.setMessage(QObject::tr("Finishing up..."));
        QCoreApplication::processEvents();
        ctx->Commit();

        loader.SetContext(nullptr);
        delete ctx;

        if (size > 1) {
            disconnect(&loader, SIGNAL(setProgressValue(int)), &progress, SLOT(setProgressValue(int)));
            disconnect(&progress, SIGNAL(abortClicked()), &loader, SLOT(abortImport()));
            progress.close();
            QCoreApplication::processEvents();
        }
        if (res < 0) {
            // res is used as an index to an array and will cause a crash if not handled.
            // Negative numbers indicate a problem with the file format or the file does not exist.
            //QString fileName = QFileInfo(files[0]).fileName();
            QString msg = QString(tr("There was a problem parsing %1 \nData File: %2")
                .arg(name, QFileInfo( files[0]).fileName() ) );
            //QString msg = QString(tr("There was a problem parsing %1 \nData File: %2") .arg(name, fileName) );
            Notify(msg,"",20*1000 /* convert sec to ms */);
        } else
        if (res == 0) {
            Notify(tr("There was a problem opening %1 Data File: %2").arg(name, files[0]));
            return;
        } else if (res < size){
            Notify(tr("%1 Data Import of %2 file(s) complete").arg(name).arg(res) + "\n\n" +
                   tr("There was a problem opening %1 Data File: %2").arg(name, files[res]),
                   tr("%1 Import Partial Success").arg(name));
        } else {
            Notify(tr("%1 Data Import complete").arg(name));
        }
        PopulatePurgeMenu();
        if (overview) overview->ReloadGraphs();
        if (welcome) welcome->refreshPage();
        daily->LoadDate(daily->getDate());
    }
}

void MainWindow::on_actionImport_Somnopose_Data_triggered()
{
    SomnoposeLoader somno;
    importNonCPAP(somno);
}

void MainWindow::on_actionImport_Viatom_Data_triggered()
{
    ViatomLoader viatom;
    importNonCPAP(viatom);
}

void MainWindow::GenerateStatistics()
{
    reset_reportModeUi() ;
    if (!p_profile) return;

    Statistics stats;
    QString htmlStats = stats.GenerateHTML();
    QString htmlRecords = stats.UpdateRecordsBox();

    updateFavourites();

    setStatsHTML(htmlStats);
    setRecBoxHTML(htmlRecords);

}


void MainWindow::JumpDaily()
{
    qDebug() << "Set current Widget to Daily";
//    sleep(3);
    ui->tabWidget->setCurrentWidget(daily);
}
void MainWindow::JumpOverview()
{
    ui->tabWidget->setCurrentWidget(overview);
}
void MainWindow::JumpStatistics()
{
    ui->tabWidget->setCurrentWidget(ui->statisticsTab);
}
void MainWindow::JumpImport()
{
    on_importButton_clicked();
}
void MainWindow::JumpOxiWizard()
{
    on_oximetryButton_clicked();
}


void MainWindow::on_statisticsButton_clicked()
{
    if (p_profile) {
        ui->tabWidget->setCurrentWidget(ui->statisticsTab);
    }
}

void MainWindow::init_reportModeUi()
{
    QTextCharFormat format = ui->statStartDate->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(Qt::black, Qt::SolidPattern));
    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();
    ui->statStartDate->blockSignals(true);
    ui->statEndDate->blockSignals(true);
    ui->statStartDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->statStartDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->statStartDate->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    ui->statStartDate->calendarWidget()->setFirstDayOfWeek(dow);
    ui->statEndDate->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->statEndDate->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->statEndDate->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    ui->statEndDate->calendarWidget()->setFirstDayOfWeek(dow);
    ui->statStartDate->blockSignals(false);
    ui->statEndDate->blockSignals(false);
}

void MainWindow::reset_reportModeUi()
{
    Statistics::updateReportDate();
    int mode = STAT_MODE_STANDARD;
    ui->statStartDate->blockSignals(true);
    ui->statEndDate->blockSignals(true);
    if (p_profile) {
        mode = p_profile->general->statReportMode();

        QDate first = p_profile->FirstDay();
        QDate last = p_profile->LastDay();
        ui->statStartDate->setMinimumDate(first);
        ui->statStartDate->setMaximumDate(last);
        ui->statEndDate->setMinimumDate(first);
        ui->statEndDate->setMaximumDate(last);
    }
    switch (mode) {
        default:
        case STAT_MODE_STANDARD:
            {
            ui->reportModeStandard->setChecked(true);
            ui->statEndDate->setVisible(true);
            ui->statStartDate->setVisible(false);
            ui->statEnableEndDisplay->setVisible(true);
            if (p_profile) {
                ui->statEndDate->setDate(p_profile->general->statReportDate());
            }
            }
            break;
        case STAT_MODE_MONTHLY:
            ui->statStartDate->setVisible(false);
            ui->statEndDate->setVisible(true);
            ui->reportModeMonthly->setChecked(true);

            ui->statEnableEndDisplay->setVisible(true);
            if (p_profile) {
                ui->statEndDate->setDate(p_profile->general->statReportDate());
            }
            break;
        case STAT_MODE_RANGE:
            ui->reportModeRange->setChecked(true);
            ui->statStartDate->setVisible(true);
            ui->statEndDate->setVisible(true);
            ui->statEnableEndDisplay->setVisible(false);
            if (p_profile) {
                ui->statStartDate->setDate( p_profile->general->statReportRangeStart());
                ui->statEndDate->setDate( p_profile->general->statReportRangeEnd());
            }
            break;
    }
    ui->statStartDate->blockSignals(false);
    ui->statEndDate->blockSignals(false);
    return;
};

void MainWindow::on_reportModeMonthly_clicked()
{
    if (p_profile->general->statReportMode() != STAT_MODE_MONTHLY) {
        p_profile->general->setStatReportMode(STAT_MODE_MONTHLY);
        GenerateStatistics();
    }
}

void MainWindow::on_reportModeStandard_clicked()
{
    if (p_profile->general->statReportMode() != STAT_MODE_STANDARD) {
        p_profile->general->setStatReportMode(STAT_MODE_STANDARD);
        GenerateStatistics();
    }
}


void MainWindow::on_reportModeRange_clicked()
{
    if (p_profile->general->statReportMode() != STAT_MODE_RANGE) {
        p_profile->general->setStatReportMode(STAT_MODE_RANGE);
        GenerateStatistics();
    }
}

void MainWindow::on_statEndDate_dateChanged(const QDate &date)
{
    if (p_profile->general->statReportMode() == STAT_MODE_RANGE ) {
        p_profile->general->setStatReportRangeEnd(date);
    } else {
        p_profile->general->setStatReportDate(date);
    };
    GenerateStatistics();
}

void MainWindow::on_statStartDate_dateChanged(const QDate &date)
{
    p_profile->general->setStatReportRangeStart(date);
    GenerateStatistics();
}

void MainWindow::on_actionPurgeCurrentDaysOximetry_triggered()
{
    if (!daily)
        return;
    QDate date = daily->getDate();
    Day * day = p_profile->GetDay(date, MT_OXIMETER);
    if (day) {
        QLocale locale;
        if (staticQMessageBox::question(this, STR_MessageBox_Warning,
            tr("Are you sure you want to delete oximetry data for %1").
                arg(
                    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                        daily->getDate().toString(Qt::DefaultLocaleLongDate)
                    #else
                        locale.toString(QDate::currentDate(), QLocale::LongFormat)
                    #endif
                    ) +"<br/><br/>"
                +
            tr("<b>Please be aware you can not undo this operation!</b>"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {
            return;
        }

        QList<Session *> sessionlist=day->getSessions(MT_OXIMETER);
        QSet<Machine *> machines;

        for (auto & sess : sessionlist) {
            machines += sess->machine();
            sess->Destroy();
            delete sess;
        }

        // We have to update the summary cache for the affected device(s),
        // otherwise OSCAR will still think this day has oximetry data at next launch.
        for (auto & mach : machines) {
            mach->SaveSummaryCache();
        }


        if (daily) {
            daily->Unload(date);
            daily->clearLastDay(); // otherwise Daily will crash
            daily->ReloadGraphs();
        }
        if (overview) overview->ReloadGraphs();
        if (welcome) welcome->refreshPage();
    } else {
        staticQMessageBox::information(this, STR_MessageBox_Information,
            tr("Select the day with valid oximetry data in daily view first."),QMessageBox::Ok);
    }
}

void MainWindow::on_importButton_clicked()
{
    on_action_Import_Data_triggered();
}


void MainWindow::on_actionLine_Cursor_toggled(bool b)
{
    AppSetting->setLineCursorMode(b);
    if (ui->tabWidget->currentWidget() == daily) {
        daily->graphView()->timedRedraw(0);
    } else if (ui->tabWidget->currentWidget() == overview) {
        overview->graphView()->timedRedraw(0);
    }
}

void MainWindow::on_actionPie_Chart_toggled(bool visible)
{
    AppSetting->setShowPieChart(visible);
    if (!setupRunning && daily) {
        daily->updateLeftSidebar();
//        daily->ReloadGraphs();
    }
}

void MainWindow::on_actionLeft_Daily_Sidebar_toggled(bool visible)
{
    if (daily) daily->setSidebarVisible(visible);
}

void MainWindow::on_actionDaily_Calendar_toggled(bool visible)
{
    if (daily) daily->setCalendarVisible(visible);
}

void MainWindow::on_actionShowPersonalData_toggled(bool visible)
{
    // This uses the Prefs routines, which require p_profile to be set
    if ( p_profile != nullptr ) {
        AppSetting->setShowPersonalData(visible);
        if ( ! setupRunning )
            GenerateStatistics();
    } else {
       staticQMessageBox::information(this, "OSCAR", tr("You must select and open the profile you wish to modify"),
                    QMessageBox::Ok);
    }
}


void MainWindow::on_actionImport_Journal_triggered()
{

    QString filename = QFileDialog::getOpenFileName(this,
            tr("Choose where to read journal"),
            profilePath(STR_PREF_LastJournalPath),
            tr("XML Files (*.xml)"));
    if (Journal::RestoreJournal(filename) ) {
		QFileInfo fi(filename);
		QDir dir = fi.dir();
		if (dir.exists() ) {
            saveProfilePath(STR_PREF_LastJournalPath,dir.absolutePath());
		}
	}
}

void MainWindow::on_actionExport_Journal_triggered()
{
    QString folder;
    folder = profilePath(STR_PREF_LastJournalPath);
	if (p_profile->contains(STR_PREF_LastJournalPath)) {
		folder = (*p_profile)[STR_PREF_LastJournalPath].toString();
	}
    QFileInfo fi(folder);
	if (!fi.isDir() ) {
        folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    folder += QDir::separator() + tr("%1's Journal").arg(p_profile->user->userName()) + ".xml";

    QString filename = QFileDialog::getSaveFileName(this, tr("Choose where to save journal"), folder, tr("XML Files (*.xml)"));

    if (Journal::BackupJournal(filename) ) {
		QFileInfo fi(filename);
		QDir dir = fi.dir();
		if (dir.exists() ) {
			(*p_profile)[STR_PREF_LastJournalPath] = dir.absolutePath();
		}
	}
}

void MainWindow::on_actionShow_Performance_Counters_toggled(bool arg1)
{
    AppSetting->setShowPerformance(arg1);
}

void MainWindow::on_actionExport_CSV_triggered()
{
    ExportCSV ex(this);

    if (ex.exec() == ExportCSV::Accepted) {
    }
}

void MainWindow::on_actionExport_Review_triggered()
{
    staticQMessageBox::information(this, STR_MessageBox_Information, tr("Export review is not yet implemented"));
}

void MainWindow::on_mainsplitter_splitterMoved(int, int)
{
    AppSetting->setRightPanelWidth(ui->mainsplitter->sizes()[1]);
}

void MainWindow::on_actionCreate_Card_zip_triggered()
{
    QList<ImportPath> datacards = selectCPAPDataCards(tr("Would you like to zip this card?"), true);

    for (auto & datacard : datacards) {
        QString cardPath = QDir(datacard.path).canonicalPath();
        QString filename;
        QString prefix;

        // Loop until a valid folder is selected or the user cancels. Disallow the SD card itself!
        while (true) {
            // Note: macOS ignores this and points to OSCAR's most recently used directory for saving.
            QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

            MachineInfo info = datacard.loader->PeekInfo(datacard.path);
            QString infostr;
            if (!info.modelnumber.isEmpty()) {
                infostr = datacard.loader->loaderName() + "-" + info.modelnumber + "-" +info.serial;
            } else {
                infostr = datacard.loader->loaderName();
            }
            prefix = infostr;
            folder += "/" + infostr + ".zip";

            filename = QFileDialog::getSaveFileName(this, tr("Choose where to save zip"), folder, tr("ZIP files (*.zip)"));

            if (filename.isEmpty()) {
                return;  // aborted
            }

            // Try again if the selected filename is within the SD card itself.
            QString selectedPath = QFileInfo(filename).dir().canonicalPath();
            if (selectedPath.startsWith(cardPath)) {
                if (staticQMessageBox::warning(this, STR_MessageBox_Error,
                        QObject::tr("Please select a location for your zip other than the data card itself!"),
                        QMessageBox::Ok)) {
                    continue;
                }
            }

            break;
        }

        if (!filename.toLower().endsWith(".zip")) {
            filename += ".zip";
        }

        qDebug() << "Create zip of SD card:" << cardPath;

        ZipFile z;
        bool ok = z.Open(filename);
        if (ok) {
            ProgressDialog * prog = new ProgressDialog(this);

            // Very full cards can sometimes take nearly a minute to scan,
            // so display the progress dialog immediately.
            prog->setMessage(tr("Calculating size..."));
            prog->setWindowModality(Qt::ApplicationModal);
            prog->open();

            // Build the list of files.
            FileQueue files;
            bool ok = files.AddDirectory(cardPath, prefix);

            // If there were any unexpected errors scanning the media, add the debug log to the zip.
            if (!ok) {
                qWarning() << "Unexpected errors when scanning SD card, adding debug log to zip.";
                QString debugLog = logger->logFileName();
                files.AddFile(debugLog, prefix + "-debug.txt");
            }

            prog->setMessage(tr("Creating zip..."));

            // Create the zip.
            ok = z.AddFiles(files, prog);
            z.Close();
        } else {
            qWarning() << "Unable to open" << filename;
        }
        if (!ok) {
            staticQMessageBox::warning(this, STR_MessageBox_Error,
                QObject::tr("Unable to create zip!"),
                QMessageBox::Ok);
        }
    }
}


void MainWindow::on_actionCreate_Log_zip_triggered()
{
    QString folder;

    // Note: macOS ignores this and points to OSCAR's most recently used directory for saving.
    folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    folder += "/OSCAR-logs.zip";
    QString filename = QFileDialog::getSaveFileName(this, tr("Choose where to save zip"), folder, tr("ZIP files (*.zip)"));
    if (filename.isEmpty()) {
        return;  // aborted
    }
    if (!filename.toLower().endsWith(".zip")) {
        filename += ".zip";
    }

    qDebug() << "Create zip of OSCAR diagnostic logs:" << filename;

    ZipFile z;
    bool ok = z.Open(filename);
    if (ok) {
        ProgressDialog * prog = new ProgressDialog(this);
        prog->setMessage(tr("Creating zip..."));

        // Build the list of files.
        FileQueue files;
        files.AddDirectory(GetLogDir(), "logs");

        // Defer the current debug log to the end.
        QString debugLog = logger->logFileName();
        QString debugLogZipName;
        int exists = files.Remove(debugLog, &debugLogZipName);
        if (exists) {
            files.AddFile(debugLog, debugLogZipName);
        }

        // Create the zip.
        ok = z.AddFiles(files, prog);
        z.Close();
    } else {
        qWarning() << "Unable to open" << filename;
    }
    if (!ok) {
        staticQMessageBox::warning(this, STR_MessageBox_Error,
            QObject::tr("Unable to create zip!"),
            QMessageBox::Ok);
    }
}


void MainWindow::on_actionCreate_OSCAR_Data_zip_triggered()
{
    QString folder;

    // Note: macOS ignores this and points to OSCAR's most recently used directory for saving.
    folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    folder += "/" + STR_AppData + ".zip";

    QString filename = QFileDialog::getSaveFileName(this, tr("Choose where to save zip"), folder, tr("ZIP files (*.zip)"));

    if (filename.isEmpty()) {
        return;  // aborted
    }

    if (!filename.toLower().endsWith(".zip")) {
        filename += ".zip";
    }

    qDebug() << "Create zip of OSCAR data folder:" << filename;

    QDir oscarData(GetAppData());

    ZipFile z;
    bool ok = z.Open(filename);
    if (ok) {
        ProgressDialog * prog = new ProgressDialog(this);
        prog->setMessage(tr("Calculating size..."));
        prog->setWindowModality(Qt::ApplicationModal);
        prog->open();

        // Build the list of files.
        FileQueue files;
        files.AddDirectory(oscarData.canonicalPath(), oscarData.dirName());

        // Defer the current debug log to the end.
        QString debugLog = logger->logFileName();
        QString debugLogZipName;
        int exists = files.Remove(debugLog, &debugLogZipName);
        if (exists) {
            files.AddFile(debugLog, debugLogZipName);
        }

        prog->setMessage(tr("Creating zip..."));

        // Create the zip.
        ok = z.AddFiles(files, prog);
        z.Close();
    } else {
        qWarning() << "Unable to open" << filename;
    }
    if (!ok) {
        staticQMessageBox::warning(this, STR_MessageBox_Error,
            QObject::tr("Unable to create zip!"),
            QMessageBox::Ok);
    }
}

#include "translation.h"
void MainWindow::on_actionReport_a_Bug_triggered()
{
//    QSettings settings;
//    QString language = settings.value(LangSetting).toString();
//
    staticQMessageBox::information(this, STR_MessageBox_Error, tr("Reporting issues is not yet implemented"));
}

void MainWindow::on_actionSystem_Information_triggered()
{
    QString text = ""; // tr("OSCAR version:") + "<br/>";
    QStringList info = getBuildInfo();
    for (int i = 0; i < info.size(); ++i) {
        text += info.at(i) + "<br/>";
    }
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // qt 6 vertically truncates last line in messagebox.
        text += ".<br/>";
    #endif
    staticQMessageBox::information(this, tr("OSCAR Information"), text);
}

void MainWindow::on_profilesButton_clicked()
{
    ui->tabWidget->setCurrentWidget(profileSelector);
}

void MainWindow::on_bookmarkView_anchorClicked(const QUrl &arg1)
{
    on_recordsBox_anchorClicked(arg1);
}

void MainWindow::on_recordsBox_anchorClicked(const QUrl &linkurl)
{
    QString link = linkurl.toString().section("=", 0, 0).toLower();
    QString data = linkurl.toString().section("=", 1).toLower();
    qDebug() << linkurl.toString() << link << data;

    if (link == "daily") {
        QDate date = QDate::fromString(data, Qt::ISODate);
        ui->tabWidget->setCurrentWidget(daily);
        QApplication::processEvents();
        daily->LoadDate(date);
    } else if (link == "overview") {
        QString date1 = data.section(",", 0, 0);
        QString date2 = data.section(",", 1);

        QDate d1 = QDate::fromString(date1, Qt::ISODate);
        QDate d2 = QDate::fromString(date2, Qt::ISODate);
        overview->setRange(d1, d2);
        ui->tabWidget->setCurrentWidget(overview);
    } else if (link == "import") {
        // Don't run the importer directly from here because it destroys the object that called this function..
        // Schedule it instead
        if (data == "cpap") QTimer::singleShot(0, mainwin, SLOT(on_importButton_clicked()));
        if (data == "oximeter") QTimer::singleShot(0, mainwin, SLOT(on_oximetryButton_clicked()));
    } else if (link == "statistics") {
        ui->tabWidget->setCurrentWidget(ui->statisticsTab);
    }
}

void MainWindow::on_statisticsView_anchorClicked(const QUrl &url)
{
    on_recordsBox_anchorClicked(url);
}
