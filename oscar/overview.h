/* Overview GUI Headers
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef OVERVIEW_H
#define OVERVIEW_H

#include <QWidget>
#ifndef BROKEN_OPENGL_BUILD
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QGLContext>
#else
#include <QtOpenGLWidgets/QOpenGLWidget>
#endif
#endif
#include <QHBoxLayout>
#include <QDateEdit>
#include <QTimer>
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"
#ifndef REMOVE_FITNESS
#include "Graphs/gOverviewGraph.h"
#endif
#include "Graphs/gSummaryChart.h"

#include <QRegularExpression>
#include <QListWidget>
#include <QListWidgetItem>
class SaveGraphLayoutSettings;

namespace Ui {
class Overview;
}

class Report;
class Overview;

class DateErrorDisplay:QObject {
	Q_OBJECT
public:
    DateErrorDisplay (Overview* overview) ;
    ~DateErrorDisplay() ;
	bool visible() {return m_visible;};
    void cancel();
	void error(bool startDate,const QDate& date);
protected:

private:
	QTimer* m_timer;
    bool m_visible=false;
    Overview* m_overview;
	QDate m_startDate;
	QDate m_endDate;
private slots:
    void timerDone();
};

enum YTickerType { YT_Number, YT_Time, YT_Weight };

/*! \class Overview
    \author Mark Watkins 
    \brief Overview tab, showing overall summary data
    */
class Overview : public QWidget
{
	friend class DateErrorDisplay;
    Q_OBJECT

  public:
    explicit Overview(QWidget *parent, gGraphView *shared = nullptr);
    ~Overview();

    //! \brief Returns Overview gGraphView object containing it's graphs
    gGraphView *graphView() { return GraphView; }

    //! \brief Recalculates Overview chart info
    void ReloadGraphs();

    //! \brief Resets font in date display
    void ResetFont();

    //! \brief Recalculates Overview chart info, but keeps the date set
    //void ResetGraphs();

    //! \brief Reset graphs to uniform heights
    void ResetGraphLayout();

    /*! \fn ResetGraphOrder()
        \brief Resets all graphs in the main gGraphView back to their initial order.
        */
    void ResetGraphOrder(int type);

    //! \brief Calls updateGL to redraw the overview charts
    void RedrawGraphs();

    //! \brief Sets the currently selected date range of the overview display
    void setRange(QDate& start, QDate& end,bool updateGraphs=true);

    /*! \brief Create an overview graph, adding it to the overview gGraphView object
        \param QString name  The title of the graph
        \param QString units The units of measurements to show in the popup */
    gGraph *createGraph(QString code, QString name, QString units = "", YTickerType yttype = YT_Number);
    gGraph *AHI, *AHIHR, *UC, *FL, *SA, *US, *PR, *LK, *NPB, *SET, *SES, *RR, *MV, *TV, *PTB, *PULSE, *SPO2, *NLL,
           *WEIGHT, *ZOMBIE, *BMI, *TGMV, *TOTLK, *STG, *SN, *TTIA;
#ifndef REMOVE_FITNESS
    gOverviewGraph *bc, *sa, *us, *pr,  *set, *ses,  *ptb, *pulse, *spo2,
                 *weight, *zombie, *bmi, *ahihr, *tgmv, *totlk;
#endif

    gSummaryChart * stg, *uc, *ahi, * pres, *lk, *npb, *rr, *mv, *tv, *nll, *sn, *ttia;

    void RebuildGraphs(bool reset = true);

  public slots:
    void onRebuildGraphs() { RebuildGraphs(true); }

    //! \brief Resets view to currently shown start & end dates
    void on_zoomButton_clicked();

  private slots:
    void updateGraphCombo();
    void on_XBoundsChanged(qint64 ,qint64);
    void on_summaryChartEmpty(gSummaryChart*,qint64,qint64,bool);

    //! \brief Resets the graph view because the Start date has been changed
    void on_dateStart_dateChanged(const QDate &date);

    //! \brief Resets the graph view because the End date has been changed
    void on_dateEnd_dateChanged(const QDate &date);

    //! \brief Updates the calendar highlighting when changing to a new month
    void dateStart_currentPageChanged(int year, int month);

    //! \brief Updates the calendar highlighting when changing to a new month
    void dateEnd_currentPageChanged(int year, int month);

    void on_rangeCombo_activated(int index);

    void on_graphCombo_activated(int index);

    void on_LineCursorUpdate(double time);
    void on_RangeUpdate(double minx, double maxx);
    void setGraphText ();

    void on_layout_clicked();
    void on_graphHelp_clicked();

  private:
    void CreateAllGraphs();
    void timedUpdateOverview(int ms=0);

    Ui::Overview *ui;
    gGraphView *GraphView;
    MyScrollBar *scrollbar;
    QHBoxLayout *layout;
    gGraphView *m_shared;
    QIcon *icon_on;
    QIcon *icon_off;
    QIcon *icon_up_down;
    QIcon *icon_warning;
    MyLabel *dateLabel;
    bool customMode=false;

    //! \brief Updates the calendar highlighting for the calendar object for this date.
    void UpdateCalendarDay(QDateEdit *calendar, QDate date,bool startDateWidget);
    void updateCube();

    Day *day; // dummy in this case


    bool checkRangeChanged(QDate& first, QDate& last);
    void connectgSummaryCharts() ;
    void disconnectgSummaryCharts() ;
    void SetXBounds(qint64 minx, qint64 maxx, short group = 0, bool refresh = true);
    void SetXBounds(QDate & start, QDate&  end, short group =0 , bool refresh  = true);
	void resetUiDates();
	DateErrorDisplay* dateErrorDisplay;
	int calculatePixels(bool startDate,ToolTipAlignment& align);
	QString shortformat;

    // Start and of dates of the current graph display
    QDate displayStartDate;
    QDate displayEndDate;

    // min / max dates of the graph Range
    QDate minRangeStartDate;
    QDate maxRangeEndDate;

    QHash<gSummaryChart*,gGraph*> chartsToBeMonitored;
    QHash<gSummaryChart*,gGraph* > chartsEmpty;

    bool settingsLoaded ;

    // Actual dates displayed in Start/End Widgets.
    QDate uiStartDate;
    QDate uiEndDate;

    // Are start and end widgets displaying the same month.
    bool samePage;

    SaveGraphLayoutSettings* saveGraphLayoutSettings=nullptr;
    QString STR_HIDE_ALL_GRAPHS =QString(tr("Hide All Graphs"));
    QString STR_SHOW_ALL_GRAPHS =QString(tr("Show All Graphs"));
    void showGraph(int index,bool show, bool updateGraph);
    void showAllGraphs(bool show);

};




#endif // OVERVIEW_H
