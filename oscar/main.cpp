/* OSCAR Main
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>
#include <QtGlobal>

#ifdef UNITTEST_MODE
#include "tests/AutoTest.h"
#endif

#include <QApplication>
#include <QGuiApplication>
#include <QMessageBox>
#include <QDebug>
#include <QTranslator>
#include <QSettings>
#include <QFileDialog>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QProgressDialog>

#include "version.h"
#include "logger.h"
#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "translation.h"
#include "speedcheck.h"
#include "SleepLib/common.h"
#include "SleepLib/deviceconnection.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "highresolution.h"
#endif

#include <ctime>
#include <chrono>

// Gah! I must add the real darn plugin system one day.
#include "SleepLib/loader_plugins/prs1_loader.h"
#include "SleepLib/loader_plugins/cms50_loader.h"
#include "SleepLib/loader_plugins/cms50f37_loader.h"
#include "SleepLib/loader_plugins/md300w1_loader.h"
#include "SleepLib/loader_plugins/zeo_loader.h"
#include "SleepLib/loader_plugins/somnopose_loader.h"
#include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/intellipap_loader.h"
#include "SleepLib/loader_plugins/icon_loader.h"
#include "SleepLib/loader_plugins/sleepstyle_loader.h"
#include "SleepLib/loader_plugins/weinmann_loader.h"
#include "SleepLib/loader_plugins/viatom_loader.h"
#include "SleepLib/loader_plugins/prisma_loader.h"
#include "SleepLib/loader_plugins/resvent_loader.h"
#include "SleepLib/loader_plugins/vrem_loader.h"
#include "SleepLib/loader_plugins/bmc_loader.h"

MainWindow *mainwin = nullptr;
extern bool openOk;

int numFilesCopied = 0;

// Count the number of files in this directory and all subdirectories
int countRecursively(QString sourceFolder) {
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return 0;

    int numFiles = sourceDir.count();

    QStringList dirs = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< dirs.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + dirs[i];
        numFiles += countRecursively(srcName);
    }

    return numFiles;
}

bool copyRecursively(QString sourceFolder, QString destFolder, QProgressDialog& progress) {
    bool success = false;
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return false;

    QDir destDir(destFolder);
    if(!destDir.exists())
        destDir.mkdir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files);
    for(int i = 0; i< files.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        success = QFile::copy(srcName, destName);
        numFilesCopied++;
        if ((numFilesCopied % 20) == 1) { // Update progress bar every 20 files
            progress.setValue(numFilesCopied);
            QCoreApplication::processEvents();
        }
        if(!success) {
            qWarning() << "copyRecursively: Unable to copy" << srcName << "to" << destName;
            return false;
        }
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< files.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
//      qDebug() << "Copy from "+srcName+" to "+destName;
        success = copyRecursively(srcName, destName, progress);
        if(!success)
            return false;
    }

    return true;
}

bool processPreferenceFile( QString path ) {
    bool success = true;
    QString fullpath = path + "/Preferences.xml";
    qDebug() << "Process " + fullpath;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    openOk = fl.open(QIODevice::ReadOnly);
    openOk = tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    bool isSleepyHead = false;
    while (instr.readLineInto(&line)) {
        if (line.contains("<SleepyHead>"))  // Is this SleepyHead or OSCAR preferences file?
            isSleepyHead = true;
        line.replace("SleepyHead","OSCAR");
        if (isSleepyHead && line.contains("VersionString")) {
            int rtAngle = line.indexOf(">", 0);
            int lfAngle = line.indexOf("<", rtAngle);
            line.replace(rtAngle+1, lfAngle-rtAngle-1, "1.0.0-beta");
        }
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool processFile( QString fullpath ) {
    bool success = true;
    qDebug() << "Process " + fullpath ;
    QFile fl(fullpath);
    QFile tmp(fullpath+".tmp");
    QString line;
    openOk = fl.open(QIODevice::ReadOnly);
    openOk = tmp.open(QIODevice::WriteOnly);
    QTextStream instr(&fl);
    QTextStream outstr(&tmp);
    while (instr.readLineInto(&line)) {
        if (line.contains("EnableMultithreading")) {
            if (line.contains("true")) {
                line.replace("true","false");
            }
        }
        line.replace("SleepyHead","OSCAR");
        outstr << line;
    }
    fl.remove();
    success = tmp.rename(fullpath);

    return success;
}

bool process_a_Profile( QString path ) {
    bool success = true;
    qDebug() << "Entering profile directory " + path;
    QDir dir(path);
    QStringList files = dir.entryList(QStringList("*.xml"), QDir::Files);
    for ( int i = 0; success && (i<files.count()); i++) {
        success = processFile( path + "/" + files[i] );
    }
    return success;
}

bool migrateFromSH(QString destDir) {
    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    QString datadir;
    bool selectingFolder = true;
    bool success = false;
//  long int startTime, countDone, allDone;

    if (destDir.isEmpty()) {
        qDebug() << "Migration path is empty string";
        return success;
    }

    while (selectingFolder) {
        datadir = QFileDialog::getExistingDirectory(nullptr,
                  QObject::tr("Choose the SleepyHead or OSCAR data folder to migrate")+" "+
                  QObject::tr("or CANCEL to skip migration."),
                  homeDocs, QFileDialog::ShowDirsOnly);
        qDebug() << "Migration folder selected: " + datadir;
        if (datadir.isEmpty()) {
            qDebug() << "No migration source directory selected";
            return false;
        } else {                        // We have a folder, see if is a SleepyHead folder
            QDir dir(datadir);
            QFile file(datadir + "/Preferences.xml");
            QDir  dirP(datadir + "/Profiles");

            if (!file.exists() || !dirP.exists()) {       // It doesn't have a Preferences.xml file or a Profiles directory in it
                // Not a new directory.. nag the user.
                QMessageBox::warning(nullptr, STR_MessageBox_Error,
                                     QObject::tr("The folder you chose does not contain valid SleepyHead or OSCAR data.") +
                                     "\n\n"+QObject::tr("You cannot use this folder:")+" " + datadir,
                                     QMessageBox::Ok);
                continue;   // Nope, don't use it, go around the loop again
            }

            qDebug() << "Migration folder is" << datadir;
            selectingFolder = false;
        }
    }

    auto startTime = std::chrono::steady_clock::now();
    int numFiles = countRecursively(datadir);       // count number of files to be copied
    auto countDone = std::chrono::steady_clock::now();
    qDebug() << "Number of files to migrate: " << numFiles;

    QProgressDialog progress (QObject::tr("Migrating ") + QString::number(numFiles) + QObject::tr(" files")+"\n"+
    QObject::tr("from ") + QDir(datadir).dirName() + "\n"+QObject::tr("to ") +
    QDir(destDir).dirName(), QString(), 0, numFiles, 0, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    progress.setValue(0);
    progress.setMinimumWidth(300);
    progress.show();

    success = copyRecursively(datadir, destDir, progress);
    if (success) {
        qDebug() << "Finished copying " + datadir;
    }

    success = processPreferenceFile( destDir );

    QDir profDir(destDir+"/Profiles");
    QStringList names = profDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (int i = 0; success && (i < names.count()); i++) {
        success = process_a_Profile( destDir+"/Profiles/"+names[i] );
    }

    progress.setValue(numFiles);
    auto allDone = std::chrono::steady_clock::now();
    auto elapsedCount = std::chrono::duration_cast<std::chrono::microseconds>(countDone - startTime);
    auto elapsedCopy  = std::chrono::duration_cast<std::chrono::microseconds>(allDone - countDone);
    qDebug() << "Counting files took " << elapsedCount.count() << " microsecs";
    qDebug() << "Migrating files took " << elapsedCopy.count() << " microsecs";

    return success;
}

#ifdef UNITTEST_MODE

int main(int argc, char* argv[])
{
    initializeStrings();
    qDebug() << STR_TR_OSCAR + " " + getVersion();

    AutoTest::run(argc, argv);
}

#else

#ifndef Q_OS_LINUX
// Due to a bug in Qt, creating multiple QApplication instances in a process
// causes subsequent native file dialog boxes to hang on Fedora 35.
// See https://bugreports.qt.io/browse/QTBUG-90616
//
// Since Linux users can simply use the --legacy command-line argument,
// we can remove the shift-key check that requires those multiple instances.
bool shiftKeyPressedAtLaunch(int argc, char *argv[])
{
    // Reliably detecting the shift key requires a QGuiApplication instance, but
    // we need to create the real QApplication afterwards, so create a temporary
    // instance here.
    QGuiApplication* app = new QGuiApplication(argc, argv);
    Qt::KeyboardModifiers keymodifier = QGuiApplication::queryKeyboardModifiers();
    delete app;
    return keymodifier == Qt::ShiftModifier;
}
#endif


void optionExit(int exitCode, QString error) {
    if (exitCode) {
        error.prepend("Command option error: ");
    }
    qCritical() << error << ( R"(
    Help Menu
    Option                   Description
    -p                       Pauses execution for 1 second
    --profile  <name>        Name of profile. if name does not exist then uses profile tab.
    --l or --language        Force language prompt
    --datadir  <folderName>  Use folderName as Oscar Data folder. For relatve paths: <Documents folder>/<relative path>.
                             If folder does not exist then prompts user.
    --help                   Displays this menu and exits.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    --hires                  Enables high Resolution
    --hiresoff               Disables high Resolution
#endif
    -l                       Force login option. Internal OSCAR call from RestartApplication.
    )" );
    exit (exitCode);
}

int main(int argc, char *argv[]) {
    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
    QCoreApplication::setApplicationName(getAppName());
    QCoreApplication::setOrganizationName(getDeveloperName());
    QCoreApplication::setOrganizationDomain(getDeveloperDomain());

    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    HighResolution::init();
    bool hiResEnabled=false;

    for (int i = 1; i < argc; i++) {
        if (0 == strcmp(argv[i] ,"--hires"))  {
            HighResolution::init(HighResolution::HRM_ENABLED);
        } else if (0 == strcmp(argv[i] ,"--hiresoff"))  {
            HighResolution::init(HighResolution::HRM_DISABLED);
        } else if (0 == strcmp(argv[i] ,"--datadir"))  { i++;
        } else if (0 == strcmp(argv[i] ,"-profile"))  { i++;
        }
    }
    if (HighResolution::isEnabled()) {
        hiResEnabled=true;
        QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }
    #else
        // high resolution is enable by default
    #endif

    QSettings settings;

    // If shift key was held down when OSCAR was launched, force Software graphics Engine (aka LegacyGFX)
    QString forcedEngine = "";
#ifndef Q_OS_LINUX
    // Shift key check is skipped on Linux due to a Qt bug, see comment at shiftKeyPressedAtLaunch().
    if (shiftKeyPressedAtLaunch(argc, argv)){
        settings.setValue(GFXEngineSetting, (unsigned int)GFX_Software);
        forcedEngine = "Software Engine forced by shift key at launch";
    }
#endif
    // This argument needs to be processed before creating the QApplication,
    // based on sample code at https://doc.qt.io/qt-5/qapplication.html#details
    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], "--legacy")) {
            settings.setValue(GFXEngineSetting, (unsigned int)GFX_Software);
            forcedEngine = "Software Engine forced by --legacy command line switch";
        }
    }
#ifdef Q_OS_WIN
    bool oscarCrashed = false;
    if (settings.value("OpenGLCompatibilityCheck").toBool()) {
        oscarCrashed = true;
    }
    if (oscarCrashed) {
        settings.setValue(GFXEngineSetting, (unsigned int)GFX_Software);
        forcedEngine = "Software Engine forced by previous crash";
        settings.remove("OpenGLCompatibilityCheck");
    }
#endif

    GFXEngine gfxEngine = (GFXEngine)qMin((unsigned int)settings.value(GFXEngineSetting, (unsigned int)GFX_OpenGL).toUInt(), (unsigned int)MaxGFXEngine);
    switch (gfxEngine) {
    case 0:  // GFX_OpenGL
        QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
        break;
    case 1:  // GFX_ANGLE
        QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
        break;
    case 2:  // GFX_Software
    default:
        QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    }

    QApplication mainapp(argc, argv);
    QStringList args = mainapp.arguments();

#ifdef Q_OS_WIN
    // QMessageBox must come after the application is created. The graphics engine has to be selected before.
    if (oscarCrashed) {
        QMessageBox::warning(nullptr, STR_MessageBox_Error,
                             QObject::tr("OSCAR crashed due to an incompatibility with your graphics hardware.") + "\n\n" +
                             QObject::tr("To resolve this, OSCAR has reverted to a slower but more compatible method of drawing."),
                             QMessageBox::Ok);
    }
#endif

    initializeLogger();
    // After initializing the logger, any qDebug() messages will be queued but not written to console
    // until MainWindow is constructed below. In spite of that, we initialize the logger here so that
    // the intervening messages show up in the debug pane.
    //
    // The only time this is really noticeable is when initTranslations() presents its language
    // selection QDialog, which waits indefinitely for user input before MainWindow is constructed.

    bool force_data_dir = false;
    QString load_profile; // null profile means no --profile param
    for (int i = 1; i < args.size(); i++) {
        if ((args[i] == "--language") || (args[i] == "--l") ) {
            settings.setValue(LangSetting,"");
        } else if ( (args[i] == "-l") || (args[i] == "-nop") || (args[i] == "") ){
            // do nothing. internal calls that current don't have any functions in main.
        } else if (args[i] == "-p") {
            QThread::msleep(1000);
        } else if (args[i] == "--profile") {
            if ((i+1) < args.size())
                load_profile = args[++i];
            else {
                // Just view all profiles.
                load_profile = " ";
            }
        } else if (args[i] == "--datadir") { // mltam's idea
            QString datadir, datadirwas ;
            if ((i+1) < args.size()) {
                datadirwas = datadir = args[++i];
                bool havefullpath = false;
                if (datadir.length() >= 2) {
                    havefullpath =    (datadir.at(1) == QLatin1Char(':')) // Allow a Windows drive letter
                                   || (datadir.at(0) == '/')              // or Linux full path
                                   || (datadir.at(0) == '\\');
                }
                if (!havefullpath) {
                    datadir = homeDocs+datadir;
                    qDebug() << "--datadir was:" << datadirwas << "; --datadir is:" << datadir;
                }
                settings.setValue("Settings/AppData", datadir);
//            force_data_dir = true;
            } else {
                optionExit(2,"Missing argument to --datadir\n");
            }
        } else if (0 == strcmp(argv[i] ,"--hires"))  {      // already handle in 1st scan
        } else if (0 == strcmp(argv[i] ,"--hiresoff"))  {       // already handle in 1st scan
        } else if (QString(args[i]).contains("help",Qt::CaseInsensitive)) {
            optionExit(0,QString(""));
        } else {
            optionExit(3,QString("Invalid Argument: '%1'").arg(args[i]));
        }

    }   // end of for args loop

    qDebug().noquote() << "OSCAR starting" << QDateTime::currentDateTime().toString();
    qDebug() << "APP-NAME:" << QCoreApplication::applicationName();
    qDebug() << "APP-PATH:" << QCoreApplication::applicationDirPath();
    qDebug() << "APP-RESOURCES:" << appResourcePath();
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    HighResolution::display(hiResEnabled);
    #endif

#ifdef QT_DEBUG
    QString relinfo = " debug";
#else
    QString relinfo = "";
#endif
    relinfo = "("+QSysInfo::kernelType()+" "+QSysInfo::currentCpuArchitecture()+relinfo+")";
    relinfo = STR_AppName + " " + getVersion() + " " + relinfo;

    qDebug().noquote() << relinfo;
    qDebug().noquote() << "Built with Qt" << QT_VERSION_STR << "on" << getBuildDateTime();
    addBuildInfo(relinfo);  // immediately add it to the build info that's accessible from the UI

    SetDateFormat();

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Language Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
    initTranslations();

    initializeStrings(); // This must be called AFTER translator is installed, but before mainwindow is setup

//    QFontDatabase::addApplicationFont("://fonts/FreeSans.ttf");
//    a.setFont(QFont("FreeSans", 11, QFont::Normal, false));

    mainwin = new MainWindow;

// Moved buildInfo calls to after translation is available as makeBuildInfo includes tr() calls

    QStringList info = makeBuildInfo(forcedEngine);
    for (int i = 0; i < info.size(); ++i) {
        qDebug().noquote() << info.at(i);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////
    // OpenGL Detection
    ////////////////////////////////////////////////////////////////////////////////////////////
    getOpenGLVersion();
    getOpenGLVersionString();

    //bool opengl2supported = glversion >= 2.0;
    //bool bad_graphics = !opengl2supported;
    //bool intel_graphics = false;
//#ifndef NO_OPENGL_BUILD

//#endif

    /*************************************************************************************
    #ifdef BROKEN_OPENGL_BUILD
        Q_UNUSED(bad_graphics)
        Q_UNUSED(intel_graphics)

        const QString BetterBuild = "Settings/BetterBuild";

        if (opengl2supported) {
            if (!settings.value(BetterBuild, false).toBool()) {
                QMessageBox::information(nullptr, QObject::tr("A faster build of OSCAR may be available"),
                    QObject::tr("This build of OSCAR is a compatability version that also works on computers lacking OpenGL 2.0 support.")+"<br/><br/>"+
                    QObject::tr("However it looks like your computer has full support for OpenGL 2.0!") + "<br/><br/>"+
                    QObject::tr("This version will run fine, but a \"<b>%1</b>\" tagged build of OSCAR will likely run a bit faster on your computer.").arg("-OpenGL")+"<br/><br/>"+
                    QObject::tr("You will not be bothered with this message again."), QMessageBox::Ok, QMessageBox::Ok);
                settings.setValue(BetterBuild, true);
            }
        }
    #else
        if (bad_graphics) {
            QMessageBox::warning(nullptr, QObject::tr("Incompatible Graphics Hardware"),
                QObject::tr("This build of OSCAR requires OpenGL 2.0 support to function correctly, and unfortunately your computer lacks this capability.") + "<br/><br/>"+
                QObject::tr("You may need to update your computers graphics drivers from the GPU makers website. %1").
                    arg(intel_graphics ? QObject::tr("(<a href='http://intel.com/support'>Intel's support site</a>)") : "")+"<br/><br/>"+
                QObject::tr("Because graphs will not render correctly, and it may cause crashes, this build will now exit.")+"<br/><br/>"+
                QObject::tr("There is another build available tagged \"<b>-BrokenGL</b>\" that should work on your computer."),
                QMessageBox::Ok, QMessageBox::Ok);
            exit(1);
        }
    #endif
    ****************************************************************************************************************/
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Datafolder location Selection
    ////////////////////////////////////////////////////////////////////////////////////////////
