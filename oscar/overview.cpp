/* Overview GUI Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

// Features enabled by conditional compilation.
#define ENABLE_GENERAL_MODIFICATION_OF_CALENDARS
#define ENABLE_START_END_WIDGET_DISPLAY_ACTUAL_GRAPH_RANGE

#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QTimeZone>
#include <QDebug>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QFileDialog>
#include <QMessageBox>

#include "SleepLib/profiles.h"
#include "overview.h"
#include "ui_overview.h"
#include "common_gui.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gLineChart.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gSessionTimesChart.h"
#include "Graphs/gPressureChart.h"
#include "Graphs/gAHIChart.h"
#include "Graphs/gUsageChart.h"
#include "Graphs/gTTIAChart.h"
#include "cprogressbar.h"
#include "saveGraphLayoutSettings.h"

#include "mainwindow.h"

extern MainWindow *mainwin;


qint64 convertDateToTimeRtn(const QDate &date,int hours,int min,int sec) {
    // date.startOfDay was introduced in 5.14.  ubuntu 22.04 LTS used QT5.15
    #if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        return date.startOfDay().addSecs(((hours*60+min)*60)+sec).toMSecsSinceEpoch();
    #else
        // this version works in QT 5.12
        return QDateTime(date).addSecs(((hours*60+min)*60)+sec).toMSecsSinceEpoch();
    #endif
}

qint64 convertDateToStartTime(const QDate &date) {
    return convertDateToTimeRtn(date,0,10,0);
}
qint64 convertDateToEndTime(const QDate &date) {
    return convertDateToTimeRtn(date,23,0,0);
}
QDate convertTimeToDate(qint64 time) {
    return QDateTime::fromMSecsSinceEpoch(time).date();
}

Overview::Overview(QWidget *parent, gGraphView *shared) :
    QWidget(parent),
    ui(new Ui::Overview),
    m_shared(shared)
{
    chartsToBeMonitored.clear();
    chartsEmpty.clear();;
    ui->setupUi(this);

    // Set Date controls locale to 4 digit years
    QLocale locale = QLocale::system();
    shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

    ui->dateStart->setDisplayFormat(shortformat);
    ui->dateEnd->setDisplayFormat(shortformat);
    #if QT_VERSION < QT_VERSION_CHECK(6,7,0)
        ui->dateStart->setTimeSpec(Qt::UTC);
        ui->dateEnd->setTimeSpec(Qt::UTC);
    #else
        ui->dateStart->setTimeZone(QTimeZone("UTC"));
        ui->dateEnd->setTimeZone(QTimeZone("UTC"));
    #endif

    Qt::DayOfWeek dow = firstDayOfWeekFromLocale();

    ui->dateStart->calendarWidget()->setFirstDayOfWeek(dow);
    ui->dateEnd->calendarWidget()->setFirstDayOfWeek(dow);


    // Stop both calendar drop downs highlighting weekends in red
    QTextCharFormat format = ui->dateStart->calendarWidget()->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateStart->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Saturday, format);
    ui->dateEnd->calendarWidget()->setWeekdayTextFormat(Qt::Sunday, format);
    samePage=true;

    // Connect the signals to update which days have CPAP data when the month is changed
    QVBoxLayout *framelayout = new QVBoxLayout;
    ui->graphArea->setLayout(framelayout);

    QFrame *border = new QFrame(ui->graphArea);

    framelayout->setContentsMargins(1, 1, 1, 1);
    border->setFrameShape(QFrame::StyledPanel);
    framelayout->addWidget(border,1);

    ///////////////////////////////////////////////////////////////////////////////
    // Create the horizontal layout to hold the GraphView object and it's scrollbar
    ///////////////////////////////////////////////////////////////////////////////
    layout = new QHBoxLayout(border);
    layout->setSpacing(0); // remove the ugly margins/spacing
    layout->setContentsMargins(0, 0, 0, 0);
    border->setLayout(layout);
    border->setAutoFillBackground(false);

    ///////////////////////////////////////////////////////////////////////////////
    // Create the GraphView Object
    ///////////////////////////////////////////////////////////////////////////////
    GraphView = new gGraphView(ui->graphArea, m_shared, this);
    GraphView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    GraphView->setEmptyText(STR_Empty_NoData);

    // Create the custom scrollbar and attach to GraphView
    scrollbar = new MyScrollBar(ui->graphArea);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);
    GraphView->setScrollBar(scrollbar);

    // Add the graphView and scrollbar to the layout.
    layout->addWidget(GraphView, 1);
    layout->addWidget(scrollbar, 0);
    layout->layout();

    ///////////////////////////////////////////////////////////////////////////////
    // Create date display at bottom of page
    ///////////////////////////////////////////////////////////////////////////////
    dateLabel = new MyLabel(this);
    dateLabel->setAlignment(Qt::AlignVCenter);
    dateLabel->setText("[Date Widget]");
    QFont font = dateLabel->font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateLabel->setFont(font);
    QPalette palette = dateLabel->palette();
    palette.setColor(QPalette::Base, Qt::blue);
    dateLabel->setPalette(palette);

    ui->dateLayout->addWidget(dateLabel,1);

    ///////////////////////////////////////////////////////////////////////////////
    // Rebuild contents
    ///////////////////////////////////////////////////////////////////////////////
    settingsLoaded=false;
    RebuildGraphs(false);

    ui->rangeCombo->setCurrentIndex(p_profile->general->lastOverviewRange());

    icon_on = new QIcon(":/icons/session-on.png");
    icon_off = new QIcon(":/icons/session-off.png");
    icon_up_down = new QIcon(":/icons/up-down.png");
    icon_warning = new QIcon(":/icons/warning.png");

    GraphView->resetLayout();
    GraphView->SaveDefaultSettings();
    GraphView->LoadSettings("Overview"); //no trans

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
	dateErrorDisplay = new DateErrorDisplay(this);

    connect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));
    connect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));
    connect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    connect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    connect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
    connect(GraphView, SIGNAL(XBoundsChanged(qint64 ,qint64)), this, SLOT(on_XBoundsChanged(qint64 ,qint64)));
    saveGraphLayoutSettings=nullptr;
}

Overview::~Overview()
{
    disconnect(GraphView, SIGNAL(XBoundsChanged(qint64 ,qint64)), this, SLOT(on_XBoundsChanged(qint64 ,qint64)));
    disconnect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));
    disconnect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    disconnect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    disconnect(ui->dateEnd->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateEnd_currentPageChanged(int, int)));
    disconnect(ui->dateStart->calendarWidget(), SIGNAL(currentPageChanged(int, int)), this, SLOT(dateStart_currentPageChanged(int, int)));
    disconnectgSummaryCharts() ;

    // Save graph orders and pin status, etc...
    GraphView->SaveSettings("Overview");//no trans

    delete ui;
	delete dateErrorDisplay;
    delete icon_on ;
    delete icon_off ;
    delete icon_up_down ;
    delete icon_warning ;
    if (saveGraphLayoutSettings!=nullptr) delete saveGraphLayoutSettings;
}

void Overview::ResetFont()
{
    QFont font = QApplication::font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateLabel->setFont(font);
}


void Overview::connectgSummaryCharts()
{
    for (auto it = chartsToBeMonitored.begin(); it != chartsToBeMonitored.end(); ++it) {
        gSummaryChart* sc =it.key();
        connect(sc, SIGNAL(summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)), this, SLOT(on_summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)));
    }
}

void Overview::disconnectgSummaryCharts()
{
    for (auto it = chartsToBeMonitored.begin(); it != chartsToBeMonitored.end(); ++it) {
        gSummaryChart* sc =it.key();
        disconnect(sc, SIGNAL(summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)), this, SLOT(on_summaryChartEmpty(gSummaryChart*,qint64,qint64,bool)));
    }
    chartsToBeMonitored.clear();
    chartsEmpty.clear();;
}

void Overview::on_summaryChartEmpty(gSummaryChart*sc,qint64 firstI,qint64 lastI,bool empty)
{

    auto it = chartsToBeMonitored.find(sc);
    if (it==chartsToBeMonitored.end()) {
        return;
    }
    gGraph* graph=it.value();
    if (empty) {
        // on next range change allow empty flag to be recalculated
        chartsEmpty.insert(sc,graph);
    } else {
        // The chart has some entry with data.
        chartsEmpty.remove(sc);
        updateGraphCombo();
    }
    Q_UNUSED(firstI);
    Q_UNUSED(lastI);
};


// Create all the graphs for the Overview page
void Overview::CreateAllGraphs() {
    ///////////////////////////////////////////////////////////////////////////////
    // Add all the graphs
    // Process is to createGraph() to make the graph object, then add a layer
    // that provides the contents for that graph.
    ///////////////////////////////////////////////////////////////////////////////
    if (chartsToBeMonitored.size()>0) {
        disconnectgSummaryCharts();
    }
    chartsEmpty.clear();;
    chartsToBeMonitored.clear();;

    // Add graphs that are always included
    ChannelID ahicode = p_profile->general->calculateRDI() ? CPAP_RDI : CPAP_AHI;
    if (ahicode == CPAP_RDI) {
        AHI = createGraph("AHIBreakdown", STR_TR_RDI, tr("Respiratory\nDisturbance\nIndex"));
    } else {
        AHI = createGraph("AHIBreakdown", STR_TR_AHI, tr("Apnea\nHypopnea\nIndex"));
    }

    ahi = new gAHIChart();
    AHI->AddLayer(ahi);
    //chartsToBeMonitored.insert(ahi,AHI);

    UC = createGraph(STR_GRAPH_Usage, tr("Usage"), tr("Usage\n(hours)"));
    uc = new gUsageChart();
    UC->AddLayer(uc);
    //chartsToBeMonitored.insert(uc,UC);

    STG = createGraph("New Session", tr("Session Times"), tr("Session Times"),  YT_Time);
    stg = new gSessionTimesChart();
    STG->AddLayer(stg);

    PR = createGraph("Pressure Settings", STR_TR_Pressure, STR_TR_Pressure + "\n(" + STR_UNIT_CMH2O + ")");
    pres = new gPressureChart();
    PR->AddLayer(pres);
    //chartsToBeMonitored.insert(pres,PR);

    TTIA = createGraph("TTIA", tr("Total Time in Apnea"), tr("Total Time in Apnea\n(Minutes)"));
    ttia = new gTTIAChart();
    TTIA->AddLayer(ttia);
    //chartsToBeMonitored.insert(ttia,TTIA);

    // Add graphs for all channels that have been marked in Preferences Dialog as wanting a graph
    QHash<ChannelID, schema::Channel *>::iterator chit;
    QHash<ChannelID, schema::Channel *>::iterator chit_end = schema::channel.channels.end();
    for (chit = schema::channel.channels.begin(); chit != chit_end; ++chit) {
        schema::Channel * chan = chit.value();

        if (chan->showInOverview()) {
            ChannelID code = chan->id();
            QString name = chan->fullname();

            #if defined(STEADY_BREATHING)
            if (AppSetting->steadyBreathing()!=SB_ACTIVE ) {
                if (code == CPAP_SteadyBreathingFlag)  continue;
                if (code == CPAP_SteadyBreathing)  continue;
            }
            #endif

            if (name.length() > 16) name = chan->label();
            gGraph *G = createGraph(chan->code(), name, chan->description());
            gSummaryChart * sc = nullptr;
            if ((chan->type() == schema::FLAG) || (chan->type() == schema::MINOR_FLAG)) {
                sc = new gSummaryChart(chan->code(), chan->machtype()); // gts was MT_CPAP
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::SPAN) {
                sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_SPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::WAVEFORM) {
                if ((code==CPAP_AHI)||(code==CPAP_Pressure) ) {
                    DEBUGF O("SKIPPING") NAME(code) Q(code);
                    //skip if channel is for AHI.
                    continue;
                }
                sc= new gSummaryChart(code, chan->machtype());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            } else if (chan->type() == schema::UNKNOWN) {
                sc = new gSummaryChart(chan->code(), MT_CPAP);
                sc->addCalc(code, ST_CPH, schema::channel[code].defaultColor());
                G->AddLayer(sc);
                chartsToBeMonitored.insert(sc,G);
            }
            if (sc!= nullptr) {
                sc ->reCalculate();
            }
        } // if showInOverview()
    } // for chit
    #ifndef REMOVE_FITNESS
    /* To enable these changes:   change the REMOTE_FITNESS define in appsettings.h */

    // Note The following do not use gSummaryChart. They use gOverviewGraph instead. and can not be monitored.
    WEIGHT = createGraph(STR_GRAPH_Weight, STR_TR_Weight, STR_TR_Weight, YT_Weight);
    weight = new gOverviewGraph("Weight", GT_LINE);
    weight->setMachineType(MT_JOURNAL);
    weight->addSlice(Journal_Weight, QColor("black"), ST_SETAVG);
    WEIGHT->AddLayer(weight);

    BMI = createGraph(STR_GRAPH_BMI, STR_TR_BMI, tr("Body\nMass\nIndex"));
    bmi = new gOverviewGraph("BMI", GT_LINE);
    bmi->setMachineType(MT_JOURNAL);
    bmi->addSlice(Journal_BMI, QColor("black"), ST_SETAVG);
    BMI->AddLayer(bmi);

    ZOMBIE = createGraph(STR_GRAPH_Zombie, STR_TR_Zombie, tr("How you felt\n(1-10)"));
    zombie = new gOverviewGraph("Feeling", GT_LINE);
    zombie->setMachineType(MT_JOURNAL);
    zombie->addSlice(Journal_ZombieMeter, QColor("black"), ST_SETAVG);
    ZOMBIE->AddLayer(zombie);
    #endif

    connectgSummaryCharts();
}


