/* SleepLib PRS1 Loader Parser Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#include "prs1_parser.h"
#include "prs1_loader.h"
#include "rawdata.h"

// The qt5.15 obsolescence of hex requires this change.
// this solution to QT's obsolescence is only used in debug statements
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    #define QTHEX     Qt::hex
    #define QTDEC     Qt::dec
#else
    #define QTHEX     hex
    #define QTDEC     dec
#endif


const PRS1ParsedEventType PRS1TidalVolumeEvent::TYPE;
const PRS1ParsedEventType PRS1SnoresAtPressureEvent::TYPE;

const PRS1ParsedEventType PRS1TimedBreathEvent::TYPE;
const PRS1ParsedEventType PRS1ObstructiveApneaEvent::TYPE;
const PRS1ParsedEventType PRS1ClearAirwayEvent::TYPE;
const PRS1ParsedEventType PRS1FlowLimitationEvent::TYPE;
const PRS1ParsedEventType PRS1PeriodicBreathingEvent::TYPE;
const PRS1ParsedEventType PRS1LargeLeakEvent::TYPE;
const PRS1ParsedEventType PRS1VariableBreathingEvent::TYPE;
const PRS1ParsedEventType PRS1HypopneaEvent::TYPE;
const PRS1ParsedEventType PRS1TotalLeakEvent::TYPE;
const PRS1ParsedEventType PRS1LeakEvent::TYPE;
const PRS1ParsedEventType PRS1AutoPressureSetEvent::TYPE;
const PRS1ParsedEventType PRS1PressureSetEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPSetEvent::TYPE;
const PRS1ParsedEventType PRS1EPAPSetEvent::TYPE;
const PRS1ParsedEventType PRS1PressureAverageEvent::TYPE;
const PRS1ParsedEventType PRS1FlexPressureAverageEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPAverageEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPHighEvent::TYPE;
const PRS1ParsedEventType PRS1IPAPLowEvent::TYPE;
const PRS1ParsedEventType PRS1EPAPAverageEvent::TYPE;
const PRS1ParsedEventType PRS1RespiratoryRateEvent::TYPE;
const PRS1ParsedEventType PRS1PatientTriggeredBreathsEvent::TYPE;
const PRS1ParsedEventType PRS1MinuteVentilationEvent::TYPE;
const PRS1ParsedEventType PRS1SnoreEvent::TYPE;
const PRS1ParsedEventType PRS1VibratorySnoreEvent::TYPE;
const PRS1ParsedEventType PRS1PressurePulseEvent::TYPE;
const PRS1ParsedEventType PRS1RERAEvent::TYPE;
const PRS1ParsedEventType PRS1FlowRateEvent::TYPE;
const PRS1ParsedEventType PRS1Test1Event::TYPE;
const PRS1ParsedEventType PRS1Test2Event::TYPE;
const PRS1ParsedEventType PRS1HypopneaCount::TYPE;
const PRS1ParsedEventType PRS1ClearAirwayCount::TYPE;
const PRS1ParsedEventType PRS1ObstructiveApneaCount::TYPE;
//const PRS1ParsedEventType PRS1DisconnectAlarmEvent::TYPE;
const PRS1ParsedEventType PRS1ApneaAlarmEvent::TYPE;
//const PRS1ParsedEventType PRS1LowMinuteVentilationAlarmEvent::TYPE;


//********************************************************************************************
// MARK: Render parsed events as text

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

#define ENUMSTRING(ENUM) case ENUM: s = QStringLiteral(#ENUM); break
QString PRS1ParsedEvent::typeName() const
{
    PRS1ParsedEventType t = m_type;
    QString s;
    switch (t) {
        ENUMSTRING(EV_PRS1_RAW);
        ENUMSTRING(EV_PRS1_UNKNOWN);
        ENUMSTRING(EV_PRS1_TB);
        ENUMSTRING(EV_PRS1_OA);
        ENUMSTRING(EV_PRS1_CA);
        ENUMSTRING(EV_PRS1_FL);
        ENUMSTRING(EV_PRS1_PB);
        ENUMSTRING(EV_PRS1_LL);
        ENUMSTRING(EV_PRS1_VB);
        ENUMSTRING(EV_PRS1_HY);
        ENUMSTRING(EV_PRS1_OA_COUNT);
        ENUMSTRING(EV_PRS1_CA_COUNT);
        ENUMSTRING(EV_PRS1_HY_COUNT);
        ENUMSTRING(EV_PRS1_TOTLEAK);
        ENUMSTRING(EV_PRS1_LEAK);
        ENUMSTRING(EV_PRS1_AUTO_PRESSURE_SET);
        ENUMSTRING(EV_PRS1_PRESSURE_SET);
        ENUMSTRING(EV_PRS1_IPAP_SET);
        ENUMSTRING(EV_PRS1_EPAP_SET);
        ENUMSTRING(EV_PRS1_PRESSURE_AVG);
        ENUMSTRING(EV_PRS1_FLEX_PRESSURE_AVG);
        ENUMSTRING(EV_PRS1_IPAP_AVG);
        ENUMSTRING(EV_PRS1_IPAPLOW);
        ENUMSTRING(EV_PRS1_IPAPHIGH);
        ENUMSTRING(EV_PRS1_EPAP_AVG);
        ENUMSTRING(EV_PRS1_RR);
        ENUMSTRING(EV_PRS1_PTB);
        ENUMSTRING(EV_PRS1_MV);
        ENUMSTRING(EV_PRS1_TV);
        ENUMSTRING(EV_PRS1_SNORE);
        ENUMSTRING(EV_PRS1_VS);
        ENUMSTRING(EV_PRS1_PP);
        ENUMSTRING(EV_PRS1_RERA);
        ENUMSTRING(EV_PRS1_FLOWRATE);
        ENUMSTRING(EV_PRS1_TEST1);
        ENUMSTRING(EV_PRS1_TEST2);
        ENUMSTRING(EV_PRS1_SETTING);
        ENUMSTRING(EV_PRS1_SLICE);
        ENUMSTRING(EV_PRS1_DISCONNECT_ALARM);
        ENUMSTRING(EV_PRS1_APNEA_ALARM);
        ENUMSTRING(EV_PRS1_LOW_MV_ALARM);
        ENUMSTRING(EV_PRS1_SNORES_AT_PRESSURE);
        ENUMSTRING(EV_PRS1_INTERVAL_BOUNDARY);
        default:
            s = hex(t);
            qDebug() << "Unknown PRS1ParsedEventType type:" << qPrintable(s);
            return s;
    }
    return s.mid(8).toLower();  // lop off initial EV_PRS1_
}

QString PRS1ParsedSettingEvent::settingName() const
{
    PRS1ParsedSettingType t = m_setting;
    QString s;
    switch (t) {
        ENUMSTRING(PRS1_SETTING_CPAP_MODE);
        ENUMSTRING(PRS1_SETTING_AUTO_TRIAL);
        ENUMSTRING(PRS1_SETTING_PRESSURE);
        ENUMSTRING(PRS1_SETTING_PRESSURE_MIN);
        ENUMSTRING(PRS1_SETTING_PRESSURE_MAX);
        ENUMSTRING(PRS1_SETTING_EPAP);
        ENUMSTRING(PRS1_SETTING_EPAP_MIN);
        ENUMSTRING(PRS1_SETTING_EPAP_MAX);
        ENUMSTRING(PRS1_SETTING_IPAP);
        ENUMSTRING(PRS1_SETTING_IPAP_MIN);
        ENUMSTRING(PRS1_SETTING_IPAP_MAX);
        ENUMSTRING(PRS1_SETTING_PS);
        ENUMSTRING(PRS1_SETTING_PS_MIN);
        ENUMSTRING(PRS1_SETTING_PS_MAX);
        ENUMSTRING(PRS1_SETTING_BACKUP_BREATH_MODE);
        ENUMSTRING(PRS1_SETTING_BACKUP_BREATH_RATE);
        ENUMSTRING(PRS1_SETTING_BACKUP_TIMED_INSPIRATION);
        ENUMSTRING(PRS1_SETTING_TIDAL_VOLUME);
        ENUMSTRING(PRS1_SETTING_EZ_START);
        ENUMSTRING(PRS1_SETTING_FLEX_LOCK);
        ENUMSTRING(PRS1_SETTING_FLEX_MODE);
        ENUMSTRING(PRS1_SETTING_FLEX_LEVEL);
        ENUMSTRING(PRS1_SETTING_RISE_TIME);
        ENUMSTRING(PRS1_SETTING_RISE_TIME_LOCK);
        ENUMSTRING(PRS1_SETTING_RAMP_TYPE);
        ENUMSTRING(PRS1_SETTING_RAMP_TIME);
        ENUMSTRING(PRS1_SETTING_RAMP_PRESSURE);
        ENUMSTRING(PRS1_SETTING_HUMID_STATUS);
        ENUMSTRING(PRS1_SETTING_HUMID_MODE);
        ENUMSTRING(PRS1_SETTING_HEATED_TUBE_TEMP);
        ENUMSTRING(PRS1_SETTING_HUMID_LEVEL);
        ENUMSTRING(PRS1_SETTING_HUMID_TARGET_TIME);
        ENUMSTRING(PRS1_SETTING_MASK_RESIST_LOCK);
        ENUMSTRING(PRS1_SETTING_MASK_RESIST_SETTING);
        ENUMSTRING(PRS1_SETTING_HOSE_DIAMETER);
        ENUMSTRING(PRS1_SETTING_TUBING_LOCK);
        ENUMSTRING(PRS1_SETTING_AUTO_ON);
        ENUMSTRING(PRS1_SETTING_AUTO_OFF);
        ENUMSTRING(PRS1_SETTING_APNEA_ALARM);
        ENUMSTRING(PRS1_SETTING_DISCONNECT_ALARM);
        ENUMSTRING(PRS1_SETTING_LOW_MV_ALARM);
        ENUMSTRING(PRS1_SETTING_LOW_TV_ALARM);
        ENUMSTRING(PRS1_SETTING_MASK_ALERT);
        ENUMSTRING(PRS1_SETTING_SHOW_AHI);
        default:
            s = hex(t);
            qDebug() << "Unknown PRS1ParsedSettingType type:" << qPrintable(s);
            return s;
    }
    return s.mid(13).toLower();  // lop off initial PRS1_SETTING_
}

QString PRS1ParsedSettingEvent::modeName() const
{
    int m = value();
    QString s;
    switch ((PRS1Mode) m) {
        ENUMSTRING(PRS1_MODE_UNKNOWN);  // TODO: Remove this when all the parsers are complete.
        ENUMSTRING(PRS1_MODE_CPAP);
        ENUMSTRING(PRS1_MODE_CPAPCHECK);
        ENUMSTRING(PRS1_MODE_AUTOTRIAL);
        ENUMSTRING(PRS1_MODE_AUTOCPAP);
        ENUMSTRING(PRS1_MODE_BILEVEL);
        ENUMSTRING(PRS1_MODE_AUTOBILEVEL);
        ENUMSTRING(PRS1_MODE_ASV);
        ENUMSTRING(PRS1_MODE_S);
        ENUMSTRING(PRS1_MODE_ST);
        ENUMSTRING(PRS1_MODE_PC);
        ENUMSTRING(PRS1_MODE_ST_AVAPS);
        ENUMSTRING(PRS1_MODE_PC_AVAPS);
        default:
            s = hex(m);
            qDebug() << "Unknown PRS1Mode:" << qPrintable(s);
            return s;
    }
    return s.mid(10).toLower();  // lop off initial PRS1_MODE_
}

QString PRS1ParsedEvent::timeStr(int t)
{
    int h = t / 3600;
    int m = (t - (h * 3600)) / 60;
    int s = t % 60;
#if 1
    // Optimized after profiling regression tests.
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
#else
    // Unoptimized original, slows down regression tests.
    return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
#endif
}

static QString byteList(QByteArray data, int limit=-1)
{
    int count = data.size();
    if (limit == -1 || limit > count) limit = count;
    QStringList l;
    for (int i = 0; i < limit; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    if (limit < count) l.push_back("...");
    QString s = l.join(" ");
    return s;
}


QMap<QString,QString> PRS1IntervalBoundaryEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    return out;
}


QMap<QString,QString> PRS1ParsedDurationEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out["duration"] = timeStr(m_duration);
    return out;
}


QMap<QString,QString> PRS1ParsedValueEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out["value"] = QString::number(value());
    return out;
}


QMap<QString,QString> PRS1UnknownDataEvent::contents(void)
{
    QMap<QString,QString> out;
    out["pos"] = QString::number(m_pos);
    out["data"] = byteList(m_data);
    return out;
}


QMap<QString,QString> PRS1ParsedSettingEvent::contents(void)
{
    QMap<QString,QString> out;
    QString v;
    if (m_setting == PRS1_SETTING_CPAP_MODE) {
        v = modeName();
    } else {
        v = QString::number(value());
    }
    out[settingName()] = v;
    return out;
}


QMap<QString,QString> PRS1ParsedSliceEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    QString s;
    switch ((SliceStatus) m_value) {
        case MaskOn: s = "MaskOn"; break;
        case MaskOff: s = "MaskOff"; break;
        case EquipmentOff: s = "EquipmentOff"; break;
        case UnknownStatus: s = "Unknown"; break;
    }
    out["status"] = s;
    return out;
}


QMap<QString,QString> PRS1ParsedAlarmEvent::contents(void)
{
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    return out;
}


QMap<QString,QString> PRS1SnoresAtPressureEvent::contents(void)
{
    QString label;
    switch (m_kind) {
        case 0: label = "pressure"; break;
        case 1: label = "epap"; break;
        case 2: label = "ipap"; break;
        default: label = "unknown_pressure"; break;
    }
    
    QMap<QString,QString> out;
    out["start"] = timeStr(m_start);
    out[label] = QString::number(value());
    out["count"] = QString::number(m_count);
    return out;
}


//********************************************************************************************
// MARK: -
// MARK: Parse chunk contents

bool PRS1DataChunk::ParseCompliance(void)
{
    switch (this->family) {
    case 0:
        switch (this->familyVersion) {
        case 2:
        case 3:
            return this->ParseComplianceF0V23();
        case 4:
            return this->ParseComplianceF0V4();
        case 5:
            return this->ParseComplianceF0V5();
        case 6:
            return this->ParseComplianceF0V6();
        }
    default:
        ;
    }

    qWarning() << "unexpected compliance family" << this->family << "familyVersion" << this->familyVersion;
    return false;
}


bool PRS1DataChunk::ParseSummary()
{
    switch (this->family) {
    case 0:
        if (this->familyVersion == 6) {
            return this->ParseSummaryF0V6();
        } else if (this->familyVersion == 4) {
            return this->ParseSummaryF0V4();
        } else {
            return this->ParseSummaryF0V23();
        }
    case 3:
        switch (this->familyVersion) {
            case 0: return this->ParseSummaryF3V03();
            case 3: return this->ParseSummaryF3V03();
            case 6: return this->ParseSummaryF3V6();
        }
        break;
    case 5:
        if (this->familyVersion == 1) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 0) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 2) {
            return this->ParseSummaryF5V012();
        } else if (this->familyVersion == 3) {
            return this->ParseSummaryF5V3();
        }
    default:
        ;
    }

    qWarning() << "unexpected family" << this->family << "familyVersion" << this->familyVersion;
    return false;
}


// TODO: The nested switch statement below just begs for per-version subclasses.
bool PRS1DataChunk::ParseEvents()
{
    bool ok = false;
    switch (this->family) {
        case 0:
            switch (this->familyVersion) {
                case 2: ok = this->ParseEventsF0V23(); break;
                case 3: ok = this->ParseEventsF0V23(); break;
                case 4: ok = this->ParseEventsF0V4(); break;
                case 6: ok = this->ParseEventsF0V6(); break;
            }
            break;
        case 3:
            switch (this->familyVersion) {
                case 0: ok = this->ParseEventsF3V03(); break;
                case 3: ok = this->ParseEventsF3V03(); break;
                case 6: ok = this->ParseEventsF3V6(); break;
            }
            break;
        case 5:
            switch (this->familyVersion) {
                case 0: ok = this->ParseEventsF5V0(); break;
                case 1: ok = this->ParseEventsF5V1(); break;
                case 2: ok = this->ParseEventsF5V2(); break;
                case 3: ok = this->ParseEventsF5V3(); break;
            }
            break;
        default:
            qDebug() << "Unknown PRS1 family" << this->family << "familyVersion" << this->familyVersion;
    }
    return ok;
}


// TODO: This really should be in some kind of class hierarchy, once we figure out
// the right one.
const QVector<PRS1ParsedEventType> & GetSupportedEvents(const PRS1DataChunk* chunk)
{
    static const QVector<PRS1ParsedEventType> none;
    
    switch (chunk->family) {
        case 0:
            switch (chunk->familyVersion) {
                case 2: return ParsedEventsF0V23; break;
                case 3: return ParsedEventsF0V23; break;
                case 4: return ParsedEventsF0V4; break;
                case 6: return ParsedEventsF0V6; break;
            }
            break;
        case 3:
            switch (chunk->familyVersion) {
                case 0: return ParsedEventsF3V0; break;
                case 3: return ParsedEventsF3V3; break;
                case 6: return ParsedEventsF3V6; break;
            }
            break;
        case 5:
            switch (chunk->familyVersion) {
                case 0: return ParsedEventsF5V0; break;
                case 1: return ParsedEventsF5V1; break;
                case 2: return ParsedEventsF5V2; break;
                case 3: return ParsedEventsF5V3; break;
            }
            break;
    }
    qWarning() << "Missing supported event list for family" << chunk->family << "version" << chunk->familyVersion;
    return none;
}


QString PRS1DataChunk::DumpEvent(int t, int code, const unsigned char* data, int size)
{
    int s = t;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    QString dump = QString("%1:%2:%3 ")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
    dump = dump + " " + hex(code) + ":";
    for (int i = 0; i < size; i++) {
        dump = dump + QString(" %1").arg(data[i]);
    }
    return dump;
}


void PRS1DataChunk::AddEvent(PRS1ParsedEvent* const event)
{
    m_parsedData.push_back(event);
}



//********************************************************************************************
// MARK: -
// MARK: Parse settings shared by multiple families

// Humid F0V2 confirmed
// 0x00 = Off (presumably no humidifier present)
// 0x80 = Off
// 0x81 = 1
// 0x82 = 2
// 0x83 = 3
// 0x84 = 4
// 0x85 = 5

// Humid F3V0 confirmed
// 0x03 = 3 (but no humidification shown on hours of usage chart)
// 0x04 = 4 (but no humidification shown on hours of usage chart)
// 0x80 = Off
// 0x81 = 1
// 0x82 = 2
// 0x83 = 3
// 0x84 = 4
// 0x85 = 5

// Humid F5V0 confirmed
// 0x00 = Off (presumably no humidifier present)
// 0x80 = Off
// 0x81 = 1, bypass = no
// 0x82 = 2, bypass = no
// 0x83 = 3, bypass = no
// 0x84 = 4, bypass = no
// 0x85 = 5, bypass = no
// 0xA0 = Off, bypass = yes

void PRS1DataChunk::ParseHumidifierSetting50Series(int humid, bool add_setting)
{
    if (humid & (0x40 | 0x10 | 0x08)) UNEXPECTED_VALUE(humid, "known bits");
    if (humid & 0x20) {
        if (this->family == 5) {
            CHECK_VALUE(humid, 0xA0);  // only example of bypass set, unsure whether it can appear otherwise
        } else {
            CHECK_VALUE(humid & 0x20, 0);  // only ever seen on 950P, where "Bypass System One humidification" is "Yes"
        }
    }
    
    bool humidifier_present = ((humid & 0x80) != 0);  // humidifier connected
    int humidlevel = humid & 7;  // humidification level

    HumidMode humidmode = HUMID_Fixed;  // 50-Series didn't have adaptive or heated tube humidification
    if (add_setting) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
        }
    }

    // Check for truly unexpected values:
    if (humidlevel > 5) UNEXPECTED_VALUE(humidlevel, "<= 5");
    //if (!humidifier_present) CHECK_VALUES(humidlevel, 0, 1);  // Some devices appear to encode the humidlevel setting even when the humidifier is not present.
}




// F0V4 confirmed:
// B3 0A = HT=5, H=3, HT
// A3 0A = HT=5, H=2, HT
// 33 0A = HT=4, H=3, HT
// 23 4A = HT=4, H=2, HT
// B3 09 = HT=3, H=3, HT
// A4 09 = HT=3, H=2, HT
// A3 49 = HT=3, H=2, HT
// 22 09 = HT=2, H=2, HT
// 33 09 = HT=2, H=3, HT
// 21 09 = HT=2, H=2, HT
// 13 09 = HT=2, H=1, HT
// B5 08 = HT=1, H=3, HT
// 03 08 = HT=off,    HT; data=tube t=0,h=0
// 05 24 =       H=5, S1
// 95 06 =       H=5, S1
// 95 05 =       H=5, S1
// 94 05 =       H=4, S1
// 04 24 =       H=4, S1
// A3 05 =       H=3, S1
// 92 05 =       H=2, S1
// A2 05 =       H=2, S1
// 01 24 =       H=1, S1
// 90 05 =       H=off, S1
// 30 05 =       H=off, S1
// 95 41 =       H=5, Classic
// A4 61 =       H=4, Classic
// A3 61 =       H=3, Classic
// A2 61 =       H=2, Classic
// A1 61 =       H=1, Classic
// 90 41 =       H=Off, Classic; data=classic h=0
// 94 11 =       H=3, S1, no data [note that bits encode H=4, so no data falls back to H=3]
// 93 11 =       H=3, S1, no data
// 04 30 =       H=3, S1, no data

// F0V5 confirmed:
// 00 60 =       H=Off, Classic
// 02 60 =       H=2, Classic
// 05 60 =       H=5, Classic
// 00 70 =       H=Off, no data in chart

// F5V1 confirmed:
// A0 4A = HT=5, H=2, HT
// B1 09 = HT=3, H=3, HT
// 91 09 = HT=3, H=1, HT
// 32 09 = HT=2, H=3, HT
// B2 08 = HT=1, H=3, HT
// 00 48 = HT=off, data=tube t=0,h=0
// 95 05 =       H=5, S1
// 94 05 =       H=4, S1
// 93 05 =       H=3, S1
// 92 05 =       H=2, S1
// 91 05 =       H=1, S1
// 90 05 =       H=Off, S1
// 95 41 =       H=5, Classic
// 94 41 =       H=4, Classic
// 93 41 =       H=3, Classic
// 92 41 =       H=2, Classic
// 01 60 =       H=1, Classic
// 00 60 =       H=Off, Classic
// 00 70 =       H=3, S1, no data [no data ignores Classic mode, H bits, falls back to S1 H=3]

// F5V2 confirmed:
// 00 48 = HT=off, data=tube t=0,h=0
// 93 09 = HT=3, H=1, HT
// 00 10 =       H=3, S1, no data

// XX XX = 60-Series Humidifier bytes
//  7    = humidity level without tube [on tube disconnect / system one with 22mm hose / classic]  :  0 = humidifier off
//  8    = [never seen]
// 3     = humidity level with tube
// 4     = maybe part of humidity level? [never seen]
// 8   3 = tube temperature (high bit of humid 1 is low bit of temp)
//     4 = "System One" mode (valid even when humidifier is off)
//     8 = heated tube present
//    10 = no data in chart, maybe no humidifier attached? Seems to fall back on System One = 3 despite other (humidity level and S1) bits.
//    20 = unknown, something tube related since whenever it's set tubepresent is false
//    40 = "Classic" mode (valid even when humidifier is off, ignored when heated tube is present)
//    80 = [never seen]

void PRS1DataChunk::ParseHumidifierSetting60Series(unsigned char humid1, unsigned char humid2, bool add_setting)
{
    int humidlevel = humid1 & 7;  // Ignored when heated tube is present: humidifier setting on tube disconnect is always reported as 3
    if (humidlevel > 5) UNEXPECTED_VALUE(humidlevel, "<= 5");
    CHECK_VALUE(humid1 & 8, 0);  // never seen
    int tubehumidlevel = (humid1 >> 4) & 7;  // This mask is a best guess based on other masks.
    if (tubehumidlevel > 5) UNEXPECTED_VALUE(tubehumidlevel, "<= 5");
    CHECK_VALUE(tubehumidlevel & 4, 0);  // never seen, but would clarify whether above mask is correct

    int tubetemp = (humid1 >> 7) | ((humid2 & 3) << 1);
    if (tubetemp > 5) UNEXPECTED_VALUE(tubetemp, "<= 5");

    CHECK_VALUE(humid2 & 0x80, 0);  // never seen
    bool humidclassic = (humid2 & 0x40) != 0;  // Set on classic mode reports; evidently ignored (sometimes set!) when tube is present
    //bool no_tube? = (humid2 & 0x20) != 0;  // Something tube related: whenever it is set, tube is never present (inverse is not true)
    bool no_data = (humid2 & 0x10) != 0;  // As described in chart, settings still show up
    int tubepresent = (humid2 & 0x08) != 0;
    bool humidsystemone = (humid2 & 0x04) != 0;  // Set on "System One" humidification mode reports when tubepresent is false
    if (humidsystemone && tubepresent) {
        // On a 560P, we've observed a spurious tubepresent bit being set during two sessions.
        // Those sessions (and the ones that followed) used a 22mm hose.
        CHECK_VALUE(add_setting, false);  // We've only seen this appear during a session, not in the initial settings.
        tubepresent = false;
    }

    // When no_data, reports always say "System One" with humidity level 3, regardless of humidlevel and humidsystemone

    if (humidsystemone + tubepresent + no_data == 0) CHECK_VALUE(humidclassic, true);  // Always set when everything else is off
    if (no_data && humidsystemone && add_setting == false) {
        // This has been seen once on a 560P in a session that also generated a file in the error directory.
        qWarning() << this->sessionid << "Humidification error during session?";
    } else {
        if (humidsystemone + tubepresent + no_data > 1) UNEXPECTED_VALUE(humid2, "one bit set");  // Only one of these ever seems to be set at a time
    }
    if (tubepresent && tubetemp == 0) CHECK_VALUE(tubehumidlevel, 0);  // When the heated tube is off, tube humidity seems to be 0
    
    if (tubepresent) humidclassic = false;  // Classic mode bit is evidently ignored when tube is present
    if (no_data) humidclassic = false;  // Classic mode bit is evidently ignored when tube is present

    //qWarning() << this->sessionid << (humidclassic ? "C" : ".") << (humid2 & 0x20 ? "?" : ".") << (tubepresent ? "T" : ".") << (no_data ? "X" : ".") << (humidsystemone ? "1" : ".");
    /*
    if (tubepresent) {
        if (tubetemp) {
            qWarning() << this->sessionid << "tube temp" << tubetemp << "tube humidity" << tubehumidlevel << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        } else {
            qWarning() << this->sessionid << "heated tube off" << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        }
    } else {
        qWarning() << this->sessionid << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
    }
    */
    HumidMode humidmode = HUMID_Fixed;
    if (tubepresent) {
        humidmode = HUMID_HeatedTube;
    } else {
        if (humidsystemone + humidclassic > 1) UNEXPECTED_VALUE(humid2, "fixed or adaptive");
        if (humidsystemone) humidmode = HUMID_Adaptive;
    }

    if (add_setting) {
        bool humidifier_present = (no_data == 0);
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            if (humidmode == HUMID_HeatedTube) {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HEATED_TUBE_TEMP, tubetemp));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, tubehumidlevel));
            } else {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
            }
        }
    }

    // Check for previously unseen data that we expect to be normal:
    if (this->family == 0) {
        // F0V4
        if (tubetemp && (tubehumidlevel < 1 || tubehumidlevel > 3)) UNEXPECTED_VALUE(tubehumidlevel, "1-3");
    } else if (this->familyVersion == 1) {
        // F5V1
        if (tubepresent) {
            // all tube temperatures seen
            if (tubetemp) {
                if (tubehumidlevel == 0 || tubehumidlevel > 3) UNEXPECTED_VALUE(tubehumidlevel, "1-3");
            }
        }
    } else if (this->familyVersion == 2) {
        // F5V2
        if (tubepresent) {
            // all tube temperatures seen
            if (tubetemp) {
                CHECK_VALUES(tubehumidlevel, 1, 3);
            }
        }
        CHECK_VALUE(humidclassic, false);
    }
}


