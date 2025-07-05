/* Daily Panel
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include <QTextCharFormat>
#include <QPalette>
#include <QTimeZone>
#include <QTextBlock>
#include <QColorDialog>
#include <QSpacerItem>
#include <QElapsedTimer>
#include <QBuffer>
#include <QPixmap>
#include <QMessageBox>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSpacerItem>
#include <QFontMetrics>
#include <QLabel>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#define CONFIGURE_MODE
#define COMBINE_MODE_3

#include "saveGraphLayoutSettings.h"
#include "daily.h"
#include "ui_daily.h"
#include "dailySearchTab.h"

#include "common_gui.h"
#include "SleepLib/profiles.h"
#include "SleepLib/session.h"
#include "Graphs/gLineOverlay.h"
#include "Graphs/gFlagsLine.h"
#include "Graphs/gFooBar.h"
#include "Graphs/gXAxis.h"
#include "Graphs/gYAxis.h"
#include "Graphs/gSegmentChart.h"
#include "Graphs/gStatsLine.h"
#include "Graphs/gdailysummary.h"
#include "Graphs/MinutesAtPressure.h"

extern MainWindow * mainwin;

QString htmlLeftHeader;
QString htmlLeftAHI;
QString htmlLeftMachineInfo;
QString htmlLeftSleepTime;
QString htmlLeftIndices;
QString htmlLeftPieChart = "";
QString htmlLeftNoHours = "";
QString htmlLeftStatistics;
QString htmlLeftOximeter;
QString htmlLeftMachineSettings;
QString htmlLeftSessionInfo;
QString htmlLeftFooter;

extern ChannelID PRS1_PeakFlow;
extern ChannelID Prisma_ObstructLevel, Prisma_rMVFluctuation, Prisma_rRMV, Prisma_PressureMeasured, Prisma_FlowFull;

#define WEIGHT_SPIN_BOX_DECIMALS 1

// This was Sean Stangl's idea.. but I couldn't apply that patch.
inline QString channelInfo(ChannelID code) {
    return schema::channel[code].fullname()+"\n"+schema::channel[code].description()+"\n("+schema::channel[code].units()+")";
//    return schema::channel[code].fullname()+"\n"+schema::channel[code].description()
//            + (schema::channel[code].units() != "0" ? "\n("+schema::channel[code].units()+")" : "");
}

// Charts displayed on the Daily page are defined in the Daily::Daily constructor.  They consist of some hard-coded charts and a table
// of channel codes for which charts are generated.  If the list of channel codes is changed, the graph order lists below will need to
// be changed correspondingly.
//
// Note that "graph codes" are strings used to identify graphs and are not the same as "channel codes."  The mapping between channel codes
// and graph codes is found in schema.cpp.  (What we here call 'graph cdoes' are called 'lookup codes' in schema.cpp.)
//
//
// List here the graph codes in the order they are to be displayed.
// Do NOT list a code twice, or Oscar will crash when the profile is closed!
//
// Standard graph order
const QList<QString> standardGraphOrder = {
    STR_GRAPH_SleepFlags, STR_GRAPH_FlowRate, STR_GRAPH_Pressure, STR_GRAPH_PressureWave, STR_GRAPH_LeakRate, STR_GRAPH_FlowLimitation,
    STR_GRAPH_Snore, STR_GRAPH_IE_Ratio, STR_GRAPH_FlowAbnormality, STR_GRAPH_TidalVolume, STR_GRAPH_MaskPressure, STR_GRAPH_RespRate, STR_GRAPH_MinuteVent,
    STR_GRAPH_PTB, STR_GRAPH_RespEvent, STR_GRAPH_Ti, STR_GRAPH_Te, STR_GRAPH_IE,
    STR_GRAPH_SleepStage, STR_GRAPH_Inclination, STR_GRAPH_Orientation, STR_GRAPH_Motion, STR_GRAPH_TestChan1,
    STR_GRAPH_Oxi_Pulse, STR_GRAPH_Oxi_SPO2, STR_GRAPH_Oxi_Perf, STR_GRAPH_Oxi_Plethy,
    STR_GRAPH_AHI, STR_GRAPH_TAP, STR_GRAPH_ObstructLevel, STR_GRAPH_PressureMeasured, STR_GRAPH_rRMV, STR_GRAPH_rMVFluctuation,
    STR_GRAPH_FlowFull
    #if defined(STEADY_BREATHING)
    , STR_GRAPH_CPAP_SteadyBreathing
    #endif
};

// Advanced graph order
const QList<QString> advancedGraphOrder = {
    STR_GRAPH_SleepFlags, STR_GRAPH_FlowRate, STR_GRAPH_PressureWave, STR_GRAPH_MaskPressure, STR_GRAPH_TidalVolume, STR_GRAPH_MinuteVent,
    STR_GRAPH_Ti, STR_GRAPH_Te, STR_GRAPH_IE, STR_GRAPH_FlowLimitation, STR_GRAPH_FlowAbnormality, STR_GRAPH_Pressure, STR_GRAPH_LeakRate, STR_GRAPH_Snore,
    STR_GRAPH_IE_Ratio, STR_GRAPH_RespRate, STR_GRAPH_PTB, STR_GRAPH_RespEvent,
    STR_GRAPH_SleepStage, STR_GRAPH_Inclination, STR_GRAPH_Orientation, STR_GRAPH_Motion, STR_GRAPH_TestChan1,
    STR_GRAPH_Oxi_Pulse, STR_GRAPH_Oxi_SPO2, STR_GRAPH_Oxi_Perf, STR_GRAPH_Oxi_Plethy,
    STR_GRAPH_AHI, STR_GRAPH_TAP, STR_GRAPH_ObstructLevel, STR_GRAPH_PressureMeasured, STR_GRAPH_rRMV, STR_GRAPH_rMVFluctuation,
    STR_GRAPH_FlowFull
    #if defined(STEADY_BREATHING)
    , STR_GRAPH_CPAP_SteadyBreathing
    #endif
};

// CPAP modes that should have Advanced graphs
const QList<int> useAdvancedGraphs = {MODE_ASV, MODE_ASV_VARIABLE_EPAP, MODE_AVAPS};


// QTreeWidgetItem specialization for sorting by timestamp
// Note: This is just used for the consolidated events, but for consistency
// it would make sense to use it for all QTreeWidgetItem's.
class EventTreeWidgetItem : public QTreeWidgetItem {
public:
    EventTreeWidgetItem(const QStringList &strings) : QTreeWidgetItem(strings) {}
    EventTreeWidgetItem(QTreeWidgetItem *parent, const QStringList &strings) : QTreeWidgetItem(parent, strings) {}

    bool operator<(const QTreeWidgetItem &other) const override {
        // Compare timestamps stored in user data
        qint64 thisTime = data(0, Qt::UserRole).toLongLong();
        qint64 otherTime = other.data(0, Qt::UserRole).toLongLong();
        return thisTime < otherTime;
    }
};

void Daily::setCalendarVisible(bool visible)
{
    on_calButton_toggled(visible);
}

void Daily::setSidebarVisible(bool visible)
{
    QList<int> a;


    int panel_width = visible ? AppSetting->dailyPanelWidth() : 0;
    qDebug() << "Daily Left Panel Width is " << panel_width;
    a.push_back(panel_width);
    a.push_back(this->width() - panel_width);
    ui->splitter_2->setStretchFactor(1,1);
    ui->splitter_2->setSizes(a);
    ui->splitter_2->setStretchFactor(1,1);
}

Daily::Daily(QWidget *parent,gGraphView * shared)
    :QWidget(parent), ui(new Ui::Daily)
{
    qDebug() << "Creating new Daily object";
    ui->setupUi(this);

    ui->JournalNotesBold->setShortcut(QKeySequence::Bold);
    ui->JournalNotesItalic->setShortcut(QKeySequence::Italic);
    ui->JournalNotesUnderline->setShortcut(QKeySequence::Underline);

    // Remove Incomplete Extras Tab
    //ui->tabWidget->removeTab(3);

    BookmarksChanged=false;

    lastcpapday=nullptr;

    setSidebarVisible(true);

    layout=new QHBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    dateDisplay=new MyLabel(this);
    dateDisplay->setAlignment(Qt::AlignCenter);
    QFont font = dateDisplay->font();
    font.setPointSizeF(font.pointSizeF()*1.3F);
    dateDisplay->setFont(font);
    QPalette palette = dateDisplay->palette();
    palette.setColor(QPalette::Base, Qt::blue);
    dateDisplay->setPalette(palette);
    //dateDisplay->setTextFormat(Qt::RichText);
    ui->sessionBarLayout->addWidget(dateDisplay,1);

//    const bool sessbar_under_graphs=false;
//    if (sessbar_under_graphs) {
//        ui->sessionBarLayout->addWidget(sessbar,1);
//    } else {
//        ui->splitter->insertWidget(2,sessbar);
//        sessbar->setMaximumHeight(sessbar->height());
//        sessbar->setMinimumHeight(sessbar->height());
//    }


    ui->calNavWidget->setMaximumHeight(ui->calNavWidget->height());
    ui->calNavWidget->setMinimumHeight(ui->calNavWidget->height());
    QWidget *widget = new QWidget(ui->tabWidget);
    sessionbar = new SessionBar(widget);
    sessionbar->setMinimumHeight(25);
    sessionbar->setMouseTracking(true);
    connect(sessionbar, SIGNAL(sessionClicked(Session*)), this, SLOT(doToggleSession(Session*)));
    QVBoxLayout *layout2 = new QVBoxLayout();
    layout2->setContentsMargins(0, 0, 0, 0);
    widget->setLayout(layout2);

    webView=new MyTextBrowser(widget);
    webView->setOpenLinks(false);
    layout2->insertWidget(0,webView, 1);
    layout2->insertWidget(1,sessionbar,0);
    // add the sessionbar after it.

    ui->tabWidget->insertTab(0, widget, QIcon(), tr("Details"));

    ui->graphFrame->setLayout(layout);
    //ui->graphMainArea->setLayout(layout);

    ui->graphMainArea->setAutoFillBackground(false);

    GraphView=new gGraphView(ui->graphFrame,shared);
//    qDebug() << "New GraphView object created in Daily";
//    sleep(3);
    GraphView->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));

    snapGV=new gGraphView(GraphView);
    snapGV->setMinimumSize(172,172);
    snapGV->hideSplitter();
    snapGV->hide();

    scrollbar=new MyScrollBar(ui->graphFrame);
    scrollbar->setOrientation(Qt::Vertical);
    scrollbar->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Expanding);
    scrollbar->setMaximumWidth(20);

    ui->bookmarkTable->setColumnCount(2);
    ui->bookmarkTable->setColumnWidth(0,70);
    //ui->bookmarkTable->setEditTriggers(QAbstractItemView::SelectedClicked);
    //ui->bookmarkTable->setColumnHidden(2,true);
    //ui->bookmarkTable->setColumnHidden(3,true);
    GraphView->setScrollBar(scrollbar);
    layout->addWidget(GraphView,1);
    layout->addWidget(scrollbar,0);

    int default_height = AppSetting->graphHeight();

    gGraph *GAHI = nullptr,
//            *TAP = nullptr,
            *SF = nullptr,
            *AHI = nullptr;

//    const QString STR_GRAPH_DailySummary = "DailySummary";

//    gGraph * SG;
//    graphlist[STR_GRAPH_DailySummary] = SG = new gGraph(STR_GRAPH_DailySummary, GraphView, tr("Summary"), tr("Summary of this daily information"), default_height);
//    SG->AddLayer(new gLabelArea(nullptr),LayerLeft,gYAxis::Margin);
//    SG->AddLayer(new gDailySummary());

    graphlist[STR_GRAPH_SleepFlags] = SF = new gGraph(STR_GRAPH_SleepFlags, GraphView, STR_TR_EventFlags, STR_TR_EventFlags, default_height);
    sleepFlags = SF;
    SF->setPinned(true);


    //============================================
    // Create graphs from 'interesting' CPAP codes
    //
    // If this list of codes is changed, you must
    // also adjust the standard and advanced graph
    // order at the beginning of daily.cpp.
    //============================================
    const ChannelID cpapcodes[] = {
        CPAP_FlowRate, CPAP_Pressure, CPAP_Leak, CPAP_FLG, CPAP_Snore, CPAP_TidalVolume,
        CPAP_MaskPressure, CPAP_RespRate, CPAP_MinuteVent, CPAP_PTB, PRS1_PeakFlow, CPAP_RespEvent, CPAP_Ti, CPAP_Te,
        CPAP_IE, ZEO_SleepStage, POS_Inclination, POS_Orientation, POS_Movement, CPAP_Test1,
        Prisma_ObstructLevel, Prisma_rRMV, Prisma_rMVFluctuation, Prisma_PressureMeasured, Prisma_FlowFull
//      ,  BMC_PressureWave, BMC_FlowAbnormality, BMC_IE_Ratio
        #if defined(STEADY_BREATHING)
        ,    CPAP_SteadyBreathing
        #endif

    };

    // Create graphs from the cpap code list
    int cpapsize = sizeof(cpapcodes) / sizeof(ChannelID);

    for (int i=0; i < cpapsize; ++i) {
        ChannelID code = cpapcodes[i];
        QString cpap_label = schema::channel[code].code();
        DEBUGXD O(i) O(code) O(cpap_label);
        if (graphlist.contains(cpap_label)) {
            // Ignores duplicates labels (e.g., Prisma ones undefined in tests).
            // Otherwise, it leads to segmentation faults during cleanup.
            qDebug() << "Ignoring duplicate code at offset" << i << code << cpap_label;
            continue;
        }
        graphlist[schema::channel[code].code()] = new gGraph(schema::channel[code].code(), GraphView, schema::channel[code].label(), channelInfo(code), default_height);
//        qDebug() << "Creating graph for code" << code << schema::channel[code].code();
    }

    const ChannelID oximetercodes[] = {
        OXI_Pulse, OXI_SPO2, OXI_Perf, OXI_Plethy
    };

    // Add graphs from the Oximeter code list
    int oxisize = sizeof(oximetercodes) / sizeof(ChannelID);

    //int oxigrp=p_profile->ExistsAndTrue("SyncOximetry") ? 0 : 1; // Contemplating killing this setting...
    for (int i=0; i < oxisize; ++i) {
        ChannelID code = oximetercodes[i];
        graphlist[schema::channel[code].code()] = new gGraph(schema::channel[code].code(), GraphView, schema::channel[code].label(), channelInfo(code), default_height);
    }

    // Check for some impossible conditions
    if ( p_profile == nullptr ) {
        qDebug() << "In daily, p_profile is NULL";
        return;
    }
    else if (p_profile->general == nullptr ) {
        qDebug() << "In daily, p_profile->general is NULL";
        return;
    }

    // Decide whether we are using AHI or RDI and create graph for the one we are using
    if (p_profile->general->calculateRDI()) {
        AHI=new gGraph(STR_GRAPH_AHI, GraphView,STR_TR_RDI, channelInfo(CPAP_RDI), default_height);
    } else {
        AHI=new gGraph(STR_GRAPH_AHI, GraphView,STR_TR_AHI, channelInfo(CPAP_AHI), default_height);
    }

    graphlist[STR_GRAPH_AHI] = AHI;

    // Event breakdown graph
    graphlist[STR_GRAPH_EventBreakdown] = GAHI = new gGraph(STR_GRAPH_EventBreakdown, snapGV,tr("Breakdown"),tr("events"),172);
    gSegmentChart * evseg=new gSegmentChart(GST_Pie);
    evseg->AddSlice(CPAP_Hypopnea,QColor(0x40,0x40,0xff,0xff),STR_TR_H);
    evseg->AddSlice(CPAP_Apnea,QColor(0x20,0x80,0x20,0xff),STR_TR_UA);
    evseg->AddSlice(CPAP_Obstructive,QColor(0x40,0xaf,0xbf,0xff),STR_TR_OA);
    evseg->AddSlice(CPAP_ClearAirway,QColor(0xb2,0x54,0xcd,0xff),STR_TR_CA);
    evseg->AddSlice(CPAP_AllApnea,QColor(0x40,0xaf,0xbf,0xff),STR_TR_A);
    evseg->AddSlice(CPAP_RERA,QColor(0xff,0xff,0x80,0xff),STR_TR_RE);
    evseg->AddSlice(CPAP_NRI,QColor(0x00,0x80,0x40,0xff),STR_TR_NR);
    evseg->AddSlice(CPAP_FlowLimit,QColor(0x40,0x40,0x40,0xff),STR_TR_FL);
    evseg->AddSlice(CPAP_SensAwake,QColor(0x40,0xC0,0x40,0xff),STR_TR_SA);
    if (AppSetting->userEventPieChart()) {
        evseg->AddSlice(CPAP_UserFlag1,QColor(0xe0,0xe0,0xe0,0xff),tr("UF1"));
        evseg->AddSlice(CPAP_UserFlag2,QColor(0xc0,0xc0,0xe0,0xff),tr("UF2"));
    }

    GAHI->AddLayer(evseg);
    GAHI->setMargins(0,0,0,0);

    // Add event flags to the event flags graph
    gFlagsGroup *fg=new gFlagsGroup();
    sleepFlagsGroup = fg;
    SF->AddLayer(fg);

    SF->setBlockZoom(true);
    SF->AddLayer(new gShadowArea());
    SF->AddLayer(new gLabelArea(fg),LayerLeft,gYAxis::Margin);
    SF->AddLayer(new gXAxis(COLOR_Text,false),LayerBottom,0,gXAxis::Margin);


    // Now take care of xgrid/yaxis labels for all graphs

    // The following list contains graphs that don't have standard xgrid/yaxis labels
    QStringList skipgraph;
    skipgraph.push_back(STR_GRAPH_EventBreakdown);
    skipgraph.push_back(STR_GRAPH_SleepFlags);
    skipgraph.push_back(STR_GRAPH_TAP);

    QHash<QString, gGraph *>::iterator it;

    for (it = graphlist.begin(); it != graphlist.end(); ++it) {
        if (skipgraph.contains(it.key())) continue;
        it.value()->AddLayer(new gXGrid());
    }


    gLineChart *l;
    l=new gLineChart(CPAP_FlowRate,false,false);

    gGraph *FRW = graphlist[schema::channel[CPAP_FlowRate].code()];
    FRW->AddLayer(l);
    l -> setMinimumHeight(80);      // set the layer height to 80. or about 130 graph height.

//    FRW->AddLayer(AddOXI(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2)));

    bool square=AppSetting->squareWavePlots();
    gLineChart *pc=new gLineChart(CPAP_Pressure, square);
    graphlist[schema::channel[CPAP_Pressure].code()]->AddLayer(pc);

  //  graphlist[schema::channel[CPAP_Pressure].code()]->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_Ramp, COLOR_Ramp, schema::channel[CPAP_Ramp].label(), FT_Span)));

    pc->addPlot(CPAP_EPAP, square);
    pc->addPlot(CPAP_IPAPLo, square);
    pc->addPlot(CPAP_IPAP, square);
    pc->addPlot(CPAP_IPAPHi, square);
    pc->addPlot(CPAP_EEPAP, square);
    pc->addPlot(CPAP_PressureSet, false);
    pc->addPlot(CPAP_EPAPSet, false);
    pc->addPlot(CPAP_IPAPSet, false);

    // Create Timea at Pressure graph
    gGraph * TAP2;
    graphlist[STR_GRAPH_TAP] = TAP2 = new gGraph(STR_GRAPH_TAP, GraphView, tr("Time at Pressure"), tr("Time at Pressure"), default_height);
    MinutesAtPressure * map;
    TAP2->AddLayer(map = new MinutesAtPressure());
    TAP2->AddLayer(new gLabelArea(map),LayerLeft,gYAxis::Margin);
    TAP2->AddLayer(new gXAxisPressure(),LayerBottom,gXAxisPressure::Margin);
    TAP2->setBlockSelect(true);

    // Fill in the AHI graph
    if (p_profile->general->calculateRDI()) {
        AHI->AddLayer(new gLineChart(CPAP_RDI, square));
//        AHI->AddLayer(AddCPAP(new AHIChart(QColor("#37a24b"))));
    } else {
        AHI->AddLayer(new gLineChart(CPAP_AHI, square));
    }

    // this is class wide because the leak redline can be reset in preferences..
    // Better way would be having a search for linechart layers in graphlist[...]
    gLineChart *leakchart=new gLineChart(CPAP_Leak, square);
  //  graphlist[schema::channel[CPAP_Leak].code()]->AddLayer(AddCPAP(new gLineOverlayBar(CPAP_LargeLeak, COLOR_LargeLeak, STR_TR_LL, FT_Span)));

    leakchart->addPlot(CPAP_LeakTotal, square);
    leakchart->addPlot(CPAP_MaxLeak, square);
//    schema::channel[CPAP_Leak].setUpperThresholdColor(Qt::red);
//    schema::channel[CPAP_Leak].setLowerThresholdColor(Qt::green);

    graphlist[schema::channel[CPAP_Leak].code()]->AddLayer(leakchart);
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_Leak, COLOR_Leak,square)));
    //LEAK->AddLayer(AddCPAP(new gLineChart(CPAP_MaxLeak, COLOR_MaxLeak,square)));
    graphlist[schema::channel[CPAP_Snore].code()]->AddLayer(new gLineChart(CPAP_Snore, true));
/*
    graphlist[schema::channel[BMC_FlowAbnormality].code()]->AddLayer(new gLineChart(BMC_FlowAbnormality, square));
    graphlist[schema::channel[BMC_PressureWave].code()]->AddLayer(new gLineChart(BMC_PressureWave, square));
    graphlist[schema::channel[BMC_IE_Ratio].code()]->AddLayer(new gLineChart(BMC_IE_Ratio, square));
*/
    graphlist[schema::channel[CPAP_PTB].code()]->AddLayer(new gLineChart(CPAP_PTB, square));
    graphlist[schema::channel[PRS1_PeakFlow].code()]->AddLayer(new gLineChart(PRS1_PeakFlow, square));

    graphlist[schema::channel[Prisma_ObstructLevel].code()]->AddLayer(new gLineChart(Prisma_ObstructLevel, square));
    graphlist[schema::channel[Prisma_PressureMeasured].code()]->AddLayer(new gLineChart(Prisma_PressureMeasured, square));
    graphlist[schema::channel[Prisma_rRMV].code()]->AddLayer(new gLineChart(Prisma_rRMV, square));
    graphlist[schema::channel[Prisma_rMVFluctuation].code()]->AddLayer(new gLineChart(Prisma_rMVFluctuation, square));
    graphlist[schema::channel[Prisma_FlowFull].code()]->AddLayer(new gLineChart(Prisma_FlowFull, square));
    #if defined(STEADY_BREATHING)
    if (AppSetting->steadyBreathing()==SB_ACTIVE) {
        graphlist[schema::channel[CPAP_SteadyBreathing].code()]->AddLayer(new gLineChart(CPAP_SteadyBreathing, false));
    }
    #endif
    graphlist[schema::channel[CPAP_Test1].code()]->AddLayer(new gLineChart(CPAP_Test1, square));
    //graphlist[schema::channel[CPAP_Test2].code()]->AddLayer(new gLineChart(CPAP_Test2, square));


    gLineChart *lc = nullptr;
    graphlist[schema::channel[CPAP_MaskPressure].code()]->AddLayer(new gLineChart(CPAP_MaskPressure, false));
    graphlist[schema::channel[CPAP_RespRate].code()]->AddLayer(lc=new gLineChart(CPAP_RespRate, square));

    graphlist[schema::channel[POS_Inclination].code()]->AddLayer(new gLineChart(POS_Inclination));
    graphlist[schema::channel[POS_Orientation].code()]->AddLayer(new gLineChart(POS_Orientation));
    graphlist[schema::channel[POS_Movement].code()]->AddLayer(new gLineChart(POS_Movement));

    graphlist[schema::channel[CPAP_MinuteVent].code()]->AddLayer(lc=new gLineChart(CPAP_MinuteVent, square));
    lc->addPlot(CPAP_TgMV, square);


    graphlist[schema::channel[CPAP_TidalVolume].code()]->AddLayer(lc=new gLineChart(CPAP_TidalVolume, square));
    //lc->addPlot(CPAP_Test2,COLOR_DarkYellow,square);

    //graphlist[schema::channel[CPAP_TidalVolume].code()]->AddLayer(AddCPAP(new gLineChart("TidalVolume2", square)));
    graphlist[schema::channel[CPAP_FLG].code()]->AddLayer(new gLineChart(CPAP_FLG, true));
    //graphlist[schema::channel[CPAP_RespiratoryEvent].code()]->AddLayer(AddCPAP(new gLineChart(CPAP_RespiratoryEvent, true)));
    graphlist[schema::channel[CPAP_IE].code()]->AddLayer(lc=new gLineChart(CPAP_IE, false));      // this should be inverse of supplied value
    graphlist[schema::channel[CPAP_Te].code()]->AddLayer(lc=new gLineChart(CPAP_Te, false));
    graphlist[schema::channel[CPAP_Ti].code()]->AddLayer(lc=new gLineChart(CPAP_Ti, false));
    //lc->addPlot(CPAP_Test2,COLOR:DarkYellow,square);

    graphlist[schema::channel[ZEO_SleepStage].code()]->AddLayer(new gLineChart(ZEO_SleepStage, true));

