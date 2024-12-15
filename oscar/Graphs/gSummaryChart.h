 /* gSessionTimesChart Header
 *
 * Copyright (c) 2019-2024 The Oscar Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */
#if 1
#ifndef GSUMMARYCHART_H
#define GSUMMARYCHART_H

#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "Graphs/gGraphView.h"
#include "SleepLib/appsettings.h"


struct TimeSpan
{
public:
    TimeSpan():begin(0), end(0) {}
    TimeSpan(float b, float e) : begin(b), end(e) {}
    TimeSpan(const TimeSpan & copy) {
        begin = copy.begin;
        end = copy.end;
    }
    ~TimeSpan() {}
    float begin;
    float end;
};

struct SummaryCalcItem {
    SummaryCalcItem() {
        code = 0;
        type = ST_CNT;
        color = Qt::black;
        wavg_sum = 0;
        avg_sum = 0;
        cnt = 0;
        divisor = 0;
        min = 0;
        max = 0;
    }

    SummaryCalcItem operator=(const SummaryCalcItem & copy) ;
    #if 0
    {
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
    #endif

    SummaryCalcItem(const SummaryCalcItem & copy) ;
    #if 0
    {
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
    #endif

    SummaryCalcItem(ChannelID code, SummaryType type, QColor color)
        :code(code), type(type), color(color) {
    }
    float mid()
    {
        float val = 0;
        switch (midcalc) {
        case 0:
            if (median_data.size() > 0)
                val = median(median_data.begin(), median_data.end());
            break;
        case 1:
            if (divisor > 0)
                val = wavg_sum / divisor;
            break;
        case 2:
            if (cnt > 0)
                val = avg_sum / cnt;
        }
        return val;
    }


    inline void update(float value, float weight) {
        if (midcalc == 0) {
            median_data.append(value);
        }

        avg_sum += value;
        cnt++;
        wavg_sum += value * weight;
        divisor += weight;
        min = qMin(min, value);
        max = qMax(max, value);
    }

    void reset(int reserve, short mc) {
        midcalc = mc;

        wavg_sum = 0;
        avg_sum = 0;
        divisor = 0;
        cnt = 0;
        min = 99999;
        max = -99999;
        median_data.clear();
        if (midcalc == 0) {
            median_data.reserve(reserve);
        }
    }
    ChannelID code;
    SummaryType type;
    QColor color;

    double wavg_sum;
    double divisor;
    double avg_sum;
    int cnt;
    EventDataType min;
    EventDataType max;
    static short midcalc;

    QList<float> median_data;

};

struct SummaryChartSlice {
    SummaryChartSlice() {
        calc = nullptr;
        height = 0;
        value = 0;
        //name = ST_CNT;
        name = "";
        color = Qt::black;
    }

    SummaryChartSlice(const SummaryChartSlice & copy) ;
    #if 0
    {
        calc = copy.calc;
        value = copy.value;
        height = copy.height;
        name = copy.name;
        color = copy.color;
//        brush = copy.brush;
    }
    #endif
    SummaryChartSlice& operator=(SummaryChartSlice & other) ;
    #if 0
    {
        if (this != &other) {  // Self-assignment check
            calc = other.calc;
            value = other.value;
            height = other.height;
            name = other.name;
            color = other.color;
        }
        return *this;
    }
    #endif

    SummaryChartSlice& operator=(const SummaryChartSlice & other) ;
    #if 0
    {
        if (this != &other) {  // Self-assignment check
            calc = other.calc;
            value = other.value;
            height = other.height;
            name = other.name;
            color = other.color;
        }
        return *this;
    }
    #endif

    SummaryChartSlice(SummaryCalcItem * calc, EventDataType value, EventDataType height, QString name, QColor color)
        :calc(calc), value(value), height(height), name(name), color(color) {
//        QLinearGradient gradient(0, 0, 1, 0);
//        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
//        gradient.setColorAt(0,color);
//        gradient.setColorAt(1,brighten(color));
//        brush = QBrush(gradient);
    }
    SummaryCalcItem * calc;
    EventDataType value;
    EventDataType height;
    QString name;
    QColor color;
//    QBrush brush;
};

class gSummaryChart : public QObject , public Layer
{
    Q_OBJECT;
public:
    gSummaryChart(QString label, MachineType machtype);
    gSummaryChart(ChannelID code, MachineType machtype);
    virtual ~gSummaryChart();

    //! \brief Renders the graph to the QPainter object
    virtual void paint(QPainter &, gGraph &, const QRegion &);

    //! \brief Called whenever data model changes underneath. Day object is not needed here, it's just here for Layer compatability.
    virtual void SetDay(Day *day = nullptr);

    //! \brief Returns true if no data was found for this day during SetDay
    virtual bool isEmpty() { return m_empty; }

    //! \brief Allows chart to recalculate empty flag.
    void reCalculate() {m_empty=false;};

    virtual void populate(Day *, int idx);

    //! \brief Override to setup custom stuff before main loop
    virtual void preCalc();

    //! \brief Override to call stuff in main loop
    virtual void customCalc(Day *, QVector<SummaryChartSlice> &);

    //! \brief Override to call stuff after draw is complete
    virtual void afterDraw(QPainter &, gGraph &, QRectF);

    //! \brief Return any extra data to show beneath the date in the hover over tooltip
    virtual QString tooltipData(Day *, int);

    virtual void dataChanged() {
        cache.clear();
    }

    virtual int addCalc(ChannelID code, SummaryType type, QColor color);
    virtual int addCalc(ChannelID code, SummaryType type);

    virtual Layer * Clone() {
        gSummaryChart * sc = new gSummaryChart(m_label, m_machtype);
        Layer::CloneInto(sc);
        CloneInto(sc);

        // copy this here, because only base summary charts need it
        sc->calcitems = calcitems;

        return sc;
    }

    void CloneInto(gSummaryChart * layer) {
        layer->m_empty = m_empty;
        layer->firstday = firstday;
        layer->lastday = lastday;
        layer->expected_slices = expected_slices;
        layer->nousedays = nousedays;
        layer->totaldays = totaldays;
        layer->peak_value = peak_value;
        layer->idx_start = idx_start;
        layer->idx_end = idx_end;
        layer->cache.clear();
        layer->dayindex = dayindex;
        layer->daylist = daylist;
    }
signals:
    void summaryChartEmpty(gSummaryChart*,qint64,qint64,bool);

protected:
    //! \brief Key was pressed that effects this layer
    virtual bool keyPressEvent(QKeyEvent *event, gGraph *graph);

    //! \brief Mouse moved over this layers area (shows the hover-over tooltips here)
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse Button was pressed over this area
    virtual bool mousePressEvent(QMouseEvent *event, gGraph *graph);

    //! \brief Mouse Button was released over this area. (jumps to daily view here)
    virtual bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

    QString durationInHoursToHhMmSs(double duration);
    QString durationInMinutesToHhMmSs(double duration);
    QString durationInSecondsToHhMmSs(double duration);

    QString m_label;
    MachineType m_machtype;
    bool m_empty;
    bool m_emptyPrev;
    int hl_day;
    int tz_offset;
    float tz_hours;
    QDate firstday;
    QDate lastday;

    QMap<QDate, int> dayindex;
    QList<Day *> daylist;

    QHash<int, QVector<SummaryChartSlice> > cache;
    QVector<SummaryCalcItem> calcitems;

    int expected_slices;

    int nousedays;
    int totaldays;

    EventDataType peak_value;
    EventDataType min_value;

    int idx_start;
    int idx_end;

    short midcalc;
};



#endif // GSUMMARYCHART_H

#endif
