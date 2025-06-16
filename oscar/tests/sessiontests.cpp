/* Session Testing Support
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QFile>
#include "sessiontests.h"

static QString ts(qint64 msecs)
{
    // TODO: make this UTC so that tests don't vary by where they're run
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

static QString dur(qint64 msecs)
{
    qint64 s = msecs / 1000L;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

#define ENUMSTRING(ENUM) case ENUM: s = #ENUM; break
static QString eventListTypeName(EventListType t)
{
    QString s;
    switch (t) {
        ENUMSTRING(EVL_Waveform);
        ENUMSTRING(EVL_Event);
        default:
            s = hex(t);
            qDebug() << "EVL" << qPrintable(s);
    }
    return s;
}

// ChannelIDs are not enums. Instead, they are global variables of the ChannelID type.
// This allows definition of different IDs within different loader plugins, while
// using Qt templates (such as QHash) that require a consistent data type for their key.
//
// Ideally there would be a central ChannelID registry class that could be queried
// for names, make sure there aren't duplicate values, etc. For now we just fill the
// below by hand.
#define CHANNELNAME(CH) if (i == CH) { s = #CH; break; }
extern ChannelID PRS1_Mode;
extern ChannelID PRS1_TimedBreath, PRS1_HumidMode, PRS1_TubeTemp;
extern ChannelID PRS1_FlexLock, PRS1_TubeLock, PRS1_RampType;
extern ChannelID PRS1_BackupBreathMode, PRS1_BackupBreathRate, PRS1_BackupBreathTi;
extern ChannelID PRS1_AutoTrial, PRS1_EZStart, PRS1_RiseTime, PRS1_RiseTimeLock;
extern ChannelID PRS1_PeakFlow;
extern ChannelID PRS1_VariableBreathing;

extern ChannelID RMS9_EPR, RMS9_EPRLevel, RMS9_Mode, RMS9_SmartStart, RMS9_HumidStatus, RMS9_HumidLevel,
         RMS9_PtAccess, RMS9_Mask, RMS9_ABFilter, RMS9_ClimateControl, RMS9_TubeType,
         RMS9_Temp, RMS9_TempEnable, RMS9_RampEnable;

static QString settingChannel(ChannelID i)
{
    QString s;
    do {
        CHANNELNAME(CPAP_Mode);
        CHANNELNAME(CPAP_Pressure);
        CHANNELNAME(CPAP_PressureMin);
        CHANNELNAME(CPAP_PressureMax);
        CHANNELNAME(CPAP_EPAP);
        CHANNELNAME(CPAP_IPAP);
        CHANNELNAME(CPAP_PS);
        CHANNELNAME(CPAP_EPAPLo);
        CHANNELNAME(CPAP_EPAPHi);
        CHANNELNAME(CPAP_IPAPLo);
        CHANNELNAME(CPAP_IPAPHi);
        CHANNELNAME(CPAP_PSMin);
        CHANNELNAME(CPAP_PSMax);
        CHANNELNAME(CPAP_RampTime);
        CHANNELNAME(CPAP_RampPressure);
        CHANNELNAME(CPAP_RespRate);
        CHANNELNAME(CPAP_TidalVolume);
        CHANNELNAME(OXI_Pulse);
        // PRS1-specific channels
        CHANNELNAME(PRS1_Mode);
        CHANNELNAME(PRS1_FlexMode);
        CHANNELNAME(PRS1_FlexLevel);
        CHANNELNAME(PRS1_HumidStatus);
        CHANNELNAME(PRS1_HumidMode);
        CHANNELNAME(PRS1_TubeTemp);
        CHANNELNAME(PRS1_HumidLevel);
        CHANNELNAME(PRS1_HumidTargetTime);
        CHANNELNAME(PRS1_MaskResistLock);
        CHANNELNAME(PRS1_MaskResistSet);
        CHANNELNAME(PRS1_TimedBreath);
        CHANNELNAME(PRS1_HoseDiam);
        CHANNELNAME(PRS1_AutoOn);
        CHANNELNAME(PRS1_AutoOff);
        CHANNELNAME(PRS1_MaskAlert);
        CHANNELNAME(PRS1_ShowAHI);
        CHANNELNAME(PRS1_FlexLock);
        CHANNELNAME(PRS1_TubeLock);
        CHANNELNAME(PRS1_RampType);
        CHANNELNAME(PRS1_BackupBreathMode);
        CHANNELNAME(PRS1_BackupBreathRate);
        CHANNELNAME(PRS1_BackupBreathTi);
        CHANNELNAME(PRS1_AutoTrial);
        CHANNELNAME(PRS1_EZStart);
        CHANNELNAME(PRS1_RiseTime);
        CHANNELNAME(PRS1_RiseTimeLock);
        // ZEO-specific channels
        CHANNELNAME(ZEO_Awakenings);
        CHANNELNAME(ZEO_MorningFeel);
        CHANNELNAME(ZEO_TimeInWake);
        CHANNELNAME(ZEO_TimeInREM);
        CHANNELNAME(ZEO_TimeInLight);
        CHANNELNAME(ZEO_TimeInDeep);
        CHANNELNAME(ZEO_TimeToZ);
        CHANNELNAME(ZEO_ZQ);
        // Resmed-specific channels
        CHANNELNAME(RMS9_EPR);
        CHANNELNAME(RMS9_EPRLevel);
        CHANNELNAME(RMS9_Mode);
        CHANNELNAME(RMS9_SmartStart);
        CHANNELNAME(RMS9_HumidStatus);
        CHANNELNAME(RMS9_HumidLevel);
        CHANNELNAME(RMS9_Temp);
        CHANNELNAME(RMS9_TempEnable);
        CHANNELNAME(RMS9_ABFilter);
        CHANNELNAME(RMS9_PtAccess);
        CHANNELNAME(RMS9_ClimateControl);
        CHANNELNAME(RMS9_Mask);
        CHANNELNAME(RMS9_RampEnable);
        s = hex(i);
        qDebug() << "setting channel" << qPrintable(s);
    } while(false);
    return s;
}

static QString eventChannel(ChannelID i)
{
    QString s;
    do {
        CHANNELNAME(CPAP_Obstructive);
        CHANNELNAME(CPAP_AllApnea);
        CHANNELNAME(CPAP_Hypopnea);
        CHANNELNAME(CPAP_PB);
        CHANNELNAME(CPAP_LeakTotal);
        CHANNELNAME(CPAP_Leak);
        CHANNELNAME(CPAP_LargeLeak);
        CHANNELNAME(CPAP_IPAP);
        CHANNELNAME(CPAP_EPAP);
        CHANNELNAME(CPAP_PS);
        CHANNELNAME(CPAP_IPAPLo);
        CHANNELNAME(CPAP_IPAPHi);
        CHANNELNAME(CPAP_RespRate);
        CHANNELNAME(CPAP_PTB);
        CHANNELNAME(PRS1_TimedBreath);
        CHANNELNAME(PRS1_PeakFlow);
        CHANNELNAME(CPAP_MinuteVent);
        CHANNELNAME(CPAP_SteadyBreathing);
        CHANNELNAME(CPAP_TidalVolume);
        CHANNELNAME(CPAP_ClearAirway);
        CHANNELNAME(CPAP_FlowLimit);
        CHANNELNAME(CPAP_Snore);
        CHANNELNAME(CPAP_VSnore);
        CHANNELNAME(CPAP_VSnore2);
        CHANNELNAME(CPAP_NRI);
        CHANNELNAME(CPAP_RERA);
        CHANNELNAME(OXI_Pulse);
        CHANNELNAME(OXI_SPO2);
        CHANNELNAME(PRS1_BND);
        CHANNELNAME(CPAP_MaskPressureHi);
        CHANNELNAME(CPAP_FlowRate);
        CHANNELNAME(CPAP_Test1);
        CHANNELNAME(CPAP_Test2);
        CHANNELNAME(CPAP_PressurePulse);
        CHANNELNAME(CPAP_Pressure);
        CHANNELNAME(PRS1_VariableBreathing);
        CHANNELNAME(CPAP_PressureSet);
        CHANNELNAME(CPAP_IPAPSet);
        CHANNELNAME(CPAP_EPAPSet);
        CHANNELNAME(POS_Movement);
        CHANNELNAME(ZEO_SleepStage);
        // Resmed-specific channels
        CHANNELNAME(CPAP_Apnea);
        CHANNELNAME(CPAP_MaskPressure);
        CHANNELNAME(CPAP_Te);
        CHANNELNAME(CPAP_Ti);
        CHANNELNAME(CPAP_IE);
        CHANNELNAME(CPAP_FLG);
        CHANNELNAME(CPAP_AHI);
        CHANNELNAME(CPAP_TgMV);
        // Calculated channels
        CHANNELNAME(CPAP_RDI);
        s = hex(i);
        qDebug() << "event channel" << qPrintable(s);
    } while(false);
    return s;
}

static QString intList(EventStoreType* data, int count, int limit=-1)
{
    if (limit == -1 || limit > count) limit = count;
    int first = limit / 2;
    int last = limit - first;
    QStringList l;
    for (int i = 0; i < first; i++) {
        l.push_back(QString::number(data[i]));
    }
    if (limit < count) l.push_back("...");
    for (int i = count - last; i < count; i++) {
        l.push_back(QString::number(data[i]));
    }
    QString s = "[ " + l.join(",") + " ]";
    return s;
}

static QString intList(quint32* data, int count, int limit=-1)
{
    if (limit == -1 || limit > count) limit = count;
    int first = limit / 2;
    int last = limit - first;
    QStringList l;
    for (int i = 0; i < first; i++) {
        l.push_back(QString::number(data[i] / 1000));
    }
    if (limit < count) l.push_back("...");
    for (int i = count - last; i < count; i++) {
        l.push_back(QString::number(data[i] / 1000));
    }
    QString s = "[ " + l.join(",") + " ]";
    return s;
}

void SessionToYaml(QString filepath, Session* session, bool ok)
{
    QFile file(filepath);
    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        qDebug() << filepath;
        Q_ASSERT(false);
    }
    QTextStream out(&file);

    out << "session:" << '\n';
    out << "  id: " << session->session() << '\n';
    out << "  start: " << ts(session->first()) << '\n';
    out << "  end: " << ts(session->last()) << '\n';
    out << "  valid: " << ok << '\n';
    
    if (!session->m_slices.isEmpty()) {
        out << "  slices:" << '\n';
        for (auto & slice : session->m_slices) {
            QString s;
            switch (slice.status) {
            case MaskOn: s = "mask on"; break;
            case MaskOff: s = "mask off"; break;
            case EquipmentOff: s = "equipment off"; break;
            default: s = "unknown"; break;
            }
            out << "  - status: " << s << '\n';
            out << "    start: " << ts(slice.start) << '\n';
            out << "    end: " << ts(slice.end) << '\n';
        }
    }
    qint64 total_time = 0;
    if (session->first() != 0) {
        Day day;
        day.addSession(session);
        total_time = day.total_time();
        day.removeSession(session);
    }
    out << "  total_time: " << dur(total_time) << '\n';

    out << "  settings:" << '\n';

    // We can't get deterministic ordering from QHash iterators, so we need to create a list
    // of sorted ChannelIDs.
    QList<ChannelID> keys = session->settings.keys();
    std::sort(keys.begin(), keys.end());
    for (QList<ChannelID>::iterator key = keys.begin(); key != keys.end(); key++) {
        QVariant & value = session->settings[*key];
        QString s;
        #if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            if (value.typeId() == qMetaTypeId<float>())
        #else
            if ((QMetaType::Type) value.type() == QMetaType::Float)
        #endif
        {
            s = QString::number(value.toFloat());  // Print the shortest accurate representation rather than QVariant's full precision.
        } else {
            s = value.toString();
        }
        out << "    " << settingChannel(*key) << ": " << s << '\n';
    }

    out << "  events:" << '\n';

    keys = session->eventlist.keys();
    std::sort(keys.begin(), keys.end());
    for (QList<ChannelID>::iterator key = keys.begin(); key != keys.end(); key++) {
        out << "    " << eventChannel(*key) << ": " << '\n';

        // Note that this is a vector of lists
        QVector<EventList *> &ev = session->eventlist[*key];
        int ev_size = ev.size();
        if (ev_size == 0) {
            continue;
        }
        EventList &e = *ev[0];
        
        // Multiple eventlists in a channel are used to account for discontiguous data.
        // See CoalesceWaveformChunks for the coalescing of multiple contiguous waveform
        // chunks and ParseWaveforms/ParseOximetry for the creation of eventlists per
        // coalesced chunk.
        //
        // This can also be used for other discontiguous data, such as PRS1 statistics
        // that are omitted when breathing is not detected.

        for (int j = 0; j < ev_size; j++) {
            e = *ev[j];
            out << "    - count: "  << (qint32)e.count() << '\n';
            if (e.count() == 0)
                continue;
            out << "      first: " << ts(e.first()) << '\n';
            out << "      last: " << ts(e.last()) << '\n';
            out << "      type: " << eventListTypeName(e.type()) << '\n';
            out << "      rate: " << e.rate() << '\n';
            out << "      gain: " << e.gain() << '\n';
            out << "      offset: " << e.offset() << '\n';
            if (!e.dimension().isEmpty()) {
                out << "      dimension: " << e.dimension() << '\n';
            }
            out << "      data:" << '\n';
            out << "        min: " << e.Min() << '\n';
            out << "        max: " << e.Max() << '\n';
            out << "        raw: " << intList((EventStoreType*) e.getData().data(), e.count(), 100) << '\n';
            if (e.type() != EVL_Waveform) {
                out << "        delta: " << intList((quint32*) e.getTime().data(), e.count(), 100) << '\n';
            }
            if (e.hasSecondField()) {
                out << "      data2:" << '\n';
                out << "        min: " << e.min2() << '\n';
                out << "        max: " << e.max2() << '\n';
                out << "        raw: " << intList((EventStoreType*) e.getData2().data(), e.count(), 100) << '\n';
            }
        }
    }
    file.close();
}