//    gLineOverlaySummary *los1=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    gLineOverlaySummary *los2=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(AddOXI(los1->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
//    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(AddOXI(los1));
//    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(AddOXI(los2->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
//    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(AddOXI(los2));

    graphlist[schema::channel[OXI_Pulse].code()]->AddLayer(new gLineChart(OXI_Pulse, square));
    graphlist[schema::channel[OXI_SPO2].code()]->AddLayer(new gLineChart(OXI_SPO2, true));
    graphlist[schema::channel[OXI_Perf].code()]->AddLayer(new gLineChart(OXI_Perf, false));
    graphlist[schema::channel[OXI_Plethy].code()]->AddLayer(new gLineChart(OXI_Plethy, false));


    // Fix me
//    gLineOverlaySummary *los3=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(los3->add(new gLineOverlayBar(OXI_PulseChange, COLOR_PulseChange, STR_TR_PC,FT_Span))));
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(los3));
//    graphlist["INTPULSE"]->AddLayer(AddCPAP(new gLineChart(OXI_Pulse, square)));
//    gLineOverlaySummary *los4=new gLineOverlaySummary(STR_UNIT_EventsPerHour,5,-4);
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(los4->add(new gLineOverlayBar(OXI_SPO2Drop, COLOR_SPO2Drop, STR_TR_O2,FT_Span))));
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(los4));
//    graphlist["INTSPO2"]->AddLayer(AddCPAP(new gLineChart(OXI_SPO2, true)));

    graphlist[schema::channel[CPAP_PTB].code()]->setForceMaxY(100);
    graphlist[schema::channel[OXI_SPO2].code()]->setForceMaxY(100);

    for (it = graphlist.begin(); it != graphlist.end(); ++it) {
        if (skipgraph.contains(it.key())) continue;

        it.value()->AddLayer(new gYAxis(),LayerLeft,gYAxis::Margin);
        it.value()->AddLayer(new gXAxis(),LayerBottom,0,gXAxis::Margin);
    }

    if (p_profile->cpap->showLeakRedline()) {
        schema::channel[CPAP_Leak].setUpperThreshold(p_profile->cpap->leakRedline());
    } else {
        schema::channel[CPAP_Leak].setUpperThreshold(0); // switch it off
    }

    layout->layout();

    QTextCharFormat format = ui->calendar->weekdayTextFormat(Qt::Saturday);
    format.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    ui->calendar->setWeekdayTextFormat(Qt::Saturday, format);
    ui->calendar->setWeekdayTextFormat(Qt::Sunday, format);

    Qt::DayOfWeek dow=firstDayOfWeekFromLocale();

    ui->calendar->setFirstDayOfWeek(dow);

    ui->tabWidget->setCurrentWidget(widget);

    connect(webView,SIGNAL(anchorClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));

    int ews=p_profile->general->eventWindowSize();
    ui->evViewSlider->setValue(ews);
    ui->evViewLCD->display(ews);


    icon_on=new QIcon(":/icons/session-on.png");
    icon_off=new QIcon(":/icons/session-off.png");
    icon_up_down=new QIcon(":/icons/up-down.png");
    icon_warning=new QIcon(":/icons/warning.png");

    ui->splitter->setVisible(false);

#ifndef REMOVE_FITNESS
#else // REMOVE_FITNESS
        // hide the parent widget to make the gridlayout invisible
        QWidget *myparent ;
        myparent = ui->bioMetrics->parentWidget();
        if(myparent) myparent->hide();
#endif

    GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
    GraphView->setEmptyText(STR_Empty_NoData);
    previous_date=QDate();

    ui->calButton->setChecked(AppSetting->calendarVisible() ? true : false);
    on_calButton_toggled(AppSetting->calendarVisible());

    GraphView->resetLayout();
    GraphView->SaveDefaultSettings();
    GraphView->LoadSettings("Daily");

    connect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    connect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    connect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));

    // Watch for focusOut events on the JournalNotes widget
    ui->JournalNotes->installEventFilter(this);
//    qDebug() << "Finished making new Daily object";
//    sleep(3);
    saveGraphLayoutSettings=nullptr;
    dailySearchTab = new DailySearchTab(this,ui->searchTab,ui->tabWidget);
    htmlLsbSectionHeaderInit(false);
#ifndef REMOVE_FITNESS
    set_ZombieUI(0,true);
#endif
}

Daily::~Daily()
{
    disconnect(GraphView, SIGNAL(updateCurrentTime(double)), this, SLOT(on_LineCursorUpdate(double)));
    disconnect(GraphView, SIGNAL(updateRange(double,double)), this, SLOT(on_RangeUpdate(double,double)));
    disconnect(GraphView, SIGNAL(GraphsChanged()), this, SLOT(updateGraphCombo()));

    disconnect(sessionbar, SIGNAL(sessionClicked(Session*)), this, SLOT(doToggleSession(Session*)));
    disconnect(webView,SIGNAL(anchorClicked(QUrl)),this,SLOT(Link_clicked(QUrl)));
    ui->JournalNotes->removeEventFilter(this);

    if (previous_date.isValid()) {
        Unload(previous_date);
    }

    // Save graph orders and pin status, etc...
    GraphView->SaveSettings("Daily");

    delete ui;
    delete icon_on;
    delete icon_off;
    delete icon_up_down;
    delete icon_warning;
    if (saveGraphLayoutSettings!=nullptr) delete saveGraphLayoutSettings;
    delete dailySearchTab;
}

