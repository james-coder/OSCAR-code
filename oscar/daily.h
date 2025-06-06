/* Daily GUI Headers
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef DAILY_H
#define DAILY_H

#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QTreeWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)  || defined (NEEDS_WORK)
#include <QtOpenGL/QGLContext>
#else
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#endif
#include <QScrollBar>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QBitArray>

#include "SleepLib/profiles.h"
#include "mainwindow.h"
#include "Graphs/gGraphView.h"
#include "Graphs/gLineChart.h"
#include "sessionbar.h"
#include "mytextbrowser.h"
class SaveGraphLayoutSettings;


namespace Ui {
    class Daily;
}

class MainWindow;
class DailySearchTab;
class gFlagsGroup;


/*! \class Daily
    \brief OSCAR's Daily view which displays the calendar and all the graphs relative to a selected Day
    */
class Daily : public QWidget
{
    Q_OBJECT

friend class Journal;
public:
    /*! \fn Daily()
        \brief Constructs a Daily object
        \param parent * (QObject parent)
        \param shared *

        Creates all the graph objects and adds them to the main gGraphView area for this tab.

        shared is not used for daily object, as it contains the default Context..
        */
    explicit Daily(QWidget *parent, gGraphView *shared);
    ~Daily();

    /*! \fn ReloadGraphs()
        \brief Reload all graph information from disk and updates the view.
        */

    void ReloadGraphs();
    /*! \fn ResetGraphLayout()
        \brief Resets all graphs in the main gGraphView back to constant heights.
        */
    void ResetGraphLayout();

    /*! \fn ResetGraphOrder()
        \brief Resets all graphs in the main gGraphView back to their initial order.
        */
    void ResetGraphOrder(int type = 0);

    /*! \fn updateLeftSidebar()
        /brief Updtes left sidebar to reflect changes in pie chart visibility
        */
    void updateLeftSidebar();

    /*! \fn graphView()
        \returns the main graphView area for the Daily View
        */
    gGraphView *graphView() { return GraphView; }

    /*! \fn RedrawGraphs()
        \brief Calls updateGL on the main graphView area, redrawing the OpenGL area
        */
    void RedrawGraphs();

    /*! \fn LoadDate()
        \brief Selects a new day object, unloading the previous one, and loads the graph data for the supplied date.
        \param QDate date
        */
    void LoadDate(QDate date);

    /*! \fn getDate()
        \brief Returns the most recently loaded Date
        \return QDate
        */
    QDate getDate() { return previous_date; }

    /*! \fn UnitsChanged()
        \brief Called by Profile editor when measurement units are changed
        */
    void UnitsChanged();

    /*! \fn GetJournalSession(QDate date)
        \brief Looks up if there is a journal object for a supplied date
        \param QDate date
        \returns Session * containing valid Journal Session object or nullptr if none found.
    */
    Session * GetJournalSession(QDate date, bool create=true);

    QString GetDetailsText();
    /*! \fn eventBreakdownPie()
        \brief Returns a pointer to the Event Breakdown Piechart for the Report Printing module
        \returns gGraph * object containing this chart
        */
    gGraph * eventBreakdownPie() { return graphlist["EventBreakdown"]; }

    void clearLastDay();

    /*! \fn Unload(QDate date)
        \brief Saves any journal changes for the provided date.
        \param QDate date
        */
    void Unload(QDate date=QDate());

    void setSidebarVisible(bool visible);
    void setCalendarVisible(bool visible);

    void addBookmark(qint64 st, qint64 et, QString text);
    void verifyBookMarkName(QTableWidgetItem *item);
    void hideSpaceHogs();
    void showSpaceHogs();

    QLabel * getDateDisplay();

    //void populateSessionWidget();

    void showAllGraphs(bool show);
    void showGraph(int index,bool show, bool updateGraph=true);
    void showAllEvents(bool show);
    void showEvent(int index,bool show, bool updateEvent=true);
    void updateEventsCombo(Day*);
    QString STR_HIDE_ALL_EVENTS =QString(tr("Hide All Events"));
    QString STR_SHOW_ALL_EVENTS =QString(tr("Show All Events"));
    QString STR_HIDE_ALL_GRAPHS =QString(tr("Hide All Graphs"));
    QString STR_SHOW_ALL_GRAPHS =QString(tr("Show All Graphs"));
    static QString convertHtmlToPlainText(QString html) ;
public slots:
    void on_LineCursorUpdate(double time);
    void on_RangeUpdate(double minx, double maxx);

private slots:
    void on_ReloadDay();

