/* SleepLib Dreem Loader Implementation
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the dreem_data_version in dreem_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include <QApplication>
#include <QMessageBox>
#include "dreem_loader.h"
#include "SleepLib/machine.h"
#include "csv.h"

static QSet<QString> s_unexpectedMessages;

DreemLoader::DreemLoader()
{
    m_type = MT_SLEEPSTAGE;
    csv = nullptr;
}

DreemLoader::~DreemLoader()
{
    closeCSV();
}

bool
DreemLoader::Detect(const QString & path)
{
    // This is only used for CPAP machines, when detecting CPAP cards.
    qDebug() << "DreemLoader::Detect(" << path << ")";
    return false;
}

int DreemLoader::OpenFile(const QString & filename)
{
    if (!openCSV(filename)) {
        closeCSV();
        return -1;
    }
    int count = 0;
    Session* sess;
    // TODO: add progress bar support, perhaps move shared logic into shared parent class with Zeo loader
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

bool DreemLoader::openCSV(const QString & filename)
{
    file.setFileName(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open Dreem file" << filename;
            return false;
        }
    } else {
        return false;
    }

    QStringList header;
    csv = new CSVReader(file, ";", "#");
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

void DreemLoader::closeCSV()
{
    if (csv != nullptr) {
        delete csv;
        csv = nullptr;
    }
    if (file.isOpen()) {
        file.close();
    }
}



const QStringList s_sleepStageLabels = { "NA", "WAKE", "REM", "Light", "Deep" };

Session* DreemLoader::readNextSession()
{
    if (csv == nullptr) {
        qWarning() << "no CSV open!";
        return nullptr;
    }
    static QHash<const QString,int> s_sleepStages;
    for (int i = 0; i < s_sleepStageLabels.size(); i++) {
        const QString & label = s_sleepStageLabels[i];
        s_sleepStages[label] = i;  // match ZEO sleep stages for now
        // TODO: generalize sleep stage integers between Dreem and Zeo
    }

    Session* sess = nullptr;

    QDateTime start_time, stop_time;
    int sleep_onset;  //, sleep_duration;
    int light_sleep_duration, deep_sleep_duration, rem_duration, awakened_duration;
    int awakenings;  //, position_changes, average_hr, average_rr;
    float sleep_efficiency;
    QStringList hypnogram;

    QHash<QString,QString> row;
    while (csv->readRow(row)) {
        SessionID sid = 0;
        invalid_fields = false;

        start_time = readDateTime(row["Start Time"]);
        if (start_time.isValid()) {
            sid = start_time.toSecsSinceEpoch();
            if (mach->SessionExists(sid)) {
                continue;
            }
        } // else invalid_fields will be true

        // "Type" always seems to be "night"
        stop_time = readDateTime(row["Stop Time"]);
        sleep_onset = readDuration(row["Sleep Onset Duration"]);
        //sleep_duration = readDuration(row["Sleep Duration"]);
        light_sleep_duration = readDuration(row["Light Sleep Duration"]);
        deep_sleep_duration = readDuration(row["Deep Sleep Duration"]);
        rem_duration = readDuration(row["REM Duration"]);
        awakened_duration = readDuration(row["Wake After Sleep Onset Duration"]);
        awakenings = readInt(row["Number of awakenings"]);
        //position_changes = readInt(row["Position Changes"]);
        //average_hr = readInt(row["Mean Heart Rate"]);  // TODO: sometimes "None"
        //average_rr = readInt(row["Mean Respiration CPM"]);
        // "Number of Stimulations" is 0 for US models
        sleep_efficiency = readInt(row["Sleep efficiency"]) / 100.0;
        
        if (invalid_fields) {
            qWarning() << "invalid Dreem row, skipping" << start_time;
            continue;
        }

        QString h = row["Hypnogram"];  // with "[" at the beginning and "]" at the end
        hypnogram = h.mid(1, h.length()-2).split(",");
        if (hypnogram.size() == 0) {
            continue;
        }

        sess = new Session(mach, sid);
        break;
    };

    if (sess) {
        const quint64 step = 30 * 1000;
        m_session = sess;

        // TODO: rename Zeo channels to be generic
        sess->settings[ZEO_Awakenings] = awakenings;
        sess->settings[ZEO_TimeToZ] = sleep_onset / 60;  // TODO: convert durations to seconds and update Zeo loader accordingly, also below
        sess->settings[ZEO_ZQ] = int(sleep_efficiency * 100.0);  // TODO: ZQ may be better expressed as a percent?
        sess->settings[ZEO_TimeInWake] = awakened_duration / 60;
        sess->settings[ZEO_TimeInREM] = rem_duration / 60;
        sess->settings[ZEO_TimeInLight] = light_sleep_duration / 60;
        sess->settings[ZEO_TimeInDeep] = deep_sleep_duration / 60;
        //sess->settings[OXI_Pulse] = average_hr;
        //sess->settings[CPAP_RespRate] = average_rr;
        // Dreem also provides:
        // total sleep duration
        // # position changes

        qint64 st = qint64(start_time.toSecsSinceEpoch()) * 1000L;
        qint64 last = qint64(stop_time.toSecsSinceEpoch()) * 1000L;
        sess->really_set_first(st);

        // It appears that the first sample occurs at start time and
        // the second sample occurs at the next 30-second boundary.
        //
        // TODO: About half the time there are still too many samples?
        qint64 tt = st;
        qint64 second_sample_tt = ((tt + step - 1L) / step) * step;

        for (int i = 0; i < hypnogram.size(); i++) {
            auto & label = hypnogram.at(i);
            if (s_sleepStages.contains(label)) {
                int stage = s_sleepStages[label];

                // It appears that the last sample occurs at the stop time.
                if (tt > last) {
                    if (i != hypnogram.size() - 1) {
                        qWarning() << sess->session() << "more hypnogram samples than time" << tt << last;
                    }
                    tt = last;
                }

                if (stage == 0) {
                    EndEventList(ZEO_SleepStage, tt);
                } else {
                    AddEvent(ZEO_SleepStage, tt, -stage);  // use negative values so that the chart is oriented the right way
                }
            } else {
                qWarning() << sess->session() << start_time << "@" << i << "unknown sleep stage" << label;
            }

            if (i == 0) {
                tt = second_sample_tt;
            } else {
                tt += step;
            }
        }
        EndEventList(ZEO_SleepStage, last);
        sess->really_set_last(last);
    }

    return sess;
}

void DreemLoader::AddEvent(ChannelID channel, qint64 t, EventDataType value)
{
    EventList* C = m_importChannels[channel];
    if (C == nullptr) {
        C = m_session->AddEventList(channel, EVL_Event, 1, 0, -5, 0);
        Q_ASSERT(C);  // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    // Add the event
    C->AddEvent(t, value);
    m_importLastValue[channel] = value;
}

void DreemLoader::EndEventList(ChannelID channel, qint64 t)
{
    EventList* C = m_importChannels[channel];
    if (C != nullptr) {
        C->AddEvent(t, m_importLastValue[channel]);
        
        // Mark this channel's event list as ended.
        m_importChannels[channel] = nullptr;
    }
}

QDateTime DreemLoader::readDateTime(const QString & text)
{
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    if (!dt.isValid()) invalid_fields = true;
    return dt;
}

int DreemLoader::readDuration(const QString & text)
{
    QTime t = QTime::fromString(text, "H:mm:ss");
    if (!t.isValid()) invalid_fields = true;
    return t.msecsSinceStartOfDay() / 1000L;
}

int DreemLoader::readInt(const QString & text)
{
    bool ok;
    int value = text.toInt(&ok);
    if (!ok) invalid_fields = true;
    return value;
}

static bool dreem_initialized = false;

void DreemLoader::Register()
{
    if (dreem_initialized) { return; }

    qDebug("Registering DreemLoader");
    RegisterLoader(new DreemLoader());
    dreem_initialized = true;
}