void Daily::showEvent(QShowEvent *)
{
//    qDebug() << "Start showEvent in Daily object";
//    sleep(3);
    RedrawGraphs();
//    qDebug() << "Finished showEvent  Daily object";
//    sleep(3);
}

bool Daily::rejectToggleSessionEnable( Session*sess) {
    if (!sess) return true;
    if (p_profile->cpap->clinicalMode()) {
       QMessageBox mbox(QMessageBox::Warning, tr("Clinical Mode"), tr(" Disabling Sessions requires Permissive Mode be set in OSCAR Preferences in the Clinical tab."), QMessageBox::Ok  , this);
            mbox.exec();
            return true;
    }
    sess->setEnabled(!sess->enabled());
    return false;
}

void Daily::doToggleSession(Session * sess)
{
    if (rejectToggleSessionEnable( sess) ) return;
    LoadDate(previous_date);
    mainwin->getOverview()->graphView()->dataChanged();
    mainwin->GenerateStatistics();
}

void Daily::Link_clicked(const QUrl &url)
{
    QString code=url.toString().section("=",0,0).toLower();
    QString data=url.toString().section("=",1);
    SessionID sid=data.toUInt();
    Day *day=nullptr;

    if (code=="togglecpapsession") { // Enable/Disable CPAP session
        day=p_profile->GetDay(previous_date,MT_CPAP);
        if (!day) return;
        Session *sess=day->find(sid, MT_CPAP);
//        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        if (rejectToggleSessionEnable( sess) ) return;

        // Reload day
        LoadDate(previous_date);
        mainwin->getOverview()->graphView()->dataChanged();
  //      webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
    } else  if (code=="toggleoxisession") { // Enable/Disable Oximetry session
        day=p_profile->GetDay(previous_date,MT_OXIMETER);
        if (!day) return;
        Session *sess=day->find(sid, MT_OXIMETER);
//        int i=webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-webView->page()->mainFrame()->scrollBarValue(Qt::Vertical);
        if (rejectToggleSessionEnable( sess) ) return;

        // Reload day
        LoadDate(previous_date);
        mainwin->getOverview()->graphView()->dataChanged();
  //      webView->page()->mainFrame()->setScrollBarValue(Qt::Vertical, webView->page()->mainFrame()->scrollBarMaximum(Qt::Vertical)-i);
    } else  if (code=="togglestagesession") { // Enable/Disable Sleep Stage session
        day=p_profile->GetDay(previous_date,MT_SLEEPSTAGE);
        if (!day) return;
        Session *sess=day->find(sid, MT_SLEEPSTAGE);
        if (rejectToggleSessionEnable( sess) ) return;
        LoadDate(previous_date);
        mainwin->getOverview()->graphView()->dataChanged();
    } else  if (code=="togglepositionsession") { // Enable/Disable Position session
        day=p_profile->GetDay(previous_date,MT_POSITION);
        if (!day) return;
        Session *sess=day->find(sid, MT_POSITION);
        if (rejectToggleSessionEnable( sess) ) return;
        LoadDate(previous_date);
        mainwin->getOverview()->graphView()->dataChanged();
    } else if (code=="cpap")  {
        day=p_profile->GetDay(previous_date,MT_CPAP);
        if (day) {
            Session *sess=day->machine(MT_CPAP)->sessionlist[sid];
            if (sess && sess->enabled()) {
                GraphView->SetXBounds(sess->first(),sess->last());
            }
        }
    } else if (code=="oxi") {
        day=p_profile->GetDay(previous_date,MT_OXIMETER);
        if (day) {
            Session *sess=day->machine(MT_OXIMETER)->sessionlist[sid];
            if (sess && sess->enabled()) {
                GraphView->SetXBounds(sess->first(),sess->last());
            }
        }

    } else if (code=="event")  {
        QList<QTreeWidgetItem *> list=ui->treeWidget->findItems(schema::channel[sid].fullname(),Qt::MatchContains);
        if (list.size()>0) {
            ui->treeWidget->collapseAll();
            ui->treeWidget->expandItem(list.at(0));
            QTreeWidgetItem *wi=list.at(0)->child(0);
            ui->treeWidget->setCurrentItem(wi);
            ui->tabWidget->setCurrentIndex(1);
        } else {
            mainwin->Notify(tr("No %1 events are recorded this day").arg(schema::channel[sid].fullname()),"",1500);
        }
    } else if (code=="graph") {
        qDebug() << "Select graph " << data;
    } else if (code=="leftsidebarenable") {
        leftSideBarEnable.toggleBit(data.toInt());
        LoadDate(previous_date);
    } else {
        qDebug() << "Clicked on" << code << data;
    }
}

void Daily::ReloadGraphs()
{
//    qDebug() << "Start ReloadGraphs  Daily object";
//    sleep(3);
    GraphView->setDay(nullptr);

    ui->splitter->setVisible(true);
    QDate d;

    if (previous_date.isValid()) {
        d=previous_date;
        //Unload(d);
    }
    QDate lastcpap = p_profile->LastDay(MT_CPAP);
    QDate lastoxi = p_profile->LastDay(MT_OXIMETER);

    d = qMax(lastcpap, lastoxi);

    if (!d.isValid()) {
        d=ui->calendar->selectedDate();
    }
    on_calendar_currentPageChanged(d.year(),d.month());
    // this fires a signal which unloads the old and loads the new
    ui->calendar->blockSignals(true);
    ui->calendar->setSelectedDate(d);
    ui->calendar->blockSignals(false);
    Load(d);
    ui->calButton->setText(ui->calendar->selectedDate().toString(MedDateFormat));
    graphView()->redraw();
//    qDebug() << "Finished ReloadGraphs in Daily object";
//    sleep(3);
}

void Daily::updateLeftSidebar() {
    if (webView && !htmlLeftHeader.isEmpty())
        webView->setHtml(getLeftSidebar(true));
}

void Daily::hideSpaceHogs()
{
    if (AppSetting->calendarVisible()) {
        ui->calendarFrame->setVisible(false);
    }
    if (AppSetting->showPieChart()) {
        webView->setHtml(getLeftSidebar(false));
    }
}
void Daily::showSpaceHogs()
{
    if (AppSetting->calendarVisible()) {
        ui->calendarFrame->setVisible(true);
    }
    if (AppSetting->showPieChart()) {
        webView->setHtml(getLeftSidebar(true));
    }
}

void Daily::on_calendar_currentPageChanged(int year, int month)
{
    QDate d(year,month,1);
    int dom=d.daysInMonth();

    for (int i=1;i<=dom;i++) {
        d=QDate(year,month,i);
        this->UpdateCalendarDay(d);
    }
}

void Daily::UpdateEventsTree(QTreeWidget *tree,Day *day)
{
    DEBUGXD O("Daily::UpdateEventsTree") O(tree) O(day);
    tree->clear();
    if (!day) return;

    tree->setColumnCount(1); // 1 visible common.. (1 hidden)
    // Disable automatic sorting of the top-level items
    tree->setSortingEnabled(false);

    QTreeWidgetItem *root=nullptr;
    QHash<ChannelID,QTreeWidgetItem *> mcroot;
    QHash<ChannelID,int> mccnt;
    QList<EventTreeWidgetItem> all_events;

    qint64 drift=0, clockdrift=p_profile->cpap->clockDrift()*1000L;
    // Get post-context preference for event display
    double event_post_context_secs=p_profile->cpap->eventPostcontext();
    DEBUGXD Q(event_post_context_secs);

    quint32 chantype = schema::FLAG | schema::SPAN | schema::MINOR_FLAG;
    if (p_profile->general->showUnknownFlags()) chantype |= schema::UNKNOWN;
    QList<ChannelID> chans = day->getSortedMachineChannels(chantype);

    // Go through all the enabled sessions of the day
    qint64 max_t_post_context = 0;
    for (QList<Session *>::iterator s=day->begin();s!=day->end();++s) {
        Session * sess = *s;
        if (!sess->enabled()) continue;

        // For each session, go through all the channels
        QHash<ChannelID,QVector<EventList *> >::iterator m;
        for (int c=0; c < chans.size(); ++c) {
            ChannelID code = chans.at(c);
            m = sess->eventlist.find(code);
            if (m == sess->eventlist.end()) continue;

            drift=(sess->type() == MT_CPAP) ? clockdrift : 0;

            // Prepare title for this code, if there are any events
            QTreeWidgetItem *mcr;
            if (mcroot.find(code)==mcroot.end()) {
                int cnt=day->count(code);
                if (!cnt) continue; // If no events than don't bother showing..
                QString st=schema::channel[code].fullname();
                if (st.isEmpty())  {
                    st=QString("Fixme %1").arg(code);
                }
                st+=" ";
                if (cnt==1) st+=tr("%1 event").arg(cnt);
                else st+=tr("%1 events").arg(cnt);

                QStringList l(st);
                l.append("");
                mcroot[code]=mcr=new QTreeWidgetItem(root,l);
                mccnt[code]=0;
            } else {
                mcr=mcroot[code];
            }

            // number of digits required for count depends on total for day
            int numDigits = ceil(log10(day->count(code)+1));

            // Now we go through the event list for the *session* (not for the day)
            for (int z=0;z<m.value().size();z++) {
                EventList & ev=*(m.value()[z]);

                for (quint32 o=0;o<ev.count();o++) {
                    qint64 t=ev.time(o)+drift;

                    if ((code == CPAP_CSR) || (code == CPAP_PB)) { // center it in the middle of span
                        t -= float(ev.raw(o) / 2.0) * 1000.0;
                    }
                    // Add in postcontext to event display (e.g., 30 seconds)
                    // Note: a no-op by default (i.e., t_post_context = t)
                    qint64 t_post_context = t + (event_post_context_secs * 1000.0);
                    max_t_post_context = max(t_post_context, max_t_post_context);

                    QStringList a, a_all;
                    QDateTime d=QDateTime::fromMSecsSinceEpoch(t); // Localtime
                    QString s=QString("#%1: %2").arg((int)(++mccnt[code]),(int)numDigits,(int)10,QChar('0')).arg(d.toString("HH:mm:ss"));
                    QString s_all=QString("%1").arg(d.toString("HH:mm:ss"));
                    if (m.value()[z]->raw(o) > 0) {
                        QString duration_spec = QString(" (%3)").arg(m.value()[z]->raw(o));
                        s += duration_spec;
                        s_all += duration_spec;
                    }

                    a.append(s);
                    QTreeWidgetItem *item=new QTreeWidgetItem(a);
                    item->setData(0,Qt::UserRole,t_post_context);
                    mcr->addChild(item);

                    // Similarly add entry for consolidated node
                    if (p_profile->cpap->consolidateEvents()) {
                        s_all += " " + schema::channel[code].label();
                        a_all.append(s_all);
                        EventTreeWidgetItem item_all=EventTreeWidgetItem(a_all);
                        item_all.setData(0,Qt::UserRole,t_post_context);
                        all_events.append(item_all);
                    }
                }
            }
        }
    }
    int cnt=0;
    for (QHash<ChannelID,QTreeWidgetItem *>::iterator m=mcroot.begin();m!=mcroot.end();m++) {
        tree->insertTopLevelItem(cnt++,m.value());
    }
    qDebug() << "max_t_post_context:" << max_t_post_context;

    // Sort the top-level nodes (i.e., event types)
    // note: Done here so that UA occurs before Session Start/End
    tree->sortByColumn(0,Qt::AscendingOrder);

    if (day->hasMachine(MT_CPAP) || day->hasMachine(MT_OXIMETER) || day->hasMachine(MT_POSITION)) {
        QTreeWidgetItem * start = new QTreeWidgetItem(QStringList(tr("Session Start Times")),eventTypeStart);
        QTreeWidgetItem * end = new QTreeWidgetItem(QStringList(tr("Session End Times")),eventTypeEnd);
        tree->insertTopLevelItem(cnt++ , start);
        tree->insertTopLevelItem(cnt++ , end);
        for (QList<Session *>::iterator s=day->begin(); s!=day->end(); ++s) {
            Session* sess = *s;
            if ( (sess->type() != MT_CPAP) && (sess->type() != MT_OXIMETER) && (sess->type() != MT_POSITION)) continue;
            QDateTime st = QDateTime::fromMSecsSinceEpoch(sess->first()); // Localtime
            QDateTime et = QDateTime::fromMSecsSinceEpoch(sess->last()); // Localtime

            QTreeWidgetItem * item = new QTreeWidgetItem(QStringList(st.toString("HH:mm:ss")));
            item->setData(0,Qt::UserRole, sess->first());
            start->addChild(item);

            item = new QTreeWidgetItem(QStringList(et.toString("HH:mm:ss")));
            item->setData(0,Qt::UserRole, sess->last());
            end->addChild(item);
        }
    }

    // Add consolidated list
    // Note: The test for consolidation is based on debugging output, so make
    // sure changes here are reflected in tests/eventstabtests.cpp. In addition,
    // mainwin is undefined during tests and access should be guarded elsewhere.
    qDebug() << "consolidate:" << p_profile->cpap->consolidateEvents();
    if (p_profile->cpap->consolidateEvents()) {

        // Sort all items by date (e.g., so CA's interleaved w/ OA's, etc.)
        std::sort(all_events.begin(), all_events.end());

        // Number items to account for timestamp rank
        int numDigits_all=ceil(log10(all_events.count()+1));
        QStringList l_all=QStringList(tr("All"));
        EventTreeWidgetItem *all_numbered_events=new EventTreeWidgetItem(root,l_all);
        for (int event_num = 0; event_num < all_events.count(); event_num++) {
            // ex: "04:15:22 (12)" => "#16: 04:15:22 (12)"
            EventTreeWidgetItem child = all_events.at(event_num);
            QString label = child.text(0);
            QString new_label=QString("#%1: %2").arg((int)event_num + 1,(int)numDigits_all,(int)10,QChar('0')).arg(label);
            qDebug() << "new_label:" << new_label;
            child.setText(0, new_label);
            all_numbered_events->addChild(new EventTreeWidgetItem(child));
        }

        // Add to control
        if (all_numbered_events->childCount()) {
            tree->insertTopLevelItem(cnt++,all_numbered_events);
        }
    }

    // Trace out event-type nodes
    qDebug() << "final top-level items:";
    for (int level = 0; level < tree->topLevelItemCount(); level++) {
        qDebug() << tree->topLevelItem(level)->text(0);
    }
}

