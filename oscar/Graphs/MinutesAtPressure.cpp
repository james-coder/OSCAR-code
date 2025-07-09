/* MinutesAtPressure Graph Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#define TEST_MACROS_SAMPLEoff
 /*

MinutesAtPressure Graph

The MinutesAtPressure (TimeAtPressure) Graph is the Pressure Graph transposed - with similar look and feel as other graphs.
    The Y-Axis (pressure) becomes the X-Axis (MinutesAtPressure).
    The X-Axis (Time) becomes the Y-Axis (duration in minutes).

The MinutesAtPressure Graph uses the configurable Plot and Overlay Settings from the Pressure Graph,
EPAP and IPAP(CPAP_Pressure) will both be conditionally displayed
Events (H, CA, OA, UA) are displayed as tick marks similar to the Pressure Graph.
Events (LL, CSR) are also displayed like the pressure Graph with lightgrap or lightgreen background coloring.
This gives the MinutesAtPressure a similar look and feel as other graphs.
The MetaData (top label Bar) now contains the total duration for each pressure pressure bucket (range of pressures) as well as the events that occured.

The MinutesAtPressure tool-tips now contains just the names of the Event Ticks (similar to the pressure Graph)
The tooltip information is minimal and only contains the name of the event and the number of occurrences for that pressure range.

On Mouse Over

1. Each data point will be highlight with a small dot.
2. Tooltips will be displayed for the appropriate Pressure Range and the respective data point will be displayed with a  square box - similar to the current current implementation.

When there is no data available (the time selected is between sessions in a day) then "No Data" will be displayed.
When no graphs are selected then "Plots Disabled" will be displayed - just like the Pressure Graph.

The X-Axis start and end pressure now use the Machine limits. If plot data has a data outside the range is appopaitely updated.

Refactoring was done to

1. Reduce duplicate/similar instances of code
2. Shorten long methods
3. Enhance readability,
5. Add Dynamic Meta data for Pressure times and Events.
6. insure data accuracy.
7. Conditional compilation for features

Total Duration displayed by Minutes AT Pressure graphs is based on the actual waveform form files.
In some cases this duration is different that the session time indicated in the daily session information - due to reasons below.

Issues found while testing based on 1.2.0 base.
* Event Flags do not display short SPANS.
* Pressure Graph does not display short SPANS.
* sessions times (as displayed in daily session information) are not always the same as the first and last time in the waveforms.
  for example for resmed the the session start time is about 40 seconds before the  first sample in the waveform (eventlist).
  also the session end time is not always the same as the last sample in the waveform. typically 1 second different (either before or after waveform).
* multiple eventlists in a session. time betwwen eventlists is small - under 1-2 minutes.
  This is not the same as multiple sessions per day.

Naming convention
pixel       == at point on the display area.
pixels      == distance between to pixels
Pressure    == a value in cmH20
Bucket      == A range of pressures to collect the ammount of time in that pressure range


some messages from Apnea Board.

0000042: Graphs Daily "Time at Pressure" graph x-axis goes to -1 and plot trace showing behind other graphs
    Graphs Daily "Time at Pressure" graph x-axis goes to -1 and plot trace showing behind other graphs On the "Daily" tab,
    the Time at Pressure plot's x-axis minimum is a value of -1. i
    Also, the plot traces on this graph jump to unreasonably high values (e.g., 508)
    which are shown behind the other graphs above it.
    Picture available on GitLab All 2-high All Issue 0000049 from SH 1.1.0 GitLab Issue List

0000038: Graphs any "Time at Pressure graph does not show the right pressure
    Graphs any "Time at Pressure graph does not show the right pressure, i
    e.g. CPAP with constant pressure at 7.5 cmH2O , but the Time at Pressure graph only has a peak around 6.2 cmH2O
    " This is especially true when looking at data at constant pressure.
    It was found with DV64 data, but is presumably true for other machine types as well as the graphs do not differentiate on machine types - to be confirmed All 2-high
    All Issue 0000054 from SH 1.1.0 GitLab Issue List
 */

#include <cmath>
#include <QApplication>
#include <QThreadPool>
#include <QMutexLocker>

#include "MinutesAtPressure.h"
#include "Graphs/gGraph.h"
#include "Graphs/gGraphView.h"
#include "SleepLib/profiles.h"

#include "Graphs/gXAxis.h"
#include "Graphs/gYAxis.h"
#include "common_gui.h"
#include "Graphs/gLineChart.h"

// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// Compile time Features
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#define ENABLE_DISPLAY_SPAN_EVENTS_AS_BACKGROUND                    // Enables SPAN event to be displayed as a background (like pressure graph)
#define ENABLE_DISPLAY_FLAG_EVENTS_AS_TICKS                         // Enables FLAG events to be displayed as tick (like pressure graph)

#define ENABLE_BUCKET_PRESSURE_AVERAGE                              // Average method
        // Bucket pressure is in the middle of the pressure range.
        // New definition of bucket                                 Pressure-0.1 - Pressure0.1 with INTERVALS_PER_CCMH2O=5
        // Original bucket definition.                              Pressure - Pressure+0.2      wiyh INTERVALS_PER_CCMH2O=5
#define EXTRA_SPACE_ON_X_AXIS                                       // adds a small space (Pressure/(INTERVALS_PER_CCMH2O*2) to each end.
#define ENABLE_SMOOTH_CURVES                                        // decreases performance.

//#define ALIGN_X_AXIS_ON_INTEGER_BOUNDS
//#define ENABLE_DISPLAY_FLAG_EVENTS_AS_GRAPH                       // enable graphing of flag events. Overlays plots on top on pressure plots.
                                                                    // ENABLE_DISPLAY_FLAG_EVENTS_AS_TICKS is enabled instead.
                                                                    // ENABLE_DISPLAY_FLAG_EVENTS_AS_TICKS and ENABLE_DISPLAY_FLAG_EVENTS_AS_GRAPH can both be enabled
// Compile time Constants
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

#define HIGHEST_POSSIBLE_PRESSURE           60      // should be the lowest possible pressure that a CPAP machine accepts - Plus spare.
#define INTERVALS_PER_CCMH2O                5       // must be a positive integer > 0. Five (5) produces good graphs. Other values will work.
                                                    // 10 also loogs good. larger number have smaller intervals and the starting pressure interval will be huge
                                                    // relation to the rest of the pressure intervals - making the graph unusable.
#define NUMBER_OF_CATMULLROMSPLINE_POINTS   5       // Higher the number decreases performance. 5 produces smooth curves. must be >= 1. 1 connects points with straight line.
                                                    // ENABLE_SMOOTH_CURVES must also be enabled.

static const int drawTickLength =12;

static const int tableSize = 1+(HIGHEST_POSSIBLE_PRESSURE * INTERVALS_PER_CCMH2O);
static constexpr EventDataType sampleIntervalDiv2 = 1.0/EventDataType(INTERVALS_PER_CCMH2O*2);         // interval pressure is Value-sampleInterval to value+sampleInterval

