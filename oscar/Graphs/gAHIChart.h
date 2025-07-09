/* gAHIChart Header
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GAHICHART_H
#define GAHICHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"

#include "Graphs/gSummaryChart.h"


class gAHIChart : public gSummaryChart
{
public:
    gAHIChart()
        :gSummaryChart("AHIChart", MT_CPAP) {
        for (int i = 0; i < ahiChannels.size(); i++)
            addCalc(ahiChannels.at(i), ST_CPH);

//        addCalc(CPAP_ClearAirway, ST_CPH);
//        addCalc(CPAP_AllApnea, ST_CPH);
//        addCalc(CPAP_Obstructive, ST_CPH);
//        addCalc(CPAP_Apnea, ST_CPH);
//        addCalc(CPAP_Hypopnea, ST_CPH);
        if (p_profile->general->calculateRDI())
            addCalc(CPAP_RERA, ST_CPH);
    }
    virtual ~gAHIChart() {}

    virtual void preCalc();
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    virtual void populate(Day *, int idx);

    virtual QString tooltipData(Day * day, int);

    virtual Layer * Clone() {
        gAHIChart * sc = new gAHIChart();
        gSummaryChart::CloneInto(sc);
        CloneInto(sc);
        return sc;
    }

    void CloneInto(gAHIChart * /* layer */) {
//        layer->ahicalc = ahicalc;
//        layer->ahi_wavg = ahi_wavg;
//        layer->ahi_avg = ahi_avg;
//        layer->total_hours = total_hours;
//        layer->max_ahi = max_ahi;
//        layer->min_ahi = min_ahi;
//        layer->total_days = total_days;
//        layer->ahi_data = ahi_data;
    }

  //  SummaryCalcItem ahicalc;
    double ahi_wavg;
    double ahi_avg;

    double total_hours;
    float max_ahi;
    float min_ahi;

    int total_days;
    QList<float> ahi_data;
};


#endif // GAHICHART_H