void Daily::UpdateCalendarDay(QDate date)
{
    QTextCharFormat nodata;
    QTextCharFormat cpaponly;
    QTextCharFormat cpapjour;
    QTextCharFormat oxiday;
    QTextCharFormat oxicpap;
    QTextCharFormat jourday;
    QTextCharFormat stageday;

    cpaponly.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpaponly.setFontWeight(QFont::Normal);
    cpapjour.setForeground(QBrush(COLOR_Blue, Qt::SolidPattern));
    cpapjour.setFontWeight(QFont::Bold);
//    cpapjour.setFontUnderline(true);
    oxiday.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxiday.setFontWeight(QFont::Normal);
    oxicpap.setForeground(QBrush(COLOR_Red, Qt::SolidPattern));
    oxicpap.setFontWeight(QFont::Bold);
    stageday.setForeground(QBrush(COLOR_Magenta, Qt::SolidPattern));
    stageday.setFontWeight(QFont::Bold);
    jourday.setForeground(QBrush(COLOR_DarkYellow, Qt::SolidPattern));
    jourday.setFontWeight(QFont::Bold);
    nodata.setForeground(QBrush(COLOR_Black, Qt::SolidPattern));
    nodata.setFontWeight(QFont::Normal);

    bool hascpap = p_profile->FindDay(date, MT_CPAP)!=nullptr;
    bool hasoxi = p_profile->FindDay(date, MT_OXIMETER)!=nullptr;
    bool hasjournal = p_profile->FindDay(date, MT_JOURNAL)!=nullptr;
    bool hasstage = p_profile->FindDay(date, MT_SLEEPSTAGE)!=nullptr;
    bool haspos = p_profile->FindDay(date, MT_POSITION)!=nullptr;
    if (hascpap) {
        if (hasoxi) {
            ui->calendar->setDateTextFormat(date, oxicpap);
        } else if (hasjournal) {
            ui->calendar->setDateTextFormat(date, cpapjour);
        } else if (hasstage || haspos) {
            ui->calendar->setDateTextFormat(date, stageday);
        } else {
            ui->calendar->setDateTextFormat(date, cpaponly);
        }
    } else if (hasoxi) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else if (hasjournal) {
        ui->calendar->setDateTextFormat(date, jourday);
    } else if (hasstage) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else if (haspos) {
        ui->calendar->setDateTextFormat(date, oxiday);
    } else {
        ui->calendar->setDateTextFormat(date, nodata);
    }
//    if (hasjournal) {
//        ui->calendar->setDateTextFormat(date, cpapjour);
//    }
    ui->calendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
}
void Daily::LoadDate(QDate date)
{
    DEBUGXD O("Daily::LoadDate") O(date);
    if (!date.isValid()) {
        qDebug() << "LoadDate called with invalid date";
        return;
    }
    ui->calendar->blockSignals(true);
    if (date.month()!=previous_date.month()) {
        on_calendar_currentPageChanged(date.year(),date.month());
    }
    ui->calendar->setSelectedDate(date);
    ui->calendar->blockSignals(false);
    on_calendar_selectionChanged();
}

void Daily::on_calendar_selectionChanged()
{
    QTimer::singleShot(0, this, SLOT(on_ReloadDay()));
}

void Daily::on_ReloadDay()
{
    static volatile bool inReload = false;

    if (inReload) {
        qDebug() << "attempt to renter on_ReloadDay()";
    }
    inReload = true;
    graphView()->releaseKeyboard();
    QElapsedTimer time;
    time_t unload_time, load_time, other_time;
    time.start();

    this->setCursor(Qt::BusyCursor);
    if (previous_date.isValid()) {
       // GraphView->fadeOut();
        Unload(previous_date);
    }

    unload_time=time.restart();
    //bool fadedir=previous_date < ui->calendar->selectedDate();
    Load(ui->calendar->selectedDate());
    load_time=time.restart();

    //GraphView->fadeIn(fadedir);
    GraphView->redraw();
    ui->calButton->setText(ui->calendar->selectedDate().toString(MedDateFormat));
    ui->calendar->setFocus(Qt::ActiveWindowFocusReason);

#ifndef REMOVE_FITNESS
    ui->weightSpinBox->setDecimals(WEIGHT_SPIN_BOX_DECIMALS);
#endif
    this->setCursor(Qt::ArrowCursor);
    other_time=time.restart();

    qDebug() << "Page change time (in ms): Unload ="<<unload_time<<"Load =" << load_time << "Other =" << other_time;
    inReload = false;
}
void Daily::ResetGraphLayout()
{
    GraphView->resetLayout();
}
void Daily::ResetGraphOrder(int type)
{
    if (type == 0) {  // Auto order
        Day * day = p_profile->GetDay(previous_date,MT_CPAP);

        int cpapMode = day->getCPAPMode();
        //    qDebug() << "Daily::ResetGraphOrder cpapMode" << cpapMode;

        if (useAdvancedGraphs.contains(cpapMode))
            GraphView->resetGraphOrder(true, advancedGraphOrder);
        else
            GraphView->resetGraphOrder(true, standardGraphOrder);
    } else if (type == 2) { // Advanced order
        GraphView->resetGraphOrder(true, advancedGraphOrder);
    } else {                // type == 1, standard order
        GraphView->resetGraphOrder(true, standardGraphOrder);
    }

    // Enable all graphs (make them not hidden)
    showAllGraphs(true);
    showAllEvents(true);

    // Reset graph heights (and repaint)
    ResetGraphLayout();
}

void Daily::graphtogglebutton_toggled(bool b)
{
    Q_UNUSED(b)
    for (int i=0;i<GraphView->size();i++) {
        QString title=(*GraphView)[i]->title();
        (*GraphView)[i]->setVisible(GraphToggles[title]->isChecked());
    }
    GraphView->updateScale();
    GraphView->redraw();
}

QString Daily::getSessionInformation(Day * day)
{
    QString html;
    if (!day) return html;

    htmlLsbSectionHeader(html,tr("Session Information"),LSB_SESSION_INFORMATION );
    if (!leftSideBarEnable[LSB_SESSION_INFORMATION] ) {
        html+="<hr/>"; // Have sep at the end of sections.
        return html;
    }
    html+="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
    html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
    QFontMetrics FM(*defaultfont);

    QDateTime fd,ld;
    //bool corrupted_waveform=false;

    QString type;

    QHash<MachineType, Machine *>::iterator mach_end = day->machines.end();
    QHash<MachineType, Machine *>::iterator mi;

    for (mi = day->machines.begin(); mi != mach_end; ++mi) {
        if (mi.key() == MT_JOURNAL) continue;
        html += "<tr><td colspan=5 align=center><i>";
        switch (mi.key()) {
            case MT_CPAP: type="cpap";
                html+=tr("CPAP Sessions");
                break;
            case MT_OXIMETER: type="oxi";
                html+=tr("Oximetry Sessions");
                break;
            case MT_SLEEPSTAGE: type="stage";
                html+=tr("Sleep Stage Sessions");
                break;
            case MT_POSITION: type="position";
                html+=tr("Position Sensor Sessions");
                break;

            default:
                type="unknown";
                html+=tr("Unknown Session");
                break;
        }
        html+="</i></td></tr>\n";
        html+=QString("<tr>"
                      "<td>"+STR_TR_On+"</td>"
                      "<td>"+STR_TR_Date+"</td>"
                      "<td>"+STR_TR_Start+"</td>"
                      "<td>"+STR_TR_End+"</td>"
                      "<td>"+tr("Duration")+"</td></tr>");

        QList<Session *> sesslist = day->getSessions(mi.key(), true);

        for (QList<Session *>::iterator s=sesslist.begin(); s != sesslist.end(); ++s) {
            /*
            if (((*s)->type() == MT_CPAP) &&
                ((*s)->settings.find(CPAP_BrokenWaveform) != (*s)->settings.end()))
                    corrupted_waveform=true;
            */

            fd=QDateTime::fromSecsSinceEpoch((*s)->first()/1000L);
            ld=QDateTime::fromSecsSinceEpoch((*s)->last()/1000L);
            int len=(*s)->length()/1000L;
            int h=len/3600;
            int m=(len/60) % 60;
            int s1=len % 60;

            Session *sess=*s;

            QString tooltip = tr("Click to %1 this session.").arg(sess->enabled() ? tr("disable") : tr("enable"));
            html+=QString("<tr><td colspan=5 align=center>%2</td></tr>"
                          "<tr>"
                          "<td width=26>"
#ifdef DITCH_ICONS
                          "[<a href='toggle"+type+"session=%1'><b title='"+tooltip+"'>%4</b></a>]"
#else
                          "<a href='toggle"+type+"session=%1'><img src='qrc:/icons/session-%4.png' title=\""+tooltip+"\"></a>"
#endif
                          "</td>"
                          "<td align=left>%5</td>"
                          "<td align=left>%6</td>"
                          "<td align=left>%7</td>"
                          "<td align=left>%3</td></tr>"
                          )
                    .arg((*s)->session())
                    .arg(tr("%1 Session #%2").arg((*s)->machine()->loaderName()).arg((*s)->session(),8,10,QChar('0')))
                    .arg(tr("%1h %2m %3s").arg(h,2,10,QChar('0')).arg(m,2,10,QChar('0')).arg(s1,2,10,QChar('0')))
                    .arg((sess->enabled() ? "on" : "off"))
                    .arg(fd.date().toString(QLocale::system().dateFormat(QLocale::ShortFormat)))
                    .arg(fd.toString("HH:mm:ss"))
                    .arg(ld.toString("HH:mm:ss"));



#ifdef SESSION_DEBUG
            for (int i=0; i< sess->session_files.size(); ++i) {
                html+=QString("<tr><td colspan=5 align=center>%1</td></tr>").arg(sess->session_files[i].section("/",-1));
            }
#endif
        }
    }

    /*
    if (corrupted_waveform) {
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg(tr("One or more waveform record(s) for this session had faulty source data. Some waveform overlay points may not match up correctly."));
    }
    */
    html+="</table>";
    html+="<hr/>"; // Have sep at the end of sections.
    return html;
}

QString Daily::getMachineSettings(Day * day) {
    QString html;

    Machine * cpap = day->machine(MT_CPAP);
    if (cpap && day->hasEnabledSessions(MT_CPAP)) {
        CPAPLoader * loader = qobject_cast<CPAPLoader *>(cpap->loader());
        if (!loader) {
            htmlLsbSectionHeader(html,tr("DEVICE SETTINGS ERROR"),LSB_DEVICE_SETTINGS );
            return html;
        }
        htmlLsbSectionHeader(html,tr("Device Settings"),LSB_DEVICE_SETTINGS );
        if (!leftSideBarEnable[LSB_DEVICE_SETTINGS] ) {
            return html;
        }
        html+="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        if (day->noSettings(cpap)) {
            html+="<tr><td colspan=5 align=center><i><font color='red'>"+tr("<b>Please Note:</b> All settings shown below are based on assumptions that nothing has changed since previous days.")+"</font></i></td></tr>\n";
        } else {
            html+="<tr><td colspan=5>&nbsp;</td></tr>";
        }

        QMap<QString, QString> other;
        Session * sess = day->firstSession(MT_CPAP);

        QHash<ChannelID, QVariant>::iterator it;
        QHash<ChannelID, QVariant>::iterator it_end;
        if (sess) {
            it_end = sess->settings.end();
            it = sess->settings.begin();
        }
        QMap<int, QString> first;

        ChannelID cpapmode = loader->CPAPModeChannel();
        schema::Channel & chan = schema::channel[cpapmode];
        first[cpapmode] = QString("<tr><td><p title='%2'>%1</p></td><td colspan=4>%3</td></tr>")
                .arg(chan.label())
                .arg(chan.description())
                .arg(day->getCPAPModeStr());

        // The order in which to present pressure settings, which will appear first.
        QVector<ChannelID> first_channels = { cpapmode, CPAP_Pressure, CPAP_PressureMin, CPAP_PressureMax, CPAP_EPAP, CPAP_EPAPLo, CPAP_EPAPHi, CPAP_IPAP, CPAP_IPAPLo, CPAP_IPAPHi, CPAP_PS, CPAP_PSMin, CPAP_PSMax };

        if (sess) for (; it != it_end; ++it) {
            ChannelID code = it.key();

            if ((code <= 1) || (code == RMS9_MaskOnTime) || (code == CPAP_Mode) || (code == cpapmode) || (code == CPAP_SummaryOnly))
                continue;

            schema::Channel & chan = schema::channel[code];

            QString data;

            if (chan.datatype() == schema::LOOKUP) {
                int value = it.value().toInt();
                data = chan.option(value);
                if (data.isEmpty()) {
                    data = QString().number(value) + " " + chan.units();;
                }
            } else if (chan.datatype() == schema::BOOL) {
                data = (it.value().toBool() ? STR_TR_Yes : STR_TR_No);
            } else if (chan.datatype() == schema::DOUBLE) {
                data = QString().number(it.value().toDouble(),'f',2) + " "+chan.units();
            } else if (chan.datatype() == schema::DEFAULT) {
                // Check for any options that override the default numeric display.
                int value = it.value().toInt();
                data = chan.option(value);
                if (data.isEmpty()) {
                    data = QString().number(it.value().toDouble(),'f',2) + " "+chan.units();
                }
            } else {

                data = it.value().toString() + " "+ chan.units();
            }
            if (code ==0xe202)      // Format EPR relief correctly
                data = formatRelief(data);

            QString tmp = QString("<tr><td><p title='%2'>%1</p></td><td colspan=4>%3</td></tr>")
                    .arg(schema::channel[code].label())
                    .arg(schema::channel[code].description())
                    .arg(data);
            if (first_channels.contains(code)) {
                first[code] = tmp;
            } else {
                other[schema::channel[code].label()] = tmp;
            }
        }

        // Put the pressure settings in order.
        for (auto & code : first_channels) {
            if (first.contains(code)) html += first[code];
        }

        // TODO: add logical (rather than alphabetical) ordering to this list, preferably driven by loader somehow
        for (QMap<QString,QString>::iterator it = other.begin(); it != other.end(); ++it) {
            html += it.value();
        }

        html+="</table>";
        html+="<hr/>\n";
    }
    return html;
}

QString Daily::getOximeterInformation(Day * day)
{
    QString html;
    Machine * oxi = day->machine(MT_OXIMETER);
    if (oxi && day->hasEnabledSessions(MT_OXIMETER)) {
        htmlLsbSectionHeader(html,tr("Oximeter Information"),LSB_OXIMETER_INFORMATION );
        if (!leftSideBarEnable[LSB_OXIMETER_INFORMATION] ) {
            return html;
        }
        html+="<table cellpadding=0 cellspacing=0 border=0 width=100%>";
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        html+="<tr><td colspan=5 align=center>"+oxi->brand()+" "+oxi->model()+"</td></tr>\n";
        html+="<tr><td colspan=5 align=center>&nbsp;</td></tr>";
        // Include SpO2 and PC drops per hour of Oximetry data in case CPAP data is missing
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3%) %4/h</td></tr>").arg(tr("SpO2 Desaturations")).arg(day->count(OXI_SPO2Drop)).arg((100.0/day->hours(MT_OXIMETER)) * (day->sum(OXI_SPO2Drop)/3600.0),0,'f',2).arg((day->count(OXI_SPO2Drop)/day->hours(MT_OXIMETER)),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2 (%3%) %4/h</td></tr>").arg(tr("Pulse Change events")).arg(day->count(OXI_PulseChange)).arg((100.0/day->hours(MT_OXIMETER)) * (day->sum(OXI_PulseChange)/3600.0),0,'f',2).arg((day->count(OXI_PulseChange)/day->hours(MT_OXIMETER)),0,'f',2);
        html+=QString("<tr><td colspan=5 align=center>%1: %2%</td></tr>").arg(tr("SpO2 Baseline Used")).arg(day->settings_wavg(OXI_SPO2Drop),0,'f',2); // CHECKME: Should this value be wavg OXI_SPO2 isntead?
        html+="</table>\n";
        html+="<hr/>\n";
    }
    return html;
}
QString Daily::getCPAPInformation(Day * day)
{
    QString html;
    if (!day)
        return html;

    Machine * cpap = day->machine(MT_CPAP);
    if (!cpap) return html;

    MachineInfo info = cpap->getInfo();
    htmlLsbSectionHeader(html , info.brand ,LSB_MACHINE_INFO );
    if (!leftSideBarEnable[LSB_MACHINE_INFO] ) {
        return html;
    }
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    QString tooltip=tr("Model %1 - %2").arg(info.modelnumber).arg(info.serial);
    tooltip=tooltip.replace(" ","&nbsp;");
    html+="<tr><td align=center><p title=\""+tooltip+"\">"+info.model+"</p></td></tr>\n";
    html+="<tr><td align=center>";

    html+=tr("PAP Mode: %1").arg(day->getCPAPModeStr())+"<br/>";
    html+= day->getPressureSettings();
    html+="</td></tr>\n";
    if (day->noSettings(cpap)) {
        html+=QString("<tr><td align=center><font color='red'><i>%1</i></font></td></tr>").arg(tr("(Mode and Pressure settings missing; yesterday's shown.)"));
    }

    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}