static constexpr EventDataType sampleIntervalStart = sampleIntervalDiv2;
static constexpr EventDataType sampleIntervalEnd = sampleIntervalDiv2;


//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< Module      <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

int calculatePrecision(EventDataType value) {
    if (value>5) return 1;
    if (value<0.5) return 3;
    return 2;
}


EventDataType pressureToBucket(EventDataType pressure, int  bucketsPerPressure) {
    return pressure * bucketsPerPressure;
}

EventDataType convertBucketToPressure(EventDataType bucket, int  bucketsPerPressure) {
    return bucket / bucketsPerPressure;
}

EventDataType pressureToXaxis( EventDataType pressure, EventDataType pixelsPerPressure , EventDataType minpressure , QRectF& drawingRect ) {
    return ((pressure-minpressure) * pixelsPerPressure) + drawingRect.left() ;
}

EventDataType convertXaxisToPressure( EventDataType mouseXaxis, EventDataType pixelsPerPressure , EventDataType minpressure , QRectF& drawingRect ) {
    return minpressure + ( EventDataType( mouseXaxis - drawingRect.left()) / pixelsPerPressure );
}

EventDataType msecToMinutes(EventDataType value) {
    return value / 60000.0;     // convert milli-seconds to minutes.
}

EventDataType getSetting(Session * sess,ChannelID code) {
    if (!sess->settings.contains(code)) {
        // qWarning() << "MinutesAtPressure could not find channel" << code;
        DEBUGCI QQ(MinutesAtPressure could not find channel,code) NAME(code);
        return -1;
    }
    auto setting=sess->settings.value(code);/*[code]; */
    enum schema::DataType datatype = schema::channel[code].datatype();
    if (!( datatype == schema::DEFAULT || datatype == schema::DOUBLE )) return -1;
    return setting.toDouble();
}

QString timeString(EventDataType milliSeconds) {
    EventDataType h,m,s = milliSeconds;
    if (s<0) return QString();
    s = 60*modf(s/60000,&m);
    m = 60*modf(m/60,&h );

   // These string are existing translations
    static const char* TR_TIME_FMT_S        =" (%3 sec)"  ;
    static const char* TR_TIME_FMT_MS       =" (%2 min, %3 sec)"  ;
    static const char* TR_TIME_FMT_HMS      ="%1 hours, %2 minutes and %3 seconds" ;

    if (m>0) {
        if (h<=0) {
            return QObject::tr(TR_TIME_FMT_MS).arg(m,0,'f',0).arg(s,0,'f',0);
        } else {
            return QString(" (%1)").arg(QObject::tr(TR_TIME_FMT_HMS).arg(h,0,'f',0).arg(m,0,'f',0).arg(s,2,'f',0));
        }
    } else if (s<3 && s>0.01) {
        return QObject::tr(TR_TIME_FMT_S).arg(s,1,'f',1);
    }
    return QObject::tr(TR_TIME_FMT_S).arg(s,0,'f',0);
}


//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< PressureInfo methods   <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
PressureInfo::PressureInfo()
{
    code = -1;
    minTime = maxTime = 0;
}

PressureInfo::PressureInfo(ChannelID code, qint64 minTime, qint64 maxTime) : code(code), minTime(minTime), maxTime(maxTime)
{
    #if defined(TEST_MACROS_SAMPLE)
    QString codes = schema::channel[ code  ].code(); DEBUGCI Q(code) Q(codes) O("__") NAME(codes) O("__") FULLNAME(codes);
    DEBUGCIS;
    DEBUGCI ;
    DEBUGCT ;
    DEBUGCI Q(code) QQ(Channelid,code);
    DEBUGCI O(code) OO(Channelid,code);
    DEBUGCI Z(code) ZZ(Channelid,code);
    DEBUGCI FULLNAME(code) ;
    DEBUGCI NAME(code) ;
    DEBUGCI DATE(minTime) ;
    DEBUGCI TIME(minTime) ;
    DEBUGCI DATETIME(minTime) ;
    DEBUGCI DATETIMEUTC(minTime) ;
    DEBUGCI COMPILER ;
    DEBUGCI FULLNAME(code) DATETIME(minTime) QQ(Minutes, ((maxTime-minTime)/60000) ) ;
    #endif
    init();
    times.resize(tableSize);
}

void PressureInfo::finishCalcs()
{
    peaktime = 0;
    peakevents = 0;
    firstPlotBucket = 0;
    lastPlotBucket = 0;

    int val;

    for (int i=0, end=times.size(); i<end; ++i) {
        val=times[i];
        if (val <= 0)  continue;
        peaktime = qMax(peaktime, val);
        if (firstPlotBucket == 0 ) firstPlotBucket = i;
        lastPlotBucket = i;
    }

    minpressure=convertBucketToPressure(firstPlotBucket,bucketsPerPressure);
    maxpressure=convertBucketToPressure(lastPlotBucket,bucketsPerPressure);

    for (const auto & cod : chans) {
        numEvents[cod]=0;
        if (schema::channel[cod].type() != schema::FLAG) continue;
        int size = events[cod].size();
        for (int i = 0; i < size; i++) {
            val = events[cod][i];
            if (val>0) numEvents[cod]+=val;
            peakevents = qMax(val, peakevents);
        }
    }
}

void PressureInfo::init() {
    chan = schema::channel[code];
    peaktime = peakevents = 0;
    firstPlotBucket = 0;
    lastPlotBucket = 0;
};

void PressureInfo::AddChannel(ChannelID c)
{
    chans.append(c);
    events[c].resize(tableSize);
}

void PressureInfo::AddChannels(QList<ChannelID> & chans)
{
    for (int i=0; i<chans.size(); ++i) {
        AddChannel(chans.at(i));
    }
}

void PressureInfo:: updateBucketsPerPressure (Session* sess) {
    bucketsPerPressure = (sess->machine()->loaderName() == "PRS1") ? 2 : INTERVALS_PER_CCMH2O;
    numberXaxisDivisions=qMin(2*bucketsPerPressure,10);
}

EventDataType PressureInfo:: rawToPressure ( EventStoreType raw,EventDataType gain) {
    return EventDataType(raw)*gain;
}

EventStoreType PressureInfo:: rawToBucketId ( EventStoreType raw,EventDataType gain) {
    EventDataType pressure = rawToPressure(raw,gain)+sampleIntervalStart;
    EventStoreType ret = floor(pressure * bucketsPerPressure );
    return ret;
}

void PressureInfo::setMachineTimes(EventDataType min,EventDataType max) {
    machinePressureMin=min;
    machinePressureMax=max;
}



//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< RecalcMAP class      <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

RecalcMAP::~RecalcMAP()
{
}
void RecalcMAP::quit() {
    map->mutex.lock();
    m_quit = true;
    map->mutex.unlock();
}

