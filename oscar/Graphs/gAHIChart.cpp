/* gAHUChart Implementation
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include "test_macros.h"

#include <math.h>
#include <QLabel>
#include <QDateTime>

#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "SleepLib/machine_common.h"
#include "gAHIChart.h"

#include "gYAxis.h"

extern MainWindow * mainwin;

//extern short SummaryCalcItem::midcalc;

////////////////////////////////////////////////////////////////////////////
/// AHI Chart Stuff
////////////////////////////////////////////////////////////////////////////
void gAHIChart::preCalc()
{
    gSummaryChart::preCalc();

    ahi_wavg = 0;
    ahi_avg = 0;
    total_days = 0;
    total_hours = 0;
    min_ahi = 99999;
    max_ahi = -99999;

    ahi_data.clear();
    ahi_data.reserve(idx_end-idx_start);
}
void gAHIChart::customCalc(Day *day, QVector<SummaryChartSlice> &list)
{
    int size = list.size();
    if (size == 0) return;
    EventDataType hours = day->hours(m_machtype);
    EventDataType ahi_cnt = 0;

    for (auto & slice : list) {
        SummaryCalcItem * calc = slice.calc;

        EventDataType value = slice.value;
        float valh =  value/ hours;

        switch (midcalc) {
        case 0:
            calc->median_data.append(valh);
            break;
        case 1:
            calc->wavg_sum += value;
            calc->divisor += hours;
            break;
        case 2:
            calc->avg_sum += value;
            calc->cnt++;
            break;
        }

        calc->min = qMin(valh, calc->min);
        calc->max = qMax(valh, calc->max);

        ahi_cnt += value;
    }
    min_ahi = qMin(ahi_cnt / hours, min_ahi);
    max_ahi = qMax(ahi_cnt / hours, max_ahi);

    ahi_data.append(ahi_cnt / hours);

    ahi_wavg += ahi_cnt;
    ahi_avg += ahi_cnt;
    total_hours += hours;
    total_days++;
}
void gAHIChart::afterDraw(QPainter & /*painter */, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    //int size = idx_end - idx_start;

    bool skip = true;
    float med = 0;
    switch (midcalc) {
    case 0:
        if (ahi_data.size() > 0) {
            med = median(ahi_data.begin(), ahi_data.end());
            skip = false;
        }
        break;
    case 1: // wavg
        if (total_hours > 0) {
            med = ahi_wavg / total_hours;
            skip = false;
        }
        break;
    case 2: // avg
        if (total_days > 0) {
            med = ahi_avg / total_days;
            skip = false;
        }
        break;
    }

    QStringList txtlist;
    if (!skip) txtlist.append(QString("%1 %2 / %3 / %4").arg(STR_TR_AHI).arg(min_ahi, 0, 'f', 2).arg(med, 0, 'f', 2).arg(max_ahi, 0, 'f', 2));

    int i = calcitems.size();
    while (i > 0) {
        i--;
        ChannelID code = calcitems[i].code;
        schema::Channel & chan = schema::channel[code];
        float mid = 0;
        skip = true;
        switch (midcalc) {
        case 0:
            if (calcitems[i].median_data.size() > 0) {
                mid = median(calcitems[i].median_data.begin(), calcitems[i].median_data.end());
                skip = false;
            }
            break;
        case 1:
            if (calcitems[i].divisor > 0) {
                mid = calcitems[i].wavg_sum / calcitems[i].divisor;
                skip = false;
            }
            break;
        case 2:
            if (calcitems[i].cnt > 0) {
                mid = calcitems[i].avg_sum / calcitems[i].cnt;
                skip = false;
            }
            break;
        }

        if (!skip) txtlist.append(QString("%1 %2 / %3 / %4").arg(chan.label()).arg(calcitems[i].min, 0, 'f', 2).arg(mid, 0, 'f', 2).arg(calcitems[i].max, 0, 'f', 2));
    }
    QString txt = txtlist.join(", ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
}

void gAHIChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];

    float hours = day->hours(m_machtype);

    for (auto & calc : calcitems) {
        ChannelID code = calc.code;
        if (!day->hasData(code, ST_CNT)) continue;

        schema::Channel *chan = schema::channel.channels.find(code).value();

        float c = day->count(code);
        slices.append(SummaryChartSlice(&calc, c, c  / hours, chan->label(), calc.color));
    }
}
QString gAHIChart::tooltipData(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
    float total = 0;
    float hour = day->hours(m_machtype);
    QString txt;
    int i = slices.size();
    while (i > 0) {
        i--;
        total += slices[i].value;
        txt += QString("\n%1: %2").arg(slices[i].name).arg(float(slices[i].value) / hour, 0, 'f', 2);
    }
    return QString("\n%1: %2").arg(STR_TR_AHI).arg(float(total) / hour,0,'f',2)+txt;
}