QString Daily::getStatisticsInfo(Day * day)
{
    if (day == nullptr) return QString();

    Machine *cpap = day->machine(MT_CPAP);
//            *oxi = day->machine(MT_OXIMETER),
//            *pos = day->machine(MT_POSITION);

    int mididx=p_profile->general->prefCalcMiddle();
    SummaryType ST_mid = ST_AVG;

    if (mididx==0) ST_mid=ST_PERC;
    if (mididx==1) ST_mid=ST_WAVG;
    if (mididx==2) ST_mid=ST_AVG;

    float percentile=p_profile->general->prefCalcPercentile()/100.0;

    SummaryType ST_max=p_profile->general->prefCalcMax() ? ST_PERC : ST_MAX;
    const EventDataType maxperc=0.995F;

    QString midname;
    if (ST_mid==ST_WAVG) midname=STR_TR_WAvg;
    else if (ST_mid==ST_AVG) midname=STR_TR_Avg;
    else if (ST_mid==ST_PERC) midname=STR_TR_Med;

    QString html;

    htmlLsbSectionHeader(html,tr("Statistics"),LSB_STATISTICS );
    if (!leftSideBarEnable[LSB_STATISTICS] ) {
        return html;
    }
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+=QString("<tr><td><b>%1</b></td><td><b>%2</b></td><td><b>%3</b></td><td><b>%4</b></td><td><b>%5</b></td></tr>")
            .arg(STR_TR_Channel)
            .arg(STR_TR_Min)
            .arg(midname)
            .arg(QString("%1%2").arg(percentile*100,0,'f',0).arg(STR_UNIT_Percentage))
            .arg(ST_max == ST_MAX?STR_TR_Max:QString("99.5%"));

    ChannelID chans[]={
        CPAP_Pressure,CPAP_PressureSet,CPAP_EPAP,CPAP_EPAPSet,CPAP_IPAP,CPAP_IPAPSet,CPAP_PS,CPAP_PTB,
        PRS1_PeakFlow,
        Prisma_ObstructLevel, Prisma_PressureMeasured, Prisma_rRMV, Prisma_rMVFluctuation,
        CPAP_MinuteVent, CPAP_RespRate, CPAP_RespEvent,CPAP_FLG,
        CPAP_Leak, CPAP_LeakTotal, CPAP_Snore,  CPAP_IE,  CPAP_Ti,CPAP_Te, CPAP_TgMV,
        CPAP_TidalVolume, OXI_Pulse, OXI_SPO2, POS_Inclination, POS_Orientation, POS_Movement
        #if defined(STEADY_BREATHING)
        , CPAP_SteadyBreathing
        #endif
    };
    int numchans=sizeof(chans)/sizeof(ChannelID);
    int ccnt=0;
    EventDataType tmp,med,perc,mx,mn;

    for (int i=0;i<numchans;i++) {

        ChannelID code=chans[i];

        if (!day->channelHasData(code))
            continue;

        QString tooltip=schema::channel[code].description();

        if (!schema::channel[code].units().isEmpty()) tooltip+=" ("+schema::channel[code].units()+")";
//        if (!schema::channel[code].units().isEmpty() && schema::channel[code].units() != "0")
//            tooltip+=" ("+schema::channel[code].units()+")";

        if (ST_max == ST_MAX) {
            mx=day->Max(code);
        } else {
            mx=day->percentile(code,maxperc);
        }

        mn=day->Min(code);
        perc=day->percentile(code,percentile);

        if (ST_mid == ST_PERC) {
            med=day->percentile(code,0.5);
            tmp=day->wavg(code);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_WAvg+": %1").arg(tmp,0,'f',2);
            }
        } else if (ST_mid == ST_WAVG) {
            med=day->wavg(code);
            tmp=day->percentile(code,0.5);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
            }
        } else if (ST_mid == ST_AVG) {
            med=day->avg(code);
            tmp=day->percentile(code,0.5);
            if (tmp>0 || mx==0) {
                tooltip+=QString("<br/>"+STR_TR_Median+": %1").arg(tmp,0,'f',2);
            }
        }

//        QString oldtip = tooltip;
        tooltip.replace("'", "&apos;");
//        qDebug() << schema::channel[code].label() << "old tooltip" << oldtip << "; new tooltip" << tooltip ;

        html+=QString("<tr><td align=left title='%6'>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
            .arg(schema::channel[code].label())
            .arg(mn,0,'f',2)
            .arg(med,0,'f',2)
            .arg(perc,0,'f',2)
            .arg(mx,0,'f',2)
            .arg(tooltip);
        ccnt++;

    }

    if (GraphView->isEmpty() && ((ccnt>0) || (cpap && day->summaryOnly()))) {
        html+="<tr><td colspan=5>&nbsp;</td></tr>\n";
        html+=QString("<tr><td colspan=5 align=center><i>%1</i></td></tr>").arg("<b>"+STR_MessageBox_PleaseNote+"</b> "+
                    tr("This day just contains summary data, only limited information is available."));
    } else if (cpap) {
        html+="<tr><td colspan=5>&nbsp;</td></tr>";

        if ((cpap->loaderName() == STR_MACH_ResMed) || ((cpap->loaderName() == STR_MACH_PRS1) && (p_profile->cpap->resyncFromUserFlagging()))) {
            int ttia = day->sum(AllAhiChannels);
            int h = ttia / 3600;
            int m = ttia / 60 % 60;
            int s = ttia % 60;
            if (ttia > 0) {
                html+="<tr><td colspan=3 align='left' bgcolor='white'><b>"+tr("Total time in apnea") +
                       QString("</b></td><td colspan=2 bgcolor='white'>%1</td></tr>").arg(QString::asprintf("%02i:%02i:%02i",h,m,s));
            }

        }
        float hours = day->hours(MT_CPAP);

        if (p_profile->cpap->showLeakRedline()) {
            float rlt = day->timeAboveThreshold(CPAP_Leak, p_profile->cpap->leakRedline()) / 60.0;
            float pc = 100.0 / hours * rlt;
            html+="<tr><td colspan=3 align='left' bgcolor='white'><b>"+tr("Time over leak redline")+
                    QString("</b></td><td colspan=2 bgcolor='white'>%1%</td></tr>").arg(pc, 0, 'f', 3);
        }
        int l = day->sum(CPAP_Ramp);

        if (l > 0) {
            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("Total ramp time")+
                    QString("</td><td colspan=2 bgcolor='white'>%1:%2:%3</td></tr>").arg(l / 3600, 2, 10, QChar('0')).arg((l / 60) % 60, 2, 10, QChar('0')).arg(l % 60, 2, 10, QChar('0'));
            float v = (hours - (float(l) / 3600.0));
            int q = v * 3600.0;
            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("Time outside of ramp")+
                    QString("</td><td colspan=2 bgcolor='white'>%1:%2:%3</td></tr>").arg(q / 3600, 2, 10, QChar('0')).arg((q / 60) % 60, 2, 10, QChar('0')).arg(q % 60, 2, 10, QChar('0'));

//            EventDataType hc = day->count(CPAP_Hypopnea) - day->countInsideSpan(CPAP_Ramp, CPAP_Hypopnea);
//            EventDataType oc = day->count(CPAP_Obstructive) - day->countInsideSpan(CPAP_Ramp, CPAP_Obstructive);

            //EventDataType tc = day->count(CPAP_Hypopnea) + day->count(CPAP_Obstructive);
//            EventDataType ahi = (hc+oc) / v;
            // Not sure if i was trying to be funny, and left on my replication of Devilbiss's bug here...  :P

//            html+="<tr><td colspan=3 align='left' bgcolor='white'>"+tr("AHI excluding ramp")+
//                    QString("</td><td colspan=2 bgcolor='white'>%1</td></tr>").arg(ahi, 0, 'f', 2);
        }

    }


    html+="</table>\n";
    html+="<hr/>\n";
    return html;
}

QString Daily::getEventBreakdown(Day * cpap)
{
    Q_UNUSED(cpap)
    QString html;
    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    html+="</table>";
    return html;
}

QString Daily::getSleepTime(Day * day)
{
    //cpap, Day * oxi
    QString html;

    if (!day || (day->hours() < 0.0000001))
        return html;

#ifndef COMBINE_MODE_3
    htmlLsbSectionHeader(html , tr("General") ,LSB_SLEEPTIME_INDICES );
    if (!leftSideBarEnable[LSB_SLEEPTIME_INDICES] ) {
        return html;
    }
#else
    if (!leftSideBarEnable[LSB_MACHINE_INFO] )  return html;
#endif

    html+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
    html+="<tr><td align='center'><b>"+STR_TR_Date+"</b></td><td align='center'><b>"+tr("Start")+"</b></td><td align='center'><b>"+tr("End")+"</b></td><td align='center'><b>"+STR_UNIT_Hours+"</b></td></tr>";
    int tt=qint64(day->total_time(MT_CPAP))/1000L;
    QDateTime date=QDateTime::fromSecsSinceEpoch(day->first()/1000L);
    QDateTime date2=QDateTime::fromSecsSinceEpoch(day->last()/1000L);

    int h=tt/3600;
    int m=(tt/60)%60;
    int s=tt % 60;
    html+=QString("<tr><td align='center'>%1</td><td align='center'>%2</td><td align='center'>%3</td><td align='center'>%4</td></tr>\n")
            .arg(date.date().toString(QLocale::system().dateFormat(QLocale::ShortFormat)))
            .arg(date.toString("HH:mm:ss"))
            .arg(date2.toString("HH:mm:ss"))
            .arg(QString::asprintf("%02i:%02i:%02i",h,m,s));
    html+="</table>\n";
    html+="<hr/>";

    return html;
}


QString Daily::getAHI(Day * day, bool isBrick) {
    QString html;

    float hours=day->hours(MT_CPAP);
    EventDataType ahi=day->count(AllAhiChannels);
    if (p_profile->general->calculateRDI()) ahi+=day->count(CPAP_RERA);
    ahi/=hours;

    html ="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    // Show application font, for debugging font issues
    // QString appFont = QApplication::font().toString();
    // html +=QString("<tr><td colspan=5 align=center>%1</td></tr>").arg(appFont);

    html +="<tr>";
    if (!isBrick) {
        ChannelID ahichan=CPAP_AHI;
        QString ahiname=STR_TR_AHI;
        if (p_profile->general->calculateRDI()) {
            ahichan=CPAP_RDI;
            ahiname=STR_TR_RDI;
        }
        html +=QString("<td colspan=5 bgcolor='%1' align=center><p title='%4'><font size=+3 color='%2'><b>%3</b></font></p> &nbsp; <font size=+3 color='%2'><b>%5</b></font></td>\n")
                .arg("#F88017").arg(COLOR_Text.name()).arg(ahiname).arg(schema::channel[ahichan].fullname()).arg(ahi,0,'f',2);
    } else {
        html +=QString("<td colspan=5 bgcolor='%1' align=center><font size=+3 color='yellow'><b>%2</b></font></td>\n")
                .arg("#F88017").arg(tr("This CPAP device does NOT record detailed data"));
    }
    html +="</tr>\n";
    html +="</table>\n";

    return html;
}

QString Daily::getIndices(Day * day, QHash<ChannelID, EventDataType>& values ) {
    QString html;
    float hours=day->hours(MT_CPAP);

    html = "<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";

    quint32 zchans = schema::SPAN | schema::FLAG;
    bool show_minors = false; // true;
    if (p_profile->general->showUnknownFlags()) zchans |= schema::UNKNOWN;

    if (show_minors) zchans |= schema::MINOR_FLAG;
    QList<ChannelID> available = day->getSortedMachineChannels(zchans);

    EventDataType val;
    for (int i=0; i < available.size(); ++i) {
        ChannelID code = available.at(i);
        schema::Channel & chan = schema::channel[code];
//                if (!chan.enabled()) continue;
        QString data;
        if (
            ( code == CPAP_UserFlag1 || code == CPAP_UserFlag2) &&
            ( ( !p_profile->cpap->userEventFlagging()) ||  (p_profile->cpap->clinicalMode()))  ){
            continue;
        }
        float channelHours = hours;
        if (chan.machtype() != MT_CPAP) {
            // Use device type hours (if available) rather than CPAP hours, since
            // might have Oximetry (for example) longer or shorter than CPAP
            channelHours = day->hours(chan.machtype());
            if (channelHours <= 0) {
                channelHours = hours;
            }
        }
        if (chan.type() == schema::SPAN) {
            val = (100.0 / channelHours)*(day->sum(code)/3600.0);
            data = QString("%1%").arg(val,0,'f',2);
        } else if (code == CPAP_VSnore2) {  // TODO: This should be generalized rather than special-casing a single channel here.
            val = day->sum(code) / channelHours;
            data = QString("%1").arg(val,0,'f',2);
        } else {
            val = day->count(code) / channelHours;
            data = QString("%1").arg(val,0,'f',2);
        }
        // TODO: percentage would be another useful option here for things like
        // percentage of patient-triggered breaths, which is much more useful
        // than the duration of timed breaths per hour.
        values[code] = val;
        QString tooltip=schema::channel[code].description();
        tooltip.replace("'", "&apos;");
        QColor altcolor = (brightness(chan.defaultColor()) < 0.3) ? Qt::white : Qt::black; // pick a contrasting color
        html+=QString("<tr><td align='left' bgcolor='%1'><b><font color='%2'><a href='event=%5' style='text-decoration:none;color:%2' title='<p>%6</p>'>%3</a></font></b></td><td width=20% bgcolor='%1'><b><font color='%2'>%4</font></b></td></tr>")
                .arg(chan.defaultColor().name())
                .arg(altcolor.name())
                .arg(chan.fullname())
                .arg(data)
                .arg(code)
                .arg(tooltip);
    }

    html+="</table><hr/>";

#ifndef COMBINE_MODE_3
    if (!leftSideBarEnable[LSB_SLEEPTIME_INDICES] )
#else
    if (!leftSideBarEnable[LSB_MACHINE_INFO] )
#endif
    {
        return QString();       // must calculate values first for pie chart.
    }

    return html;
}


void Daily::htmlLsbSectionHeaderInit (bool section) {
    if (!section) {
        leftSideBarEnable.fill(true);  // turn all sections On
        //leftSideBarEnable.fill(false);  // turn all sections off for testing
    }
    htmlLsbPrevSectionHeader = true;
}

void Daily::htmlLsbSectionHeader (QString&html , const QString& name,LEFT_SIDEBAR checkBox) {
        QString handle = "leftsidebarenable";
        bool on   = leftSideBarEnable[checkBox];  // this section is enabled
        bool prev = htmlLsbPrevSectionHeader;  // prev section will be displayed
                                                // only false when just section name is display
                                                // true when full section should be displayed.
        htmlLsbPrevSectionHeader = on;

        if ( (!prev) && on) {
            html+="<hr/>";
        }
        html += QString(
            "<table cellspacing=0 cellpadding=0 border=0 width='100%'>"
            "<tr>"
            "<td align=left>"
            #ifdef CONFIGURE_MODE
                "<a href='leftsidebarenable=%3'>" "<img src='qrc:/icons/session-%2.png'/a>"
            #else
                " "
            #endif
            "</td>"
            "<td align=centered;>"
                "<a href='leftsidebarenable=%3' style='text-decoration:none;color:black'>"
                   "<b>%1</b></a>"
            "</td>"
            "</table>\n"
            )
            .arg(name)
            .arg(on ? "on" : "off")
            .arg(checkBox);
}