// find  pressure given the time of the event.
void RecalcMAP::updateFlagData(int &currentLoc, int & currentEL,int& currentData,qint64 eventTime, QVector<int> &dataArray, PressureInfo & info )
{
    for (; currentEL<info.eventLists.size() ; currentEL++) {
        auto EL =info.eventLists[currentEL];
        for (; currentLoc<(int)EL->count() ; currentLoc++) {
            if (m_quit) return ;
            qint64 sampleTime = EL->time(currentLoc);
            int raw = EL->raw(currentLoc);
            EventDataType gain= EL->gain();
            EventStoreType data = info.rawToBucketId(raw,gain);
            if (data>=tableSize) {
                data=tableSize-1;
            }
            if (sampleTime<=eventTime)  {
                currentData=data;
                if (sampleTime<eventTime) continue;
                // Note: The equal part of the comparision allows the pressure graph and the TimeAtPressure to reference the same pressure.
            }
            if (currentData<0) currentData=data;
            if (currentData>=0) {
                dataArray[currentData]++;
            }
            return ;
        }
        currentLoc=0;
    }
    return ;
}

void RecalcMAP::updateSpanData(int &currentLoc, int & currentEL,int& currentData,qint64 startSpan,qint64 eventTime , QVector<int> &dataArray, PressureInfo & info ) {
    EventStoreType useddata = ~0;
    for (; currentEL<info.eventLists.size() ; currentEL++) {
        auto EL =info.eventLists[currentEL];
        for (; currentLoc<(int)EL->count() ; currentLoc++) {
            if (m_quit) return ;
            qint64 sampleTime = EL->time(currentLoc);
            int raw = EL->raw(currentLoc);
            EventDataType gain= EL->gain();
            EventStoreType data = info.rawToBucketId(raw,gain);
            if (data>=tableSize) {
                data=tableSize-1;
            }
            if (sampleTime<startSpan) {
                currentData=data;
                continue;
            }
            if (currentData<0) {
                // have first sample
                currentData=data;
            }
            if (useddata!=currentData  && currentData>0) {
                dataArray[currentData]++;
                useddata=currentData;
            }
            if (data<=0) {
                break;
            }
            if (sampleTime>=eventTime)  return;
            currentData=data;
        }
        currentLoc=0;
    }
    return ;
}

void RecalcMAP::updateEventsChannel(Session*sess,ChannelID chanId, QVector<int> &dataArray, PressureInfo & info )
{
    this->chanId=chanId;
    EventDataType duration = 0, gain;

    qint64 t , start;
    EventStoreType *dptr;
    EventStoreType *eptr;
    quint32 *tptr;
    int cnt= 0;

    schema::ChanType chanType = schema::channel[ chanId ].type();

    auto channelEvents = sess->eventlist.value(chanId);
    // Loop through event lists
    for (int index =0; index<channelEvents.size() ; index++) {
        auto EL =channelEvents[index];
        qint64 minx = info.minTime;
        qint64 maxx = info.maxTime ;

        start= EL->first();
        tptr = EL->rawTime();
        dptr = EL->rawData();
        cnt = EL->count();
        eptr = dptr + cnt;
        gain = EL->gain();

        int  currentLoc =0;
        int  currentEL  =0;
        int  currentData  =-1;

        for (; dptr < eptr; dptr++) {
            if (m_quit) return ;
            t = start + *tptr++;
            if (t<minx) continue;
            duration = EventDataType(*dptr) * gain;
            if (chanType == schema::SPAN) {
                qint64 ts= t-qint64(duration*1000);
                // ts is the start of the span sequence. t is the end of span sequence - when span goes away.
                if (ts>maxx) continue;
                if (ts<minx) ts=minx;
                if (t>maxx) t=maxx;
                updateSpanData(currentLoc ,  currentEL , currentData , ts , t , dataArray , info ) ;
            } else {
                if (t>maxx) continue;
                if (schema::channel[ chanId ].type() == schema::FLAG) {
                    updateFlagData(currentLoc ,  currentEL , currentData , t , dataArray , info ) ;
                }
            }
        }
    }
    return ;
}

void RecalcMAP::updateEvents(Session*sess,PressureInfo & info) {
    QHash<ChannelID, QVector<EventList *> >::iterator ei = sess->eventlist.find(info.code);
    if (ei == sess->eventlist.end()) return ;
    for (const auto & cod : info.chans) {
        updateEventsChannel(sess,cod, info.events[cod],info);
        if (m_quit) return ;
    }
}

void RecalcMAP::updateTimesValues(qint64 d1,qint64 d2, int key,PressureInfo & info) {
    qint64 duration = (d2 - d1);
    info.times[key] += duration;
    info.totalDuration+=duration;
}

//! \brief Updates Time At Pressure from session *sess
void RecalcMAP::updateTimes(PressureInfo & info) {
    qint64 d1,d2;
    qint64 minx=0,maxx=0;
    //qint64 prevSessDuration=info.totalDuration;
    EventDataType gain;
    EventStoreType data, lastdata;
    qint64 time, lasttime;
    int ELsize;
    bool first;
    if (info.eventLists.size()==0) return;

    // Find pressure channel
    // Loop through event lists
    EventList* EL=info.eventLists[0];
    int idx=0;
    for (; idx<info.eventLists.size() ; idx++) {
        EL =info.eventLists[idx];
        ELsize = EL->count();
        if (ELsize < 1) continue;
        gain = EL->gain();
        // Workaround for the popout function. when the MAP popout graph is created the time selction mixx and miny are both zero.
        // this indicates that there is no data to be displayed.   WHY ??
        // This workaround uses the session min/max times when the selection min/max times are zero.
        if (map->numCloned>0) {
            bool cloneWorkAround =false;
            if (info.minTime==0) {
                cloneWorkAround = true;
                info.minTime = EL->first();
            }
            if (cloneWorkAround) {
                if (info.maxTime<EL->last()) info.maxTime=EL->last();
            }
        }

        // Skip if outside of range
        if ((EL->first() > info.maxTime) || (EL->last() < info.minTime)) {
            continue;
        }

        // adjust for multiple sessions.
        // EL->first and last are for the current session while minTime and MaxTime are for a set of seesion for the day.
        minx = qMax(info.minTime , EL->first());
        maxx = qMin(info.maxTime , EL->last());

        lasttime = 0;
        lastdata = 0;
        data = 0;
        first = true;

        // Scan through pressure samples
        for (int e = 0; e < ELsize; ++e) {
            if (m_quit) return ;

            time = EL->time(e);
            EventStoreType raw = EL->raw(e);
            data = ipap_info->rawToBucketId(raw,gain);

            if (data>=tableSize) {
                data=tableSize-1;
            }

            if ((time < minx) || first) {
                lasttime = time;
                lastdata = data;
                first = false;
                continue;
            }

            if (lastdata != data) {
                d1 = qMax(minx, lasttime);
                d2 = qMin(maxx, time);
                updateTimesValues(d1,d2,lastdata,info) ;
                lasttime = time;
                lastdata = data;
            }
            if (time > maxx) {
                break;
            }

        }
        if ((lasttime>0) &&((lasttime <= maxx) || (lastdata == data))) {
            d1 = qMax(minx, lasttime);
            d2 = qMin(maxx, EL->last());
            updateTimesValues(d1,d2, lastdata,info) ;
        }
    }
}