// F0V6 confirmed
// 90 B0 = HT=3!,H=3!,data=none [no humidifier appears to ignore HT and H bits and show HT=3,H=3 in details]
// 8C 6C = HT=3, H=3, data=none
// 80 00 = nothing listed in details, data=none, only seen on 400G and 502G
// 54 B4 = HT=5, H=5, data=tube
// 50 90 = HT=4, H=4, data=tube
// 4C 6C = HT=3, H=3, data=tube
// 48 68 = HT=3, H=2, data=tube
// 40 60 = HT=3, H=Off, data=tube t=3,h=0
// 50 50 = HT=2, H=4, data=tube
// 4C 4C = HT=2, H=3, data=tube
// 50 30 = HT=1, H=4, data=tube
// 4C 0C = HT=off, H=3, data=tube t=0,h=3
// 34 74 = HT=3, H=5, data=adaptive (5)
// 50 B0 = HT=5, H=4, adaptive
// 30 B0 = HT=3, H=4, data=adaptive (4)
// 30 50 = HT=3, H=4, data=adaptive (4)
// 30 10 = HT=3!,H=4, data=adaptive (4) [adaptive mode appears to ignore HT bits and show HT=3 in details]
// 30 70 = HT=3, H=4, data=adaptive (4)
// 2C 6C = HT=3, H=3, data=adaptive (3)
// 28 08 =       H=2, data=adaptive (2), no details (400G)
// 28 48 = HT=3!,H=2, data=adaptive (2) [adaptive mode appears to ignore HT bits and show HT=3 in details]
// 28 68 = HT=3, H=2, data=adaptive (2)
// 24 64 = HT=3, H=1, data=adaptive (1)
// 20 60 = HT=3, H=off, data=adaptive (0)
// 14 74 = HT=3, H=5, data=fixed (5)
// 10 70 = HT=3, H=4, data=fixed (4)
// 0C 6C = HT=3, H=3, data=fixed (3)
// 08 48 = HT=3, H=2, data=fixed (2)
// 08 68 = HT=3, H=2, data=fixed (2)
// 04 64 = HT=3, H=1, data=fixed (1)
// 00 00 = HT=3, H=off, data=fixed (0)