QString Daily::getPieChart (float values, Day * day) {
    QString pieChartName = tr("Event Breakdown");
    QString html ;
    htmlLsbSectionHeader(html , pieChartName,LSB_PIE_CHART );
    if (!leftSideBarEnable[LSB_PIE_CHART] ) {
        return html;
    }
    html += "<table cellspacing=0 cellpadding=0 border=0 width='100%'>";
    if (values > 0) {
        html += QString("<tr><td align=center><b>%1</b></td></tr>").arg("");
        eventBreakdownPie()->setShowTitle(false);

        int w=155;
        int h=155;
        QPixmap pixmap=eventBreakdownPie()->renderPixmap(w,h,false);
        if (!pixmap.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray); // use buffer to store pixmap into byteArray
            buffer.open(QIODevice::WriteOnly);
            pixmap.save(&buffer, "PNG");
            // Close section where pie chart is pressed
            html += QString("<tr><td align=center>"
                "<a href='%1' style='text-decoration:none;color:black'>"
                "<img src=\"data:image/png;base64," + byteArray.toBase64() + "\"></td></tr>\n")
                .arg("leftsidebarenable=%1").arg(LSB_PIE_CHART)
                ;
        } else {
            html += "<tr><td align=center>"+tr("Unable to display Pie Chart on this system")+"</td></tr>\n";
        }
    } else {
        bool gotsome = false;
        for (int i = 0; i < ahiChannels.size(); i++)
            gotsome = gotsome || day->channelHasData(ahiChannels.at(i));

//        if (   day->channelHasData(CPAP_Obstructive)
//               || day->channelHasData(CPAP_AllApnea)
//               || day->channelHasData(CPAP_Hypopnea)
//               || day->channelHasData(CPAP_ClearAirway)
//               || day->channelHasData(CPAP_Apnea)

        if (   gotsome
               || day->channelHasData(CPAP_RERA)
               || day->channelHasData(CPAP_FlowLimit)
               || day->channelHasData(CPAP_SensAwake)
               ) {
            html += "<tr><td align=center><img src=\"qrc:/docs/0.0.gif\"></td></tr>\n";
        }
    }
    html+="</table>\n";
    html+="<hr/>\n";

    return html;
}

// honorPieChart true - show pie chart if it is enabled.  False, do not show pie chart
QString Daily::getLeftSidebar (bool honorPieChart) {
    QString html =   htmlLeftHeader
                   + htmlLeftAHI
                   + htmlLeftMachineInfo
                   + htmlLeftSleepTime
                   + htmlLeftIndices;
    // Include pie chart if wanted and enabled.
    if (honorPieChart && AppSetting->showPieChart())
        html += htmlLeftPieChart;

    html +=   htmlLeftNoHours
            + htmlLeftStatistics
            + htmlLeftOximeter
            + htmlLeftMachineSettings
            + htmlLeftSessionInfo
            + htmlLeftFooter;

    return html;
}

QVariant MyTextBrowser::loadResource(int type, const QUrl &url)
{
    if (type == QTextDocument::ImageResource && url.scheme().compare(QLatin1String("data"), Qt::CaseInsensitive) == 0) {
        static QRegularExpression re("^image/[^;]+;base64,.+={0,2}$");
        QRegularExpressionMatch match = re.match(url.path());
        if (match.hasMatch())
            return QVariant();
    }
    return QTextBrowser::loadResource(type, url);
}


void Daily::Load(QDate date)
{
    qDebug() << "Daily::Load called for" << date.toString() << "using" << QApplication::font().toString();

    qDebug() << "Setting App font in Daily::Load";
    setApplicationFont();

    dateDisplay->setText("<i>"+date.toString(QLocale::system().dateFormat(QLocale::ShortFormat))+"</i>");
    previous_date=date;

    Day * day = p_profile->GetDay(date);
    Machine *cpap = nullptr,
            *oxi = nullptr,
            //*stage = nullptr,
            *posit = nullptr;

    if (day) {
        cpap = day->machine(MT_CPAP);
        oxi = day->machine(MT_OXIMETER);
   //     stage = day->machine(MT_SLEEPSTAGE);
        posit = day->machine(MT_POSITION);
    }
    else {
        qDebug() << "Warning: unable to load day";
    }

    if (!AppSetting->cacheSessions()) {
        // Getting trashed on purge last day...

        // lastcpapday can get purged and be invalid
        if (lastcpapday && (lastcpapday!=day)) {
            for (QMap<QDate, Day *>::iterator di = p_profile->daylist.begin(); di!= p_profile->daylist.end(); ++di) {
                Day * d = di.value();
                if (d->eventsLoaded()) {
                    if (d->useCounter() == 0) {
                        d->CloseEvents();
                    }
                }
            }
        }
    }

    lastcpapday=day;

    // Clear the components of the left sidebar prior to recreating them
    htmlLeftAHI.clear();
    htmlLeftMachineInfo.clear();
    htmlLeftSleepTime.clear();
    htmlLeftIndices.clear();
    htmlLeftPieChart.clear();
    htmlLeftNoHours.clear();
    htmlLeftStatistics.clear();
    htmlLeftOximeter.clear();
    htmlLeftMachineSettings.clear();
    htmlLeftSessionInfo.clear();

    htmlLeftHeader = "<html><head>"
    "</head>"
    "<body leftmargin=0 rightmargin=0 topmargin=0 marginwidth=0 marginheight=0>";

    if (day) {
        day->OpenEvents();
    }
    GraphView->setDay(day);


    UpdateEventsTree(ui->treeWidget, day);

    // FIXME:
    // Generating entire statistics because bookmarks may have changed.. (This updates the side panel too)
    if (mainwin) {
        mainwin->GenerateStatistics();
    }

    snapGV->setDay(day);

    QString modestr;
    CPAPMode mode=MODE_UNKNOWN;
    QString a;
    bool isBrick=false;

    updateGraphCombo();

    updateEventsCombo(day);

    if (!cpap) {
        GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
    }
    if (cpap) {
        float hours=day->hours(MT_CPAP);
        if (GraphView->isEmpty() && (hours>0)) {
            // TODO: Eventually we should get isBrick from the loader, since some summary days
            // on a non-brick might legitimately have no graph data.
            bool gotsome = false;
            for (int i = 0; i < ahiChannels.size() && !gotsome ; i++) {
                gotsome = p_profile->hasChannel(ahiChannels.at(i));
            }
            if (!gotsome) {
                GraphView->setEmptyText(STR_Empty_Brick);

                GraphView->setEmptyImage(QPixmap(":/icons/sadface.png"));
                isBrick=true;
            } else {
                GraphView->setEmptyImage(QPixmap(":/icons/logo-md.png"));
            }
        }

        mode=(CPAPMode)(int)day->settings_max(CPAP_Mode);

        modestr=schema::channel[CPAP_Mode].m_options[mode];
        if (hours>0) {
            htmlLeftAHI= getAHI(day,isBrick);

            htmlLeftMachineInfo = getCPAPInformation(day);

            htmlLsbSectionHeaderInit();

            htmlLeftSleepTime = getSleepTime(day);

            QHash<ChannelID, EventDataType> values;
            htmlLeftIndices = getIndices(day,values);

            htmlLeftPieChart = getPieChart((values[CPAP_Obstructive] + values[CPAP_Hypopnea] + values[CPAP_AllApnea] +
                                            values[CPAP_ClearAirway] + values[CPAP_Apnea] + values[CPAP_RERA] +
                                            values[CPAP_FlowLimit] + values[CPAP_SensAwake]), day);
            htmlLsbSectionHeaderInit();

        } else {  // No hours
            htmlLeftNoHours+="<table cellspacing=0 cellpadding=0 border=0 width='100%'>\n";
            if (!isBrick) {
                htmlLeftNoHours+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
                if (day->size()>0) {
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><font size='+3'>"+tr("Sessions all off!")+"</font></td></tr>";
                    htmlLeftNoHours+="<tr><td colspan=5 align='center><img src='qrc:/icons/logo-md.png'></td></tr>";
                    htmlLeftNoHours+="<tr bgcolor='#89abcd'><td colspan=5 align='center'><i><font color=white size=+1>"+tr("Sessions exist for this day but are switched off.")+"</font></i></td></tr>\n";
                    GraphView->setEmptyText(STR_Empty_NoSessions);
                } else {
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><b><h2>"+tr("Impossibly short session")+"</h2></b></td></tr>";
                    htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Zero hours??")+"</i></td></tr>\n";
                }
            } else { // Device is a brick
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><b><h2>"+tr("no data :(")+"</h2></b></td></tr>";
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Sorry, this device only provides compliance data.")+"</i></td></tr>\n";
                htmlLeftNoHours+="<tr><td colspan=5 align='center'><i>"+tr("Complain to your Equipment Provider!")+"</i></td></tr>\n";
            }
            htmlLeftNoHours+="<tr><td colspan='5'>&nbsp;</td></tr>\n";
            htmlLeftNoHours+="</table><hr/>\n";
        }

    } else { // if (!CPAP)

        htmlLeftSleepTime = getSleepTime(day);
    }
    if (day) {
        htmlLeftOximeter = getOximeterInformation(day);
        htmlLeftMachineSettings = getMachineSettings(day);
        htmlLeftSessionInfo= getSessionInformation(day);
    }

    if ((cpap && !isBrick && (day->hours()>0)) || oxi || posit) {

        if ( (!cpap) && (!isBrick) ) {
            // add message when there is no cpap data but there exists oximeter data.
            QString  msg = tr("No CPAP data is available for this day");
            QString  beg = "<table cellspacing=0 cellpadding=0 border=0 width='100%'>"
            "<tr><td colspan=5 bgcolor='#89abcd' align=center><p title=''> &nbsp; <b><font size=+0 color=white>";
            htmlLeftAHI  = QString("%1 %2 </b></td></tr></table>").arg(beg).arg(msg);
        }

        htmlLeftStatistics = getStatisticsInfo(day);

    } else {
        if (cpap && day->hours(MT_CPAP)<0.0000001) {
        } else if (!isBrick) {
            htmlLeftAHI.clear();  // clear AHI (no cpap data) msg when there is no oximeter data
            htmlLeftSessionInfo.clear(); // clear session info

            htmlLeftStatistics ="<table cellspacing=0 cellpadding=0 border=0 height=100% width=100%>";
            htmlLeftStatistics+="<tr height=25%><td align=center></td></tr>";
            htmlLeftStatistics+="<tr><td align=center><font size='+3'>"+tr("\"Nothing's here!\"")+"</font></td></tr>";
            htmlLeftStatistics+="<tr><td align=center><img src='qrc:/icons/logo-md.png'></td></tr>";
            htmlLeftStatistics+="<tr height=5px><td align=center></td></tr>";
            htmlLeftStatistics+="<tr bgcolor='#89abcd'><td align=center><i><font size=+1 color=white>"+tr("No data is available for this day.")+"</font></i></td></tr>";
            htmlLeftStatistics+="<tr height=25%><td align=center></td></tr>";
            htmlLeftStatistics+="</table>\n";
        }

    }

    htmlLeftFooter ="</body></html>";

    // SessionBar colors.  Colors alternate.
    QColor cols[]={
        COLOR_Gold,
//      QColor("light blue"),
        QColor("skyblue"),
    };
    const int maxcolors=sizeof(cols)/sizeof(QColor);
    QList<Session *>::iterator i;

    sessionbar->clear();    // clear sessionbar as some days don't have sessions

    if (cpap) {
        int c=0;

        for (i=day->begin();i!=day->end();++i) {
            Session * s=*i;
            if ((*s).type() == MT_CPAP)
                sessionbar->add(s, cols[c % maxcolors]);
            c++;
        }
    }
    //sessbar->update();

#ifdef DEBUG_DAILY_HTML
    QString name = GetAppData()+"/test.html";
    QFile file(name);
    if (file.open(QFile::WriteOnly)) {
        const QByteArray & b = html.toLocal8Bit();
        file.write(b.data(), b.size());
        file.close();
    }
#endif

    webView->setHtml(getLeftSidebar(true));

    ui->JournalNotes->clear();

    ui->bookmarkTable->clearContents();
    ui->bookmarkTable->setRowCount(0);
    QStringList sl;
    ui->bookmarkTable->setHorizontalHeaderLabels(sl);
#ifndef REMOVE_FITNESS
    user_height_cm  = p_profile->user->height();
    set_ZombieUI(0);
    set_WeightUI(0);

#endif
    BookmarksChanged=false;
    Session *journal=GetJournalSession(date);
    if (journal) {
        if (journal->settings.contains(Journal_Notes)) {
            set_NotesUI(journal->settings[Journal_Notes].toString());
        }

#ifndef REMOVE_FITNESS
        bool ok;
        if (journal->settings.contains(Journal_Weight)) {
            double kg=journal->settings[Journal_Weight].toDouble(&ok);
            set_WeightUI(kg);
        }

        if (journal->settings.contains(Journal_ZombieMeter)) {
            int value = journal->settings[Journal_ZombieMeter].toInt(&ok);
            set_ZombieUI(value);
        }
#endif

        if (journal->settings.contains(Bookmark_Start)) {
            QVariantList start=journal->settings[Bookmark_Start].toList();
            QVariantList end=journal->settings[Bookmark_End].toList();
            QStringList notes=journal->settings[Bookmark_Notes].toStringList();

            if (start.size() > 0) {
                // Careful with drift here - apply to the label but not the
                // stored data (which will be saved if journal changes occur).
                qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
                Day * dday=p_profile->GetDay(previous_date,MT_CPAP);
                drift=(dday!=nullptr) ? clockdrift : 0;
                set_BookmarksUI(start ,end , notes, drift);
            }
        } // if (journal->settings.contains(Bookmark_Start))
    } // if (journal)
}

void Daily::UnitsChanged()
{
#ifndef REMOVE_FITNESS
    // Called from newprofile when height changed or when units changed metric / english
    // just as if weight changed. may make bmi visible.
    on_weightSpinBox_editingFinished();
#endif
}

double  Daily::calculateBMI(double weight_kg, double height_cm) {
    double height = height_cm/100.0;
    double bmi    = weight_kg/(height * height);
    return bmi;
}


QString Daily::convertHtmlToPlainText( QString html) {
    QTextDocument doc;
    doc.setHtml(html);
    QString plain = doc.toPlainText();
    return plain.simplified();
}

void Daily::set_JournalZombie(QDate& date, int value) {
    Session *journal=GetJournalSession(date);
    if (!journal) {
        journal=CreateJournalSession(date);
    }
    if (value==0) {
        // should delete zombie entry here. if null.
        auto jit = journal->settings.find(Journal_ZombieMeter);
        if (jit != journal->settings.end()) {
           journal->settings.erase(jit);
        }
    } else {
        journal->settings[Journal_ZombieMeter]=value;
    }
    journal->SetChanged(true);
};


void Daily::set_JournalWeightValue(QDate& date, double kg) {
    Session *journal=GetJournalSession(date);
    if (!journal) {
        journal=CreateJournalSession(date);
    }

    if (journal->settings.contains(Journal_Weight)) {
        QVariant old = journal->settings[Journal_Weight];
        if (abs(old.toDouble() - kg) < zeroD  && kg > zeroD) {
            // No change to weight - skip
            return ;
        }
    } else if (kg < zeroD) {
        // Still zero - skip
        return ;
    }
    if (kg > zeroD) {
        journal->settings[Journal_Weight]=kg;
    } else {
        // Weight now zero - remove from journal
        auto jit = journal->settings.find(Journal_Weight);
        if (jit != journal->settings.end()) {
            journal->settings.erase(jit);
        }
    }
    journal->SetChanged(true);
    return ;
}

void Daily::set_JournalNotesHtml(QDate& date, QString html) {
    QString newHtmlPlaintText = convertHtmlToPlainText(html); // have a look as plaintext to see if really empty.
    bool newHtmlHasContent = !ui->JournalNotes->document()->isEmpty() ;
    Session* journal = GetJournalSession(date,false);
    QString prevHtml;
    if (journal) {
        auto jit = journal->settings.find(Journal_Notes);
        if (jit != journal->settings.end()) {
            prevHtml = journal->settings[Journal_Notes].toString();
        }
    }
    if (!newHtmlHasContent) {
        if (!journal)  {
            return ; //no action required
        }
        // removing previous notes.
        auto jit = journal->settings.find(Journal_Notes);
        if (jit != journal->settings.end()) {
            journal->settings.erase(jit);
            journal->SetChanged(true);
        }
        return;
    } else if (html == prevHtml) {
        return ; //no action required
    }
    if (!journal) {
        journal = GetJournalSession(date,true);
        if (!journal) {
            journal = CreateJournalSession(date);
        }
    }
    journal->settings[Journal_Notes] = html;
    journal->SetChanged(true);
}


void Daily::clearLastDay()
{
    lastcpapday=nullptr;
}

void Daily::Unload(QDate date)
{
    if (!date.isValid()) {
        date = getDate();
        if (!date.isValid()) {
            graphView()->setDay(nullptr);
            return;
        }
    }

    // Update the journal notes
    set_JournalNotesHtml(date,ui->JournalNotes->toHtml() ) ;
    Session *journal = GetJournalSession(date);
    if (journal) {
        if (journal->IsChanged()) {
            journal->settings[LastUpdated] = QDateTime::currentDateTime();
            journal->machine()->SaveSummaryCache();
            journal->SetChanged(false); // save summary doesn't automatically do this
        }
    }
    UpdateCalendarDay(date);
}

void Daily::on_JournalNotesItalic_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontItalic(!format.fontItalic());


    cursor.mergeCharFormat(format);

}

