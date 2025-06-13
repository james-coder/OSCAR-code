/* gPressureChart Header
 *
 * Copyright (c) 2020-2025 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef GPRESSURECHART_H
#define GPRESSURECHART_H

#include "gSummaryChart.h"

class gPressureChart : public gSummaryChart
{
public:
    gPressureChart();
    virtual ~gPressureChart() {}

    virtual Layer * Clone() {
        gPressureChart * sc = new gPressureChart();
        gSummaryChart::CloneInto(sc);
        return sc;
    }

//    virtual void preCalc();
    virtual void customCalc(Day *day, QVector<SummaryChartSlice> &slices) {
        int size = slices.size();
        float hour = day->hours(m_machtype);
        for (int i=0; i < size; ++i) {
            SummaryChartSlice & slice = slices[i];
            SummaryCalcItem * calc = slices[i].calc;

            calc->update(slice.value, hour);
         }
    }
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    virtual void populate(Day * day, int idx);

    virtual QString tooltipData(Day * day, int idx) {
        return day->getCPAPModeStr() + "\n" + day->getPressureSettings() + gSummaryChart::tooltipData(day, idx);
    }

    virtual int addCalc(ChannelID code, SummaryType type);

protected:
    SummaryCalcItem* getCalc(ChannelID code, SummaryType type = ST_SETMAX);
    QString channelRange(ChannelID code, const QString & label);
    void addSlice(ChannelID code, SummaryType type = ST_SETMAX);
    void sortSlices();

    QHash<ChannelID,QHash<SummaryType,int>> m_calcs;

    // State passed between populate() and addSlice():
    Day* m_day;
    QVector<SummaryChartSlice>* m_slices;
    QMultiMap<float,SummaryChartSlice> m_sort_slices;
};

#endif // GPRESSURECHART_H
