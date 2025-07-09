/* SleepLib ZEO Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the zeo_data_version in zeo_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include "zeo_loader.h"
#include "SleepLib/machine.h"
#include "csv.h"

ZEOLoader::ZEOLoader()
{
    m_type = MT_SLEEPSTAGE;
    csv = nullptr;
}

ZEOLoader::~ZEOLoader()
{
    closeCSV();
}

/*15233: "Sleep Date"
15234: "ZQ"
15236: "Total Z"
15237: "Time to Z"
15237: "Time in Wake"
15238: "Time in REM"
15238: "Time in Light"
15241: "Time in Deep"
15242: "Awakenings"
15245: "Start of Night"
15246: "End of Night"
15246: "Rise Time"
15247: "Alarm Reason"
15247: "Snooze Time"
15254: "Wake Tone"
15259: "Wake Window"
15259: "Alarm Type"
15260: "First Alarm Ring"
15261: "Last Alarm Ring"
15261: "First Snooze Time"
15265: "Last Snooze Time"
15266: "Set Alarm Time"
15266: "Morning Feel"
15267: "Sleep Graph"
15267: "Detailed Sleep Graph"
15268: "Firmware Version"  */

int ZEOLoader::OpenFile(const QString & filename)
{
    if (!openCSV(filename)) {
        closeCSV();
        return -1;
    }
    int count = 0;
    Session* sess;
    // TODO: add progress bar support, perhaps move shared logic into shared parent class with Dreem loader
    while ((sess = readNextSession()) != nullptr) {
        sess->SetChanged(true);
        mach->AddSession(sess);
        count++;
    }
    if (count > 0) {
        mach->Save();
        mach->SaveSummaryCache();
        p_profile->StoreMachines();
    }
    closeCSV();
    return count;
}

bool ZEOLoader::openCSV(const QString & filename)
{
    file.setFileName(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open zeo file" << filename;
            return false;
        }
    } else {// if (filename.toLower().endsWith(".dat")) {
        // TODO: add direct support for .dat files
        return false;
        // not supported.
    }

    QStringList header;
    csv = new CSVReader(file);
    bool ok = csv->readRow(header);
    if (!ok) {
        qWarning() << "no header row";
        return false;
    }
    csv->setFieldNames(header);

    MachineInfo info = newInfo();
    mach = p_profile->CreateMachine(info);

    return true;
}

void ZEOLoader::closeCSV()
{
    if (csv != nullptr) {
        delete csv;
        csv = nullptr;
    }
    if (file.isOpen()) {
        file.close();
    }
}

//    int idxTotalZ = header.indexOf("Total Z");
//    int idxAlarmReason = header.indexOf("Alarm Reason");
//    int idxSnoozeTime = header.indexOf("Snooze Time");
//    int idxWakeTone = header.indexOf("Wake Tone");
//    int idxWakeWindow = header.indexOf("Wake Window");
//    int idxAlarmType = header.indexOf("Alarm Type");

static const EventDataType GAIN = 0.25;  // allow for fractional sleep stages (such as Deep (2))