// Recalculates Overview chart info
void Overview::RebuildGraphs(bool reset)
{
    qint64 minx, maxx;
    if (reset) {
        GraphView->GetXBounds(minx, maxx);
    }
    if (settingsLoaded) GraphView->SaveSettings("Overview");
    settingsLoaded=false;
    minRangeStartDate=p_profile->LastDay(MT_CPAP);
    maxRangeEndDate=minRangeStartDate.addDays(-1);      // force a range change;
    disconnectgSummaryCharts() ;
    GraphView->trashGraphs(true);       // Remove all existing graphs
    CreateAllGraphs();
    GraphView->LoadSettings("Overview");
    settingsLoaded = true;

    if (reset) {
        GraphView->resetLayout();
        GraphView->setDay(nullptr);
        SetXBounds(minx, maxx, 0, false);
        GraphView->resetLayout();
        updateGraphCombo();
    }
}

// Create an overview graph, adding it to the overview gGraphView object
// param QString name  The title of the graph
// param QString units The units of measurements to show in the popup
gGraph *Overview::createGraph(QString code, QString name, QString units, YTickerType yttype)
{
    int default_height = AppSetting->graphHeight();
    gGraph *g = new gGraph(code, GraphView, name, units, default_height, 0);

    gYAxis *yt;

    switch (yttype) {
    case YT_Time:
        yt = new gYAxisTime(false); // Time scale false=24hourFormat
        break;

    case YT_Weight:
        yt = new gYAxisWeight(p_profile->general->unitSystem());
        break;

    default:
        yt = new gYAxis(); // Plain numeric scale
        break;
    }

    g->AddLayer(yt, LayerLeft, gYAxis::Margin);
    gXAxisDay *x = new gXAxisDay();
    g->AddLayer(x, LayerBottom, 0, gXAxisDay::Margin);
    g->AddLayer(new gXGrid());
    return g;
}