    /*! \fn on_calendar_currentPageChanged(int year, int month);
        \brief Scans through all days for this month, updating the day colors for the calendar object
        \param int year
        \param int month
        */
    void on_calendar_currentPageChanged(int year, int month);

    /*! \fn on_calendar_selectionChanged();
        \brief Called when the calendar object is clicked. Selects and loads a new date, unloading the previous one.
        */
    void on_calendar_selectionChanged();

    /* Journal Notes edit buttons I don't want to document */
    void on_JournalNotesItalic_clicked();
    void on_JournalNotesBold_clicked();
    void on_JournalNotesFontsize_activated(int index);
    void on_JournalNotesColour_clicked();
    void on_JournalNotesUnderline_clicked();

    void on_treeWidget_itemSelectionChanged();


    /*! \fn on_nextDayButton_clicked();
        \brief Step backwards one day (if possible)
        */
    void on_prevDayButton_clicked();

    /*! \fn on_nextDayButton_clicked();
        \brief Step forward one day (if possible)
        */
    void on_nextDayButton_clicked();

    /*! \fn on_calButton_toggled();
        \brief Hides the calendar and put it out of the way, giving more room for the Details area.
        */
    void on_calButton_toggled(bool checked);

    /*! \fn on_todayButton_clicked();
        \brief Select the most recent day.
        */
    void on_todayButton_clicked();

    /*! \fn Link_clicked(const QUrl &url);
        \brief Called when a link is clicked on in the HTML Details tab
        \param const QUrl & url
        */
    void Link_clicked(const QUrl &url);

    void on_evViewSlider_valueChanged(int value);

    /*! \fn on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);
        \brief Called when an event is selected in the Event tab.. Zooms into the graph area.
        \param QTreeWidgetItem *item
        \param int column
        */
    void on_treeWidget_itemClicked(QTreeWidgetItem *item, int column);

    /*! \fn graphtogglebutton_toggled(bool)
        \brief Called to hide/show a graph when on of the toggle bottoms underneath the graph area is clicked
        \param bool button status
        */
    void graphtogglebutton_toggled(bool);

    /*! \fn on_addBookmarkButton_clicked()
        \brief Current selected graph Area is added to Bookmark's list for this day's journal object.
        */
    void on_addBookmarkButton_clicked();

    /*! \fn on_removeBookmarkButton_clicked()
        \brief Currently selected bookmark is removed from this day's Bookmark list.
        */
    void on_removeBookmarkButton_clicked();

    /*! \fn on_bookmarkTable_currentItemChanged(QTableWidgetItem *item, QTableWidgetItem *previous)
        \brief Called when a bookmark has been selected.. Zooms in on the area
        \param QTableWidgetItem *item
        \param QTableWidgetItem *previous
        */

   void on_bookmarkTable_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

    /*! \fn on_bookmarkTable_itemChanged(QTableWidgetItem *item);
        \brief Called when bookmarks have been altered.. Saves the bookmark list to Journal object.
        */
    void on_bookmarkTable_itemChanged(QTableWidgetItem *item);

    void on_graphCombo_activated(int index);

    void set_NotesUI(QString htmlNote);
    void set_BookmarksUI( QVariantList& start , QVariantList& end , QStringList& notes, qint64 drift=0);

#ifndef REMOVE_FITNESS
    /*! \fn on_ouncesSpinBox_valueChanged(int arg1);
        \brief Called when the zombie slider has been moved.. Updates the BMI dislpay and journal objects.

        Also Refreshes the Overview charts
        */

    void setup_ZombieUIWidgets(int zombieValue, bool zombieMode, bool setup);
    void set_ZombieUI(int ,bool init=false);
    void on_ZombieSlider_valueChanged(int value);
    void on_ZombieSpinBox_editingFinished();
    void on_Units10_100_clicked();

    void set_WeightUI(double weight_kg);
    void set_BmiUI(double weight_kg);                               // should be a part of weight.

    /*! \fn on_weightSpinBox_editingFinished();
        \brief Called when weight has changed.. Updates the BMI dislpay and journal objects.

        Also Refreshes the Overview charts
        */

    void on_weightSpinBox_editingFinished();

    void on_weightSpinBox_valueChanged(double arg1);
#endif

    bool rejectToggleSessionEnable(Session * sess);