// F5V3 confirmed:
// 90 70 = HT=3, H=3, adaptive, data=no data
// 54 14 = HT=Off, H=5, adaptive, data=tube t=0,h=5
// 54 34 = HT=1, H=5, adaptive, data=tube t=1,h=5
// 50 70 = HT=3, H=4, adaptive, data=tube t=3,h=4
// 4C 6C = HT=3, H=3, adaptive, data=tube t=3,h=3
// 4C 4C = HT=2, H=3, adaptive, data=tube t=2,h=3
// 4C 2C = HT=1, H=3, adaptive, data=tube t=1,h=3
// 4C 0C = HT=off, H=3, adaptive, data=tube t=0,h=3
// 48 08 = HT=off, H=2, adaptive, data=tube t=0,h=2
// 44 04 = HT=off, H=1, adaptive, data=tube t=0,h=1
// 40 00 = HT=off,H=off, adaptive, data=tube t=0,h=0
// 34 74 = HT=3, H=5, adaptive, data=s1 (5)
// 30 70 = HT=3, H=4, adaptive, data=s1 (4)
// 2C 6C = HT=3, H=3, adaptive, data=s1 (3)
// 28 68 = HT=3, H=2, adaptive, data=s1 (2)
// 24 64 = HT=3, H=1, adaptive, data=s1 (1)