void RecalcMAP:: setSelectionRange(gGraph* graph) {
    graph->graphView()->GetXBounds(minTime, maxTime);
    // changes suggested by grnbrg
    qint64 clockdrift = qint64(p_profile->cpap->clockDrift()) * 1000L;
    minTime -= clockdrift;
    maxTime -= clockdrift;
}

void RecalcMAP::run()
{
    QMutexLocker locker(&map->mutex);
    map->m_recalculating = true;
    Day * day = map->m_day;
    if (!day) return;



    // Get the channels for specified Channel types
    QList<ChannelID> chans;
    chans = day->getSortedMachineChannels(schema::FLAG);
    chans.removeAll(CPAP_VSnore);
    chans.removeAll(CPAP_VSnore2);
    chans.removeAll(CPAP_FlowLimit);
    chans.removeAll(CPAP_RERA);

    // Get the channels for specified Channel types
    QList<ChannelID> chansSpan ;
    chansSpan = day->getSortedMachineChannels(schema::SPAN);
    chansSpan.removeAll(CPAP_Ramp);

    ChannelID ipapcode = CPAP_Pressure;  // default
    for (auto & ch : { CPAP_IPAPSet, CPAP_IPAP, CPAP_PressureSet } ) {
        if (day->channelExists(ch)) {
            ipapcode = ch;
            break;
        }
    }

    ChannelID epapcode = NoChannel;  // default
    for (auto & ch : { CPAP_EPAPSet, CPAP_EPAP } ) {
        if (day->channelExists(ch)) {
            epapcode = ch;
            break;
        }
    }
    PressureInfo IPAP(ipapcode, minTime, maxTime), EPAP(epapcode, minTime, maxTime);
    ipap_info=&IPAP;

    chans+=chansSpan;
    IPAP.AddChannels(chans);

    EventDataType minP = HIGHEST_POSSIBLE_PRESSURE;
    EventDataType maxP = 0;
    auto & sessions = day->sessions;

    for ( int idx=0; idx<sessions.size() ; idx++ ) {
        auto & sess = sessions[idx];
        if (sess->type() == MT_CPAP) {
            IPAP.updateBucketsPerPressure(sess);
            EPAP.updateBucketsPerPressure(sess);
            IPAP.eventLists = sess->eventlist.value(ipapcode);
            EPAP.eventLists = sess->eventlist.value(epapcode);

            updateTimes(IPAP);
            updateTimes(EPAP);
            // Pressure Min abd Max do not exist for CPAP machines
            EventDataType value = getSetting(sess, CPAP_PressureMin);
            if (value >=0.1 && minP >value)  minP=value;
            value = getSetting(sess, CPAP_PressureMax);
            if (value >=0.1 && maxP <value)  maxP=value;

            updateEvents(sess,IPAP);

            if (m_quit)  break;
        }

    }
    if (minP>maxP) minP=maxP;
    IPAP.setMachineTimes(minP,maxP);

    if (m_quit) {
        m_done = true;
        return;
    }

    IPAP.finishCalcs();
    EPAP.finishCalcs();

    map->timelock.lock();
    map->epap = EPAP;
    map->ipap = IPAP;
    map->timelock.unlock();
    map->recalcFinished();
    m_done = true;
}


//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MinutesAtPressure class      <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

MinutesAtPressure::MinutesAtPressure() :Layer(NoChannel)
{
    m_remap = nullptr;
    m_minimum_height = 0;
}
MinutesAtPressure::~MinutesAtPressure()
{
    while (recalculating()) {};
}

void MinutesAtPressure::SetDay(Day *day)
{
    #if defined(TEST_MACROS_SAMPLE)
    // TEST_MACROS_ENABLED examples. -- do not remove
    IF (day) DEBUGCI O(day->date());
    #endif

    Layer::SetDay(day);

    m_empty = false;
    m_recalculating = false;
    m_lastminx = 0;
    m_lastmaxx = 0;
    m_empty = !m_day || !(m_day->channelExists(CPAP_Pressure) || m_day->channelExists(CPAP_EPAP) || m_day->channelExists(CPAP_PressureSet) || m_day->channelExists(CPAP_EPAPSet));
}


int MinutesAtPressure::minimumHeight()
{
    return m_minimum_height;
}


bool MinutesAtPressure::isEmpty()
{

    return m_empty;
}