void Daily::on_JournalNotesBold_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    int fw=format.fontWeight();
    if (fw!=QFont::Bold)
        format.setFontWeight(QFont::Bold);
    else
        format.setFontWeight(QFont::Normal);

    cursor.mergeCharFormat(format);

}

QVector<int> journalNotesFontSizeArray={10,15,20};
//QVector<int> journalNotesFontSizeArray={10,15,25};

void Daily::on_JournalNotesFontsize_activated(int index)
{
    int fontsize = journalNotesFontSizeArray[index];

    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (cursor.atEnd()  && cursor.atStart() ) {
        // set default font size
        QFont defaultFont = ui->JournalNotes->font();
        defaultFont.setPointSize(fontsize);
        defaultFont.setItalic(false);
        defaultFont.setBold(false);
        ui->JournalNotes->setFont(defaultFont);
    }

    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    QTextCharFormat format=cursor.charFormat();

    QFont font=format.font();
    font.setPointSize(fontsize);
    format.setFont(font);

    cursor.mergeCharFormat(format);
}

void Daily::on_JournalNotesColour_clicked()
{
    QColor col=QColorDialog::getColor(COLOR_Black,this,tr("Pick a Colour")); //,QColorDialog::NoButtons);
    if (!col.isValid()) return;

    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QBrush b(col);
    QPalette newPalette = palette();
    newPalette.setColor(QPalette::ButtonText, col);
    ui->JournalNotesColour->setPalette(newPalette);

    QTextCharFormat format=cursor.charFormat();

    format.setForeground(b);

    cursor.setCharFormat(format);
}
Session * Daily::CreateJournalSession(QDate date)
{
    Machine *m = p_profile->GetMachine(MT_JOURNAL);
    if (!m) {
        m=new Machine(p_profile, 0);
        MachineInfo info;
        info.loadername = "Journal";
        info.serial = m->hexid();
        info.brand = "Journal";
        info.type = MT_JOURNAL;
        m->setInfo(info);
        m->setType(MT_JOURNAL);
        p_profile->AddMachine(m);
    }

    Session *sess=new Session(m,0);
    qint64 st,et;

    Day *cday=p_profile->GetDay(date);
    if (cday) {
        st=cday->first();
        et=cday->last();
    } else {
        QDateTime dt(date,QTime(20,0));
        st=qint64(dt.toSecsSinceEpoch())*1000L;
        et=st+3600000L;
    }
    sess->SetSessionID(st / 1000L);
    sess->set_first(st);
    sess->set_last(et);
    m->AddSession(sess, true);
    return sess;
}

Session * Daily::GetJournalSession(QDate date , bool create) // Get the first journal session
{
    Day *day=p_profile->GetDay(date, MT_JOURNAL);
    if (day) {
        Session * session = day->firstSession(MT_JOURNAL);
        if (!session && create) {
            session = CreateJournalSession(date);
        }
        return session;
    }
    return nullptr;
}


void Daily::RedrawGraphs()
{
    // setting this here, because it needs to be done when preferences change
    if (p_profile->cpap->showLeakRedline()) {
        schema::channel[CPAP_Leak].setUpperThreshold(p_profile->cpap->leakRedline());
    } else {
        schema::channel[CPAP_Leak].setUpperThreshold(0); // switch it off
    }

    QFont appFont = QApplication::font();
    dateDisplay->setFont(appFont);

    GraphView->redraw();
}

void Daily::on_LineCursorUpdate(double time)
{
    if (time > 1) {
        // use local time since this string is displayed to the user
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(time, QTimeZone::systemTimeZone());
        QString txt = dt.toString("MMM dd HH:mm:ss.zzz");
        dateDisplay->setText(txt);
    } else dateDisplay->setText(QString(GraphView->emptyText()));
}

void Daily::on_RangeUpdate(double minx, double /*maxx*/)
{
    if (minx > 1) {
        dateDisplay->setText(GraphView->getRangeString());
    } else {
        dateDisplay->setText(QString(GraphView->emptyText()));
    }
}


void Daily::on_treeWidget_itemClicked(QTreeWidgetItem *item, int )
{
    if (!item) return;
    QTreeWidgetItem* parent = item->parent();
    if (!parent) return;
    gGraph *g=GraphView->findGraph(STR_GRAPH_SleepFlags);
    if (!g) return;
    QDateTime d;
    if (!item->data(0,Qt::UserRole).isNull()) {
        int eventType = parent->type();
        qint64 time = item->data(0,Qt::UserRole).toLongLong();

        // for events display 3 minutes before and 20 seconds after
        // for start time skip abit before graph
        // for end   time skip abut after graph

        qint64 period = qint64(p_profile->general->eventWindowSize())*60000L;  // eventwindowsize units  minutes
        qint64 small  = period/10;

        qint64 start,end;
        if (eventType == eventTypeStart ) {
            start = time - small;
            end = time + period;
        } else {
            start = time - period;
            end   = time + small;
        }

        GraphView->SetXBounds(start,end);
    }
}

void Daily::on_treeWidget_itemSelectionChanged()
{
    if (ui->treeWidget->selectedItems().size()==0) return;
    QTreeWidgetItem *item=ui->treeWidget->selectedItems().at(0);
    if (!item) return;
    on_treeWidget_itemClicked(item, 0);
}

void Daily::on_JournalNotesUnderline_clicked()
{
    QTextCursor cursor = ui->JournalNotes->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);

    QTextCharFormat format=cursor.charFormat();

    format.setFontUnderline(!format.fontUnderline());

    cursor.mergeCharFormat(format);
}