void Overview::on_LineCursorUpdate(double time)
{
    if (time > 1) {
        // even though the generated string is displayed to the user
        // no time zone conversion is neccessary, so pass UTC
        // to prevent QT from automatically converting to local time
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(time, QTimeZone::systemTimeZone()/*, Qt::UTC*/);
        QString txt = dt.toString("dd MMM yyyy (dddd)");
        dateLabel->setText(txt);
    } else dateLabel->setText(QString(GraphView->emptyText()));
}

void Overview::on_RangeUpdate(double minx, double /* maxx */)
{
    if (minx > 1) {
        dateLabel->setText(GraphView->getRangeInDaysString());
    } else {
        dateLabel->setText(QString(GraphView->emptyText()));
    }
}

void Overview::ReloadGraphs()
{
    GraphView->setDay(nullptr);
    updateCube();

    on_rangeCombo_activated(ui->rangeCombo->currentIndex());
}

void Overview::setGraphText () {
    int numOff = 0;
    int numTotal = 0;

    gGraph *g;
    for (int i=0;i<GraphView->size();i++) {
        g=(*GraphView)[i];
        if (!g->isEmpty()) {
            numTotal++;
            if (!g->visible()) {
                numOff++;
            }
            //DEBUGFW  Q(numTotal) Q(numOff)  Q(g->name());
        }
    }
    ui->graphCombo->setItemIcon(0, numOff ? *icon_warning : *icon_up_down);
    QString graphText;
    int lastIndex = ui->graphCombo->count()-1;

    if (numOff == 0) {
        // all graphs are shown
        graphText = QObject::tr("%1 Graphs").arg(numTotal);
        ui->graphCombo->setItemText(lastIndex,STR_HIDE_ALL_GRAPHS);
    } else {
        // some graphs are hidden
        graphText = QObject::tr("%1 of %2 Graphs").arg(numTotal-numOff).arg(numTotal);
        if (numOff == numTotal) {
            // all graphs are hidden
            ui->graphCombo->setItemText(lastIndex,STR_SHOW_ALL_GRAPHS);
        }
    }
    ui->graphCombo->setItemText(0, graphText);
}