void MinutesAtPressure::paint(QPainter &painter, gGraph &graph, const QRegion &region)
{

    QRectF boundingRect = region.boundingRect();

    m_minx = graph.min_x;
    m_maxx = graph.max_x;

    if (graph.printing() || ((m_lastminx != m_minx) || (m_lastmaxx != m_maxx))) {
        // note: this doesn't run on popout graphs that aren't linked with scrolling...
        // it's a pretty useless graph to popout, probably should just block it's popout instead.
        recalculate(&graph);
    }
    if (!initialized) return;

    // conditional display of TimeAtPressure Plots  based on Plots displayed by the Pressure Graph.
    // if no Plots are displayed on the Pressure Graph then Displays "Plots Disabled"
    // if Pressure Graph is not displayed then "time at Pressure" displays both Plots
    // the max Y Axis value is updated for the displayed plot.
    setEnabled(graph);
    bool display_pressure = isEnabled(ipap.code);
    bool display_epap = isEnabled(epap.code);
    if (!( display_epap || display_pressure )) {
        // No Data
        QString msg = QObject::tr("Plots Disabled");
        int x, y;
        GetTextExtent(msg, x, y, bigfont);
        graph.renderText(msg, boundingRect, Qt::AlignCenter, 0, Qt::gray, bigfont);
        return;
    }
    // check for empty data.
    if ( ipap.lastPlotBucket ==0 )  display_pressure = false;
    if ( epap.lastPlotBucket ==0 )  display_epap = false;

    if (!( display_epap || display_pressure )) {
        QString msg = QObject::tr("No Data");
        int x, y;
        GetTextExtent(msg, x, y, bigfont);
        graph.renderText(msg, boundingRect, Qt::AlignCenter, 0, Qt::gray, bigfont);
        return;
    }

    // need to Check if windows has changed.
    m_lastminx = m_minx;
    m_lastmaxx = m_maxx;

    if (graph.printing()) {
        // Could just lock the mutex QMutex instead
        mutex.lock();
        // do nothing between, it should hang until complete.
        mutex.unlock();
        //while (recalculating()) { QThread::yieldCurrentThread(); } // yield or whatever
    }

    if (!painter.isActive()) return;


    // Recalculating in the background...  So we just draw an old copy until then the new data is ready
    // (it will refresh itself when complete)
    // The only time we can't draw when at the end of the recalc when the map variables are being updated
    // So use a mutex to lock
    QMutexLocker TimeLock(&timelock);

    ////////////////////////////////////////////////////////////////////
    // calculate  pressure Ranges
    ////////////////////////////////////////////////////////////////////
    int peaktime=0;

    EventDataType minpressure = ipap.machinePressureMin;
    EventDataType maxpressure = ipap.machinePressureMax;

    if (maxpressure < 0.5 || minpressure > maxpressure) {
        minpressure  = HIGHEST_POSSIBLE_PRESSURE;
        maxpressure=0;
    }

    // Calculate pressure range for current display.
    if (display_pressure) {
        maxpressure = qMax(ipap.maxpressure, maxpressure);
        minpressure = qMin(ipap.minpressure, minpressure);
        peaktime = qMax(ipap.peaktime, peaktime);
    }

    if (display_epap) {
        maxpressure = qMax(epap.maxpressure, maxpressure);
        minpressure = qMin(epap.minpressure, minpressure);
        peaktime = qMax(epap.peaktime, peaktime);
    }

    // insure pressure range is above minumum. - especially for constant pressures.
    // software needs a range of pressures.
    static const EventDataType minPressureRange = 3 ;   // 3 is same as pressure graph
    if ((maxpressure - minpressure)<=minPressureRange) {
        minpressure-=(minPressureRange);
        maxpressure+=(minPressureRange);
    }

    EventDataType startGraphBucket = pressureToBucket(minpressure,ipap.bucketsPerPressure);
    EventDataType endGraphBucket = pressureToBucket(maxpressure,ipap.bucketsPerPressure);
    EventDataType numGraphBucket = endGraphBucket - startGraphBucket;

    ////////////////////////////////////////////////////////////////////
    // Generate  drawing Info
    ////////////////////////////////////////////////////////////////////

    if (ipap.minpressure <= 0.01 ) return;

    // draw rectangle for graph boundary.
    painter.setFont(*defaultfont);
    painter.setPen(Qt::black);
    painter.drawRect(boundingRect);


    ////////////////////////////////////////////////////////////////////
    // Calculate drawing bounds
    ////////////////////////////////////////////////////////////////////
    EventDataType rectOffset   = 0.0;
    rectOffset  = ((boundingRect.width() /numGraphBucket) * ipap.bucketsPerPressure)/ipap.numberXaxisDivisions;

    QRectF drawingRect = QRectF( boundingRect.left()+rectOffset,boundingRect.top(),boundingRect.width()-(2*rectOffset),boundingRect.height()-2 );


    EventDataType pixelsPerBucket = EventDataType(drawingRect.width()) / EventDataType(numGraphBucket);

    MapPainter mapPainter(painter, graph, drawingRect , boundingRect)  ;
    mapPainter.setHorizontal(minpressure,maxpressure,pixelsPerBucket, ipap.bucketsPerPressure,NUMBER_OF_CATMULLROMSPLINE_POINTS);

    ////////////////////////////////////////////////////////////////////
    // Draw X and Y axis's and Top Bar (mouse)
    ////////////////////////////////////////////////////////////////////

    int widest_YAxis = mapPainter.drawYaxis(peaktime);
    mapPainter.drawXaxis(ipap.numberXaxisDivisions , startGraphBucket , endGraphBucket );
    mapPainter.drawEventYaxis(ipap.peakevents,widest_YAxis);
    mapPainter.drawMetaData(last_mouse, topBarLabel,ipap ,epap, m_enabled, minpressure ,maxpressure);

    ////////////////////////////////////////////////////////////////////
    // Draw Events and waveforms.
    ////////////////////////////////////////////////////////////////////

    for (const auto ch : ipap.chans) {
        // display Event H, CA, OA, UA, H as tick marks or CSR or LargeLeaks as background - based on Presssure graph configuration.
        if (!isEnabled(ch) ) continue;  // skip if not enabled
        if ( (schema::channel[ch].type() == schema::FLAG) && (ipap.numEvents[ch]==0) ) continue;
        mapPainter.setChannelInfo(ch, ipap.events[ch], mapPainter.yPixelsPerEvent ,startGraphBucket,endGraphBucket);
        mapPainter.drawEvent();
    }

    if (display_pressure) {
        mapPainter.setChannelInfo(ipap.code, ipap.times, mapPainter.yPixelsPerMsec ,ipap.firstPlotBucket,ipap.lastPlotBucket);
        mapPainter.drawPlot();
    }
    if (display_epap) {
        mapPainter.setChannelInfo(epap.code, epap.times, mapPainter.yPixelsPerMsec ,epap.firstPlotBucket,epap.lastPlotBucket);
        mapPainter.drawPlot();
    }
}

void MinutesAtPressure::recalculate(gGraph * graph)
{
    while (recalculating())
        m_remap->quit();

    m_remap = new RecalcMAP(this);
    m_remap->setAutoDelete(true);

    m_remap->setSelectionRange(graph);
    m_graph=graph;

    QThreadPool * tp = QThreadPool::globalInstance();

    if (graph->printing()) {
        m_remap->run();
    } else {
        while(!tp->tryStart(m_remap));

        m_lastmaxx = m_maxx;
        m_lastminx = m_minx;

    }

}

void MinutesAtPressure::recalcFinished()
{
    if (m_graph && !m_graph->printing()) {
        // Can't call this using standard timedRedraw function, we are in another thread, so have to use a throwaway timer
        QTimer::singleShot(0, m_graph->graphView(), SLOT(refreshTimeout()));
        // this causes MinutesAtPressure:: paint to be called.
    }
    m_remap = nullptr;
    m_recalculating = false;
    initialized=true;
}


bool MinutesAtPressure::mouseMoveEvent(QMouseEvent *, gGraph *graph)
{
    if (graph) graph->timedRedraw(0);
    return false;
}

bool MinutesAtPressure::mousePressEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event);
    Q_UNUSED(graph);
    return false;
}

bool MinutesAtPressure::mouseReleaseEvent(QMouseEvent *event, gGraph *graph)
{
    Q_UNUSED(event);
    Q_UNUSED(graph);
    return false;
}


bool MinutesAtPressure::isEnabled(ChannelID id) {
    return m_enabled[id];
} ;

