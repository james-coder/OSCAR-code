/* OSCAR MainWindow Headers
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#ifndef BROKEN_OPENGL_BUILD
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        #include <QGLContext>
    #else
        // #include <QtOpenGLWidgets/QOpenGLWidget>
    #endif
#endif

#include <QSystemTrayIcon>
#include <QTimer>

#include "daily.h"
#include "overview.h"
#include "welcome.h"
#ifndef helpless
#include "help.h"
#endif

#include "profileselector.h"
#include "preferencesdialog.h"

extern Profile *profile;
QString getCPAPPixmap(QString mach_class);

namespace Ui {
class MainWindow;
}


/*! \mainpage OSCAR

 \section intro_sec Introduction

 Open Source CPAP Analysis Reporter (OSCAR) is a program derived from the SleepyHead program written by Mark Watkins.

 SleepyHead was a Cross-Platform Open-Source software for reviewing data from %CPAP devices, which are used in the treatment of Sleep Disorders.

 SleepyHead was created by Mark Watkins, an Australian software developer.

 This document is an attempt to provide a little technical insight into OSCAR's program internals.

 \section project_info Further Information
 OSCAR is hosted on <a href="https://gitlab.com/pholy/OSCAR-code">Gitlab</a> with full source code available there.

 Help for users can be found in the <a href="http://www.apneaboard.com/wiki/index.php?title=OSCAR_Help">OSCAR Help Wiki</a>.

 Data structures are described in a <a href="http://www.apneaboard.com/wiki/index.php?title=OSCAR_Data_Information">OSCAR Data Information Wiki</a>.


 \section structure Program Structure
 OSCAR is written in C++ using Qt Toolkit library, and comprises of 3 main components
 \list
 \li The SleepLib Database, a specialized database for working with multiple sources of Sleep device data.
 \li A custom designed, high performance and interactive OpenGL Graphing Library.
 \li and the main Application user interface.
 \endlist

 This document is still a work in progress, right now all the classes and sections are jumbled together.

 */

// * \section install_sec Installation

extern QStatusBar *qstatusbar;

//QString getCPAPPixmap(QString mach_class);


class Daily;
class Report;
class Overview;


/*! \class MainWindow
    \author Mark Watkins
    \brief The Main Application window for OSCAR
    */

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    //! \brief Setup the rest of the GUI stuff
    void SetupGUI();

    //! \brief Update the list of Favourites (Bookmarks) in the right sidebar.
    void updateFavourites();

    //! \brief Update statistics report
    void GenerateStatistics();

    //! \brief Create a new menu object in the main menubar.
    QMenu *CreateMenu(QString title);

    //! \brief Start the automatic update checker process
    void CheckForUpdates(bool showWhenCurrent);

    void EnableTabs(bool b);

    void CloseProfile();
    bool OpenProfile(QString name, bool skippassword = false);

    /*! \fn Notify(QString s, QString title="OSCAR (version)", int ms=5000);
        \brief Pops up a message box near the system tray
        \param QString string
        \param title
        \param int ms

        Title is shown in bold
        string is the main message content to show
        ms = millisecond delay of how long to show popup

        Mac needs Growl notification system for this to work
        */
    void Notify(QString s, QString title = "", int ms = 5000);

