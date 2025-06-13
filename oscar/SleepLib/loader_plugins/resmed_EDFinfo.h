/* SleepLib RESMED EDFinfo Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef RESMED_EDFINFO_H
#define RESMED_EDFINFO_H

#include <QVector>
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"
#include "SleepLib/loader_plugins/edfparser.h"

// moved to edfparser.h
// enum EDFType { EDF_UNKNOWN, EDF_BRP, EDF_PLD, EDF_SAD, EDF_EVE, EDF_CSL, EDF_AEV };

EDFType lookupEDFType(const QString & filename);

const QString resmed_class_name = STR_MACH_ResMed;

class STRFile; // forward

class ResMedEDFInfo : public EDFInfo
{
public:
    ResMedEDFInfo();
    ~ResMedEDFInfo();
    
    virtual bool Parse() override;		// overrides and calls the super's Parse
    
    virtual qint64 GetDurationMillis() { return dur_data_record; }	// overrides the super 
    
    EDFSignal *lookupSignal(ChannelID ch);

    //! \brief The following are computed from the edfHdr data
    QString serialnumber;
    qint64 dur_data_record;
    qint64 startdate;
    qint64 enddate;
};

class EDFduration
{
public:
    EDFduration() { start = end = 0; type = EDF_UNKNOWN; }
    EDFduration(quint32 start, quint32 end, QString path) :
        start(start), end(end), path(path) {}

    quint32 start;
    quint32 end;
    QString path;
    QString filename;
    EDFType type;
};

void dumpEDFduration( EDFduration dur );

class STRRecord
{
public:
    STRRecord() {
        maskon.clear();
        maskoff.clear();
        maskdur = 0;
        maskevents = -1;
        mode = -1;
        rms9_mode = -1;
        set_pressure = -1;
        epap = -1;
        epapAuto = -1;
        max_pressure = -1;
        min_pressure = -1;
        max_epap = -1;
        min_epap = -1;
        max_ps = -1;
        min_ps = -1;
        ps = -1;
        ipap = -1;
        max_ipap = -1;
        min_ipap = -1;
        epr = -1;
        epr_level = -1;
        sessionid = 0;

        ahi = -1;
        oai = -1;
        ai = -1;
        hi = -1;
        uai = -1;
        cai = -1;
        csr = -1;

        leak50 = -1;
        leak95 = -1;
        leakmax = -1;

        rr50 = -1;
        rr95 = -1;
        rrmax = -1;

        mv50 = -1;
        mv95 = -1;
        mvmax = -1;

        ie50 = -1;
        ie95 = -1;
        iemax = -1;

        tv50 = -1;
        tv95 = -1;
        tvmax = -1;

        mp50 = -1;
        mp95 = -1;
        mpmax = -1;

        tgtepap50 = -1;
        tgtepap95 = -1;
        tgtepapmax = -1;

        tgtipap50 = -1;
        tgtipap95 = -1;
        tgtipapmax = -1;

        s_RampTime = -1;
        s_RampEnable = -1;
        s_EPR_ClinEnable = -1;
        s_EPREnable = -1;

        s_PtAccess = -1;
        s_PtView   = -1;
        s_ABFilter = -1;
        s_Mask = -1;
        s_Tube = -1;
        s_ClimateControl = -1;
        s_HumEnable = -1;
        s_HumLevel = -1;
        s_TempEnable = -1;
        s_Temp = -1;
        s_SmartStart = -1;
        s_SmartStop  = -1;
        s_Comfort = -1;

        s_RampPressure = -1;

        s_EasyBreathe = -1;
        s_RiseEnable = -1;
        s_RiseTime = -1;
        s_Cycle = -1;
        s_Trigger = -1;
        s_TiMax = -1;
        s_TiMin = -1;

        date=QDate();
    }
    
    STRRecord(const STRRecord & /*copy*/) = default;
    ~STRRecord() {};    // required to get rid of warning

// All the data members

    QVector<quint32> maskon;
    QVector<quint32> maskoff;

    EventDataType maskdur;
    EventDataType maskevents;
    EventDataType mode;
    EventDataType rms9_mode;
    EventDataType set_pressure;
    EventDataType max_pressure;
    EventDataType min_pressure;
    EventDataType epap;
    EventDataType epapAuto;
    EventDataType max_ps;
    EventDataType min_ps;
    EventDataType ps;
    EventDataType max_epap;
    EventDataType min_epap;
    EventDataType ipap;
    EventDataType max_ipap;
    EventDataType min_ipap;
    EventDataType epr;
    EventDataType epr_level;
    quint32 sessionid;
    EventDataType ahi;
    EventDataType oai;
    EventDataType ai;
    EventDataType hi;
    EventDataType uai;
    EventDataType cai;
    EventDataType csr;
    EventDataType leak50;
    EventDataType leak95;
    EventDataType leakmax;
    EventDataType rr50;
    EventDataType rr95;
    EventDataType rrmax;
    EventDataType mv50;
    EventDataType mv95;
    EventDataType mvmax;
    EventDataType tv50;
    EventDataType tv95;
    EventDataType tvmax;
    EventDataType mp50;
    EventDataType mp95;
    EventDataType mpmax;
    EventDataType ie50;
    EventDataType ie95;
    EventDataType iemax;
    EventDataType tgtepap50;
    EventDataType tgtepap95;
    EventDataType tgtepapmax;
    EventDataType tgtipap50;
    EventDataType tgtipap95;
    EventDataType tgtipapmax;

    EventDataType s_RampPressure;
    QDate date;

    EventDataType s_RampTime;
    int s_RampEnable;
    int s_EPR_ClinEnable;
    int s_EPREnable;

    int s_PtAccess;
    int s_PtView;
    int s_ABFilter;
    int s_Mask;
    int s_Tube;
    int s_ClimateControl;
    int s_HumEnable;
    EventDataType s_HumLevel;
    int s_TempEnable;
    EventDataType s_Temp;
    int s_SmartStart;
    int s_SmartStop;
    int s_Comfort;

    int s_EasyBreathe;
    int s_RiseEnable;
    EventDataType s_RiseTime;
    int s_Cycle;
    int s_Trigger;
    EventDataType s_TiMax;
    EventDataType s_TiMin;

};


class STRFile 
{
public:
    STRFile() :
        filename(QString()), days(0), edf(nullptr) {}
    STRFile(QString name, long int recCnt, ResMedEDFInfo *str) :
        filename(name), days(recCnt), edf(str) {}
    STRFile(const STRFile & /*copy*/) = default;

    virtual ~STRFile() {}

    QString         filename;
    long int        days;
    ResMedEDFInfo * edf;
};


#endif // RESMED_EDFINFO_H
