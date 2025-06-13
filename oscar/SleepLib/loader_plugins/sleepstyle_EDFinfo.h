/* SleepLib SleepStyle EDFinfo Header
 *
 * Copyright (c) 2021-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SLEEPSTYLE_EDFINFO_H
#define SLEEPSTYLE_EDFINFO_H

#include <QVector>
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"
#include "SleepLib/loader_plugins/edfparser.h"

//enum EDFType { EDF_UNKNOWN, EDF_BRP, EDF_PLD, EDF_SAD, EDF_EVE, EDF_CSL, EDF_AEV };
//enum EDFType { EDF_UNKNOWN, EDF_RT };     // moved to edfparser.h

// EDFType lookupEDFType(const QString & filename);

const QString SLEEPSTYLE_class_name = STR_MACH_ResMed;

//class STRFile; // forward

class SleepStyleEDFInfo : public EDFInfo
{
public:
    SleepStyleEDFInfo();
    ~SleepStyleEDFInfo();
    
    virtual bool Parse() override;		// overrides and calls the super's Parse
    
    virtual qint64 GetDurationMillis() { return dur_data_record; }	// overrides the super 
    
    EDFSignal *lookupSignal(ChannelID ch);

    QDateTime getStartDT( QString dateTimeStr );

    //! \brief The following are computed from the edfHdr data
    QString serialnumber;
    qint64 dur_data_record;
    qint64 startdate;
    qint64 enddate;
};

class ssEDFduration
{
public:
    ssEDFduration() { start = end = 0; type = EDF_UNKNOWN; }
    ssEDFduration(quint32 start, quint32 end, QString path) :
        start(start), end(end), path(path) {}

    quint32 start;
    quint32 end;
    QString path;
    QString filename;
    EDFType type;
};

void dumpEDFduration( ssEDFduration dur );

#endif // SLEEPSTYLE_EDFINFO_H
