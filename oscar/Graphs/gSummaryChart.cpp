/* gSummaryChart Implementation
 *
 * Copyright (c) 2019-2024 The Oscar Team
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
#include <QTimeZone>

#include "mainwindow.h"
#include "SleepLib/profiles.h"
#include "SleepLib/machine_common.h"
#include "gSummaryChart.h"

#include "gYAxis.h"

extern MainWindow * mainwin;

short SummaryCalcItem::midcalc;

gSummaryChart::gSummaryChart(QString label, MachineType machtype)
    :Layer(NoChannel), m_label(label), m_machtype(machtype)
{
    m_layertype = LT_Overview;
    QDateTime d1 = QDateTime::currentDateTime();
    QDateTime d2 = d1;
    d1.setTimeZone(QTimeZone("UTC"));  // CHECK: Does this deal with DST?
    tz_offset = d2.secsTo(d1);
    tz_hours = tz_offset / 3600.0;
    expected_slices = 5;

    idx_end = 0;
    idx_start = 0;
}

gSummaryChart::gSummaryChart(ChannelID code, MachineType machtype)
    :Layer(code), m_machtype(machtype)
{
    m_layertype = LT_Overview;
    QDateTime d1 = QDateTime::currentDateTime();
    QDateTime d2 = d1;
    d1.setTimeZone(QTimeZone("UTC"));  // CHECK: Does this deal with DST?
    tz_offset = d2.secsTo(d1);
    tz_hours = tz_offset / 3600.0;
    expected_slices = 5;

    addCalc(code, ST_MIN, brighten(schema::channel[code].defaultColor() ,0.60f));
    addCalc(code, ST_MID, brighten(schema::channel[code].defaultColor() ,1.20f));
    addCalc(code, ST_90P, brighten(schema::channel[code].defaultColor() ,1.70f));
    addCalc(code, ST_MAX, brighten(schema::channel[code].defaultColor() ,2.30f));

    idx_end = 0;
    idx_start = 0;
}

gSummaryChart::~gSummaryChart()
{
}

void gSummaryChart::SetDay(Day *unused_day)
{
    cache.clear();

    Q_UNUSED(unused_day)
    Layer::SetDay(nullptr);

    if (m_machtype != MT_CPAP) {
        // Channels' machine types are not terribly reliable: oximetry channels can be reported by a CPAP,
        // and position channels can be reported by an oximeter. So look for any days with data.
        firstday = p_profile->FirstDay();
        lastday = p_profile->LastDay();
    } else {
        // But CPAP channels (like pressure settings) can only be reported by a CPAP.
        firstday = p_profile->FirstDay(m_machtype);
        lastday = p_profile->LastDay(m_machtype);
    }

    dayindex.clear();
    daylist.clear();

    if (!firstday.isValid() || !lastday.isValid()) return;
   // daylist.reserve(firstday.daysTo(lastday)+1);
    QDate date = firstday;
    int idx = 0;
    do {
        auto di = p_profile->daylist.find(date);
        Day * day = nullptr;
        if (di != p_profile->daylist.end()) {
            day = di.value();
        }
        daylist.append(day);
        dayindex[date] = idx;
        idx++;
        date = date.addDays(1);
    } while (date <= lastday);

    m_minx = QDateTime(firstday, QTime(0,0,0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    m_maxx = QDateTime(lastday, QTime(23,59,59), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    m_miny = 0;
    m_maxy = 20;

    m_empty = false;
    m_emptyPrev = true;

}


int gSummaryChart::addCalc(ChannelID code, SummaryType type, QColor color)
{
    calcitems.append(SummaryCalcItem(code, type, color));
    return calcitems.size() - 1;  // return the index of the newly appended calc
}

int gSummaryChart::addCalc(ChannelID code, SummaryType type)
{
    return addCalc(code, type, schema::channel[code].defaultColor());
}


bool gSummaryChart::keyPressEvent(QKeyEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mouseMoveEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mousePressEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event)
    Q_UNUSED(graph)
    return false;
}

bool gSummaryChart::mouseReleaseEvent(QMouseEvent *event, gGraph *graph)
{
    if (!(event->modifiers() & Qt::ShiftModifier)) return false;

    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        float x = event->x();
    #else   
        float x = event->position().x();
    #endif  
    x -= m_rect.left();
    
    EventDataType miny;
    EventDataType maxy;

    graph->roundY(miny, maxy);

    QDate date = QDateTime::fromMSecsSinceEpoch(m_minx, QTimeZone::systemTimeZone()).date();

    int days = ceil(double(m_maxx - m_minx) / 86400000.0);

    float barw = float(m_rect.width()) / float(days);

    float idx = x/barw;

    date = date.addDays(idx);

    auto it = dayindex.find(date);
    if (it != dayindex.end()) {
        Day * day = daylist.at(it.value());
        if (day) {
            mainwin->getDaily()->LoadDate(date);
            mainwin->JumpDaily();
        }
    }

    return true;
}

void gSummaryChart::preCalc()
{
    midcalc = p_profile->general->prefCalcMiddle();

    for (auto & calc : calcitems) {
        calc.reset(idx_end - idx_start, midcalc);
    }
}

void gSummaryChart::customCalc(Day *day, QVector<SummaryChartSlice> & slices)
{
    int size = slices.size();
    if (size != calcitems.size()) {
        return;
    }
    float hour = day->hours(m_machtype);

    for (int i=0; i < size; ++i) {
        const SummaryChartSlice & slice = slices.at(i);
        SummaryCalcItem & calc = calcitems[i];

        calc.update(slice.value, hour);
     }
}

void gSummaryChart::afterDraw(QPainter &painter, gGraph &graph, QRectF rect)
{
    if (totaldays == nousedays) return;

    if (calcitems.size() == 0) return;

    QStringList strlist;
    QString txt;

    int midcalc = p_profile->general->prefCalcMiddle();
    QString midstr;
    if (midcalc == 0) {
        midstr = QObject::tr("Med.");
    } else if (midcalc == 1) {
        midstr = QObject::tr("W-Avg");
    } else {
        midstr = QObject::tr("Avg");
    }


    float perc = p_profile->general->prefCalcPercentile();
    QString percstr = QString("%1%").arg(perc, 0, 'f',0);

    schema::Channel & chan = schema::channel[calcitems.at(0).code];

    for (auto & calc : calcitems) {

        if (calcitems.size() == 1) {
            float val = calc.min;
            if (val < 99998)
                strlist.append(QObject::tr("Min: %1").arg(val,0,'f',2));
        }

        float mid = 0;
        switch (midcalc) {
        case 0:
            if (calc.median_data.size() > 0) {
                mid = median(calc.median_data.begin(), calc.median_data.end());
            }
            break;
        case 1:
            mid = calc.wavg_sum / calc.divisor;
            break;
        case 2:
            mid = calc.avg_sum / calc.cnt;
            break;
        }

        float val = 0;
        switch (calc.type) {
        case ST_CPH:
            val = mid;
            txt = midstr+": ";
            break;
        case ST_SPH:
            val = mid;
            txt = midstr+": ";
            break;
        case ST_MIN:
            val = calc.min;
            if (val >= 99998) continue;
            txt = QObject::tr("Min: ");
            break;
        case ST_MAX:
            val = calc.max;
            if (val <= -99998) continue;
            txt = QObject::tr("Max: ");
            break;
        case ST_SETMIN:
            val = calc.min;
            if (val >= 99998) continue;
            txt = QObject::tr("Min: ");
            break;
        case ST_SETMAX:
            val = calc.max;
            if (val <= -99998) continue;
            txt = QObject::tr("Max: ");
            break;
        case ST_MID:
            val = mid;
            txt = QString("%1: ").arg(midstr);
            break;
        case ST_90P:
            val = mid;
            txt = QString("%1: ").arg(percstr);
            break;
        default:
            val = mid;
            txt = QString("???: ");
            break;
        }
        strlist.append(QString("%1%2").arg(txt).arg(val,0,'f',2));
        if (calcitems.size() == 1) {
            val = calc.max;
            if (val > -99998)
                strlist.append(QObject::tr("Max: %1").arg(val,0,'f',2));
        }
    }

    QString str;
    if (totaldays > 1) {
        str = QObject::tr("%1 (%2 days): ").arg(chan.fullname()).arg(totaldays);
    } else {
        str = QObject::tr("%1 (%2 day): ").arg(chan.fullname()).arg(totaldays);
    }
    str += " "+strlist.join(", ");

    QRectF rec(rect.left(), rect.top(), 0,0);
    painter.setFont(*defaultfont);
    rec = painter.boundingRect(rec, Qt::AlignTop, str);
    rec.moveBottom(rect.top()-3*graph.printScaleY());
    painter.drawText(rec, Qt::AlignTop, str);

//    graph.renderText(str, rect.left(), rect.top()-5*graph.printScaleY(), 0);


}

QString gSummaryChart::tooltipData(Day *, int idx)
{
    QString txt;
    const auto & slices = cache[idx];
    int i = slices.size();
    while (i > 0) {
        i--;
        txt += QString("\n%1: %2").arg(slices[i].name).arg(float(slices[i].value), 0, 'f', 2);
    }
    return txt;
}

void gSummaryChart::populate(Day * day, int idx)
{

    bool good = false;
    for (const auto & item : calcitems) {
        if (day->hasData(item.code, item.type)) {
            good = true;
            break;
        }
    }
    if (!good) return;

    auto & slices = cache[idx];

    float hours = day->hours(m_machtype);
    if ((hours==0) && (m_machtype != MT_CPAP)) hours = day->hours();
    float base = 0;

    for (auto & item : calcitems) {
        ChannelID code = item.code;
        schema::Channel & chan = schema::channel[code];
        float value = 0;
        QString name;
        QColor color;
        switch (item.type) {
        case ST_CPH:
            value = day->count(code) / hours;
            name = chan.label();
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value, name, color));
            break;
        case ST_SPH:
            value = (100.0 / hours) * (day->sum(code) / 3600.0);
            name = QObject::tr("% in %1").arg(chan.label());
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value, name, color));
            break;
        case ST_HOURS:
            value = hours;
            name = QObject::tr("Hours");
            color = COLOR_LightBlue;
            slices.append(SummaryChartSlice(&item, value, hours, name, color));
            break;
        case ST_MIN:
            value = day->Min(code);
            name = QObject::tr("Min %1").arg(chan.label());
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_MID:
            value = day->calcMiddle(code);
            name = day->calcMiddleLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_90P:
            value = day->calcPercentile(code);
            name = day->calcPercentileLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        case ST_MAX:
            value = day->calcMax(code);
            name = day->calcMaxLabel(code);
            color = item.color;
            slices.append(SummaryChartSlice(&item, value, value - base, name, color));
            base = value;
            break;
        default:
            break;
        }
    }
}

void gSummaryChart::paint(QPainter &painter, gGraph &graph, const QRegion &region)
{
    QRectF rect = region.boundingRect();

    rect.translate(0.0f, 0.001f);

    painter.setPen(QColor(Qt::black));
    painter.drawRect(rect);

    rect.moveBottom(rect.bottom()+1);

    m_minx = graph.min_x;
    m_maxx = graph.max_x;

    QDateTime date2 = QDateTime::fromMSecsSinceEpoch(m_minx, QTimeZone::systemTimeZone());
    QDateTime enddate2 = QDateTime::fromMSecsSinceEpoch(m_maxx, QTimeZone::systemTimeZone());

    QDate date = date2.date();
    QDate enddate = enddate2.date();

    int days = ceil(double(m_maxx - m_minx) / 86400000.0);

    //float lasty1 = rect.bottom();

    auto it = dayindex.find(date);
    idx_start = 0;
    if (it == dayindex.end()) {
        it = dayindex.begin();
    } else {
        idx_start = it.value();
    }

    int idx = idx_start;

    // Determine how many days after the first day of the chart that this data is to begin
    int numDaysOffset = 0;
    if (firstday > date) {  // date = beginning date of chart; firstday = beginning date of data
        numDaysOffset = date.daysTo(firstday);
    }

    // Determine how many days before the last day of the chart that this data is to end
    int numDaysAfter = 0;
    if (enddate > lastday) {  // enddate = last date of chart; lastday = last date of data
        numDaysAfter = lastday.daysTo(enddate);
    }
    if (numDaysAfter > days)        // Nothing to do if this data is off the left edge of the chart
        return;

    auto ite = dayindex.find(enddate);
    idx_end = daylist.size()-1;
    if (ite != dayindex.end()) {
        idx_end = ite.value();
    }

    QPoint mouse = graph.graphView()->currentMousePos();

    nousedays = 0;
    totaldays = 0;

    QRectF hl_rect;
    QDate hl_date;
    Day * hl_day = nullptr;
    int hl_idx = -1;
    bool hl = false;

    if ((daylist.size() == 0) || (it == dayindex.end()))
        return;

    //Day * lastday = nullptr;

    //    int dc = 0;
//    for (int i=idx; i<=idx_end; ++i) {
//        Day * day = daylist.at(i);
//        if (day || lastday) {
//            dc++;
//        }
//        lastday = day;
//    }
//    days = dc;
//    lastday = nullptr;
    float barw = float(rect.width()) / float(days);

    QString hl2_text = "";

    QVector<QRectF> outlines;
    int size = idx_end - idx;
    outlines.reserve(size * expected_slices);

    // Virtual call to setup any custom graph stuff
    preCalc();

    float lastx1 = rect.left();
    lastx1 += numDaysOffset * barw;
    float right_edge = (rect.left()+rect.width()+1);


    /////////////////////////////////////////////////////////////////////
    /// Calculate Graph Peaks
    /////////////////////////////////////////////////////////////////////
    peak_value = 0;
    for (int i=idx; i <= idx_end; ++i, lastx1 += barw) {
        Day * day = daylist.at(i);

        if ((lastx1 + barw) > right_edge)
            break;

        if (!day) {
            continue;
        }

        day->OpenSummary();

        auto cit = cache.find(i);

        if (cit == cache.end()) {
            populate(day, i);
            cit = cache.find(i);
        }

        if (cit != cache.end()) {
            float base = 0, val;
            for (const auto & slice : cit.value()) {
                val = slice.height;
                base += val;
            }
            peak_value = qMax(peak_value, base);
        }
    }
    m_miny = 0;
    m_maxy = ceil(peak_value);

    /////////////////////////////////////////////////////////////////////
    /// Y-Axis scaling
    /////////////////////////////////////////////////////////////////////

    EventDataType miny;
    EventDataType maxy;

    graph.roundY(miny, maxy);
    float ymult = float(rect.height()) / (maxy-miny);

    lastx1 = rect.left();
    lastx1 += numDaysOffset * barw;

    /////////////////////////////////////////////////////////////////////
    /// Main drawing loop
    /////////////////////////////////////////////////////////////////////
    do {
        Day * day = daylist.at(idx);

        if ((lastx1 + barw) > right_edge)
            break;

        totaldays++;

        if (!day)
        {
           // lasty1 = rect.bottom();
            lastx1 += barw;
            it++;
            nousedays++;
            //lastday = day;
            continue;
        }

        //lastday = day;

        float x1 = lastx1 + barw;

        day->OpenSummary();
        QRectF hl2_rect;

        bool hlday = false;
        QRectF rec2(lastx1, rect.top(), barw, rect.height());
        if (rec2.contains(mouse)) {
            hl_rect = rec2;
            hl_day = day;
            hl_date = it.key();
            hl_idx = idx;

            hl = true;
            hlday = true;
        }

        auto cit = cache.find(idx);

        if (cit == cache.end()) {
            populate(day, idx);
            cit = cache.find(idx);
        }

        float lastval = 0, val, y1,y2;
        if (cit != cache.end()) {
            /////////////////////////////////////////////////////////////////////////////////////
            /// Draw pressure settings
            /////////////////////////////////////////////////////////////////////////////////////
            QVector<SummaryChartSlice> & list = cit.value();
            customCalc(day, list);

            QLinearGradient gradient(lastx1, 0, lastx1 + barw, 0); //rect.bottom(), barw, rect.bottom());

            for (const auto & slice : list) {
                val = slice.height;
                y1 = ((lastval-miny) * ymult);
                y2 = (val * ymult);
                QColor color = slice.color;

                QRectF rec = QRectF(lastx1, rect.bottom() - y1, barw, -y2).intersected(rect);

                if (hlday) {
                    if (rec.contains(mouse.x(), mouse.y())) {
                        color = Qt::yellow;
                        hl2_rect = rec;
                    }
                }

                if (barw <= 3) {
                    painter.fillRect(rec, QBrush(color));
                } else if (barw > 8) {
                    gradient.setColorAt(0,color);
                    gradient.setColorAt(1,brighten(color, 2.0));
                    painter.fillRect(rec, QBrush(gradient));
//                    painter.fillRect(rec, slice.brush);
                    outlines.append(rec);
                } else {
                    painter.fillRect(rec, brighten(color, 1.25));
                    outlines.append(rec);
                }

                lastval += val;
            }
        }

        lastx1 = x1;
        it++;
    } while (++idx <= idx_end);
    painter.setPen(QPen(Qt::black,1));
    painter.drawRects(outlines);

    if (hl) {
        QColor col2(255,0,0,64);
        painter.fillRect(hl_rect, QBrush(col2));

        QString txt = hl_date.toString(QLocale::system().dateFormat(QLocale::ShortFormat))+" ";
        if (hl_day) {
            // grab extra tooltip data
            txt += tooltipData(hl_day, hl_idx);
            if (!hl2_text.isEmpty()) {
                QColor col = Qt::yellow;
                col.setAlpha(255);
           //     painter.fillRect(hl2_rect, QBrush(col));
                txt += hl2_text;
            }
        }

        graph.ToolTip(txt, mouse.x()-15, mouse.y()+5, TT_AlignRight);
    }
    try {
        afterDraw(painter, graph, rect);
    } catch(...) {
        qDebug() << "Bad median call in" << m_label;
    }


    // This could be turning off graphs prematurely..
    if (cache.size() == 0) {
        m_empty = true;
        m_emptyPrev = true;
        graph.graphView()->updateScale();
        emit summaryChartEmpty(this,m_minx,m_maxx,true);
    } else if (m_emptyPrev) {
        m_emptyPrev = false;
        emit summaryChartEmpty(this,m_minx,m_maxx,false);
    }

}

QString gSummaryChart::durationInHoursToHhMmSs(double duration) {
    return durationInSecondsToHhMmSs(duration * 3600);
}

QString gSummaryChart::durationInMinutesToHhMmSs(double duration) {
    return durationInSecondsToHhMmSs(duration * 60);
}

QString gSummaryChart::durationInSecondsToHhMmSs(double duration) {
    // ensure that a negative duration is supported (could potentially occur when start and end occur in different timezones without compensation)
    double duration_abs = abs(duration);
    int seconds_abs = static_cast<int>(0.5 + duration_abs);
    int daily_hours_abs = seconds_abs / 3600;
    QString result;
    if (daily_hours_abs < 24) {
        result = QTime(0,0,0,0).addSecs(seconds_abs).toString("hh:mm:ss");
    } else {
        result = QString::number(daily_hours_abs + seconds_abs % 86400 / 3600) + ":" + QTime(0, 0, 0, 0).addSecs(seconds_abs).toString("mm:ss");
    }

    if (duration == duration_abs) {
        return result;
    } else {
        return "-" + result;
    }
}





    SummaryCalcItem SummaryCalcItem::operator=(const SummaryCalcItem & copy) {
        code = copy.code;
        type = copy.type;
        color = copy.color;

        wavg_sum = 0;
        avg_sum = 0;
        cnt = 0;
        divisor = 0;
        min = 0;
        max = 0;
        midcalc = p_profile->general->prefCalcMiddle();
        return *this;
    }

    SummaryCalcItem::SummaryCalcItem(const SummaryCalcItem & copy) {
        code = copy.code;
        type = copy.type;
        color = copy.color;

        wavg_sum = 0;
        avg_sum = 0;
        cnt = 0;
        divisor = 0;
        min = 0;
        max = 0;
        midcalc = p_profile->general->prefCalcMiddle();
    }


    SummaryChartSlice::SummaryChartSlice(const SummaryChartSlice & copy) {
        calc = copy.calc;
        value = copy.value;
        height = copy.height;
        name = copy.name;
        color = copy.color;
//        brush = copy.brush;
    }
    
    SummaryChartSlice& SummaryChartSlice::operator=(SummaryChartSlice & other) {
        if (this != &other) {  // Self-assignment check
            calc = other.calc;
            value = other.value;
            height = other.height;
            name = other.name;
            color = other.color;
        }
        return *this;
    }

    SummaryChartSlice& SummaryChartSlice::operator=(const SummaryChartSlice & other) {
        if (this != &other) {  // Self-assignment check
            calc = other.calc;
            value = other.value;
            height = other.height;
            name = other.name;
            color = other.color;
        }
        return *this;
    }