// F3V6 confirmed:
// 84 24 = HT=3, H=3, disconnect=adaptive, data=no data
// 50 90 = HT=4, H=4, disconnect=adaptive, data=tube t=4,h=4
// 44 84 = HT=4, H=1, disconnect=adaptive, data=tube t=4,h=1
// 40 80 = HT=4, H=Off,disconnect=adaptive, data=tube t=4,h=0
// 4C 6C = HT=3, H=3, disconnect=adaptive, data=tube t=3,h=3
// 48 68 = HT=3, H=2, disconnect=adaptive, data=tube t=3,h=2
// 44 44 = HT=2, H=1, disconnect=adaptive, data=tube t=2,h=1
// 48 28 = HT=1, H=2, disconnect=adaptive, data=tube t=1,h=2
// 54 14 = HT=Off,H=5, disconnect=adaptive data=tube t=0,h=5
// 34 14 = HT=3, H=5, disconnect=adaptive, data=s1 (5)
// 30 70 = HT=3, H=4, disconnect=adaptive, data=s1 (4)
// 2C 6C = HT=3, H=3, disconnect=adaptive, data=s1 (3)
// 28 08 = HT=3, H=2, disconnect=adaptive, data=s1 (2)
// 20 20 = HT=3, H=Off, disconnect=adaptive, data=s1 (0)
// 14 14 = HT=3, H=3, disconnect=fixed, data=classic (5)
// 10 10 = HT=3, H=4, disconnect=fixed, data=classic (4) [fixed mode appears to ignore HT bits and show HT=3 in details]
// 0C 0C = HT=3, H=3, disconnect=fixed, data=classic (3)
// 08 08 = HT=3, H=2, disconnect=fixed, data=classic (2)
// 04 64 = HT=3, H=1, disconnect=fixed, data=classic (1)