void MinutesAtPressure::setEnabled(gGraph &graph) {
    QList<ChannelID> channels;
    channels+=ipap.code;
    channels+=epap.code;
    channels+=ipap.chans;

    gGraphView *graphView = graph.graphView();
    gGraph* pressureGraph = graphView->findGraph(STR_GRAPH_Pressure);
    gLineChart * pressureGraphLC = NULL;
    if (!pressureGraph ) return;
    pressureGraphLC = dynamic_cast<gLineChart *>(pressureGraph->getLineChart());
    if (!pressureGraph->visible()) return;
    if (!pressureGraphLC) return;
    m_enabled.clear();
    for (QList<ChannelID>::iterator it = channels.begin(); it != channels.end(); ++it) {
        ChannelID ch=*it;
        bool value;
        schema::Channel & chan =schema::channel[ch] ;
        value = chan.enabled();
        if (chan.type() == schema::WAVEFORM) {
            value=pressureGraphLC->plotEnabled(ch);
        } else {
            value &= pressureGraphLC->m_flags_enabled[ch];
        }
        m_enabled[ch]=value;
    }
};

EventDataType getStep(int &stepi, EventDataType& stepmult ) {
    static const QList<EventDataType>  stepArray {1.0, 2.0,5.0};
    return stepmult * stepArray[stepi];
}

void decStep(int &stepi, EventDataType& stepmult ) {
    stepmult = stepmult / 10;
    Q_UNUSED(stepi);
}

void MapPainter::calculatePeakY(int peaktime ){
    GetTextExtent("W", singleCharWidth, textHeight);

    peakMinutes = msecToMinutes(peaktime+1);        // peakMinutes must not be zero.

    static const QList<EventDataType>  stepArray {1.0, 2.0,5.0};
    //static const QList<EventDataType>  stepArray {1.0, 2.5,5.0, 7.5};
    int stepArraySize=stepArray.size();
    int height = drawingRect.height();

    height -= qMin ( int(drawingRect.height()/10), qMax(textHeight, drawTickLength));

    int maxsteps=ceil(height / textHeight);
    #define MINSTEPS 1
    #define MAXSTEPS 15
    maxsteps=qMax (MINSTEPS, qMin(maxsteps,MAXSTEPS));
    EventDataType minStep = peakMinutes / maxsteps;

    int stepi=0;         // o - ArraySize-1
    EventDataType stepmult=1;   //10**n
    int numberSteps=1;
    EventDataType step = 1;

    yPixelsPerStep  = 0;
    EventDataType totalMinutes = 0;
    EventDataType pixelsPerMinute = 0;
    bool up=false;      // find smallest step

    // find smallest step that where step label do not overlap
    for (;;)  {
        step = stepmult * stepArray[stepi];
        if (step>minStep) {
            if (!up) {
                // very low levels.
                stepmult = stepmult / 10;
                continue;
            }
            numberSteps = ceil(peakMinutes/step);
            if (numberSteps==0) numberSteps=1;
            totalMinutes = step*numberSteps;
            if (totalMinutes>=peakMinutes) {
                // this works.
                break;
            }
        }
        up=true;
        stepi=(stepi+1)%stepArraySize;  // next step module array size.
        if (stepi==0) stepmult*=10;
    }

    // determine Y-axis scale
    pixelsPerMinute = height / totalMinutes;
    totalMinutes = step*numberSteps;
    pixelsPerMinute = height / totalMinutes;
    yPixelsPerStep = height / numberSteps;

    // update parameters required for the Y-axis
    yPixelsPerMsec = pixelsPerMinute/60000;
    yMinutesPerStep=step;
    peakMinutes = totalMinutes;
    //DEBUG << O(drawingRect.height() ) << O(textHeight) << O(peaktime) << O(peakmult) << O(yPixelsPerMsec) << O(yMinutesPerStep) << O(peakMinutes) ;

}

int MapPainter::drawYaxis(int peaktime) {
    MapPainter::calculatePeakY(peaktime );
    ////////////////////////////////////////////////////////////////////
    // Draw Y Axis labels
    ////////////////////////////////////////////////////////////////////
    QString label;
    int labelWidth,labelHeight;
    EventDataType bot = drawingRect.bottom();
    int left= boundingRect.left();
    int width= boundingRect.width();
    int widest_YAxis = 2;
    EventDataType limit =peakMinutes +(yMinutesPerStep/2) ;
    for (EventDataType f=0.0; f<limit; f+=yMinutesPerStep,bot-=yPixelsPerStep) {
        painter.setPen(Qt::black);
        painter.drawLine(left, bot, left-4, bot);

        painter.setPen(QColor(128,128,128,30));
        painter.drawLine(left, bot, left+width, bot);

        label = QString("%1").arg(f);
        GetTextExtent(label, labelWidth, labelHeight);
        widest_YAxis = qMax(widest_YAxis, labelWidth +8);
        graph.renderText(label, left-(labelWidth+8),  bot+labelHeight/2-2 );
        //DEBUG << O(label) << O(bot) <<O(yPixelsPerStep) <<OO(bot,drawingRect.bottom()) ;
    }
    return widest_YAxis;
}

