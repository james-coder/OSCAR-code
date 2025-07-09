/* gUsageChart Implementation
 *
 * Copyright (c) 2019-2025 The Oscar Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */
#if 1
#define TEST_MACROS_ENABLEDoff
#include "test_macros.h"

#include <math.h>
#include <QLabel>
#include <QDateTime>

#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "SleepLib/machine_common.h"
#include "gUsageChart.h"

#include "gYAxis.h"

extern MainWindow * mainwin;

// short SummaryCalcItem::midcalc;

QString gUsageChart::tooltipData(Day * day, int)
{
    return QObject::tr("\nLength: %1").arg(durationInHoursToHhMmSs(day->hours(m_machtype)));
}

void gUsageChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];

    float hours = day->hours(m_machtype);

    QColor cpapcolor = day->summaryOnly() ? QColor(128,128,128) : calcitems[0].color;
    bool haveoxi = day->hasMachine(MT_OXIMETER);

    QColor goodcolor = haveoxi ? QColor(128,255,196) : cpapcolor;

    QColor color = (hours < compliance_threshold) ? QColor(255,64,64) : goodcolor;
    slices.append(SummaryChartSlice(&calcitems[0], hours, hours, QObject::tr("Hours"), color));
}

void gUsageChart::preCalc()
{
    midcalc = p_profile->general->prefCalcMiddle();

    compliance_threshold = p_profile->cpap->complianceHours();
    incompdays = 0;

    SummaryCalcItem & calc = calcitems[0];
    calc.reset(idx_end - idx_start, midcalc);
}

void gUsageChart::customCalc(Day *, QVector<SummaryChartSlice> &list)
{
    if (list.size() == 0) {
        incompdays++;
        return;
    }

    SummaryChartSlice & slice = list[0];
    SummaryCalcItem & calc = calcitems[0];

    if (slice.value < compliance_threshold) incompdays++;

    calc.update(slice.value, 1);
}

void gUsageChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    if (totaldays > 1) {
        float comp = 100.0 - ((float(incompdays + nousedays) / float(totaldays)) * 100.0);

        int midcalc = p_profile->general->prefCalcMiddle();
        float mid = 0;
        SummaryCalcItem & calc = calcitems[0];
        switch (midcalc) {
        case 0: // median
            mid = median(calc.median_data.begin(), calc.median_data.end());
            break;
        case 1: // w-avg
            mid = calc.wavg_sum / calc.divisor;
            break;
        case 2:
            mid = calc.avg_sum / calc.cnt;
            break;
        }

        QString txt = QObject::tr("%1 low usage, %2 no usage, out of %3 days (%4% compliant.) Length: %5 / %6 / %7").
                arg(incompdays).arg(nousedays).arg(totaldays).arg(comp,0,'f',1).arg(durationInHoursToHhMmSs(calc.min)).arg(durationInHoursToHhMmSs(mid)).arg(durationInHoursToHhMmSs(calc.max));
        graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
    }
}

#endif