Session* ZEOLoader::readNextSession()
{
    if (csv == nullptr) {
        qWarning() << "no CSV open!";
        return nullptr;
    }
    Session* sess = nullptr;
    QDateTime start_of_night;  //, end_of_night, rise_time;

    qint64 st, tt;
    int stage;

    int ZQ, TimeToZ, TimeInWake, TimeInREM, TimeInLight, TimeInDeep, Awakenings;
    int MorningFeel;
    //QString FirmwareVersion, MyZeoVersion;
    //QDateTime FirstAlarmRing, LastAlarmRing, FirstSnoozeTime, LastSnoozeTime, SetAlarmTime;
    QStringList /*SG,*/ DSG;

    QHash<QString,QString> row;
    while (csv->readRow(row)) {
        SessionID sid = 0;
        invalid_fields = false;

        start_of_night = readDateTime(row["Start of Night"]);
        if (start_of_night.isValid()) {
            sid = start_of_night.toSecsSinceEpoch();
            if (mach->SessionExists(sid)) {
                continue;
            }
        } // else invalid_fields will be true

        ZQ = readInt(row["ZQ"]);
        TimeToZ = readInt(row["Time to Z"]);
        TimeInWake = readInt(row["Time in Wake"]);
        TimeInREM = readInt(row["Time in REM"]);
        TimeInLight = readInt(row["Time in Light"]);
        TimeInDeep = readInt(row["Time in Deep"]);
        Awakenings = readInt(row["Awakenings"]);
        //end_of_night = readDateTime(row["End of Night"]);
        //rise_time = readDateTime(row["Rise Time"]);
        //FirstAlarmRing = readDateTime(row["First Alarm Ring"], false);
        //LastAlarmRing = readDateTime(row["Last Alarm Ring"], false);
        //FirstSnoozeTime = readDateTime(row["First Snooze Time"], false);
        //LastSnoozeTime = readDateTime(row["Last Snooze Time"], false);
        //SetAlarmTime = readDateTime(row["Set Alarm Time"], false);
        MorningFeel = readInt(row["Morning Feel"], false);
        //FirmwareVersion = row["Firmware Version"];
        //MyZeoVersion = row["My ZEO Version"];

        if (invalid_fields) {
            continue;
        }

        //SG = row["Sleep Graph"].trimmed().split(" ");
        DSG = row["Detailed Sleep Graph"].trimmed().split(" ");

        if (DSG.size() == 0) {
            continue;
        }

        sess = new Session(mach, sid);
        break;
    };

    if (sess) {
        const int WindowSize = 30 * 1000;
        m_session = sess;

        sess->settings[ZEO_Awakenings] = Awakenings;
        sess->settings[ZEO_MorningFeel] = MorningFeel;
        sess->settings[ZEO_TimeToZ] = TimeToZ;
        sess->settings[ZEO_ZQ] = ZQ;
        sess->settings[ZEO_TimeInWake] = TimeInWake;
        sess->settings[ZEO_TimeInREM] = TimeInREM;
        sess->settings[ZEO_TimeInLight] = TimeInLight;
        sess->settings[ZEO_TimeInDeep] = TimeInDeep;

        st = qint64(start_of_night.toSecsSinceEpoch()) * 1000L;
        sess->really_set_first(st);
        tt = st;

        for (int i = 0; i < DSG.size(); i++) {
            bool ok;
            stage = DSG[i].toInt(&ok);
            if (ok) {
                // 0 = no data, 1 = Awake, 2 = REM, 3 = Light Sleep, 4 = Deep Sleep, 6 = Deep Sleep (2), drawn slightly less deep
                int value = -stage / GAIN;  // use negative values so that the chart is oriented the right way
                switch (stage) {
                    case 0:
                        EndEventList(ZEO_SleepStage, tt);
                        break;
                    case 6:
                        // According to ZeoViewer, 6 is a "Deep (2)" and is drawn somewhere between Light and Deep.
                        value = -3.75 / GAIN;
                        // fall through
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        AddEvent(ZEO_SleepStage, tt, value);
                        break;
                    default:
                        qWarning() << sess->session() << start_of_night << "@" << i << "unknown sleep stage" << stage;
                        break;
                }
            } else {
                qWarning() << sess->session() << start_of_night << "@" << i << "unknown sleep stage" << DSG[i];
            }
            tt += WindowSize;
        }

        EndEventList(ZEO_SleepStage, tt);
        sess->really_set_last(tt);
        //int size = DSG.size();
        //qDebug() << linecomp[0] << start_of_night << end_of_night << rise_time << size << "30 second chunks";
    }

    return sess;
}

void ZEOLoader::AddEvent(ChannelID channel, qint64 t, EventDataType value)
{
    EventList* C = m_importChannels[channel];
    if (C == nullptr) {
        C = m_session->AddEventList(channel, EVL_Event, GAIN, 0, -5, 0);
        Q_ASSERT(C);  // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    // Add the event
    C->AddEvent(t, value);
    m_importLastValue[channel] = value;
}

void ZEOLoader::EndEventList(ChannelID channel, qint64 t)
{
    EventList* C = m_importChannels[channel];
    if (C != nullptr) {
        C->AddEvent(t, m_importLastValue[channel]);
        
        // Mark this channel's event list as ended.
        m_importChannels[channel] = nullptr;
    }
}

QDateTime ZEOLoader::readDateTime(const QString & text, bool required)
{
    QDateTime dt = QDateTime::fromString(text, "MM/dd/yyyy HH:mm");
    if (required || !text.isEmpty()) {
        if (!dt.isValid()) {
            dt = QDateTime::fromString(text, "yyyy-MM-dd HH:mm:ss");
            if (!dt.isValid()) {
                invalid_fields = true;
            }
        }
    }
    return dt;
}

int ZEOLoader::readInt(const QString & text, bool required)
{
    bool ok;
    int value = text.toInt(&ok);
    if (!ok) {
        if (required) {
            invalid_fields = true;
        } else {
            value = 0;
        }
    }
    return value;
}

static bool zeo_initialized = false;

void ZEOLoader::Register()
{
    if (zeo_initialized) { return; }

    qDebug("Registering ZEOLoader");
    RegisterLoader(new ZEOLoader());
    //InitModelMap();
    zeo_initialized = true;
}

