/* gTTIAChart Implementation
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
#include "gTTIAChart.h"

#include "gYAxis.h"

extern MainWindow * mainwin;

// short SummaryCalcItem::midcalc;

////////////////////////////////////////////////////////////////////////////
/// Total Time in Apnea Chart Stuff
////////////////////////////////////////////////////////////////////////////

void gTTIAChart::preCalc()
{
    gSummaryChart::preCalc();
}

void gTTIAChart::customCalc(Day *, QVector<SummaryChartSlice> & slices)
{
    if (slices.size() == 0) return;
    const SummaryChartSlice & slice = slices.at(0);

    calcitems[0].update(slice.value, slice.value);
}

void gTTIAChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    QStringList txtlist;

    for (auto & calc : calcitems) {
        //ChannelID code = calc.code;
        //schema::Channel & chan = schema::channel[code];
        float mid = 0;
        switch (midcalc) {
        case 0:
            if (calc.median_data.size() > 0) {
                mid = median(calc.median_data.begin(), calc.median_data.end());
            }
            break;
        case 1:
            if (calc.divisor > 0) {
                mid = calc.wavg_sum / calc.divisor;
            }
            break;
        case 2:
            if (calc.cnt > 0) {
                mid = calc.avg_sum / calc.cnt;
            }
            break;
        }

        txtlist.append(QString("%1 %2 / %3 / %4").arg(QObject::tr("TTIA:")).arg(durationInMinutesToHhMmSs(calc.min)).arg(durationInMinutesToHhMmSs(mid)).arg(durationInMinutesToHhMmSs(calc.max)));
    }
    QString txt = txtlist.join(", ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);
}

void gTTIAChart::populate(Day *day, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
//    float ttia = day->sum(CPAP_AllApnea) + day->sum(CPAP_Obstructive) + day->sum(CPAP_ClearAirway) + day->sum(CPAP_Apnea) + day->sum(CPAP_Hypopnea);
    float ttia = day->sum(AllAhiChannels);

    slices.append(SummaryChartSlice(&calcitems[0], ttia / 60.0, ttia / 60.0, QObject::tr("\nTTIA: %1").arg(durationInSecondsToHhMmSs(ttia)), QColor(255,147,150)));
}

QString gTTIAChart::tooltipData(Day *, int idx)
{
    QVector<SummaryChartSlice> & slices = cache[idx];
    if (slices.size() == 0) return QString();

    const SummaryChartSlice & slice = slices.at(0);
    return slice.name;
}


