/* gUsageChart Header
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */
#if 1
#ifndef GUSAGECHART_H
#define GUSAGECHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"
#include "Graphs/gSummaryChart.h"


class gUsageChart : public gSummaryChart
{
public:
    gUsageChart()
        :gSummaryChart("Usage", MT_CPAP) {
        addCalc(NoChannel, ST_HOURS, QColor(64,128,255));
    }
    virtual ~gUsageChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);
    virtual void populate(Day *day, int idx);

    virtual QString tooltipData(Day * day, int);


    virtual Layer * Clone() {
        gUsageChart * sc = new gUsageChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gUsageChart * layer) {
        layer->incompdays = incompdays;
        layer->compliance_threshold = compliance_threshold;
    }

private:
    int incompdays;
    EventDataType compliance_threshold;
};

#endif // GUSAGECHART_H
#endif