//  bool change_data_dir = force_data_dir;
//
//  bool havefolder = false;

    if (!settings.contains("Settings/AppData")) {       // This is first time execution
        if ( settings.contains("Settings/AppRoot") ) {  // allow for old AppRoot here - not really first time
            settings.setValue("Settings/AppData", settings.value("Settings/AppRoot"));
        } else {
            settings.setValue("Settings/AppData", homeDocs + getModifiedAppData());    // set up new data directory path
        }
        qDebug() << "First time: Setting " + GetAppData();
    }

    QDir dir(GetAppData());

    if ( ! dir.exists() ) {             // directory doesn't exist, verify user's choice
        if ( ! force_data_dir ) {       // unless they explicitly selected it by --datadir param
            if (QMessageBox::question(nullptr, STR_MessageBox_Question,
                                      QObject::tr("OSCAR will set up a folder for your data.")+"\n"+
                                      QObject::tr("If you have been using SleepyHead or an older version of OSCAR,") + "\n" +
                                      QObject::tr("OSCAR can copy your old data to this folder later.")+"\n"+
                                      QObject::tr("We suggest you use this folder: ")+QDir::toNativeSeparators(GetAppData())+"\n"+
                                      QObject::tr("Click Ok to accept this, or No if you want to use a different folder."),
                                      QMessageBox::Ok | QMessageBox::No, QMessageBox::Ok) == QMessageBox::No) {
                // User wants a different folder for data
                bool change_data_dir = true;
                while (change_data_dir) {           // Create or select an acceptable folder
                    QString datadir = QFileDialog::getExistingDirectory(nullptr,
                                      QObject::tr("Choose or create a new folder for OSCAR data"), homeDocs, QFileDialog::ShowDirsOnly);

                    if (datadir.isEmpty()) {        // User hit Cancel instead of selecting or creating a folder
                        QMessageBox::information(nullptr, QObject::tr("Exiting"),
                                                 QObject::tr("As you did not select a data folder, OSCAR will exit.")+"\n"+
                                                 QObject::tr("Next time you run OSCAR, you will be asked again."));
                        return 0;
                    } else {                        // We have a folder, see if is already an OSCAR folder
                        QDir dir(datadir);
                        QFile file(datadir + "/Preferences.xml");
                        QDir  dirP(datadir + "/Profiles");

                        if (!file.exists() || !dirP.exists()) {       // It doesn't have a Preferences.xml file or a Profiles directory in it
                            if (dir.count() > 2) {  // but it has more than dot and dotdot
                                // Not a new directory.. nag the user.
                                if (QMessageBox::question(nullptr, STR_MessageBox_Warning,
                                                          QObject::tr("The folder you chose is not empty, nor does it already contain valid OSCAR data.") +
                                                          "\n\n"+QObject::tr("Are you sure you want to use this folder?")+"\n\n" +
                                                          datadir, QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
                                    continue;   // Nope, don't use it, go around the loop again
                                }
                            }
                        }

                        settings.setValue("Settings/AppData", datadir);
                        qDebug() << "Changing data folder to" << datadir;
                        change_data_dir = false;
                    }
                }           // the while loop
            }           // user wants a different folder
        }           // user used --datadir folder to select a folder
    }           // The folder doesn't exist
    else
        qDebug() << "AppData folder already exists, so ...";
    qDebug().noquote() << "Using " + GetAppData() + " as OSCAR data folder";

    QString path = GetAppData();
    addBuildInfo(QObject::tr("Data directory:") + " <a href=\"file:///" + path + "\">" + path + "</a>");

    QDir newDir(GetAppData());
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    if ( ! newDir.exists() || newDir.count() == 0 )      // directory doesn't exist yet or is empty, try to migrate old data
#else
    if ( ! newDir.exists() || newDir.isEmpty() )         // directory doesn't exist yet or is empty, try to migrate old data
#endif
    {
        if (QMessageBox::question(nullptr, QObject::tr("Migrate SleepyHead or OSCAR Data?"),
                                  QObject::tr("On the next screen OSCAR will ask you to select a folder with SleepyHead or OSCAR data") +"\n" +
                                  QObject::tr("Click [OK] to go to the next screen or [No] if you do not wish to use any SleepyHead or OSCAR data."),
                                  QMessageBox::Ok|QMessageBox::No, QMessageBox::Ok) == QMessageBox::Ok) {
            migrateFromSH( GetAppData() );              // doesn't matter if no migration
        }
    }

    // Make sure the data directory exists.
    if (!newDir.mkpath(".")) {
        QMessageBox::warning(nullptr, QObject::tr("Exiting"),
                             QObject::tr("Unable to create the OSCAR data folder at")+"\n"+
                             GetAppData());
        return 0;
    }

    // Make sure we can write to the data directory
    QFile testFile(GetAppData()+"/testfile.txt");
    if (testFile.exists())
        testFile.remove();
    if (!testFile.open(QFile::ReadWrite)) {
        QString errMsg = QObject::tr("Unable to write to OSCAR data directory") + " " + GetAppData() + "\n\n" +
                         QObject::tr("Error code") + ": " + QString::number(testFile.error()) + " - " + testFile.errorString() + "\n\n" +
                         QObject::tr("OSCAR cannot continue and is exiting.") + "\n";
        qCritical() << errMsg;
        QMessageBox::critical(nullptr, QObject::tr("Exiting"), errMsg);
        return 0;
    }
    else
        testFile.remove();

    // Begin logging to file now that there's a data folder.
    if (!logger->logToFile()) {
        QMessageBox::warning(nullptr, STR_MessageBox_Warning,
                             QObject::tr("Unable to write to debug log. You can still use the debug pane (Help/Troubleshooting/Show Debug Pane) but the debug log will not be written to disk."));
    }


    ///////////////////////////////////////////////////////////////////////////////////////////
    // Initialize preferences system (Don't use p_pref before this point!)
    ///////////////////////////////////////////////////////////////////////////////////////////
    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    QString language = settings.value(LangSetting, "").toString();
    AppSetting->setLanguage(language);
    if (!load_profile.isEmpty()) AppSetting->setProfileName(load_profile);

    // Set fonts from preferences file
    qDebug() << "App font before Prefs setting" << QApplication::font();
    validateAllFonts();
    setApplicationFont();

    // one-time translate GraphSnapshots to ShowPieChart
    p_pref->Rename(STR_AS_GraphSnapshots, STR_AS_ShowPieChart);

    p_pref->Erase(STR_AppName);
    p_pref->Erase(STR_GEN_SkipLogin);

#ifndef NO_CHECKUPDATES
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Check when last checked for updates..
    ////////////////////////////////////////////////////////////////////////////////////////////
    QDateTime lastchecked, today = QDateTime::currentDateTime();

    bool check_updates = false;

    if (!getVersion().IsReleaseVersion()) {
        // If test build, force update autocheck, no more than 7 day interval, show test versions
        AppSetting->setUpdatesAutoCheck(true);
        AppSetting->setUpdateCheckFrequency(min(AppSetting->updateCheckFrequency(), 7));
        AppSetting->setAllowEarlyUpdates(true);
    }

    if (AppSetting->updatesAutoCheck()) {
        int update_frequency = AppSetting->updateCheckFrequency();
        int days = 1000;
        lastchecked = AppSetting->updatesLastChecked();

        if (lastchecked.isValid()) {
            days = lastchecked.secsTo(today);
            days /= 86400;
        }

        if (days >= update_frequency) {
            check_updates = true;
        }
    }
#endif

    Version settingsVersion = Version(AppSetting->versionString());
    Version currentVersion = getVersion();
    if (currentVersion.IsValid() == false) {
        // The defined version MUST be valid, otherwise comparisons between versions will fail.
        QMessageBox::critical(nullptr, STR_MessageBox_Error, QObject::tr("Version \"%1\" is invalid, cannot continue!").arg(currentVersion));
        return 0;
    }
    if (currentVersion > settingsVersion) {
        AppSetting->setShowAboutDialog(1);
//      release_notes();
//      check_updates = false;
    } else if (currentVersion < settingsVersion) {
        if (QMessageBox::warning(nullptr, STR_MessageBox_Error,
                                 QObject::tr("The version of OSCAR you are running (%1) is OLDER than the one used to create this data (%2).")
                                    .arg(currentVersion.displayString())
                                    .arg(settingsVersion.displayString())
                                 +"\n\n"+
                                 QObject::tr("It is likely that doing this will cause data corruption, are you sure you want to do this?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No) {

            return 0;
        }
    }

    AppSetting->setVersionString(getVersion());

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Register Importer Modules for autoscanner
    ////////////////////////////////////////////////////////////////////////////////////////////
    schema::init();
    PRS1Loader::Register();
    ResmedLoader::Register();
    IntellipapLoader::Register();
    SleepStyleLoader::Register();
    FPIconLoader::Register();
    WeinmannLoader::Register();
    CMS50Loader::Register();
    CMS50F37Loader::Register();
    MD300W1Loader::Register();
    ViatomLoader::Register();
    PrismaLoader::Register();
    ResventLoader::Register();
    BmcLoader::Register();
    VREMLoader::Register();


    // Begin logging device connection activity.
    QString connectionsLogDir = GetLogDir() + "/connections";
    rotateLogs(connectionsLogDir);  // keep a limited set of previous logs
    if (!QDir(connectionsLogDir).mkpath(".")) {
        qWarning().noquote() << "Unable to create directory" << connectionsLogDir;
    }

    QFile deviceLog(connectionsLogDir + "/devices.xml");
    if (deviceLog.open(QFile::ReadWrite)) {
        qDebug().noquote() << "Logging device connections to" << deviceLog.fileName();
        DeviceConnectionManager::getInstance().record(&deviceLog);
    } else {
        qWarning().noquote() << "Unable to start device connection logging to" << deviceLog.fileName();
    }

    schema::setOrders(); // could be called in init...

    // Scan for user profiles
    SpeedCheck sc(500, "Scanning for profiles");
    Profiles::Scan();
    sc.check();

#ifndef NO_CHECKUPDATES
    if (check_updates) {
        mainwin->CheckForUpdates(false);
    }
#endif

    sc.restart(400,"SetupGUI");
    mainwin->SetupGUI();
    sc.check();
    mainwin->show();

    int result = mainapp.exec();

    DeviceConnectionManager::getInstance().record(nullptr);

    return result;
}

#endif // !UNITTEST_MODE