// The data is consistent among all fileVersion 3 models: F0V6, F5V3, F3V6.
//
// NOTE: F5V3 and F3V6 charts report the "Adaptive" setting as "System One" and the "Fixed"
// setting as "Classic", despite labeling the settings "Adaptive" and "Fixed" just like F0V6.
// F0V6 is consistent and labels both settings and chart as "Adaptive" and "Fixed".
//
// 400G and 502G appear to omit the humidifier settings in their details, though they
// do support humidifiers, and will show the humidification in the charts.

void PRS1DataChunk::ParseHumidifierSettingV3(unsigned char byte1, unsigned char byte2, bool add_setting)
{
    bool humidifier_present = true;
    bool humidfixed = false;  // formerly called "Classic"
    bool humidadaptive = false;  // formerly called "System One"
    bool tubepresent = false;
    bool passover = false;
    bool error = false;

    // Byte 1: 0x90 (no humidifier data), 0x50 (15ht, tube 4/5, humid 4), 0x54 (15ht, tube 5, humid 5) 0x4c (15ht, tube temp 3, humidifier 3)
    // 0x0c (15, tube 3, humid 3, fixed)
    // 0b1001 0000 no humidifier data
    // 0b0101 0000 tube 4 and 5, humidifier 4
    // 0b0101 0100 15ht, tube 5, humidifier 5
    // 0b0100 1100 15ht, tube 3, humidifier 3
    // 0b1011 0000 15, tube 3, humidifier 3, "Error" on humidification chart with asterisk at 4
    // 0b0111 0000 15, tube 3, humidifier 3, "Passover" on humidification chart with notch at 4
    //   842       = humidifier status
    //      1 84   = humidifier setting
    //          ??
    CHECK_VALUE(byte1 & 3, 0);
    int humid = byte1 >> 5;
    switch (humid) {
    case 0: humidfixed = true; break;  // fixed, ignores tubetemp bits and reports tubetemp=3
    case 1: humidadaptive = true; break;  // adaptive, ignores tubetemp bits and reports tubetemp=3
    case 2: tubepresent = true; break;  // heated tube
    case 3: passover = true; break;  // passover mode (only visible in chart)
    case 4: humidifier_present = false; break;  // no humidifier, reports tubetemp=3 and humidlevel=3
    case 5: error = true; break;  // "Error" in humidification chart, reports tubetemp=3 and humidlevel=3 in settings
    default:
        UNEXPECTED_VALUE(humid, "known value");
        break;
    }
    int humidlevel = (byte1 >> 2) & 7;

    // Byte 2: 0xB4 (15ht, tube 5, humid 5), 0xB0 (15ht, tube 5, humid 4), 0x90 (tube 4, humid 4), 0x6C (15ht, tube temp 3, humidifier 3)
    // 0x80?
    // 0b1011 0100 15ht, tube 5, humidifier 5
    // 0b1011 0000 15ht, tube 5, humidifier 4
    // 0b1001 0000 tube 4, humidifier 4
    // 0b0110 1100 15ht, tube 3, humidifier 3
    //   842       = tube temperature
    //      1 84   = humidity level when using heated tube, thus far always identical to humidlevel
    //          ??
    CHECK_VALUE(byte2 & 3, 0);
    int tubehumidlevel = (byte2 >> 2) & 7;
    CHECK_VALUE(humidlevel, tubehumidlevel);  // thus far always the same
    int tubetemp = (byte2 >> 5) & 7;
    if (humidifier_present) {
        if (humidlevel > 5 || humidlevel < 0) UNEXPECTED_VALUE(humidlevel, "0-5");  // 0=off is valid when a humidifier is attached
        if (humid == 2) {  // heated tube
            if (tubetemp > 5 || tubetemp < 0) UNEXPECTED_VALUE(tubetemp, "0-5");  // TODO: maybe this is only if heated tube? 0=off is valid even in heated tube mode
        }
    }

    // TODO: move this up into the switch statement above, given how many modes there now are.
    HumidMode humidmode = HUMID_Fixed;
    if (tubepresent) {
        humidmode = HUMID_HeatedTube;
    } else if (humidadaptive) {
        humidmode = HUMID_Adaptive;
    } else if (passover) {
        humidmode = HUMID_Passover;
    } else if (error) {
        humidmode = HUMID_Error;
    }

    if (add_setting) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            if (humidmode == HUMID_HeatedTube) {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HEATED_TUBE_TEMP, tubetemp));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, tubehumidlevel));
            } else {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
            }
        }
    }

    // Check for previously unseen data that we expect to be normal:
    if (family == 0) {
        // All variations seen.
    } else if (family == 5) {
        if (tubepresent) {
            // All tube temperature and humidity levels seen.
        } else if (humidadaptive) {
            // All humidity levels seen.
        } else if (humidfixed) {
            if (humidlevel < 3) UNEXPECTED_VALUE(humidlevel, "3-5");
        }
    } else if (family == 3) {
        if (tubepresent) {
            // All tube temperature and humidity levels seen.
        } else if (humidadaptive) {
            // All humidity levels seen.
        } else if (humidfixed) {
            // All humidity levels seen.
        }
    }
}


void PRS1DataChunk::ParseTubingTypeV3(unsigned char type)
{
    int diam;
    switch (type) {
    case 0: diam = 22; break;
    case 1: diam = 15; break;
    case 2: diam = 15; break;  // 15HT, though the reports only say "15" for DreamStation models
    case 3: diam = 12; break;  // seen on DreamStation Go models
    case 4: diam = 12; break;  // HT12, seen on DreamStation 2 models
    default:
        UNEXPECTED_VALUE(type, "known tubing type");
        return;
    }
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, diam));
}


//********************************************************************************************
// MARK: -
// MARK: Parse and verify chunk from stream

typedef quint16 crc16_t;
typedef quint32 crc32_t;
static crc16_t CRC16(unsigned char * data, size_t data_len, crc16_t crc=0);
static crc32_t CRC32(const unsigned char *data, size_t data_len, crc32_t crc=0xffffffffU);
static crc32_t CRC32wchar(const unsigned char *data, size_t data_len, crc32_t crc=0xffffffffU);