void Overview::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;

    ui->graphCombo->addItem(*icon_up_down, "", true); 
    for (int i = 0; i < GraphView->size(); i++) {
        g = (*GraphView)[i];

        if (g->isEmpty()) { continue; }

        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on, g->title(), true);
        } else {
            ui->graphCombo->addItem(*icon_off, g->title(), false);
        }
    }
    ui->graphCombo->addItem(*icon_on,STR_HIDE_ALL_GRAPHS,true);
    ui->graphCombo->setCurrentIndex(0);
    setGraphText();
    updateCube();
}

void Overview::RedrawGraphs()
{
    GraphView->redraw();
}

// Updates calendar format and profile data.
void Overview::UpdateCalendarDay(QDateEdit *dateedit, QDate date,bool startDateWidget)
{
    QCalendarWidget *calendar = dateedit->calendarWidget();
    bool hascpap = p_profile->FindDay(date, MT_CPAP) != nullptr;
    bool hasoxi = p_profile->FindDay(date, MT_OXIMETER) != nullptr;

    QTextCharFormat normal;
    normal.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
    normal.setFontWeight(QFont::Bold);
    if (startDateWidget) {
        // reduce font size if range can not be reached
        if (date > uiEndDate) {
            normal.setFontWeight(QFont::Light);
        }
    } else if (date < uiStartDate) {
        // reduce font size if range can not be reached
        normal.setFontWeight(QFont::Light);
    }

    if (hascpap) {
        if (hasoxi) {
            normal.setForeground(QBrush(Qt::red, Qt::SolidPattern));
            calendar->setDateTextFormat(date, normal);
        } else {
            normal.setForeground(QBrush(Qt::blue, Qt::SolidPattern));
            calendar->setDateTextFormat(date, normal);
        }
    } else if (p_profile->GetDay(date)) {
        calendar->setDateTextFormat(date, normal);
    } else {
        // is invalid.
        normal.setForeground(QBrush(Qt::gray, Qt::SolidPattern));
        normal.setFontWeight(QFont::Light);
        calendar->setDateTextFormat(date, normal);
    }

    calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}

