/* gPressureChart Implementation
 *
 * Copyright (c) 2020-2024 The Oscar Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include "gPressureChart.h"

gPressureChart::gPressureChart()
    : gSummaryChart("Pressure", MT_CPAP)
{
    addCalc(CPAP_Pressure, ST_SETMAX);
    addCalc(CPAP_Pressure, ST_MID);
    addCalc(CPAP_Pressure, ST_90P);
    addCalc(CPAP_PressureMin, ST_SETMIN);
    addCalc(CPAP_PressureMax, ST_SETMAX);

    addCalc(CPAP_EPAP, ST_SETMAX);
    addCalc(CPAP_IPAP, ST_SETMAX);
    addCalc(CPAP_EPAPLo, ST_SETMAX);
    addCalc(CPAP_IPAPHi, ST_SETMAX);

    addCalc(CPAP_EPAP, ST_MID);
    addCalc(CPAP_EPAP, ST_90P);
    addCalc(CPAP_IPAP, ST_MID);
    addCalc(CPAP_IPAP, ST_90P);

    // PRS1 reports pressure adjustments instead of observed pressures on some machines
    addCalc(CPAP_PressureSet, ST_MID);
    addCalc(CPAP_PressureSet, ST_90P);
    addCalc(CPAP_EPAPSet, ST_MID);
    addCalc(CPAP_EPAPSet, ST_90P);
    addCalc(CPAP_IPAPSet, ST_MID);
    addCalc(CPAP_IPAPSet, ST_90P);
}


int gPressureChart::addCalc(ChannelID code, SummaryType type)
{
    QColor color = schema::channel[code].defaultColor();
    if (type == ST_90P) {
        color = brighten(color, 1.33f);
    }

    int index = gSummaryChart::addCalc(code, type, color);

    // Save the code and type used to add this calculation so that getCalc()
    // can retrieve it by code and type instead of by hard-coded index.
    m_calcs[code][type] = index;

    return index;
}


SummaryCalcItem* gPressureChart::getCalc(ChannelID code, SummaryType type)
{
    return &calcitems[m_calcs[code][type]];
}


void gPressureChart::afterDraw(QPainter &, gGraph &graph, QRectF rect)
{
    QStringList presstr;

    if (getCalc(CPAP_Pressure)->cnt > 0) {
        presstr.append(channelRange(CPAP_Pressure, STR_TR_CPAP));
    }

    if (getCalc(CPAP_PressureMin, ST_SETMIN)->cnt > 0) {
        // TODO: If using machines from different manufacturers in an overview,
        // the below may not accurately find the APAP pressure channel for all
        // days; but it only affects the summary label at the top.
        ChannelID pressure = CPAP_Pressure;
        if (getCalc(CPAP_PressureSet, ST_MID)->cnt > 0) {
            pressure = CPAP_PressureSet;
        }
        presstr.append(QString("%1 %2/%3/%4/%5").
                arg(STR_TR_APAP).
                arg(getCalc(CPAP_PressureMin, ST_SETMIN)->min,0,'f',1).
                arg(getCalc(pressure, ST_MID)->mid(), 0, 'f', 1).
                arg(getCalc(pressure, ST_90P)->mid(),0,'f',1).
                arg(getCalc(CPAP_PressureMax, ST_SETMAX)->max, 0, 'f', 1));

    }

    if (getCalc(CPAP_EPAP)->cnt > 0) {
        // See CPAP_PressureSet note above.
        ChannelID epap = CPAP_EPAP;
        if (getCalc(CPAP_EPAPSet, ST_MID)->cnt > 0) {
            epap = CPAP_EPAPSet;
        }
        presstr.append(channelRange(epap, STR_TR_EPAP));
    }

    if (getCalc(CPAP_IPAP)->cnt > 0) {
        // See CPAP_PressureSet note above.
        ChannelID ipap = CPAP_IPAP;
        if (getCalc(CPAP_IPAPSet, ST_MID)->cnt > 0) {
            ipap = CPAP_IPAPSet;
        }
        presstr.append(channelRange(ipap, STR_TR_IPAP));
    }

    if (getCalc(CPAP_EPAPLo)->cnt > 0) {
        presstr.append(channelRange(CPAP_EPAPLo, STR_TR_EPAPLo));
    }

    if (getCalc(CPAP_IPAPHi)->cnt > 0) {
        presstr.append(channelRange(CPAP_IPAPHi, STR_TR_IPAPHi));
    }

    QString txt = presstr.join(" ");
    graph.renderText(txt, rect.left(), rect.top()-5*graph.printScaleY(), 0);

}


QString gPressureChart::channelRange(ChannelID code, const QString & label)
{
    SummaryCalcItem* calc = getCalc(code);
    return QString("%1 %2/%3/%4").
            arg(label).
            arg(calc->min, 0, 'f', 1).
            arg(calc->mid(), 0, 'f', 1).
            arg(calc->max, 0, 'f', 1);
}


void gPressureChart::addSlice(ChannelID code, SummaryType type)
{
    float value = 0;
    QString label;

    switch (type) {
    case ST_SETMIN:
        value = m_day->settings_min(code);
        label = schema::channel[code].label();
        break;
    case ST_SETMAX:
        value = m_day->settings_max(code);
        label = schema::channel[code].label();
        break;
    case ST_MID:
        value = m_day->calcMiddle(code);
        label = m_day->calcMiddleLabel(code);
        break;
    case ST_90P:
        value = m_day->calcPercentile(code);
        label = m_day->calcPercentileLabel(code);
        break;
    default:
        qWarning() << "Unsupported summary type in gPressureChart";
        break;
    }

    SummaryCalcItem* calc = getCalc(code, type);
    m_sort_slices.insert(value,SummaryChartSlice(calc, value, 0, label, calc->color));
}

void gPressureChart::sortSlices() {
    float prevPressure = 0;
    for (auto it = m_sort_slices.begin(); it != m_sort_slices.end(); it++ ) {
        float pressure = it.key();
        if (pressure<=0) {
            // no valid data.
            continue;
        }
        float rectHeight = pressure - prevPressure;
        SummaryChartSlice slice = it.value();
        slice.height = rectHeight;
        m_slices->append(slice);
        prevPressure = pressure;
    }
    m_sort_slices.clear();
}

void gPressureChart::populate(Day * day, int idx)
{
    CPAPMode mode =  (CPAPMode)(int)qRound(day->settings_wavg(CPAP_Mode));
    m_day = day;
    m_slices = &cache[idx];
    m_sort_slices.clear();

    if (mode == MODE_CPAP) {
        addSlice(CPAP_Pressure);

    } else if (mode == MODE_APAP) {
        addSlice(CPAP_PressureMin, ST_SETMIN);
        if (!day->summaryOnly()) {
            // Handle PRS1 pressure adjustments reported separately from average (EPAP) pressure
            ChannelID pressure = CPAP_Pressure;
            if (m_day->channelHasData(CPAP_PressureSet)) {
                pressure = CPAP_PressureSet;
            }
            addSlice(pressure, ST_MID);
            addSlice(pressure, ST_90P);
        }
        addSlice(CPAP_PressureMax, ST_SETMAX);

    } else if (mode == MODE_BILEVEL_FIXED) {
        addSlice(CPAP_EPAP);
        addSlice(CPAP_IPAP);

    } else if (mode == MODE_BILEVEL_AUTO_FIXED_PS) {
        addSlice(CPAP_EPAPLo);
        if (!day->summaryOnly()) {
            addSlice(CPAP_EPAP, ST_MID);
            addSlice(CPAP_EPAP, ST_90P);
            addSlice(CPAP_IPAP, ST_MID);
            addSlice(CPAP_IPAP, ST_90P);
        }
        addSlice(CPAP_IPAPHi);

    } else if ((mode == MODE_BILEVEL_AUTO_VARIABLE_PS) || (mode == MODE_ASV_VARIABLE_EPAP)) {
        addSlice(CPAP_EPAPLo);
        if (!day->summaryOnly()) {
            // Handle PRS1 pressure adjustments when reported instead of observed pressures
            ChannelID epap = CPAP_EPAP;
            if (m_day->channelHasData(CPAP_EPAPSet)) {
                epap = CPAP_EPAPSet;
            }
            ChannelID ipap = CPAP_IPAP;
            if (m_day->channelHasData(CPAP_IPAPSet)) {
                ipap = CPAP_IPAPSet;
            }
            addSlice(epap, ST_MID);
            addSlice(epap, ST_90P);
            addSlice(ipap, ST_MID);
            addSlice(ipap, ST_90P);
        }
        addSlice(CPAP_IPAPHi);

    } else if (mode == MODE_TRILEVEL_AUTO_VARIABLE_PDIFF) {
        addSlice(CPAP_EEPAPLo);
        if (!day->summaryOnly()) {
            ChannelID eepap = CPAP_EEPAP;
            ChannelID ipap = CPAP_IPAP;
            addSlice(eepap, ST_MID);
            addSlice(eepap, ST_90P);
            addSlice(ipap, ST_MID);
            addSlice(ipap, ST_90P);
        }
        addSlice(CPAP_IPAPHi);

    } else if (mode == MODE_ASV) {
        addSlice(CPAP_EPAP);
        if (!day->summaryOnly()) {
            addSlice(CPAP_IPAP, ST_MID);
            addSlice(CPAP_IPAP, ST_90P);
        }
        addSlice(CPAP_IPAPHi);

    } else if (mode == MODE_AVAPS) {
        addSlice(CPAP_EPAP);
        if (!day->summaryOnly()) {
            addSlice(CPAP_IPAP, ST_MID);
            addSlice(CPAP_IPAP, ST_90P);
        }
        addSlice(CPAP_IPAPHi);
    }
    sortSlices();
}