PRS1DataChunk::PRS1DataChunk(RawDataDevice & f, PRS1Loader* in_loader) : loader(in_loader)
{
    m_path = f.name();
}

PRS1DataChunk::~PRS1DataChunk()
{
    for (int i=0; i < m_parsedData.count(); i++) {
        PRS1ParsedEvent* e = m_parsedData.at(i);
        delete e;
    }
}


PRS1DataChunk* PRS1DataChunk::ParseNext(RawDataDevice & f, PRS1Loader* loader)
{
    PRS1DataChunk* out_chunk = nullptr;
    PRS1DataChunk* chunk = new PRS1DataChunk(f, loader);

    do {
        // Parse the header and calculate its checksum.
        bool ok = chunk->ReadHeader(f);
        if (!ok) {
            break;
        }

        // Make sure the calculated checksum matches the stored checksum.
        if (chunk->calcChecksum != chunk->storedChecksum) {
            qWarning() << chunk->m_path << "header checksum calc" << chunk->calcChecksum << "!= stored" << chunk->storedChecksum;
            break;
        }

        // Read the block's data and calculate the block CRC.
        ok = chunk->ReadData(f);
        if (!ok) {
            break;
        }
        
        // Make sure the calculated CRC over the entire chunk (header and data) matches the stored CRC.
        if (chunk->calcCrc != chunk->storedCrc) {
            // Corrupt data block, warn about it.
            qWarning() << chunk->m_path << "@" << chunk->m_filepos << "block CRC calc" << QTHEX << chunk->calcCrc << "!= stored" << QTHEX << chunk->storedCrc;
            
            // TODO: When this happens, it's usually because the chunk was truncated and another chunk header
            // exists within the blockSize bytes. In theory it should be possible to rewing and resync by
            // looking for another chunk header with the same fileVersion, htype, family, familyVersion, and
            // ext (blockSize and other fields could vary).
            //
            // But this is quite rare, so for now we bail on the rest of the file.
            break;
        }

        // Only return the chunk if it has passed all tests above.
        out_chunk = chunk;
    } while (false);

    if (out_chunk == nullptr) delete chunk;
    return out_chunk;
}


bool PRS1DataChunk::ReadHeader(RawDataDevice & f)
{
    bool ok = false;
    do {
        // Read common header fields.
        this->m_filepos = f.pos();
        this->m_header = f.read(15);
        if (this->m_header.size() != 15) {
            if (this->m_header.size() == 0) {
                qWarning() << this->m_path << "empty, skipping";
            } else {
                qWarning() << this->m_path << "file too short?";
            }
            break;
        }
        
        unsigned char * header = (unsigned char *)this->m_header.data();
        this->fileVersion = header[0];    // Correlates to DataFileVersion in PROP[erties].TXT, only 2 or 3 has ever been observed
        this->blockSize = (header[2] << 8) | header[1];
        this->htype = header[3];      // 00 = normal, 01=waveform
        this->family = header[4];
        this->familyVersion = header[5];
        this->ext = header[6];
        this->sessionid = (header[10] << 24) | (header[9] << 16) | (header[8] << 8) | header[7];
        this->timestamp = (header[14] << 24) | (header[13] << 16) | (header[12] << 8) | header[11];

        // Do a few early sanity checks before any variable-length header data.
        if (this->blockSize == 0) {
            qWarning() << this->m_path << "@" << QTHEX << this->m_filepos << "blocksize 0, skipping remainder of file";
            break;
        }
        if (this->fileVersion < 2 || this->fileVersion > 3) {
            if (this->m_filepos > 0) {
                qWarning() << this->m_path << "@" << QTHEX << this->m_filepos << "corrupt PRS1 header, skipping remainder of file";
            } else {
                qWarning() << this->m_path << "unsupported PRS1 header version" << this->fileVersion;
            }
            break;
        }
        if (this->htype != PRS1_HTYPE_NORMAL && this->htype != PRS1_HTYPE_INTERVAL) {
            qWarning() << this->m_path << "unexpected htype:" << this->htype;
            break;
        }

        // Read format-specific variable-length header data.
        bool hdr_ok = false;
        if (this->htype != PRS1_HTYPE_INTERVAL) {  // Not just waveforms: the 1160P uses this for its .002 events file.
            // Not a waveform/interval chunk
            switch (this->fileVersion) {
                case 2:
                    hdr_ok = ReadNormalHeaderV2(f);
                    break;
                case 3:
                    hdr_ok = ReadNormalHeaderV3(f);
                    break;
                default:
                    //hdr_ok remains false, warning is above
                    break;
            }
        } else {
            // Waveform/interval chunk
            hdr_ok = ReadWaveformHeader(f);
        }
        if (!hdr_ok) {
            break;
        }

        // The 8bit checksum comes at the end.
        QByteArray checksum = f.read(1);
        if (checksum.size() < 1) {
            qWarning() << this->m_path << "read error header checksum";
            break;
        }
        this->storedChecksum = checksum.data()[0];

        // Calculate 8bit additive header checksum.
        header = (unsigned char *)this->m_header.data(); // important because its memory location could move
        int header_size = this->m_header.size();
        quint8 achk=0;
        for (int i=0; i < header_size; i++) {
            achk += header[i];
        }
        this->calcChecksum = achk;
        
        // Append the stored checksum to the raw data *after* calculating the checksum on the preceding data.
        this->m_header.append(checksum);

        ok = true;
    } while (false);

    return ok;
}


bool PRS1DataChunk::ReadNormalHeaderV2(RawDataDevice & /*f*/)
{
    this->m_headerblock = QByteArray();
    return true;  // always OK
}


bool PRS1DataChunk::ReadNormalHeaderV3(RawDataDevice & f)
{
    bool ok = false;
    unsigned char * header;
    QByteArray headerB2;

    // This is a new device, byte 15 is header data block length
    // followed by variable, data byte pairs
    do {
        QByteArray extra = f.read(1);
        if (extra.size() < 1) {
            qWarning() << this->m_path << "read error extended header";
            break;
        }
        this->m_header.append(extra);
        header = (unsigned char *)this->m_header.data();

        int hdb_len = header[15];
        int hdb_size = hdb_len * 2;

        headerB2 = f.read(hdb_size);
        if (headerB2.size() != hdb_size) {
            qWarning() << this->m_path << "read error in extended header";
            break;
        }
        this->m_headerblock = headerB2;
        
        this->m_header.append(headerB2);
        header = (unsigned char *)this->m_header.data();
        const unsigned char * hd = (unsigned char *)headerB2.constData();
        int pos = 0;
        int recs = header[15];
        for (int i=0; i<recs; i++) {
            this->hblock[hd[pos]] = hd[pos+1];
            pos += 2;
        }
        
        ok = true;
    } while (false);

    return ok;
}


bool PRS1DataChunk::ReadWaveformHeader(RawDataDevice & f)
{
    bool ok = false;
    unsigned char * header;
    do {
        // Read the fixed-length waveform header.
        QByteArray extra = f.read(4);
        if (extra.size() != 4) {
            qWarning() << this->m_path << "read error in waveform header";
            break;
        }
        this->m_header.append(extra);
        header = (unsigned char *)this->m_header.data();

        // Parse the fixed-length portion.
        this->interval_count = header[0x0f] | header[0x10] << 8;
        this->interval_seconds = header[0x11];  // not always 1 after all
        this->duration = this->interval_count * this->interval_seconds;  // ??? the last entry doesn't always seem to be a full interval?
        quint8 wvfm_signals = header[0x12];

        // Read the variable-length data + trailing byte.
        int ws_size = (this->fileVersion == 3) ? 4 : 3;
        int sbsize = wvfm_signals * ws_size + 1;

        extra = f.read(sbsize);
        if (extra.size() != sbsize) {
            qWarning() << this->m_path << "read error in waveform header 2";
            break;
        }
        this->m_header.append(extra);
        header = (unsigned char *)this->m_header.data();

        // Parse the variable-length waveform information.
        // TODO: move these checks into the parser, after the header checksum has been verified
        // For now just skip them for the one known sample with a bad checksum.
        if (this->sessionid == 268962649) return true;

        int pos = 0x13;
        for (int i = 0; i < wvfm_signals; ++i) {
            quint8 kind = header[pos];
            CHECK_VALUE(kind, i);  // always seems to range from 0...wvfm_signals-1, alert if not
            quint16 interleave = header[pos + 1] | header[pos + 2] << 8;  // samples per interval
            if (this->fileVersion == 2) {
                this->waveformInfo.push_back(PRS1Waveform(interleave, kind));
                pos += 3;
            } else if (this->fileVersion == 3) {
                int always_8 = header[pos + 3];  // sample size in bits?
                CHECK_VALUE(always_8, 8);
                this->waveformInfo.push_back(PRS1Waveform(interleave, kind));
                pos += 4;
            }
        }
        
        // And the trailing byte, whatever it is.
        int always_0 = header[pos];
        CHECK_VALUE(always_0, 0);
       
        ok = true;
    } while (false);

    return ok;
}