void Overview::SetXBounds(qint64 start, qint64 end, short group , bool refresh )
{
     GraphView->SetXBounds( start , end ,group,refresh);
}

void Overview::SetXBounds(QDate & start, QDate&  end, short group , bool refresh ) {
    SetXBounds(convertDateToStartTime(start),convertDateToEndTime(end),group,refresh);
}


void Overview::on_XBoundsChanged(qint64 start,qint64 end)
{
    displayStartDate = convertTimeToDate(start);
    displayEndDate = convertTimeToDate(end);

    bool largerRange=false;
    if (displayStartDate>maxRangeEndDate || minRangeStartDate>displayEndDate) {
        // have non-overlaping ranges
        // Only occurs when custom mode is switched to/from a latest mode. custom mode to/from last week.
        // All other displays expand the existing range.
        // reset all empty flags to not empty
        largerRange=true;
        chartsEmpty = QHash<gSummaryChart*, gGraph*>( chartsToBeMonitored );
        minRangeStartDate = displayStartDate;
        maxRangeEndDate   = displayEndDate;
    } else {
        // new range overlaps with old range
        if (displayStartDate<minRangeStartDate) {
            largerRange=true;
            minRangeStartDate = displayStartDate;
        }
        if (displayEndDate>maxRangeEndDate) {
            largerRange=true;
            maxRangeEndDate   = displayEndDate;
        }
    }

    if (largerRange) {
        for (auto it= chartsEmpty.begin();it!=chartsEmpty.end();it++) {
            gSummaryChart* sc = it.key();
            bool empty=sc->isEmpty();
            if (empty) {
                sc ->reCalculate();
                GraphView->updateScale();
                GraphView->redraw();
                GraphView->timedRedraw(150);
            }
        }
        chartsEmpty.clear();
        updateGraphCombo();
    }
    #if defined(ENABLE_START_END_WIDGET_DISPLAY_ACTUAL_GRAPH_RANGE)
    setRange(displayStartDate,displayEndDate, false);
    #endif
}