    void doToggleSession(Session *);

    void on_eventsCombo_activated(int index);

    void updateGraphCombo();

    void on_splitter_2_splitterMoved(int pos, int index);

    void on_layout_clicked();
    void on_graphHelp_clicked();

protected:
    virtual void showEvent(QShowEvent *);

private:
    double  calculateBMI(double weight_kg, double height_cm);
    void set_JournalZombie(QDate&, int);
    void set_JournalWeightValue(QDate&, double kgs);
    void set_JournalNotesHtml(QDate&, QString html);

    /*! \fn CreateJournalSession()
        \brief Create a new journal session for this date, if one doesn't exist.
        \param QDate date

        Creates a new journal device record if necessary.
        */
    Session * CreateJournalSession(QDate date);

    /*! \fn update_Bookmarks()
        \brief Saves the current bookmark list to the Journal object
        */
    void update_Bookmarks();

    /*! \fn Load(QDate date)
        \brief Selects a new day object, loads it's content and generates the HTML for the Details tab
        \param QDate date
        */
    void Load(QDate date);
    /*! \fn UpdateCalendarDay(QDate date)
        \brief Updates the calendar visual information, changing a dates color depending on what data is available.
        \param QDate date
        */
    void UpdateCalendarDay(QDate date);
    /*! \fn UpdateEventsTree(QDate date)
        \brief Populates the Events tree from the supplied Day object.
        \param QTreeWidget * tree
        \param Day *
        */
    void UpdateEventsTree(QTreeWidget * tree,Day *day);

    virtual bool eventFilter(QObject *object, QEvent *event);

    void updateCube();

    void setGraphText();
    void setFlagText();
    DailySearchTab* dailySearchTab = nullptr;


    QString getAHI (Day * day, bool isBrick);
    QString getSessionInformation(Day *);
    QString getMachineSettings(Day *);
    QString getStatisticsInfo(Day *);
    QString getCPAPInformation(Day *);
    QString getOximeterInformation(Day *);
    QString getEventBreakdown(Day *);
    QString getPieChart(float values, Day *);
    QString getIndices(Day * day, QHash<ChannelID, EventDataType>& values );
    QString getSleepTime(Day *);
    QString getLeftSidebar (bool honorPieChart);

    QHash<QString, gGraph *> graphlist;

    QHash<QString,QPushButton *> GraphToggles;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)  || defined (NEEDS_WORK)
    QGLContext *offscreen_context;
#else
    QOpenGLContext *offscreen_context;
#endif

    QList<int> splitter_sizes;

    Ui::Daily *ui;
    QDate previous_date;
    QMenu *show_graph_menu;

    gGraphView *GraphView,*snapGV;
    gGraph* sleepFlags;
    gFlagsGroup* sleepFlagsGroup;
    MyScrollBar *scrollbar;
    QHBoxLayout *layout;
    QLabel *emptyToggleArea;
    QIcon * icon_on;
    QIcon * icon_off;
    QIcon * icon_up_down;
    QIcon * icon_warning;

    SessionBar * sessionbar;
    MyLabel * dateDisplay;

    MyTextBrowser * webView;
    Day * lastcpapday;

    gLineChart *leakchart;

    const int eventTypeStart = QTreeWidgetItem::UserType+1;
    const int eventTypeEnd   = QTreeWidgetItem::UserType+2;

#ifndef REMOVE_FITNESS
    double user_weight_kg;
    double user_height_cm;
#endif
    constexpr static double zeroD = 0.0001 ;
    bool BookmarksChanged;

    SaveGraphLayoutSettings* saveGraphLayoutSettings=nullptr;

    enum LEFT_SIDEBAR { LSB_FIRST ,
        LSB_MACHINE_INFO ,
        LSB_SLEEPTIME_INDICES ,
        LSB_PIE_CHART ,
        LSB_STATISTICS ,
        LSB_OXIMETER_INFORMATION ,
        LSB_DEVICE_SETTINGS  ,
        LSB_SESSION_INFORMATION  ,
        LSB_END_SIZE };
    QBitArray leftSideBarEnable = QBitArray(LSB_END_SIZE,true);
    void htmlLsbSectionHeader (QString& html, const QString& name,LEFT_SIDEBAR checkBox) ;
    void htmlLsbSectionHeaderInit (bool section=true) ;
    bool htmlLsbPrevSectionHeader = true;
};

#endif // DAILY_H