bool PRS1DataChunk::ReadData(RawDataDevice & f)
{
    bool ok = false;
    do {
        // Read data block
        int data_size = this->blockSize - this->m_header.size();
        if (data_size < 0) {
            qWarning() << this->m_path << "chunk size smaller than header";
            break;
        }
        this->m_data = f.read(data_size);
        if (this->m_data.size() < data_size) {
            qWarning() << this->m_path << "less data in file than specified in header";
            break;
        }

        // Extract the stored CRC from the data buffer and calculate the current CRC.
        if (this->fileVersion==3) {
            // The last 4 bytes contain a CRC32 checksum of the data.
            if (!ExtractStoredCrc(4)) {
                break;
            }
            this->calcCrc = CRC32wchar((unsigned char *)this->m_data.data(), this->m_data.size());
        } else {
            // The last 2 bytes contain a CRC16 checksum of the data.
            if (!ExtractStoredCrc(2)) {
                break;
            }
            this->calcCrc = CRC16((unsigned char *)this->m_data.data(), this->m_data.size());
        }
        
        ok = true;
    } while (false);

    return ok;
}


bool PRS1DataChunk::ExtractStoredCrc(int size)
{
    // Make sure there's enough data for the CRC.
    int offset = this->m_data.size() - size;
    if (offset < 0) {
        qWarning() << this->m_path << "chunk truncated";
        return false;
    }
    
    // Read the last 16- or 32-bit little-endian integer.
    quint32 storedCrc = 0;
    unsigned char* data = (unsigned char*)this->m_data.data();
    for (int i=0; i < size; i++) {
        storedCrc |= data[offset+i] << (8*i);
    }
    this->storedCrc = storedCrc;

    // Drop the CRC from the data.
    this->m_data.chop(size);
    
    return true;
}


// CRC-16/KERMIT, polynomial: 0x11021, bit reverse algorithm
// Table generated by crcmod (crc-kermit)

typedef quint16 crc16_t;
static crc16_t CRC16(unsigned char * data, size_t data_len, crc16_t crc)
{
    static const crc16_t table[256] = {
    0x0000U, 0x1189U, 0x2312U, 0x329bU, 0x4624U, 0x57adU, 0x6536U, 0x74bfU,
    0x8c48U, 0x9dc1U, 0xaf5aU, 0xbed3U, 0xca6cU, 0xdbe5U, 0xe97eU, 0xf8f7U,
    0x1081U, 0x0108U, 0x3393U, 0x221aU, 0x56a5U, 0x472cU, 0x75b7U, 0x643eU,
    0x9cc9U, 0x8d40U, 0xbfdbU, 0xae52U, 0xdaedU, 0xcb64U, 0xf9ffU, 0xe876U,
    0x2102U, 0x308bU, 0x0210U, 0x1399U, 0x6726U, 0x76afU, 0x4434U, 0x55bdU,
    0xad4aU, 0xbcc3U, 0x8e58U, 0x9fd1U, 0xeb6eU, 0xfae7U, 0xc87cU, 0xd9f5U,
    0x3183U, 0x200aU, 0x1291U, 0x0318U, 0x77a7U, 0x662eU, 0x54b5U, 0x453cU,
    0xbdcbU, 0xac42U, 0x9ed9U, 0x8f50U, 0xfbefU, 0xea66U, 0xd8fdU, 0xc974U,
    0x4204U, 0x538dU, 0x6116U, 0x709fU, 0x0420U, 0x15a9U, 0x2732U, 0x36bbU,
    0xce4cU, 0xdfc5U, 0xed5eU, 0xfcd7U, 0x8868U, 0x99e1U, 0xab7aU, 0xbaf3U,
    0x5285U, 0x430cU, 0x7197U, 0x601eU, 0x14a1U, 0x0528U, 0x37b3U, 0x263aU,
    0xdecdU, 0xcf44U, 0xfddfU, 0xec56U, 0x98e9U, 0x8960U, 0xbbfbU, 0xaa72U,
    0x6306U, 0x728fU, 0x4014U, 0x519dU, 0x2522U, 0x34abU, 0x0630U, 0x17b9U,
    0xef4eU, 0xfec7U, 0xcc5cU, 0xddd5U, 0xa96aU, 0xb8e3U, 0x8a78U, 0x9bf1U,
    0x7387U, 0x620eU, 0x5095U, 0x411cU, 0x35a3U, 0x242aU, 0x16b1U, 0x0738U,
    0xffcfU, 0xee46U, 0xdcddU, 0xcd54U, 0xb9ebU, 0xa862U, 0x9af9U, 0x8b70U,
    0x8408U, 0x9581U, 0xa71aU, 0xb693U, 0xc22cU, 0xd3a5U, 0xe13eU, 0xf0b7U,
    0x0840U, 0x19c9U, 0x2b52U, 0x3adbU, 0x4e64U, 0x5fedU, 0x6d76U, 0x7cffU,
    0x9489U, 0x8500U, 0xb79bU, 0xa612U, 0xd2adU, 0xc324U, 0xf1bfU, 0xe036U,
    0x18c1U, 0x0948U, 0x3bd3U, 0x2a5aU, 0x5ee5U, 0x4f6cU, 0x7df7U, 0x6c7eU,
    0xa50aU, 0xb483U, 0x8618U, 0x9791U, 0xe32eU, 0xf2a7U, 0xc03cU, 0xd1b5U,
    0x2942U, 0x38cbU, 0x0a50U, 0x1bd9U, 0x6f66U, 0x7eefU, 0x4c74U, 0x5dfdU,
    0xb58bU, 0xa402U, 0x9699U, 0x8710U, 0xf3afU, 0xe226U, 0xd0bdU, 0xc134U,
    0x39c3U, 0x284aU, 0x1ad1U, 0x0b58U, 0x7fe7U, 0x6e6eU, 0x5cf5U, 0x4d7cU,
    0xc60cU, 0xd785U, 0xe51eU, 0xf497U, 0x8028U, 0x91a1U, 0xa33aU, 0xb2b3U,
    0x4a44U, 0x5bcdU, 0x6956U, 0x78dfU, 0x0c60U, 0x1de9U, 0x2f72U, 0x3efbU,
    0xd68dU, 0xc704U, 0xf59fU, 0xe416U, 0x90a9U, 0x8120U, 0xb3bbU, 0xa232U,
    0x5ac5U, 0x4b4cU, 0x79d7U, 0x685eU, 0x1ce1U, 0x0d68U, 0x3ff3U, 0x2e7aU,
    0xe70eU, 0xf687U, 0xc41cU, 0xd595U, 0xa12aU, 0xb0a3U, 0x8238U, 0x93b1U,
    0x6b46U, 0x7acfU, 0x4854U, 0x59ddU, 0x2d62U, 0x3cebU, 0x0e70U, 0x1ff9U,
    0xf78fU, 0xe606U, 0xd49dU, 0xc514U, 0xb1abU, 0xa022U, 0x92b9U, 0x8330U,
    0x7bc7U, 0x6a4eU, 0x58d5U, 0x495cU, 0x3de3U, 0x2c6aU, 0x1ef1U, 0x0f78U,
    };

    for (size_t i=0; i < data_len; i++) {
        crc = table[(*data ^ (unsigned char)crc) & 0xFF] ^ (crc >> 8);
        data++;
    }
    return crc;
}