void Overview::dateStart_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();

    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateStart, d,true /*startWidget*/);
    }
}

void Overview::dateEnd_currentPageChanged(int year, int month)
{
    QDate d(year, month, 1);
    int dom = d.daysInMonth();
    for (int i = 1; i <= dom; i++) {
        d = QDate(year, month, i);
        UpdateCalendarDay(ui->dateEnd, d,false /* not startWidget*/);
    }
}


void Overview::on_dateEnd_dateChanged(const QDate &date)
{
	if (date<uiStartDate) {
		dateErrorDisplay->error(false,date);
		return;
	}
    QDate d2(date);
    if (customMode) {
        p_profile->general->setCustomOverviewRangeEnd(d2);
    }
    setRange(uiStartDate,d2, true);
    if ( (uiStartDate.month() ==uiEndDate.month()) && (uiStartDate.year() ==uiEndDate.year()) ) {
        dateEnd_currentPageChanged(uiEndDate.year(),uiEndDate.month());
    }
}

void Overview::on_dateStart_dateChanged(const QDate &date)
{
	if (date>uiEndDate) {
		// change date back to last date.
		dateErrorDisplay->error(true,date);
		return;
    }
    QDate d1(date);
    if (customMode) {
        p_profile->general->setCustomOverviewRangeStart(d1);
    }
    setRange(d1, uiEndDate, true);
    if ( (uiStartDate.month() ==uiEndDate.month()) && (uiStartDate.year() ==uiEndDate.year()) ) {
        dateStart_currentPageChanged(uiStartDate.year(),uiStartDate.month());
    }
}

// Zoom to 100% button clicked or called back from 100% zoom in popup menu
void Overview::on_zoomButton_clicked()
{
    // the Current behaviour is to zoom back to the last range created by on_rangeCombo_activation
    // This change preserves OSCAR behaviour
    on_rangeCombo_activated(p_profile->general->lastOverviewRange());  // type of range in last use
}


void Overview::ResetGraphLayout()
{
    GraphView->resetLayout();
}

void Overview::ResetGraphOrder(int type)
{
    Q_UNUSED(type)
    GraphView->resetGraphOrder(false);
    ResetGraphLayout();
}

