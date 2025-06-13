/* gSessionTimesChart Header
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GSESSIONTIMESCHART_H
#define GSESSIONTIMESCHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"

#include "Graphs/gSummaryChart.h"


/*! \class gSessionTimesChart
    \brief Displays a summary of session times
    */
class gSessionTimesChart : public gSummaryChart
{
public:
    gSessionTimesChart()
        :gSummaryChart("SessionTimes", MT_CPAP) {
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
        addCalc(NoChannel, ST_SESSIONS, QColor(64,128,255));
    }
    virtual ~gSessionTimesChart() {}

    virtual void SetDay(Day * day = nullptr) {
        gSummaryChart::SetDay(day);
        split = p_profile->session->daySplitTime();

        m_miny = 0;
        m_maxy = 28;
    }

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> & slices);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    //! \brief Renders the graph to the QPainter object
    virtual void paint(QPainter &painter, gGraph &graph, const QRegion &region);

    virtual Layer * Clone() {
        gSessionTimesChart * sc = new gSessionTimesChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gSessionTimesChart * layer) {
        layer->split = split;
    }
    QTime split;
    int num_slices;
    int num_days;
    int total_slices;
    double total_length;
    QList<float> session_data;

};


#endif // GSESSIONTIMESCHART_H