// CRC-32/MPEG-2, polynomial: 0x104C11DB7
// Table generated by crcmod (crc-32-mpeg)

static crc32_t CRC32(const unsigned char *data, size_t data_len, crc32_t crc)
{
    static const crc32_t table[256] = {
    0x00000000U, 0x04c11db7U, 0x09823b6eU, 0x0d4326d9U,
    0x130476dcU, 0x17c56b6bU, 0x1a864db2U, 0x1e475005U,
    0x2608edb8U, 0x22c9f00fU, 0x2f8ad6d6U, 0x2b4bcb61U,
    0x350c9b64U, 0x31cd86d3U, 0x3c8ea00aU, 0x384fbdbdU,
    0x4c11db70U, 0x48d0c6c7U, 0x4593e01eU, 0x4152fda9U,
    0x5f15adacU, 0x5bd4b01bU, 0x569796c2U, 0x52568b75U,
    0x6a1936c8U, 0x6ed82b7fU, 0x639b0da6U, 0x675a1011U,
    0x791d4014U, 0x7ddc5da3U, 0x709f7b7aU, 0x745e66cdU,
    0x9823b6e0U, 0x9ce2ab57U, 0x91a18d8eU, 0x95609039U,
    0x8b27c03cU, 0x8fe6dd8bU, 0x82a5fb52U, 0x8664e6e5U,
    0xbe2b5b58U, 0xbaea46efU, 0xb7a96036U, 0xb3687d81U,
    0xad2f2d84U, 0xa9ee3033U, 0xa4ad16eaU, 0xa06c0b5dU,
    0xd4326d90U, 0xd0f37027U, 0xddb056feU, 0xd9714b49U,
    0xc7361b4cU, 0xc3f706fbU, 0xceb42022U, 0xca753d95U,
    0xf23a8028U, 0xf6fb9d9fU, 0xfbb8bb46U, 0xff79a6f1U,
    0xe13ef6f4U, 0xe5ffeb43U, 0xe8bccd9aU, 0xec7dd02dU,
    0x34867077U, 0x30476dc0U, 0x3d044b19U, 0x39c556aeU,
    0x278206abU, 0x23431b1cU, 0x2e003dc5U, 0x2ac12072U,
    0x128e9dcfU, 0x164f8078U, 0x1b0ca6a1U, 0x1fcdbb16U,
    0x018aeb13U, 0x054bf6a4U, 0x0808d07dU, 0x0cc9cdcaU,
    0x7897ab07U, 0x7c56b6b0U, 0x71159069U, 0x75d48ddeU,
    0x6b93dddbU, 0x6f52c06cU, 0x6211e6b5U, 0x66d0fb02U,
    0x5e9f46bfU, 0x5a5e5b08U, 0x571d7dd1U, 0x53dc6066U,
    0x4d9b3063U, 0x495a2dd4U, 0x44190b0dU, 0x40d816baU,
    0xaca5c697U, 0xa864db20U, 0xa527fdf9U, 0xa1e6e04eU,
    0xbfa1b04bU, 0xbb60adfcU, 0xb6238b25U, 0xb2e29692U,
    0x8aad2b2fU, 0x8e6c3698U, 0x832f1041U, 0x87ee0df6U,
    0x99a95df3U, 0x9d684044U, 0x902b669dU, 0x94ea7b2aU,
    0xe0b41de7U, 0xe4750050U, 0xe9362689U, 0xedf73b3eU,
    0xf3b06b3bU, 0xf771768cU, 0xfa325055U, 0xfef34de2U,
    0xc6bcf05fU, 0xc27dede8U, 0xcf3ecb31U, 0xcbffd686U,
    0xd5b88683U, 0xd1799b34U, 0xdc3abdedU, 0xd8fba05aU,
    0x690ce0eeU, 0x6dcdfd59U, 0x608edb80U, 0x644fc637U,
    0x7a089632U, 0x7ec98b85U, 0x738aad5cU, 0x774bb0ebU,
    0x4f040d56U, 0x4bc510e1U, 0x46863638U, 0x42472b8fU,
    0x5c007b8aU, 0x58c1663dU, 0x558240e4U, 0x51435d53U,
    0x251d3b9eU, 0x21dc2629U, 0x2c9f00f0U, 0x285e1d47U,
    0x36194d42U, 0x32d850f5U, 0x3f9b762cU, 0x3b5a6b9bU,
    0x0315d626U, 0x07d4cb91U, 0x0a97ed48U, 0x0e56f0ffU,
    0x1011a0faU, 0x14d0bd4dU, 0x19939b94U, 0x1d528623U,
    0xf12f560eU, 0xf5ee4bb9U, 0xf8ad6d60U, 0xfc6c70d7U,
    0xe22b20d2U, 0xe6ea3d65U, 0xeba91bbcU, 0xef68060bU,
    0xd727bbb6U, 0xd3e6a601U, 0xdea580d8U, 0xda649d6fU,
    0xc423cd6aU, 0xc0e2d0ddU, 0xcda1f604U, 0xc960ebb3U,
    0xbd3e8d7eU, 0xb9ff90c9U, 0xb4bcb610U, 0xb07daba7U,
    0xae3afba2U, 0xaafbe615U, 0xa7b8c0ccU, 0xa379dd7bU,
    0x9b3660c6U, 0x9ff77d71U, 0x92b45ba8U, 0x9675461fU,
    0x8832161aU, 0x8cf30badU, 0x81b02d74U, 0x857130c3U,
    0x5d8a9099U, 0x594b8d2eU, 0x5408abf7U, 0x50c9b640U,
    0x4e8ee645U, 0x4a4ffbf2U, 0x470cdd2bU, 0x43cdc09cU,
    0x7b827d21U, 0x7f436096U, 0x7200464fU, 0x76c15bf8U,
    0x68860bfdU, 0x6c47164aU, 0x61043093U, 0x65c52d24U,
    0x119b4be9U, 0x155a565eU, 0x18197087U, 0x1cd86d30U,
    0x029f3d35U, 0x065e2082U, 0x0b1d065bU, 0x0fdc1becU,
    0x3793a651U, 0x3352bbe6U, 0x3e119d3fU, 0x3ad08088U,
    0x2497d08dU, 0x2056cd3aU, 0x2d15ebe3U, 0x29d4f654U,
    0xc5a92679U, 0xc1683bceU, 0xcc2b1d17U, 0xc8ea00a0U,
    0xd6ad50a5U, 0xd26c4d12U, 0xdf2f6bcbU, 0xdbee767cU,
    0xe3a1cbc1U, 0xe760d676U, 0xea23f0afU, 0xeee2ed18U,
    0xf0a5bd1dU, 0xf464a0aaU, 0xf9278673U, 0xfde69bc4U,
    0x89b8fd09U, 0x8d79e0beU, 0x803ac667U, 0x84fbdbd0U,
    0x9abc8bd5U, 0x9e7d9662U, 0x933eb0bbU, 0x97ffad0cU,
    0xafb010b1U, 0xab710d06U, 0xa6322bdfU, 0xa2f33668U,
    0xbcb4666dU, 0xb8757bdaU, 0xb5365d03U, 0xb1f740b4U,
    };
    
    for (size_t i=0; i < data_len; i++) {
        crc = table[(*data ^ (unsigned char)(crc >> 24)) & 0xFF] ^ (crc << 8);
        data++;
    }
    return crc;
}


// The PRS1 CRC32 considers every byte a 32-bit wchar_t, presumably due to
// use of the STM32 CRC calculation unit, in which "CRC computation is done
// on the whole 32-bit data word, and not byte per byte".

static crc32_t CRC32wchar(const unsigned char *data, size_t data_len, crc32_t crc)
{
    for (size_t i=0; i < data_len; i++) {
        static unsigned char wch[4] = { 0, 0, 0, 0 };
        wch[3] = *data++;
        crc = CRC32(wch, 4, crc);
    }
    return crc;
}
