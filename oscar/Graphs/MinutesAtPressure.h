/* Minutes At Pressure Graph Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MINUTESATPRESSURE_H
#define MINUTESATPRESSURE_H

#include <QPen>
#include "Graphs/layer.h"
#include "SleepLib/day.h"
#include "SleepLib/schema.h"
#include "Graphs/gLineChart.h"

class MinutesAtPressure;
struct PressureInfo
{
public:
    PressureInfo();
    PressureInfo(ChannelID code, qint64 minTime, qint64 maxTime) ;
    PressureInfo(PressureInfo &copy) = default;
    ~PressureInfo() {} ;

    void AddChannel(ChannelID c);
    void AddChannels(QList<ChannelID> & chans);
    void finishCalcs();
    void setMachineTimes(EventDataType min,EventDataType max);

    ChannelID code;
    schema::Channel chan;
    qint64 minTime, maxTime;
    QVector<int> times;
    int peaktime, peakevents;

    QHash<ChannelID, QVector<int> > events;
    QHash<ChannelID, int> numEvents;
    QList<ChannelID> chans;
    QVector<EventList*> eventLists;

    void updateBucketsPerPressure(Session* sess);
    int bucketsPerPressure = 1;
    int numberXaxisDivisions =10;

    EventDataType rawToPressure ( EventStoreType raw,EventDataType gain);
    EventStoreType rawToBucketId ( EventStoreType raw,EventDataType gain);
    EventDataType minpressure = 0.0;
    EventDataType maxpressure = 0.0;
    qint64 totalDuration = 0;

    EventDataType machinePressureMin = 0.0;
    EventDataType machinePressureMax = 0.0;

    int firstPlotBucket =0;
    int lastPlotBucket =0;

private:
    void init();
};


class RecalcMAP:public QRunnable
{
    friend class MinutesAtPressure;
public:
    explicit RecalcMAP(MinutesAtPressure * map) :map(map), m_quit(false), m_done(false) {}
    virtual ~RecalcMAP();
    virtual void run();
    void quit();


protected:
    MinutesAtPressure * map;
    volatile bool m_quit;
    volatile bool m_done;

private:
    void setSelectionRange(gGraph* graph);
    qint64 minTime, maxTime;
    ChannelID chanId;       // required for debug.

    PressureInfo * ipap_info;
    void updateTimes(PressureInfo & info);
    void updateEvents(Session*sess,PressureInfo & info);
    void updateTimesValues(qint64 d1,qint64 d2, int key,PressureInfo & info);
    void updateEventsChannel(Session * sess,ChannelID id, QVector<int> &background, PressureInfo & info );
    void updateFlagData(int &currentLoc, int & currentEL,int& currentData,qint64 eventTime, QVector<int> &dataArray, PressureInfo & info ) ;
    void updateSpanData(int &currentLoc, int & currentEL,int& currentData,qint64 startSpan, qint64 eventTime , QVector<int> &dataArray, PressureInfo & info ) ;

};



class MinutesAtPressure:public Layer
{
    friend class RecalcMAP;
public:
    MinutesAtPressure();
    virtual ~MinutesAtPressure();

    virtual void recalculate(gGraph * graph);

    virtual void SetDay(Day *d);

    virtual bool isEmpty();
    virtual int minimumHeight();

    //! Draw filled rectangles behind Event Flag's, and an outlines around them all, Calls the individual paint for each gFlagLine
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);
    bool mousePressEvent(QMouseEvent *event, gGraph *graph);
    bool mouseReleaseEvent(QMouseEvent *event, gGraph *graph);

    virtual void recalcFinished();
    virtual Layer * Clone() {
        MinutesAtPressure * map = new MinutesAtPressure();
        Layer::CloneInto(map);
        CloneInto(map);
        return map;
    }


protected:

    int numCloned =0;
    void CloneInto(MinutesAtPressure * layer) {
        mutex.lock();
        timelock.lock();
        layer->m_empty = m_empty;
        layer->m_minimum_height = m_minimum_height;
        layer->m_lastminx = m_lastminx;
        layer->m_lastmaxx = m_lastmaxx;
        layer->ipap = ipap;
        layer->epap = epap;
        layer->numCloned=numCloned+1;

        timelock.unlock();
        layer->m_enabled = m_enabled;
        mutex.unlock();
    }
    bool isCLoned() {return numCloned!=0;};
    RecalcMAP * m_remap;
    bool initialized=false;
    bool m_empty;
    QMutex mutex;
    QMutex timelock;
    int m_minimum_height;
    //QAtomicInteger<int> m_recalcCount;

private:

    PressureInfo epap, ipap;
    void setEnabled(gGraph &graph);
    QHash<ChannelID, bool > m_enabled;
    gGraph * m_graph;
    qint64 m_lastminx;
    qint64 m_lastmaxx;
    QPoint last_mouse=QPoint(0,0);

    //EventDataType m_last_height=0;     // re-calculate only when needed.
    //int m_last_peaktime=0;      // re-calculate only when needed.

    bool isEnabled(ChannelID id) ;
    QString topBarLabel;

};


class MapPainter
{
public:
    // environment - set in constructor
    QPainter& painter;
    gGraph& graph;
    QRectF& drawingRect;
    QRectF& boundingRect;
    EventDataType lineThickness;

    MapPainter(
                QPainter& painter,
                gGraph& graph,
                QRectF& drawingRect ,
                QRectF& boundingRect ) :
                painter(painter),
                graph(graph),
                drawingRect(drawingRect) ,
                boundingRect(boundingRect)  {
        lineThickness= AppSetting->lineThickness();
    };

    // mouse related
    int mouseOverKey;
    bool graphSelected;
    void setMouse(int mouseOverKey,bool graphSelected) {
        this->mouseOverKey=mouseOverKey;
        this->graphSelected=graphSelected;
    };

    // for all graphs horizonatal
    int startGraphBucket;
    int endGraphBucket;
    EventDataType pixelsPerBucket;
    EventDataType minpressure;
    int bucketsPerPressure;
    void setHorizontal( EventDataType minpressure, EventDataType maxpressure,EventDataType  pixelsPerBucket , int bucketsPerPressure, int catmullRomSplineNumberOfPoints) {
        this->startGraphBucket = minpressure*bucketsPerPressure;
        this->endGraphBucket = maxpressure*bucketsPerPressure;
        this->pixelsPerBucket = pixelsPerBucket;
        this->minpressure = minpressure;
        this->bucketsPerPressure = bucketsPerPressure;
        initCatmullRomSpline(pixelsPerBucket,catmullRomSplineNumberOfPoints);
    };

    void drawEvent();
    void drawSpanEvents();
    void drawEventTick();     


    //  Pen type for drawing - per graph
    QPen linePen;
    QPen pointSelectionPen;
    QPen pointEnhancePen;
    QPen tickPen;
    QPen tickEnhancePen;
    QPen tickEnhanceTransparentPen;

    //EventDataType bottom,top,height,left,right;

    schema::Channel* channel;
    schema::ChanType chanType;
    EventDataType yPixelsPerUnit;
    QVector<int> dataArray;
    int startBucket;
    int endBucket;
    void setChannelInfo(ChannelID id, QVector<int> dataArray, EventDataType yPixelsPerUnit ,int ,int) ;

    void initCatmullRomSpline(EventDataType pixelsPerBucket,int numberOfPoints);
    EventDataType catmullRomSplineIncrement, catmullRomSplineInterval;
    int catmullRomSplineNumberOfPoints;
    EventDataType catmullRomSplineXstep;    // based on pixelsPerBucket.

    void setPenColorAlpha(ChannelID channelId ,int opacity) ;
    void setPenColorAlpha(ChannelID channelId ) ;
    void drawPlot();
    EventDataType dataToYaxis(int value) ;
    EventDataType verifyYaxis(EventDataType value);
    void initCatmullRomSpline(int numberOfPoints);
    void drawPoint(bool fill,int xp, int yp);
    EventDataType drawSegment ( int i , EventDataType fromx,EventDataType fromy) ;

    EventDataType yPixelsPerMsec ;       
    EventDataType yPixelsPerEvent;
    EventDataType pixelsPerPressure;

    EventDataType  yPixelsPerStep ;
    EventDataType  yMinutesPerStep;
    EventDataType  peakMinutes;

    int singleCharWidth, textHeight;
    void calculatePeakY(int peaktime );
    int drawYaxis(int peaktime);

    void drawEventYaxis(EventDataType peakEvents,int widest_YAxis);
    void drawXaxis(int numberXaxisDivisions , int startGraphBucket , int endGraphBucket);

    void drawMetaData(QPoint& last_mouse , QString& topBarLabel,PressureInfo& ipap , PressureInfo& epap ,QHash<ChannelID, bool >& enabled , EventDataType  minpressure , EventDataType  maxpressure);


};

#endif // MINUTESATPRESSURE_H

