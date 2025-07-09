/* SleepLib SleepStyle Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

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

#include "SleepLib/loader_plugins/sleepstyle_EDFinfo.h"


SleepStyleEDFInfo::SleepStyleEDFInfo() : EDFInfo() {
    setTimeZoneUTC();   // Ask EDF Parser to assume data is in UTC, not in local time
}
SleepStyleEDFInfo::~SleepStyleEDFInfo() { }

bool SleepStyleEDFInfo::Parse( )	// overrides and calls the super's Parse
{
	if ( ! EDFInfo::Parse(  ) ) {
      qWarning() << "sleepStyle EDFInfo::Parse failed!";
//      sleep(1);
        return false;
    }
	
    // Now massage some stuff into OSCAR's layout
    // Extract the serial number from header string
    QStringList parts = edfHdr.recordingident.split(' ');
    serialnumber = parts[6];

    if (!edfHdr.startdate_orig.isValid()) {
        qDebug() << "sleepStyle EDFInfo::Parse Invalid date time retreieved parsing EDF File" << filename;
//      sleep(1);
        return false;
    }

    startdate = qint64(edfHdr.startdate_orig.toSecsSinceEpoch()) * 1000L;
    //startdate-=timezoneOffset();
    if (startdate == 0) {
        qDebug() << "sleepStyle EDFInfo::Parse Invalid startdate = 0 in EDF File" << filename;
//      sleep(1);
        return false;
    }

    dur_data_record = (edfHdr.duration_Seconds * 1000.0L);

    enddate = startdate + dur_data_record * qint64(edfHdr.num_data_records);

    return true;
	
}

extern QHash<ChannelID, QStringList> resmed_codes;

// Looks up foreign language Signal names that match this channelID
EDFSignal *SleepStyleEDFInfo::lookupSignal(ChannelID ch)
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
        if (sig) 
            return sig;
    }

    // Failed
    return nullptr;
}

QDateTime SleepStyleEDFInfo::getStartDT( QString dateTimeStr )
{
//  edfHdr.startdate_orig = QDateTime::fromString(QString::fromLatin1(hdrPtr->datetime, 16), "dd.MM.yyHH.mm.ss");
//  QString dateTimeStr;    // , dateStr, timeStr;
    QDate qDate;
    QTime qTime;
//  dateTimeStr = QString::fromLatin1(hdrPtr->datetime, 16);
//    dateStr = dateTimeStr.left(8);
//    timeStr = dateTimeStr.right(8);
    qDate = QDate::fromString(dateTimeStr.left(8), "dd.MM.yy");
    if (qDate.year() < 2000) {
        qDate = qDate.addYears(100);
    }
    qTime = QTime::fromString(dateTimeStr.right(8), "HH.mm.ss");
    return QDateTime(qDate, qTime, QTimeZone("UTC"));
}


void dumpEDFduration( ssEDFduration dur )
{
    qDebug() << "Fullpath" << dur.path << "Filename" << dur.filename << "Start" << dur.start << "End" << dur.end;
}