// Process new range selection from combo button
void Overview::on_rangeCombo_activated(int index)
{
    // Exclude Journal in calculating the last day
    QDate end = p_profile->LastDay(MT_CPAP);
    end = max(end, p_profile->LastDay(MT_OXIMETER));
    end = max(end, p_profile->LastDay(MT_POSITION));
    end = max(end, p_profile->LastDay(MT_SLEEPSTAGE));
    QDate start;


    if (index == 0) {
        start = end.addDays(-6);
    } else if (index == 1) {
        start = end.addDays(-13);
    } else if (index == 2) {
        start = end.addMonths(-1).addDays(1);
    } else if (index == 3) {
        start = end.addMonths(-2).addDays(1);
    } else if (index == 4) {
        start = end.addMonths(-3).addDays(1);
    } else if (index == 5) {
        start = end.addMonths(-6).addDays(1);
    } else if (index == 6) {
        start = end.addYears(-1).addDays(1);
    } else if (index == 7) { // Everything
        start = p_profile->FirstDay();
    } else if (index == 8 || index == 9) { // Custom
        // Validate save Overview Custom Range for first access.
        if (!p_profile->general->customOverviewRangeStart().isValid()
            || (!p_profile->general->customOverviewRangeEnd().isValid() )
            || (index==9 /* New Coustom mode - to reset custom range to displayed date range*/)
        ) {
            // Reset Custom Range to current range displayed
            // on first initialization of this version of OSCAR
            // or on new custom Mode to reset range.
            qint64 istart,iend;
            GraphView->GetXBounds(istart , iend);
            start = QDateTime::fromMSecsSinceEpoch( istart ).date();
            end =   QDateTime::fromMSecsSinceEpoch( iend ).date();
            p_profile->general->setCustomOverviewRangeStart(start);
            p_profile->general->setCustomOverviewRangeEnd(end);
            index=8;
            ui->rangeCombo->setCurrentIndex(index);
        } else {
            // have a change in RangeCombo selection. Use last saved values.
            start = p_profile->general->customOverviewRangeStart() ;
            end   = p_profile->general->customOverviewRangeEnd() ;
        }
    }

    if (start < p_profile->FirstDay()) { start = p_profile->FirstDay(); }

    customMode = (index == 8) ;
    #if !defined(ENABLE_GENERAL_MODIFICATION_OF_CALENDARS)
    ui->dateStartLabel->setEnabled(customMode);
    ui->dateEndLabel->setEnabled(customMode);
    ui->dateEnd->setEnabled(customMode);
    ui->dateStart->setEnabled(customMode);
    #endif


    p_profile->general->setLastOverviewRange(index);  // type of range in last use

    // Ensure that all summary files are available and update version numbers if required
    int size = start.daysTo(end);
    // qDebug() << "Overview range combo from" << start << "to" << end << "with" << size << "days";
    QDate dateback = end;
    CProgressBar * progress = new CProgressBar (QObject::tr("Loading summaries"), mainwin, size);
    for (int i=1; i < size; ++i) {
        progress->add(1);
        auto di = p_profile->daylist.find(dateback);
        dateback = dateback.addDays(-1);
        if (di == p_profile->daylist.end())  // Check for no Day entry
           continue;
        Day * day = di.value();
        if (!day)
            continue;
        if (day->size() <= 0)
            continue;
        day->OpenSummary();     // This can be slow if summary needs to be updated to new version
    }
    progress->close();
    delete progress;

    setRange(start, end);
}


DateErrorDisplay::DateErrorDisplay (Overview* overview)
	:	m_overview(overview)
{
	m_visible=false;
	m_timer = new QTimer();
	connect(m_timer, SIGNAL(timeout()),this, SLOT(timerDone()));
};

DateErrorDisplay::~DateErrorDisplay() {
	disconnect(m_timer, SIGNAL(timeout()),this, SLOT(timerDone()));
	delete m_timer;
};

void DateErrorDisplay::cancel() {
	m_visible=false;
	m_timer->stop();
};

void DateErrorDisplay::timerDone() {
	m_visible=false;
	m_overview->resetUiDates();
	m_overview->graphView()->m_parent_tooltip->cancel();
};

int Overview::calculatePixels(bool startDate,ToolTipAlignment& align) {
	// Center error message over start and end dates combined.
	// Other allignement were tested but this is the best for this problem.
	Q_UNUSED(startDate);
	int space=4;
	align=TT_AlignCenter;
	return ((4*space) + ui->label_3->width() + ui->rangeCombo->width() + ui->dateStartLabel->width()  +ui->dateStart->width() );
}

void DateErrorDisplay::error(bool startDate,const QDate& dateEntered) {
	m_startDate =m_overview->uiStartDate;
	m_endDate =m_overview->uiEndDate;
	ToolTipAlignment align=TT_AlignCenter;


	QString dateFormatted =dateEntered.toString(m_overview->shortformat);

	QString txt = QString(tr("ERROR\nThe start date MUST be before the end date"));
	txt.append("\n");
	if (startDate) {
		txt.append(tr("The entered start date %1 is after the end date %2").arg(dateFormatted).arg(m_endDate.toString(m_overview->shortformat)));
		txt.append(tr("\nHint: Change the end date first"));
	} else {
		txt.append(tr("The entered end date %1 ").arg(dateFormatted));
		txt.append(tr("is before the start date %1").arg(m_startDate.toString(m_overview->shortformat)));
		txt.append(tr("\nHint: Change the start date first"));
	}

	gGraphView* gv=m_overview->graphView();

	int pixelsFromLeft = m_overview->calculatePixels(startDate,align);
	int pixelsAboveBottom = 0;
	int warningDurationMS =4000;
	int resetUiDatesTimerDelayMS =1000;
	//QFont* font=mediumfont;
	QFont* font=defaultfont;

	gv->m_parent_tooltip->display(gv,txt,pixelsAboveBottom,pixelsFromLeft, align, warningDurationMS , font);

	m_timer->setInterval( resetUiDatesTimerDelayMS );
	m_timer->setSingleShot(true);
	m_timer->start();

};