void Daily::on_prevDayButton_clicked()
{
    if (previous_date.isValid()) {
         Unload(previous_date);
    }
    if (!p_profile->ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(-1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(-1);
            if (p_profile->GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

bool Daily::eventFilter(QObject *object, QEvent *event)
{
    if (false && object == ui->JournalNotes && event->type() == QEvent::FocusOut) {
        // Trigger immediate save of journal when we focus out from it so we never
        // lose any journal entry text...
        if (previous_date.isValid()) {
            Unload(previous_date);
        }
    }
    return false;
}

void Daily::on_nextDayButton_clicked()
{
    if (previous_date.isValid()) {
         Unload(previous_date);
    }
    if (!p_profile->ExistsAndTrue("SkipEmptyDays")) {
        LoadDate(previous_date.addDays(1));
    } else {
        QDate d=previous_date;
        for (int i=0;i<90;i++) {
            d=d.addDays(1);
            if (p_profile->GetDay(d)) {
                LoadDate(d);
                break;
            }
        }
    }
}

void Daily::on_calButton_toggled(bool checked)
{
    bool b=checked;
    ui->calendarFrame->setVisible(b);
    AppSetting->setCalendarVisible(b);

    if (!b) {
        ui->calButton->setArrowType(Qt::DownArrow);
    } else {
        ui->calButton->setArrowType(Qt::UpArrow);
    }
}


void Daily::on_todayButton_clicked()
{
    if (previous_date.isValid()) {
         Unload(previous_date);
    }
//    QDate d=QDate::currentDate();
//    if (d > p_profile->LastDay()) {
        QDate lastcpap = p_profile->LastDay(MT_CPAP);
        QDate lastoxi = p_profile->LastDay(MT_OXIMETER);

        QDate d = qMax(lastcpap, lastoxi);
//    }
    LoadDate(d);
}

void Daily::on_evViewSlider_valueChanged(int value)
{
    ui->evViewLCD->display(value);
    p_profile->general->setEventWindowSize(value);

    int winsize=value*60;

    gGraph *g=GraphView->findGraph(STR_GRAPH_SleepFlags);
    if (!g) return;
    qint64 st=g->min_x;
    qint64 et=g->max_x;
    qint64 len=et-st;
    qint64 d=st+len/2.0;

    st=d-(winsize/2)*1000;
    et=d+(winsize/2)*1000;
    if (st<g->rmin_x) {
        st=g->rmin_x;
        et=st+winsize*1000;
    }
    if (et>g->rmax_x) {
        et=g->rmax_x;
        st=et-winsize*1000;
    }
    GraphView->SetXBounds(st,et);
}

static QRegularExpression emptyStr("^\\s*$") ;
void Daily::verifyBookMarkName(QTableWidgetItem* item) {
    if (!item) {
        return;
    }

    int column=item->column();
    if (column != 1) {
        int row=item->row();
        item = ui->bookmarkTable->item(row,1);
    }
    // item now points to bookmarks that has notes (text).
    if (item->text().contains(emptyStr) ) {
        bool ok=false;
        qint64 start=item->data(Qt::UserRole).toLongLong(&ok);
        QDateTime d=QDateTime::fromSecsSinceEpoch(start/1000L, QTimeZone::systemTimeZone());
        QString text = tr("Bookmark at %1").arg(d.time().toString("HH:mm:ss"));
        item->setText(text);
    }
}

void Daily::on_bookmarkTable_currentItemChanged(QTableWidgetItem *item, QTableWidgetItem *item2)
{
    if(!item) {
        // can get here at times.
        if(item2) {
            DEBUGCI QQ(row,item2->row()) QQ(col,item2->column()) O(item2->text());
        }
        return;
    }
    int row=item->row();
    qint64 st,et;

    qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
    Day * dday=p_profile->GetDay(previous_date,MT_CPAP);
    drift=(dday!=nullptr) ? clockdrift : 0;

    QTableWidgetItem *it=ui->bookmarkTable->item(row,1);
    bool ok;

    // Add drift back in (it was removed in addBookmark)
    st=it->data(Qt::UserRole).toLongLong(&ok) + drift;
    et=it->data(Qt::UserRole+1).toLongLong(&ok) + drift;
    qint64 st2=0,et2=0,st3,et3;
    Day * day=p_profile->GetGoodDay(previous_date,MT_CPAP);
    if (day) {
        st2=day->first();
        et2=day->last();
    }
    Day * oxi=p_profile->GetGoodDay(previous_date,MT_OXIMETER);
    if (oxi) {
        st3=oxi->first();
        et3=oxi->last();
    }
    if (oxi && day) {
        st2=qMin(st2,st3);
        et2=qMax(et2,et3);
    } else if (oxi) {
        st2=st3;
        et2=et3;
    } else if (!day) return;
    if ((et<st2) || (st>et2)) {
        mainwin->Notify(tr("This bookmark is in a currently disabled area.."));
        return;
    }

    if (st<st2) st=st2;
    if (et>et2) et=et2;
    GraphView->SetXBounds(st,et);
    GraphView->redraw();
}

void Daily::addBookmark(qint64 st, qint64 et, QString text)
{
    ui->bookmarkTable->blockSignals(true);
    QDateTime d=QDateTime::fromSecsSinceEpoch(st/1000L, QTimeZone::systemTimeZone());
    int row=ui->bookmarkTable->rowCount();
    ui->bookmarkTable->insertRow(row);
    QTableWidgetItem *tw=new QTableWidgetItem(text);
    QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
    dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    ui->bookmarkTable->setItem(row,0,dw);
    ui->bookmarkTable->setItem(row,1,tw);
    qint64 clockdrift=p_profile->cpap->clockDrift()*1000L,drift;
    Day * day=p_profile->GetDay(previous_date,MT_CPAP);
    drift=(day!=nullptr) ? clockdrift : 0;

    // Counter CPAP clock drift for storage, in case user changes it later on
    // This won't fix the text string names..

    tw->setData(Qt::UserRole,st-drift);
    tw->setData(Qt::UserRole+1,et-drift);

    ui->bookmarkTable->blockSignals(false);
    update_Bookmarks();
    mainwin->updateFavourites();

}

void Daily::on_addBookmarkButton_clicked()
{
    qint64 st,et;
    GraphView->GetXBounds(st,et);
    QDateTime d=QDateTime::fromSecsSinceEpoch(st/1000L, QTimeZone::systemTimeZone());
    addBookmark(st,et,"" );
}

void Daily::update_Bookmarks()
{
    QVariantList start;
    QVariantList end;
    QStringList notes;
    QTableWidgetItem *item;
    for (int row=0;row<ui->bookmarkTable->rowCount();row++) {
        item=ui->bookmarkTable->item(row,1);
        verifyBookMarkName(item);

        start.push_back(item->data(Qt::UserRole));
        end.push_back(item->data(Qt::UserRole+1));
        notes.push_back(item->text());
    }
    Session *journal=GetJournalSession(previous_date);
    if (!journal) {
        journal=CreateJournalSession(previous_date);
    }

    journal->settings[Bookmark_Start]=start;
    journal->settings[Bookmark_End]=end;
    journal->settings[Bookmark_Notes]=notes;
    journal->settings[LastUpdated]=QDateTime::currentDateTime();
    journal->SetChanged(true);
    BookmarksChanged=true;
    mainwin->updateFavourites();
}

void Daily::on_removeBookmarkButton_clicked()
{
    int row=ui->bookmarkTable->currentRow();
    if (row>=0) {
        ui->bookmarkTable->blockSignals(true);
        ui->bookmarkTable->removeRow(row);
        ui->bookmarkTable->blockSignals(false);
        update_Bookmarks();
    }
    mainwin->updateFavourites();
}

#ifndef REMOVE_FITNESS
void Daily::set_NotesUI(QString htmlNote) {
    ui->JournalNotes->setHtml(htmlNote);
};

void Daily::set_BmiUI(double user_weight_kg) {
    if ((user_height_cm>zeroD) && (user_weight_kg>zeroD)) {
        double bmi = calculateBMI(user_weight_kg, user_height_cm);
        QString label=QString("B.M.I   %1").arg(bmi,0,'f',2);
        ui->BMIlabel->setText(label);
        ui->BMIlabel->setVisible(true);
    } else {
        // BMI now zero - remove it
        // And make it invisible
        ui->BMIlabel->setVisible(false);
    }
    if (mainwin) {
        mainwin->updateOverview();
    }
};

void Daily::set_WeightUI(double kg) {
    ui->weightSpinBox->blockSignals(true);
    ui->weightSpinBox->setDecimals(WEIGHT_SPIN_BOX_DECIMALS);
    if (p_profile->general->unitSystem()==US_Metric) {
        ui->weightSpinBox->setSuffix(QString(" %1").arg(STR_UNIT_KG));
        ui->weightSpinBox->setValue(kg);
    } else {
        double lbs = kg * pounds_per_kg;
        ui->weightSpinBox->setSuffix(QString(" %1").arg(STR_UNIT_POUND));
        ui->weightSpinBox->setValue(lbs);
    }
    ui->weightSpinBox->blockSignals(false);
    set_BmiUI(kg);
};

void Daily::set_BookmarksUI( QVariantList& start , QVariantList& end , QStringList& notes, qint64 drift) {
    ui->bookmarkTable->blockSignals(true);
    ui->bookmarkTable->setRowCount(0);
    // Careful with drift here - apply to the label but not the
    // stored data (which will be saved if journal changes occur).
    bool ok;
    int qty = start.size();
    for (int i=0;i<qty;i++) {
        qint64 st=start.at(i).toLongLong(&ok);
        qint64 et=end.at(i).toLongLong(&ok);

        QDateTime d=QDateTime::fromSecsSinceEpoch((st+drift)/1000L);
        ui->bookmarkTable->insertRow(i);
        QTableWidgetItem *tw=new QTableWidgetItem(notes.at(i));
        QTableWidgetItem *dw=new QTableWidgetItem(d.time().toString("HH:mm:ss"));
        dw->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
        ui->bookmarkTable->setItem(i,0,dw);
        ui->bookmarkTable->setItem(i,1,tw);
        tw->setData(Qt::UserRole,st);
        tw->setData(Qt::UserRole+1,et);
        verifyBookMarkName(tw);
    }
    ui->bookmarkTable->blockSignals(false);
}

void Daily::on_bookmarkTable_itemChanged(QTableWidgetItem *)
{
    update_Bookmarks();
}

/*
Feelings (Zombie originally used in Code)
Originally had a range for 1-10. with 0 being null.
the value stored on disk was just an integer.
Adding a new range of 1-100 without destroying the original range of 1-10
the value 1-10 are the min/max for the original feature.
the value displayed are called uiValues, which are seen by the end user.
the value stored on disk and used are call ZombieValues.
ZombieValues have
    0          null
    1-10       range 1-10
    N+(1-100)  range 1-100
The user will have a double spinBox to display the value.
    and a push button that display the range and toggles range
The slider will be display the current range.
in the range 1-10  the user can input a number between 0-10 with tenths
in the range 1-100  the user can input a number between 0-100
ZombieValue saved in journal

*/
static int ZombieModeOffset = 20;

void Daily::setup_ZombieUIWidgets(int zombieValue, bool zombieMode, bool setup) {
    ui->ZombieSpinBox->blockSignals(true);
    ui->ZombieSlider->blockSignals(true);
    int value100;
    if (zombieValue<ZombieModeOffset) {
        value100 = zombieValue * 10;
    } else {
        value100= zombieValue-ZombieModeOffset;
    }
    if (value100<0||value100>100) {
        // illegal value detected.
        value100=0;
    }

    ui->ZombieSlider->setValue(value100);

    // new setup spin box
    double spinValue = value100;
    if (setup) {
        ui->ZombieSpinBox->setMaximum(zombieMode ?  100 : 10);
        ui->ZombieSpinBox->setDecimals(zombieMode ?  0 : 1);
        ui->ZombieSpinBox->setSingleStep(zombieMode ?  10 : 1);
        ui->Units10_100->setText(zombieMode ?  QStringLiteral("/100") : QStringLiteral("/10"));
    }
    ui->ZombieSpinBox->setValue(zombieMode ? spinValue  : (spinValue/10.0));

    ui->ZombieSlider->blockSignals(false);
    ui->ZombieSpinBox->blockSignals(false);
}

void Daily::set_ZombieUI(int zombieValue, bool init)
{
    setup_ZombieUIWidgets(zombieValue, p_profile->appearance->zombieMode(),init );
}

void Daily::on_ZombieSlider_valueChanged(int uiValue)
{
    int zombieValue = uiValue+ZombieModeOffset;
    set_ZombieUI(zombieValue);
    set_JournalZombie(previous_date,zombieValue);
    mainwin->updateOverview();
}

void Daily::on_ZombieSpinBox_valueChanged(double spinValue)
{
    if (ui->ZombieSpinBox->decimals() == 1) {
        spinValue *=10;
    }
    on_ZombieSlider_valueChanged(spinValue);
}

void Daily::on_Units10_100_clicked(){
    bool mode=p_profile->appearance->zombieMode();
    p_profile->appearance->setZombieMode(!mode);
    int uiValue = ui->ZombieSlider->value();
    int zombieValue = uiValue+ZombieModeOffset;
    set_ZombieUI(zombieValue,true);
}

void Daily::on_weightSpinBox_valueChanged(double )
{
    on_weightSpinBox_editingFinished();
}

void Daily::on_weightSpinBox_editingFinished()
{
    user_height_cm = p_profile->user->height();
    double kg = ui->weightSpinBox->value();
    if (p_profile->general->unitSystem()==US_English) {
        double lbs = ui->weightSpinBox->value();
        kg =  lbs*kgs_per_pound; // convert pounds to kg.
    }
    if (kg < zeroD) kg = 0.0;
    set_JournalWeightValue(previous_date,kg) ;
    set_BmiUI(kg);
    gGraphView *gv=mainwin->getOverview()->graphView();
    gGraph *g;
    if (gv) {
        g=gv->findGraph(STR_GRAPH_Weight);
        if (g) g->setDay(nullptr);
    }
    if (gv) {
        g=gv->findGraph(STR_GRAPH_BMI);
        if (g) g->setDay(nullptr);
    }
    mainwin->updateOverview();
}
#endif

QString Daily::GetDetailsText()
{
    QTextDocument * doc = webView->document();
    QString content = doc->toHtml();

    return content;
}

void Daily::setGraphText () {
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
        }
    }
    ui->graphCombo->setItemIcon(0, numOff ? *icon_warning : *icon_up_down);
    QString graphText;
    int lastIndex = ui->graphCombo->count()-1;  // account for hideshow button
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

void Daily::setFlagText () {
    int numOff = 0;
    int numTotal = 0;

    int lastIndex = ui->eventsCombo->count()-1;  // account for hideshow button
    for (int i=1; i < lastIndex; ++i) {
        numTotal++;
        ChannelID code = ui->eventsCombo->itemData(i, Qt::UserRole).toUInt();
        #if defined(STEADY_BREATHING)
        if (AppSetting->steadyBreathing()!=SB_ACTIVE) {
            if (code == CPAP_SteadyBreathing || code == CPAP_SteadyBreathingFlag)  {
                continue;
            }
        }
        #endif
        schema::Channel * chan = &schema::channel[code];

        if (!chan->enabled())
            numOff++;
    }

    int numOn=numTotal;
    ui->eventsCombo->setItemIcon(0, numOff ? *icon_warning : *icon_up_down);
    QString flagsText;
    if (numOff == 0) {
        flagsText = (QObject::tr("%1 Event Types")+"       ").arg(numTotal);
        ui->eventsCombo->setItemText(lastIndex,STR_HIDE_ALL_EVENTS);
    } else {
        numOn-=numOff;
        flagsText = QObject::tr("%1 of %2 Event Types").arg(numOn).arg(numTotal);
        if (numOn==0) ui->eventsCombo->setItemText(lastIndex,STR_SHOW_ALL_EVENTS);
    }

    ui->eventsCombo->setItemText(0, flagsText);
    sleepFlagsGroup->refreshConfiguration(sleepFlags); // need to know display changes before painting.
}

void Daily::showAllGraphs(bool show) {
    //Skip over first button - label for comboBox
    int lastIndex = ui->graphCombo->count()-1;
    for (int i=1;i<lastIndex;i++) {
        showGraph(i,show);
    }
    setGraphText();
    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}

void Daily::showGraph(int index,bool show, bool updateGraph) {
    ui->graphCombo->setItemData(index,show,Qt::UserRole);
    ui->graphCombo->setItemIcon(index, show ? *icon_on : *icon_off);
    if (!updateGraph) return;
    QString graphName = ui->graphCombo->itemText(index);
    gGraph* graph=GraphView->findGraphTitle(graphName);
    if (graph) graph->setVisible(show);
}

void Daily::on_graphCombo_activated(int index)
{
    if (index<0) return;
    int lastIndex = ui->graphCombo->count()-1;
    if (index > 0) {
        bool nextOn =!ui->graphCombo->itemData(index,Qt::UserRole).toBool();
        if ( index == lastIndex ) {
            // user just pressed hide show button - toggle sates of the button and apply the new state
            showAllGraphs(nextOn);
            showGraph(index,nextOn,false);
        } else {
            showGraph(index,nextOn,true);
        }
        ui->graphCombo->showPopup();
    }
    ui->graphCombo->setCurrentIndex(0);
    setGraphText();
    updateCube();
    GraphView->updateScale();
    GraphView->redraw();
}

void Daily::updateCube()
{
    //brick..
    if ((GraphView->visibleGraphs()==0)) {
        if (ui->graphCombo->count() > 0) {
            GraphView->setEmptyText(STR_Empty_NoGraphs);
        } else {
            if (!p_profile->GetGoodDay(getDate(), MT_CPAP)
              && !p_profile->GetGoodDay(getDate(), MT_OXIMETER)
              && !p_profile->GetGoodDay(getDate(), MT_SLEEPSTAGE)
              && !p_profile->GetGoodDay(getDate(), MT_POSITION)) {
                GraphView->setEmptyText(STR_Empty_NoData);

            } else {
                if (GraphView->emptyText() != STR_Empty_Brick)
                    GraphView->setEmptyText(STR_Empty_SummaryOnly);
            }
        }
    }
}


void Daily::updateGraphCombo()
{
    ui->graphCombo->clear();
    gGraph *g;

    ui->graphCombo->addItem(*icon_up_down, "", true);   // text updated in setGRaphText

    for (int i=0;i<GraphView->size();i++) {
        g=(*GraphView)[i];
        if (g->isEmpty()) continue;

        if (g->visible()) {
            ui->graphCombo->addItem(*icon_on,g->title(),true);
        } else {
            ui->graphCombo->addItem(*icon_off,g->title(),false);
        }
    }
    ui->graphCombo->addItem(*icon_on,STR_HIDE_ALL_GRAPHS,true);
    ui->graphCombo->setCurrentIndex(0);
    setGraphText();
    updateCube();
}

void Daily::updateEventsCombo(Day* day) {

    quint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
    if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;

    QList<ChannelID> available;
    if (day) available.append(day->getSortedMachineChannels(chans));

    ui->eventsCombo->clear();
    ui->eventsCombo->addItem(*icon_up_down, "", 0);   // text updated in setflagText
    for (int i=0; i < available.size(); ++i) {
        ChannelID code = available.at(i);
        #if false && defined(STEADY_BREATHING)
        if (!AppSetting->steadyBreathing()==SB_ACTIVE) {
            if (code == CPAP_SteadyBreathing || code == CPAP_SteadyBreathingFlag)  {
                continue;
            }
        }
        #endif
        int comboxBoxIndex = i+1;
        schema::Channel & chan = schema::channel[code];
        ui->eventsCombo->addItem(chan.enabled() ? *icon_on : * icon_off, chan.label(), code);
        ui->eventsCombo->setItemData(comboxBoxIndex, chan.fullname(), Qt::ToolTipRole);
        dailySearchTab->updateEvents(code,chan.fullname());
    }
    ui->eventsCombo->addItem(*icon_on,"" , Qt::ToolTipRole);
    ui->eventsCombo->setCurrentIndex(0);
    setFlagText();
}

void Daily::showAllEvents(bool show) {
    // Mark all events as active
    int lastIndex = ui->eventsCombo->count()-1;  // account for hideshow button
    for (int i=1;i<lastIndex;i++) {
        // If disabled, emulate a click to enable the event
        ChannelID code = ui->eventsCombo->itemData(i, Qt::UserRole).toUInt();
        #if false && defined(STEADY_BREATHING)
        if (!AppSetting->steadyBreathing()==SB_ACTIVE) {
            if (code == CPAP_SteadyBreathing || code == CPAP_SteadyBreathingFlag)  {
                continue;
            }
        }
        #endif
        schema::Channel * chan = &schema::channel[code];
        if (chan->enabled()!=show) {
            Daily::on_eventsCombo_activated(i);
        }
    }
    ui->eventsCombo->setItemData(lastIndex,show,Qt::UserRole);
    ui->eventsCombo->setCurrentIndex(0);
    setFlagText();
}

void Daily::on_eventsCombo_activated(int index)
{
    if (index<0)
        return;

    int lastIndex = ui->eventsCombo->count()-1;
    if (index > 0) {
        if ( index == lastIndex ) {
            bool nextOn =!ui->eventsCombo->itemData(index,Qt::UserRole).toBool();
            showAllEvents(nextOn);
        } else {
            ChannelID code = ui->eventsCombo->itemData(index, Qt::UserRole).toUInt();
            schema::Channel * chan = &schema::channel[code];

            bool b = !chan->enabled();
            chan->setEnabled(b);
            ui->eventsCombo->setItemIcon(index,b ? *icon_on : *icon_off);
        }
        ui->eventsCombo->showPopup();
    }

    ui->eventsCombo->setCurrentIndex(0);
    setFlagText();
    GraphView->redraw();
}

void Daily::on_splitter_2_splitterMoved(int, int)
{
    int size = ui->splitter_2->sizes()[0];
    if (size == 0) return;
//  qDebug() << "Left Panel width set to " << size;
    AppSetting->setDailyPanelWidth(size);
}

void Daily::on_graphHelp_clicked() {
    if (!saveGraphLayoutSettings) {
        saveGraphLayoutSettings= new SaveGraphLayoutSettings("daily",this);
    }
    if (saveGraphLayoutSettings) {
        saveGraphLayoutSettings->hintHelp();
    }
}

void Daily::on_layout_clicked() {
    if (!saveGraphLayoutSettings) {
        saveGraphLayoutSettings= new SaveGraphLayoutSettings("daily",this);
    }
    if (saveGraphLayoutSettings) {
        saveGraphLayoutSettings->triggerLayout(GraphView);
    }
}
