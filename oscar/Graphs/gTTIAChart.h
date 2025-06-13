/* gTTIAChart Header
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GTTIACHART_H
#define GTTIACHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"
#include "Graphs/gSummaryChart.h"


class gTTIAChart : public gSummaryChart
{
public:
    gTTIAChart()
        :gSummaryChart("TTIA", MT_CPAP) {
        addCalc(NoChannel, ST_CNT, QColor(255,147,150));
    }
    virtual ~gTTIAChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);
    virtual void populate(Day *day, int idx);
    virtual QString tooltipData(Day * day, int);

    virtual Layer * Clone() {
        gTTIAChart * sc = new gTTIAChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gTTIAChart * /* layer*/) {
    }

private:
};
#endif // GTTIACHART_H