void Overview::resetUiDates() {
    ui->dateStart->blockSignals(true);
    ui->dateStart->setMinimumDate(p_profile->FirstDay());  // first and last dates for ANY machine type
    ui->dateStart->setMaximumDate(p_profile->LastDay());
    ui->dateStart->setDate(uiStartDate);
    ui->dateStart->blockSignals(false);

    ui->dateEnd->blockSignals(true);
    ui->dateEnd->setMinimumDate(p_profile->FirstDay());  // first and last dates for ANY machine type
    ui->dateEnd->setMaximumDate(p_profile->LastDay());
    ui->dateEnd->setDate(uiEndDate);
    ui->dateEnd->blockSignals(false);
}

// Saves dates in UI, clicks zoom button, and updates combo box
// 1. Updates the dates in the start / end date boxs
// 2. optionally also changes display range for graphs.
void Overview::setRange(QDate& start, QDate& end, bool updateGraphs/*zoom*/)
{

	if (start>end) {
		// this is an ERROR and shold NEVER occur.
		return;
	}

    // first setting of the date (setDate) will cause pageChanged to be executed.
    // The pageChanged processing requires access to the other ui's date.
    // so save them to memory before the first call.
    uiStartDate =start;
    uiEndDate = end;

    //bool nextSamePage= start.daysTo(end)<=31;
    bool nextSamePage= (start.year() == end.year() && start.month() == end.month()) ;
    if (samePage>0 ||nextSamePage) {
        // The widgets do not signal pageChanged on opening - since the page hasn't changed.
        // however the highlighting may need to be changed.
        // The following forces a page change signal when calendar widget is opened.
        ui->dateStart->calendarWidget()->setCurrentPage(1970,1);
        ui->dateEnd->calendarWidget()->setCurrentPage(1970,1);
        samePage=nextSamePage;
    }

	resetUiDates();
    if (updateGraphs) SetXBounds(uiStartDate,uiEndDate);
    updateGraphCombo();
}

void Overview::showGraph(int index,bool show, bool updateGraph) {
    ui->graphCombo->setItemData(index,show,Qt::UserRole);
    ui->graphCombo->setItemIcon(index, show ? *icon_on : *icon_off);
    if (!updateGraph) return;
    QString graphName = ui->graphCombo->itemText(index);
    gGraph* graph=GraphView->findGraphTitle(graphName);
    if (graph) graph->setVisible(show);
}

void Overview::showAllGraphs(bool show) {
    //Skip over first button - label for comboBox
    int lastIndex = ui->graphCombo->count()-1;
    for (int i=1;i<lastIndex;i++) {
        showGraph(i,show,true);
    }
}


void Overview::on_graphCombo_activated(int index)
{
   if (index<0) return;
    if (index > 0) {
        bool nextOn =!ui->graphCombo->itemData(index,Qt::UserRole).toBool();
        int lastIndex = ui->graphCombo->count()-1;
        if ( index == lastIndex ) {
            // user just pressed hide show button - toggle states of the button and apply the new state
            showAllGraphs(nextOn);
            showGraph(index,nextOn,false);
        } else {
            showGraph(index,nextOn,true);
        }
        ui->graphCombo->showPopup();
    }

    ui->graphCombo->setCurrentIndex(0);
    updateCube();
    setGraphText();
    GraphView->updateScale();
    GraphView->redraw();
}
void Overview::updateCube()
{
    if ((GraphView->visibleGraphs() == 0)) {
        if (ui->graphCombo->count() > 0) {
            GraphView->setEmptyText(STR_Empty_NoGraphs);
        } else {
            GraphView->setEmptyText(STR_Empty_NoData);
        }
    }
}

void Overview::on_graphHelp_clicked() {
    if (!saveGraphLayoutSettings) {
        saveGraphLayoutSettings= new SaveGraphLayoutSettings("overview",this);
    }
    if (saveGraphLayoutSettings) {
        saveGraphLayoutSettings->hintHelp();
    }
}

void Overview::on_layout_clicked() {
    if (!saveGraphLayoutSettings) {
        saveGraphLayoutSettings= new SaveGraphLayoutSettings("overview",this);
    }
    if (saveGraphLayoutSettings) {
        saveGraphLayoutSettings->triggerLayout(GraphView);
    }
}