//    /*! \fn gGraphView *snapshotGraph()
//        \brief Returns the current snapshotGraph object used by the report printing system */
//    gGraphView *snapshotGraph() { return SnapshotGraph; }

    //! \brief Returns the Daily Tab object
    Daily *getDaily() { return daily; }

    //! \brief Returns the Overview Tab object
    Overview *getOverview() { return overview; }

    void updateOverview();

    void JumpDaily();
    void JumpOverview();
    void JumpStatistics();
    void JumpImport();
    void JumpOxiWizard();

    void sendStatsUrl(QString msg) { on_recordsBox_anchorClicked(QUrl(msg)); }

    //! \brief Sets up recalculation of all event summaries and flags
    void reprocessEvents(bool restart = false);
    void recompressEvents();


    //! \brief Internal function to set Records Box html from statistics module
    void setRecBoxHTML(QString html);
    //! \brief Internal function to set Statistics page html from statistics module
    void setStatsHTML(QString html);

    int importCPAP(ImportPath import, const QString &message);
    void finishCPAPImport();

    void startImportDialog() { on_action_Import_Data_triggered(); }

    void log(QString text);

    bool importScanCancelled;
    void firstRunMessage();


  public slots:
    //! \brief Recalculate all event summaries and flags
    void doReprocessEvents();
    void doRecompressEvents();
    /*! \fn void RestartApplication(bool force_login=false);
        \brief Closes down OSCAR and restarts it
        \param bool force_login

        If force_login is set, it will return to the login menu even if it's set to skip
        allow timer to restart application.
        */
    void RestartApplication(bool force_login = false, QString cmdline = QString());


    QString profilePath(QString folderProfileName );
    void saveProfilePath(QString folderProfileName , QString pathName);

  protected:
    void closeEvent(QCloseEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;

  private slots:
    /*! \fn void on_action_Import_Data_triggered();
        \brief Provide the file dialog for selecting import location, and start the import process
        This is called when the Import button is clicked
        */
    void on_action_Import_Data_triggered();

    //! \brief Toggle Fullscreen (currently F11)
    void on_action_Fullscreen_triggered();

    //! \brief Selects the Daily page and redraws the graphs
    void on_dailyButton_clicked();

    //! \brief Selects the Overview page and redraws the graphs
    void on_overviewButton_clicked();

    //! \brief Display About Dialog
    void on_action_About_triggered();

#ifdef Q_OS_WIN
   //! \brief Called on Windows to see whether the current OpenGL driver will cause the application to crash
    void TestWindowsOpenGL();
#endif

    //! \brief Called after a timeout to initiate loading of all summary data for this profile
    void Startup();

    //! \brief Toggle the Debug pane on and off
    void on_actionDebug_toggled(bool arg1);

    //! \brief passes the ResetGraphLayout menu click to the Daily & Overview views
    void on_action_Reset_Graph_Layout_triggered();

    //! \brief passes the ResetGraphOrder menu click to the Daily & Overview views
    //void on_action_Reset_Graph_Order_triggered();

    //! \brief passes the ResetGraphOrder menu click to the Daily & Overview views
    void on_action_Standard_Graph_Order_triggered();

    //! \brief passes the ResetGraphOrder menu click to the Daily & Overview views
    void on_action_Advanced_Graph_Order_triggered();

    //! \brief Opens the Preferences Dialog, and saving changes if OK is pressed
    void on_action_Preferences_triggered();

    //! \brief Opens and/or shows the Oximetry page
    void on_oximetryButton_clicked();

    //! \brief Creates the CheckUpdates object that actually does the real check for updates
    void on_action_Check_for_Updates_triggered();

    //! \brief Attempts to do a screenshot of the application window
    void on_action_Screenshot_triggered();

    //! \brief This is the actual screenshot code.. It's delayed with a QTimer to give the menu time to close.
    void DelayedScreenshot();

    //! \brief a slot that calls the real Oximetry tab selector
    void on_actionView_Oximetry_triggered();

    //! \brief Passes the Daily, Overview & Oximetry object to Print Report, based on current tab
    void on_actionPrint_Report_triggered();

    //! \brief Opens the Profile Editor
    void on_action_Edit_Profile_triggered();

    //! \brief Selects the next view tab
    void on_action_CycleTabs_triggered();

    //! \brief Opens the User Guide at the wiki in the welcome browser.
    void on_actionOnline_Users_Guide_triggered();

    //! \brief Opens the Frequently Asked Questions at the wiki in the welcome browser.
    void on_action_Frequently_Asked_Questions_triggered();

    /*! \fn void on_action_Rebuild_Oximetry_Index_triggered();
        \brief This function scans over all oximetry data and reindexes and tries to fix any inconsistencies.
        */
    void on_action_Rebuild_Oximetry_Index_triggered();

    //! \brief Destroy the CPAP data for the currently selected day, so it can be freshly imported again
    void on_actionPurge_Current_Day_triggered();
    void on_actionPurgeCurrentDayOximetry_triggered();
    void on_actionPurgeCurrentDaySleepStage_triggered();
    void on_actionPurgeCurrentDayPosition_triggered();
    void on_actionPurgeCurrentDayAllExceptNotes_triggered();
    void on_actionPurgeCurrentDayAll_triggered();

    void on_action_Sidebar_Toggle_toggled(bool arg1);

        void on_helpButton_clicked();

    void on_actionView_Statistics_triggered();

    //void on_favouritesList_itemSelectionChanged();

    //void on_favouritesList_itemClicked(QListWidgetItem *item);

    void on_tabWidget_currentChanged(int index);

    void on_filterBookmarks_editingFinished();

    void on_filterBookmarksButton_clicked();

    void on_actionImport_ZEO_Data_triggered();

    void on_actionImport_Dreem_Data_triggered();

    void on_actionImport_RemStar_MSeries_Data_triggered();

    void on_actionSleep_Disorder_Terms_Glossary_triggered();

    //void on_actionHelp_Support_OSCAR_Development_triggered();

    void aboutBoxLinkClicked(const QUrl &url);

    void on_actionChange_Language_triggered();

    void on_actionChange_Data_Folder_triggered();

    void on_actionImport_Somnopose_Data_triggered();

    void on_actionImport_Viatom_Data_triggered();

    //! \brief Populates the statistics with information.
    void on_statisticsButton_clicked();

    void on_reportModeMonthly_clicked();

    void on_reportModeStandard_clicked();

    void init_reportModeUi();
    void reset_reportModeUi();

    void on_actionRebuildCPAP(QAction *action);

    void on_actionPurgeMachine(QAction *action);

    void on_reportModeRange_clicked();

    void on_statEndDate_dateChanged(const QDate &date);

    void on_statStartDate_dateChanged(const QDate &date);

    void on_actionPurgeCurrentDaysOximetry_triggered();

    void logMessage(QString msg);

    void on_importButton_clicked();

    void on_actionLine_Cursor_toggled(bool arg1);

    void on_actionLeft_Daily_Sidebar_toggled(bool arg1);

    void on_actionDaily_Calendar_toggled(bool arg1);

    void on_actionPie_Chart_toggled(bool arg1);

    void on_actionExport_Journal_triggered();

    void on_actionImport_Journal_triggered();

    void on_actionShow_Performance_Counters_toggled(bool arg1);

    void on_actionExport_CSV_triggered();

    void on_actionExport_Review_triggered();

    void on_mainsplitter_splitterMoved(int pos, int index);

    void on_actionCreate_Card_zip_triggered();

    void on_actionCreate_Log_zip_triggered();

    void on_actionCreate_OSCAR_Data_zip_triggered();

    void on_actionReport_a_Bug_triggered();

    void on_actionSystem_Information_triggered();

    void on_profilesButton_clicked();

    void on_bookmarkView_anchorClicked(const QUrl &arg1);

    void on_recordsBox_anchorClicked(const QUrl &linkurl);

    void on_statisticsView_anchorClicked(const QUrl &url);

    void on_actionShowPersonalData_toggled(bool visible);

public slots:
    void reloadProfile();

private:
    QString getMainWindowTitle();
    void importCPAPBackups();
    QList<ImportPath> detectCPAPCards();
    QList<ImportPath> selectCPAPDataCards(const QString & prompt, bool alwaysPrompt = false);
    void importCPAPDataCards(const QList<ImportPath> & datacards);
    void addMachineToMenu(Machine* mach, QMenu* menu);
    void purgeDay(MachineType type);
    void importNonCPAP(MachineLoader &loader);

//    QString getWelcomeHTML();
    void FreeSessions();

    Ui::MainWindow *ui;
    Daily *daily;
    Overview *overview;
    ProfileSelector *profileSelector;
    Welcome * welcome;
#ifndef helpless
    Help * help;
#endif
    bool first_load;
    PreferencesDialog *prefdialog;
    QTime logtime;
    QSystemTrayIcon *systray;
    QMenu *systraymenu;
//    gGraphView *SnapshotGraph;
    QString bookmarkFilter;
    bool m_restartRequired;
    bool m_clinicalMode = false;
    volatile bool m_inRecalculation;

    void PopulatePurgeMenu();

    //! \brief Destroy ALL the CPAP data for the selected device
    void purgeMachine(Machine *);

    int warnidx;
    QStringList warnmsg;
    QTimer wtimer;
};

class ImportDialogScan:public QDialog
{
    Q_OBJECT
public:
    ImportDialogScan(QWidget * parent) :QDialog(parent, Qt::SplashScreen)
    {
    }
    virtual ~ImportDialogScan()
    {
    }
public slots:
    virtual void cancelbutton();
};


#endif // MAINWINDOW_H
