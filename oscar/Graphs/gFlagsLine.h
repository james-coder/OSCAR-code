/* gFlagsLine Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code for more details. */

#ifndef GFLAGSLINE_H
#define GFLAGSLINE_H

#include "gGraphView.h"
#include "gspacer.h"

class gFlagsGroup;

/*! \class gYSpacer
    \brief A dummy vertical spacer object
   */
class gLabelArea: public gSpacer
{
  public:
    gLabelArea(Layer * layer);
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region) {
        Q_UNUSED(w);
        Q_UNUSED(painter);
        Q_UNUSED(region);
    }
    virtual int minimumWidth();
  protected:
    Layer *m_mainlayer;
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);

    virtual Layer * Clone() {
        gLabelArea * layer = new gLabelArea(nullptr);  //ouchie..
        Layer::CloneInto(layer);
        CloneInto(layer);
        return layer;
    }

    void CloneInto(gLabelArea * ) {
    }

};


/*! \class gFlagsLine
    \brief One single line of event flags in the Event Flags chart
    */
class gFlagsLine: public Layer
{
    friend class gFlagsGroup;
  public:
    /*! \brief Constructs an individual gFlagsLine object
        \param code  The Channel the data is sourced from
        \param col   The colour to draw this flag
        \param label The label to show to the left of the Flags line.
        \param always_visible  Whether to always show this line, even if empty
        \param Type of Flag, either FT_Bar, or FT_Span
        */
    gFlagsLine(ChannelID code);
    virtual ~gFlagsLine();

    //! \brief Drawing code to add the flags and span markers to the Vertex buffers.
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    virtual Layer * Clone() {
        gFlagsLine * layer = new gFlagsLine(NoChannel);  //ouchie..
        Layer::CloneInto(layer);
        CloneInto(layer);
        return layer;
    }

    void CloneInto(gFlagsLine * layer ) {
        layer->m_always_visible = m_always_visible;
        layer->m_lx = m_lx;
        layer->m_ly = m_ly;
    }

  protected:

    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);

    bool m_always_visible;
    int m_lx, m_ly;

};

/*! \class gFlagsGroup
    \brief Contains multiple gFlagsLine entries for the Events Flag graph
    */
class gFlagsGroup: public LayerGroup
{
    friend class gFlagsLabelArea;

  public:
    gFlagsGroup();
    virtual ~gFlagsGroup();

    //! Draw filled rectangles behind Event Flag's, and an outlines around them all, Calls the individual paint for each gFlagLine
    virtual void paint(QPainter &painter, gGraph &w, const QRegion &region);

    //! Returns the first time represented by all gFlagLine layers, in milliseconds since epoch
    virtual qint64 Minx();
    //! Returns the last time represented by all gFlagLine layers, in milliseconds since epoch.
    virtual qint64 Maxx();

    //! Checks if each flag has data, and adds valid gFlagLines to the visible layers list
    virtual void SetDay(Day *);

    //! Returns true if none of the gFlagLine objects contain any data for this day
    virtual bool isEmpty();

    //! Returns a list of Visible gFlagsLine layers to draw
    QVector<gFlagsLine *> &visibleLayers() { return lvisible; }

    void alwaysVisible(ChannelID code) { m_alwaysvisible.push_back(code); }
    void refreshConfiguration(gGraph* graph) ;

    virtual Layer * Clone() {
        gFlagsGroup * layer = new gFlagsGroup();  //ouchie..
        Layer::CloneInto(layer);
        CloneInto(layer);
        return layer;
    }

    void CloneInto(gFlagsGroup * layer) {
        layer->m_alwaysvisible = m_alwaysvisible;
        layer->availableChans = availableChans;
        layer->m_sessions = m_sessions;

        for (int i=0; i<lvisible.size(); i++) {
            layer->lvisible.append(dynamic_cast<gFlagsLine *>(lvisible.at(i)->Clone()));
        }
        layer->m_barh = m_barh;
        layer->m_empty = m_empty;
        layer->m_rebuild_cpap = m_rebuild_cpap;
    }


  protected:
    virtual bool mouseMoveEvent(QMouseEvent *event, gGraph *graph);
    QList<ChannelID> m_alwaysvisible;
    QList<ChannelID> availableChans;
    QList<Session*> m_sessions;
    qint64 m_start  ;
    qint64 m_duration ;
    QVector<gFlagsLine*> lvisible;
    float m_barh;
    bool m_empty;
    bool m_rebuild_cpap;
    int sessionBarHeight();
};

#endif // GFLAGSLINE_H
