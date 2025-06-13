/* SleepLib ResMed Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <QStringList>
#include <cmath>

#include "SleepLib/session.h"
#include "SleepLib/calcs.h"

// #include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/resmed_EDFinfo.h"


ResMedEDFInfo::ResMedEDFInfo() :EDFInfo() { }
ResMedEDFInfo::~ResMedEDFInfo() { }

bool ResMedEDFInfo::Parse( )	// overrides and calls the super's Parse
{
	if ( ! EDFInfo::Parse(  ) ) {
//      qWarning() << "EDFInfo::Parse failed!";
//      sleep(1);
        return false;
    }
	
    // Now massage some stuff into OSCAR's layout
    int snp = edfHdr.recordingident.indexOf("SRN=");
    serialnumber.clear();

    for (int i = snp + 4; i < edfHdr.recordingident.length(); i++) {
        if (edfHdr.recordingident[i] == ' ') {
            break;
        }
        serialnumber += edfHdr.recordingident[i];
    }

    if (!edfHdr.startdate_orig.isValid()) {
        qDebug() << "Invalid date time retreieved parsing EDF File " << filename;
//      sleep(1);
        return false;
    }

    startdate = qint64(edfHdr.startdate_orig.toSecsSinceEpoch()) * 1000LL;
    //startdate-=timezoneOffset();
    if (startdate == 0) {
        qDebug() << "Invalid startdate = 0 in EDF File " << filename;
//      sleep(1);
        return false;
    }

    dur_data_record = (edfHdr.duration_Seconds * 1000.0L);

    enddate = startdate + dur_data_record * qint64(edfHdr.num_data_records);

    return true;
	
}

//resmed_codes are preinitialized from resmed_loader.
extern QHash<ChannelID, QStringList> resmed_codes;

// Looks up foreign language Signal names that match this channelID
EDFSignal *ResMedEDFInfo::lookupSignal(ChannelID ch)
{
    // Get list of all known foreign language names for this channel
    auto channames = resmed_codes.find(ch);
    if (channames == resmed_codes.end()) {
        // no alternatives strings found for this channel
        return nullptr;
    }

    // This is bad, because ResMed thinks it was a cool idea to use two channels with the same name.

    // Scan through EDF's list of signals to see if any match
    for (auto & name : channames.value()) {
        EDFSignal *sig = lookupLabel(name);
        if (sig)  {
            return sig;
        }
    }

    // Failed
    return nullptr;
}

void dumpEDFduration( EDFduration dur )
{
    qDebug() << "Fullpath" << dur.path << "Filename" << dur.filename << "Start" << dur.start << "End" << dur.end;
}