void MapPainter::drawEventYaxis(EventDataType ,int ) {
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MapPainter class      <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

void MapPainter::drawXaxis(int numberXaxisDivisions , int startGraphBucket , int endGraphBucket )
{
    ////////////////////////////////////////////////////////////////////
    // Draw X Axis labels
    ////////////////////////////////////////////////////////////////////

    QString label;
    EventDataType xp,yp;

    pixelsPerPressure    = (pixelsPerBucket * bucketsPerPressure);
    EventDataType pixelsPerTick        = pixelsPerPressure/numberXaxisDivisions;
    EventDataType tickPressure = minpressure;
    xp = drawingRect.left();
    int jlabel        = 0;
    int accentTick= numberXaxisDivisions / 2 ;

    EventDataType deltaSteps    = ( ceil(minpressure) - minpressure ) * pixelsPerPressure;
    EventDataType pixelsFirstTick = fmod(deltaSteps , pixelsPerTick);
    jlabel               = floor(deltaSteps / pixelsPerTick) ;
    EventDataType presFirstTick = (pixelsFirstTick/pixelsPerPressure);

    xp += pixelsFirstTick;
    tickPressure += presFirstTick ;
    EventDataType pressurePerTick = pixelsPerTick / pixelsPerPressure;
    accentTick=(jlabel+accentTick )%numberXaxisDivisions;

    yp = boundingRect.bottom()+1;
    EventDataType ypEnd = yp+6;
    EventDataType ypAccentEnd = yp+12;
    int j=0,labelWidth,labelHeight;
    int right = drawingRect.right()+1;
    int end=(endGraphBucket-startGraphBucket);
    painter.setPen(Qt::black);

    for (int i=0 ; i<=end  && xp <=right ; ++i) {
        for (j=0; j<numberXaxisDivisions && xp<=right; ++j,xp+=pixelsPerTick,tickPressure+= pressurePerTick) {
            if (j==jlabel) {
                label = QString("%1").arg(tickPressure,0,'f',0);
                GetTextExtent(label, labelWidth, labelHeight);
                graph.renderText(label, xp-labelWidth/2, yp+labelHeight+4);
            }
            painter.drawLine(xp, yp, xp, (j==accentTick?ypAccentEnd:ypEnd));
        }
    }
}

QString displayMetaData(QString label, EventDataType mop, EventDataType min , EventDataType max, QString timeString,  QString label2 , QString timeString2) {
    return QString("%1:%2[%3-%4]%5    %6%7")
        .arg(label)
        .arg(mop,3,'f',1)
        .arg(min,3,'f',1)
        .arg(max,3,'f',1)
        .arg(timeString)
        .arg(label2)
        .arg(timeString2)
    ;
}

void MapPainter::drawMetaData(QPoint& last_mouse , QString& topBarLabel,PressureInfo& ipap , PressureInfo& epap ,QHash<ChannelID, bool >& enabled , EventDataType  minpressure , EventDataType  maxpressure)
 {
    int top=boundingRect.top();
    int bottom=boundingRect.bottom();
    ////////////////////////////////////////////////////////////////////
    // Draw mouse over events
    ////////////////////////////////////////////////////////////////////
    mouseOverKey = -1;
    QPoint mouse=graph.graphView()->currentMousePos();

    bool toolTipOff=false;
    if (mouse.x()==0 && mouse.y() ==0) {
        toolTipOff=true;
        mouse=last_mouse;
    } else {
        last_mouse=mouse;
    }

    graphSelected= (mouse.y()<=boundingRect.bottom() && mouse.y()>=boundingRect.top() );
    bool eventOccured = false;
    if ((mouse.x()<boundingRect.left() ||  mouse.x()>boundingRect.right() )) {
        graphSelected= false;
        // note until Session start times are synced with waveforms start time. there will be a difference in the total time displayed.
        // so don't display the total waveform time, because the user can see the  difference between sessions times and the total duration
        // calculated.  both the first and last times can be different for resmed machines. This can be confusing so don't display questionable data.

        topBarLabel = displayMetaData(ipap.chan.label(),minpressure, minpressure, maxpressure, timeString(ipap.totalDuration),"","");
        //topBarLabel = displayMetaData(ipap.chan.label(),minpressure, minpressure, maxpressure, "" ,"","");
        //So just display original Label instead of total Duration.
        // topBarLabel = QObject::tr("Peak %1").arg(msecToMinutes(qMax(ipap.peaktime, epap.peaktime)),1,'f',1);
        Q_UNUSED(maxpressure);
    } else {
        // Mouse is in the horizantile ploting area of all graphs.
        EventDataType pMousePressure =  minpressure + ( (mouse.x() - drawingRect.left()) / pixelsPerPressure);
        mouseOverKey = floor((pMousePressure+sampleIntervalStart)*bucketsPerPressure);
        EventDataType mouseOverPressure = (EventDataType)mouseOverKey/bucketsPerPressure;

        int bucketX = ((mouseOverPressure-minpressure)*pixelsPerPressure) +drawingRect.left() ;

        // Draw veritical line for mouse cursor. jump to closest pressure bucket.
        painter.setPen(QPen(QColor(128,128,128,30), 1.5*AppSetting->lineThickness()));
        painter.drawLine(bucketX, top, bucketX, bottom);

        bool epapEnabled = enabled[epap.code]  ;
        topBarLabel = displayMetaData(
            ipap.chan.label(),
            mouseOverPressure,
            mouseOverPressure-sampleIntervalStart,
            mouseOverPressure+sampleIntervalEnd,
            timeString(ipap.times[mouseOverKey]) ,
            epapEnabled?epap.chan.label():"",
            epapEnabled?timeString(epap.times[mouseOverKey]):""
        );


        QString toolTipLabel = QString();
        int nc = ipap.chans.size();
        for (int i=0;i<nc;++i) {
            ChannelID ch = ipap.chans.at(i);
            if (!enabled[ch] ) continue;
            schema::Channel & chan = schema::channel[ch];
            if (chan.type() != schema::FLAG) continue;
            int numEvents = ipap.numEvents[ch];
            if (numEvents==0) continue;
            int cnt = ipap.events[ch].at(mouseOverKey);
            if (cnt==0) continue;
            topBarLabel += QString("    %1(%2)").arg(chan.label()).arg(cnt);
            toolTipLabel += QString("%1 %2\n").arg(chan.fullname()).arg(cnt);
            eventOccured=true;
        }
        toolTipLabel.remove(QRegularExpression("\\n+$"));

        // DRAW tooltip information
        if (!toolTipOff && graphSelected && eventOccured) {
            graph.ToolTip(toolTipLabel, bucketX-10, drawingRect.top()+10, TT_AlignRight);
        }
    }

    // DRAW top line status information
    int topBarTop=boundingRect.top()-5;
    graph.renderText(topBarLabel, boundingRect.left(),  topBarTop);
}

// set the opacity and default color
void MapPainter::setPenColorAlpha(ChannelID channelId ,int opacity) {
        QColor color=schema::channel[channelId].defaultColor();
        if (opacity>0 &&  opacity<=255) {
            color.setAlpha(opacity);
            //DEBUG << FULLNAME(channelId) << O(color.name()) << O(opacity);
        }
        linePen=QPen(color, AppSetting->lineThickness());
        pointEnhancePen =QPen(QColor(Qt::black), 2.5*AppSetting->lineThickness());
        pointSelectionPen=QPen(color, 1.5*AppSetting->lineThickness());
}

void MapPainter::setChannelInfo(ChannelID id, QVector<int> dataArray, EventDataType yPixelsPerUnit ,int startBucket ,int endBucket)
{
    channel =  &schema::channel[id];
    chanType = channel->type(); //schema::channel[id].type() ;
    this->dataArray = dataArray;
    this->yPixelsPerUnit = yPixelsPerUnit;
    this->startBucket = startBucket;
    this->endBucket = endBucket;
    setPenColorAlpha(id , chanType!=schema::WAVEFORM ? 50  : 255);
}

// converts a y value to a graph point
// Adjusting values from the min and max graph ranges.
EventDataType MapPainter::verifyYaxis(EventDataType value) {
    EventDataType top=drawingRect.top();
    EventDataType bottom=drawingRect.bottom();
    if (value<=top) {
        return top+2;
    } else if (value>=bottom) {
        return bottom;
    }
    return value;
}

EventDataType MapPainter::dataToYaxis(int pp) {
    EventDataType val=verifyYaxis (drawingRect.bottom() - ((pp*yPixelsPerUnit)));
    return val;
}


// Initializes values used based
void MapPainter::initCatmullRomSpline(EventDataType pixelsPerBucket,int numberOfPoints)
{
    catmullRomSplineNumberOfPoints = numberOfPoints;
    catmullRomSplineIncrement = 1.0 / catmullRomSplineNumberOfPoints ;
    catmullRomSplineInterval  = 0.0f;
    catmullRomSplineXstep = pixelsPerBucket / catmullRomSplineNumberOfPoints;
}

// Draws a line between two points
// The line will be stright if anti-aliasing is turned off.
// otherwise the line will be be curved to fit the data.
EventDataType MapPainter::drawSegment( int bucket ,EventDataType lastxp,EventDataType lastyp)
{

    EventDataType xp=lastxp;
    EventDataType dM1  = dataToYaxis(dataArray[bucket-1]);
    EventDataType yp  = dataToYaxis(dataArray[bucket +0]);
    EventDataType d1  = dataToYaxis(dataArray[bucket +1]);
    EventDataType d2  = dataToYaxis(dataArray[bucket +2]);

    catmullRomSplineInterval=catmullRomSplineIncrement;

    for (int loop=0;loop<catmullRomSplineNumberOfPoints;loop++,catmullRomSplineInterval += catmullRomSplineIncrement) {

        // The catmull-Rom-Spline algorithm calculates N points to be displayed between points P1 and P2.
        // The algorithm takes four values (Y-axis) (equival-distant) P0,P1,P2,P3, and an interval
        // The interval range: 0<= interval <=1
        // Interval =   (distance from P1)/(distance from P1 to P2) on the X axis
        // When the Interval is zero (0) the algorithm returns P1
        // When the Interval is One (1) the algorithm retunrs P2
        // P0 and P3 impact the values of the points between P1 and P2 to produce the smooth results.
        // The number of points must be greater than zero
        // when the numberOfPoints is 1 then a straight line is generated between points P1 and P2.
        // Good smoothed graphs are available when the numberOfPoints is 5.
        if (catmullRomSplineNumberOfPoints>1) {
            yp= CatmullRomSpline( dM1, yp , d1, d2 , catmullRomSplineInterval) ;
        }
        yp=verifyYaxis(yp);
        xp+=catmullRomSplineXstep;

        painter.drawLine(lastxp, lastyp, xp, yp);
        lastxp = xp;
        lastyp = yp;
    }
    return yp;
}

void MapPainter::drawPoint(bool accent,int xp, int yp) {
        if (!graphSelected) return;
        if (yp>=drawingRect.bottom() && chanType!=schema::WAVEFORM) return;
        //DEBUG << FULLNAME(info.code) << OO(id,channel.id());
        if (accent) {
            painter.setPen(pointEnhancePen);
        } else {
            painter.setPen(pointSelectionPen);
        }
        int radius=1;
        painter.drawEllipse(QPoint(xp,yp),radius,radius);
        painter.setPen(linePen);
}

// Draw a plot of points on Graphs
// CPAP_Pressure or CPAP_EPAP or Events
void MapPainter::drawPlot() {
        tickPen =                   QPen(Qt::black, 1);
        bool started=false;
        EventDataType yp=drawingRect.bottom();
        EventDataType xp=drawingRect.left();

        painter.setPen(linePen);
        for (int i=startGraphBucket; i<=endBucket; ++i,xp+=pixelsPerBucket) {
            //DEBUG << O(i) << OO(data,dataArray[i]);
            bool accent = (i==mouseOverKey);
            if (!started ) {
                if (dataArray[i]<=0) continue;
                if (i>=startBucket) {
                    //draw vertical line to first point.
                    started=true;
                    // following used to test Y axis labels position.
                    //int tmp=dataArray[i];
                    //if (tmp>61000 && tmp<63000) tmp=60000;
                    //yp = dataToYaxis(tmp);
                    yp = dataToYaxis(dataArray[i]);
                    //DEBUG << OO(bucket,i) <<O(xp) << O(yp) <<OO(msec,dataArray[i]);
                    painter.drawLine(xp ,drawingRect.bottom(), xp, yp);
                    drawPoint( accent ,xp, drawingRect.bottom());
                } else {
                    continue;
                }
            }
            if (i>=endBucket) {
                // draw vertical line to last point.
                EventDataType lastxp=xp;
                if (yp >=drawingRect.bottom() )  {
                    //last point was at bottom.
                    lastxp-=pixelsPerBucket;
                    yp = dataToYaxis(dataArray[i]);
                }
                drawPoint( accent ,xp, yp);
                painter.drawLine(lastxp ,drawingRect.bottom(), xp, yp);
                return;
            }
            drawPoint( accent ,xp, yp);
            yp=drawSegment (i,xp,yp) ;
        }
}

// Draw a an Event tick at the top of ther graph
void MapPainter::drawSpanEvents() {
    if (dataArray.isEmpty()) return;
    EventDataType xp = drawingRect.left();
    EventDataType pixelsPerBucket = this->pixelsPerBucket;
    EventDataType pixelsPerBucket2 = pixelsPerBucket/2;
    pixelsPerBucket = pixelsPerBucket2;
    QRectF box= QRectF(xp,boundingRect.top(),pixelsPerBucket,boundingRect.height());
    int tickTop = boundingRect.top();
    int tickHeight =boundingRect.height();
    QColor color=schema::channel[channel->id()].defaultColor();
    color.setAlpha(128);

    for (int i=startGraphBucket;  i<dataArray.size(); ++i) {
        if (i>=endGraphBucket) {
                if (i==endGraphBucket) {
                    pixelsPerBucket = pixelsPerBucket2;
                } else {
                    return;
                }
        }
        int data=dataArray[i];
        if (data>0) {
            box.setRect(xp,tickTop,pixelsPerBucket,tickHeight);
            painter.fillRect(box,color);
        }
        xp+=pixelsPerBucket;
        pixelsPerBucket=this->pixelsPerBucket;
    }
}

void MapPainter::drawEventTick() {
    EventDataType xp=drawingRect.left();
    int tickLength=drawTickLength;

    int top = boundingRect.top();
    int bottom = boundingRect.bottom();

    painter.setPen(tickPen);
    for (int i=startGraphBucket; i<dataArray.size(); ++i,xp+=pixelsPerBucket) {
        if (dataArray[i]<=0) continue;
        if ((i==mouseOverKey) && (graphSelected) ) {
            painter.setPen(tickEnhancePen);
            painter.drawLine(xp ,top, xp, top+tickLength);
            painter.drawLine(xp ,bottom, xp, bottom-tickLength);
            painter.setPen(tickEnhanceTransparentPen);
            painter.drawLine(xp ,top+tickLength, xp, bottom-tickLength);
            painter.setPen(tickPen);
        } else {
            painter.setPen(QColor(128,128,128,30));
            painter.drawLine(xp ,top+tickLength, xp, bottom);
            painter.setPen(tickPen);
            painter.drawLine(xp ,top, xp, top+tickLength);
        }
    }
}

void MapPainter::drawEvent() {
    if (chanType == schema::FLAG) {
        tickPen =                   QPen(Qt::black, 1);
        tickEnhancePen =            QPen(QColor(0,0,255),    3.5*AppSetting->lineThickness());
        tickEnhanceTransparentPen = QPen(QColor(0,0,255,60), 3.5*AppSetting->lineThickness());
        drawEventTick();
        return;
    }
    if (chanType == schema::SPAN) {
        drawSpanEvents();
        return;
    }
}

