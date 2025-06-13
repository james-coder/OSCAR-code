/* SleepLib PRS1 Loader Parser Header
*
* Copyright (c) 2019-2025 The OSCAR Team
* Portions copyright (c) 2011-2018 Mark Watkins 
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#ifndef PRS1PARSER_H
#define PRS1PARSER_H

#include <QMap>
#include <QString>
#include "SleepLib/session.h"

// Include and configure the CHECK_VALUE and UNEXPECTED_VALUE macros.
#include "SleepLib/importcontext.h"
#define IMPORT_CTX loader->context()
#define SESSIONID this->sessionid

//********************************************************************************************
// MARK: -
// MARK: Internal PRS1 parsed data types
//********************************************************************************************

// For new events, add an enum here and then a class below with an PRS1_*_EVENT macro
enum PRS1ParsedEventType
{
    EV_PRS1_RAW = -1,     // these only get logged
    EV_PRS1_UNKNOWN = 0,  // these have their value graphed
    EV_PRS1_TB,
    EV_PRS1_OA,
    EV_PRS1_CA,
    EV_PRS1_FL,
    EV_PRS1_PB,
    EV_PRS1_LL,
    EV_PRS1_VB,           // UNCONFIRMED
    EV_PRS1_HY,
    EV_PRS1_OA_COUNT,     // F3V3 only
    EV_PRS1_CA_COUNT,     // F3V3 only
    EV_PRS1_HY_COUNT,     // F3V3 only
    EV_PRS1_TOTLEAK,
    EV_PRS1_LEAK,  // unintentional leak
    EV_PRS1_AUTO_PRESSURE_SET,
    EV_PRS1_PRESSURE_SET,
    EV_PRS1_IPAP_SET,
    EV_PRS1_EPAP_SET,
    EV_PRS1_PRESSURE_AVG,
    EV_PRS1_FLEX_PRESSURE_AVG,
    EV_PRS1_IPAP_AVG,
    EV_PRS1_IPAPLOW,
    EV_PRS1_IPAPHIGH,
    EV_PRS1_EPAP_AVG,
    EV_PRS1_RR,
    EV_PRS1_PTB,
    EV_PRS1_MV,
    EV_PRS1_TV,
    EV_PRS1_SNORE,
    EV_PRS1_VS,
    EV_PRS1_PP,
    EV_PRS1_RERA,
    EV_PRS1_FLOWRATE,
    EV_PRS1_TEST1,
    EV_PRS1_TEST2,
    EV_PRS1_SETTING,
    EV_PRS1_SLICE,
    EV_PRS1_DISCONNECT_ALARM,
    EV_PRS1_APNEA_ALARM,
    EV_PRS1_LOW_MV_ALARM,
    EV_PRS1_SNORES_AT_PRESSURE,
    EV_PRS1_INTERVAL_BOUNDARY,  // An artificial internal-only event used to separate stat intervals
};

enum PRS1ParsedEventUnit
{
    PRS1_UNIT_NONE,
    PRS1_UNIT_CMH2O,
    PRS1_UNIT_ML,
    PRS1_UNIT_S,
};

enum PRS1ParsedSettingType
{
    PRS1_SETTING_CPAP_MODE,
    PRS1_SETTING_AUTO_TRIAL,
    PRS1_SETTING_PRESSURE,
    PRS1_SETTING_PRESSURE_MIN,
    PRS1_SETTING_PRESSURE_MAX,
    PRS1_SETTING_EPAP,
    PRS1_SETTING_EPAP_MIN,
    PRS1_SETTING_EPAP_MAX,
    PRS1_SETTING_IPAP,
    PRS1_SETTING_IPAP_MIN,
    PRS1_SETTING_IPAP_MAX,
    PRS1_SETTING_PS,
    PRS1_SETTING_PS_MIN,
    PRS1_SETTING_PS_MAX,
    PRS1_SETTING_BACKUP_BREATH_MODE,
    PRS1_SETTING_BACKUP_BREATH_RATE,
    PRS1_SETTING_BACKUP_TIMED_INSPIRATION,
    PRS1_SETTING_TIDAL_VOLUME,
    PRS1_SETTING_EZ_START,
    PRS1_SETTING_FLEX_LOCK,
    PRS1_SETTING_FLEX_MODE,
    PRS1_SETTING_FLEX_LEVEL,
    PRS1_SETTING_RISE_TIME,
    PRS1_SETTING_RISE_TIME_LOCK,
    PRS1_SETTING_RAMP_TYPE,
    PRS1_SETTING_RAMP_TIME,
    PRS1_SETTING_RAMP_PRESSURE,
    PRS1_SETTING_HUMID_STATUS,
    PRS1_SETTING_HUMID_MODE,
    PRS1_SETTING_HEATED_TUBE_TEMP,
    PRS1_SETTING_HUMID_LEVEL,
    PRS1_SETTING_MASK_RESIST_LOCK,
    PRS1_SETTING_MASK_RESIST_SETTING,
    PRS1_SETTING_HOSE_DIAMETER,
    PRS1_SETTING_TUBING_LOCK,
    PRS1_SETTING_AUTO_ON,
    PRS1_SETTING_AUTO_OFF,
    PRS1_SETTING_APNEA_ALARM,
    PRS1_SETTING_DISCONNECT_ALARM,  // Is this any different from mask alert?
    PRS1_SETTING_LOW_MV_ALARM,
    PRS1_SETTING_LOW_TV_ALARM,
    PRS1_SETTING_MASK_ALERT,
    PRS1_SETTING_SHOW_AHI,
    PRS1_SETTING_HUMID_TARGET_TIME,
};


class PRS1ParsedEvent
{
public:
    PRS1ParsedEventType m_type;
    int m_start;     // seconds relative to chunk timestamp at which this event began
    int m_duration;
    int m_value;
    float m_offset;
    float m_gain;
    PRS1ParsedEventUnit m_unit;

    inline float value(void) const { return (m_value * m_gain) + m_offset; }
    
    static const PRS1ParsedEventType TYPE = EV_PRS1_UNKNOWN;
    static constexpr float GAIN = 1.0;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_NONE;
    
    QString typeName(void) const;
    virtual QMap<QString,QString> contents(void) = 0;

protected:
    PRS1ParsedEvent(PRS1ParsedEventType type, int start)
    : m_type(type), m_start(start), m_duration(0), m_value(0), m_offset(0.0), m_gain(GAIN), m_unit(UNIT)
    {
    }
    static QString timeStr(int t);
public:
    virtual ~PRS1ParsedEvent()
    {
    }
};


class PRS1IntervalBoundaryEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_INTERVAL_BOUNDARY;

    PRS1IntervalBoundaryEvent(int start) : PRS1ParsedEvent(TYPE, start) {}
};


class PRS1ParsedDurationEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_S;
    
    PRS1ParsedDurationEvent(PRS1ParsedEventType type, int start, int duration) : PRS1ParsedEvent(type, start) { m_duration = duration; }
};


class PRS1ParsedValueEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

protected:
    PRS1ParsedValueEvent(PRS1ParsedEventType type, int start, int value) : PRS1ParsedEvent(type, start) { m_value = value; }
};

/*
class PRS1UnknownValueEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void)
    {
        QMap<QString,QString> out;
        out["start"] = timeStr(m_start);
        out["code"] = hex(m_code);
        out["value"] = QString::number(value());
        return out;
    }

    int m_code;
    PRS1UnknownValueEvent(int code, int start, int value, float gain=1.0) : PRS1ParsedValueEvent(TYPE, start, value), m_code(code) { m_gain = gain; }
};
*/

class PRS1UnknownDataEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_RAW;
    
    int m_pos;
    unsigned char m_code;
    QByteArray m_data;
    
    PRS1UnknownDataEvent(const QByteArray & data, int pos, int len=18)
        : PRS1ParsedEvent(TYPE, 0)
    {
        m_pos = pos;
        m_data = data.mid(pos, len);
        Q_ASSERT(m_data.size() >= 1);
        m_code = m_data.at(0);
    }
};

class PRS1PressureEvent : public PRS1ParsedValueEvent
{
public:
    static constexpr float GAIN = 0.1;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_CMH2O;
    
    PRS1PressureEvent(PRS1ParsedEventType type, int start, int value, float gain=GAIN)
        : PRS1ParsedValueEvent(type, start, value)
    {
        m_gain = gain;
        m_unit = UNIT;
    }
};

class PRS1TidalVolumeEvent : public PRS1ParsedValueEvent
{
public:
    static const PRS1ParsedEventType TYPE = EV_PRS1_TV;

    static constexpr float GAIN = 10.0;
    static const PRS1ParsedEventUnit UNIT = PRS1_UNIT_ML;
    
    PRS1TidalVolumeEvent(int start, int value)
        : PRS1ParsedValueEvent(TYPE, start, value)
    {
        m_gain = GAIN;
        m_unit = UNIT;
    }
};

class PRS1ParsedSettingEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void);
    
    static const PRS1ParsedEventType TYPE = EV_PRS1_SETTING;
    PRS1ParsedSettingType m_setting;
    
    PRS1ParsedSettingEvent(PRS1ParsedSettingType setting, int value) : PRS1ParsedValueEvent(TYPE, 0, value), m_setting(setting) {}

protected:
    QString settingName(void) const;
    QString modeName(void) const;
};

class PRS1ScaledSettingEvent : public PRS1ParsedSettingEvent
{
public:
    PRS1ScaledSettingEvent(PRS1ParsedSettingType setting, int value, float gain)
        : PRS1ParsedSettingEvent(setting, value)
    {
        m_gain = gain;
    }
};

class PRS1PressureSettingEvent : public PRS1ScaledSettingEvent
{
public:
    static constexpr float GAIN = PRS1PressureEvent::GAIN;
    static const PRS1ParsedEventUnit UNIT = PRS1PressureEvent::UNIT;
    
    PRS1PressureSettingEvent(PRS1ParsedSettingType setting, int value, float gain=GAIN)
        : PRS1ScaledSettingEvent(setting, value, gain)
    {
        m_unit = UNIT;
    }
};

class PRS1ParsedSliceEvent : public PRS1ParsedValueEvent
{
public:
    virtual QMap<QString,QString> contents(void);

    static const PRS1ParsedEventType TYPE = EV_PRS1_SLICE;
    
    PRS1ParsedSliceEvent(int start, SliceStatus status) : PRS1ParsedValueEvent(TYPE, start, (int) status) {}
};


class PRS1ParsedAlarmEvent : public PRS1ParsedEvent
{
public:
    virtual QMap<QString,QString> contents(void);

protected:
    PRS1ParsedAlarmEvent(PRS1ParsedEventType type, int start, int /*unused*/) : PRS1ParsedEvent(type, start) {}
};


class PRS1SnoresAtPressureEvent : public PRS1PressureEvent
{
public:
    static const PRS1ParsedEventType TYPE = EV_PRS1_SNORES_AT_PRESSURE;

    PRS1SnoresAtPressureEvent(int start, int kind, int pressure, int count, float gain=GAIN)
        : PRS1PressureEvent(TYPE, start, pressure, gain)
    {
        m_kind = kind;
        m_count = count;
    }

    virtual QMap<QString,QString> contents(void);

protected:
    int m_kind;
    // m_value is pressure
    int m_count;
};


#define _PRS1_EVENT(T, E, P, ARG) \
class T : public P \
{ \
public: \
    static const PRS1ParsedEventType TYPE = E; \
    T(int start, int ARG) : P(TYPE, start, ARG) {} \
};
#define PRS1_DURATION_EVENT(T, E) _PRS1_EVENT(T, E, PRS1ParsedDurationEvent, duration)
#define PRS1_VALUE_EVENT(T, E)    _PRS1_EVENT(T, E, PRS1ParsedValueEvent, value)
#define PRS1_ALARM_EVENT(T, E)    _PRS1_EVENT(T, E, PRS1ParsedAlarmEvent, value)
#define PRS1_PRESSURE_EVENT(T, E) \
class T : public PRS1PressureEvent \
{ \
public: \
    static const PRS1ParsedEventType TYPE = E; \
    T(int start, int value, float gain=PRS1PressureEvent::GAIN) : PRS1PressureEvent(TYPE, start, value, gain) {} \
};

PRS1_DURATION_EVENT(PRS1TimedBreathEvent, EV_PRS1_TB);
PRS1_DURATION_EVENT(PRS1ObstructiveApneaEvent, EV_PRS1_OA);
PRS1_DURATION_EVENT(PRS1ClearAirwayEvent, EV_PRS1_CA);
PRS1_DURATION_EVENT(PRS1FlowLimitationEvent, EV_PRS1_FL);
PRS1_DURATION_EVENT(PRS1PeriodicBreathingEvent, EV_PRS1_PB);
PRS1_DURATION_EVENT(PRS1LargeLeakEvent, EV_PRS1_LL);
PRS1_DURATION_EVENT(PRS1VariableBreathingEvent, EV_PRS1_VB);
PRS1_DURATION_EVENT(PRS1HypopneaEvent, EV_PRS1_HY);

PRS1_VALUE_EVENT(PRS1TotalLeakEvent, EV_PRS1_TOTLEAK);
PRS1_VALUE_EVENT(PRS1LeakEvent, EV_PRS1_LEAK);

PRS1_PRESSURE_EVENT(PRS1AutoPressureSetEvent, EV_PRS1_AUTO_PRESSURE_SET);
PRS1_PRESSURE_EVENT(PRS1PressureSetEvent, EV_PRS1_PRESSURE_SET);
PRS1_PRESSURE_EVENT(PRS1IPAPSetEvent, EV_PRS1_IPAP_SET);
PRS1_PRESSURE_EVENT(PRS1EPAPSetEvent, EV_PRS1_EPAP_SET);
PRS1_PRESSURE_EVENT(PRS1PressureAverageEvent, EV_PRS1_PRESSURE_AVG);
PRS1_PRESSURE_EVENT(PRS1FlexPressureAverageEvent, EV_PRS1_FLEX_PRESSURE_AVG);
PRS1_PRESSURE_EVENT(PRS1IPAPAverageEvent, EV_PRS1_IPAP_AVG);
PRS1_PRESSURE_EVENT(PRS1IPAPHighEvent, EV_PRS1_IPAPHIGH);
PRS1_PRESSURE_EVENT(PRS1IPAPLowEvent, EV_PRS1_IPAPLOW);
PRS1_PRESSURE_EVENT(PRS1EPAPAverageEvent, EV_PRS1_EPAP_AVG);

PRS1_VALUE_EVENT(PRS1RespiratoryRateEvent, EV_PRS1_RR);
PRS1_VALUE_EVENT(PRS1PatientTriggeredBreathsEvent, EV_PRS1_PTB);
PRS1_VALUE_EVENT(PRS1MinuteVentilationEvent, EV_PRS1_MV);
PRS1_VALUE_EVENT(PRS1SnoreEvent, EV_PRS1_SNORE);
PRS1_VALUE_EVENT(PRS1VibratorySnoreEvent, EV_PRS1_VS);
PRS1_VALUE_EVENT(PRS1PressurePulseEvent, EV_PRS1_PP);
PRS1_VALUE_EVENT(PRS1RERAEvent, EV_PRS1_RERA);  // TODO: should this really be a duration event?
PRS1_VALUE_EVENT(PRS1FlowRateEvent, EV_PRS1_FLOWRATE);  // TODO: is this a single event or an index/hour?
PRS1_VALUE_EVENT(PRS1Test1Event, EV_PRS1_TEST1);
PRS1_VALUE_EVENT(PRS1Test2Event, EV_PRS1_TEST2);
PRS1_VALUE_EVENT(PRS1HypopneaCount, EV_PRS1_HY_COUNT);  // F3V3 only
PRS1_VALUE_EVENT(PRS1ClearAirwayCount, EV_PRS1_CA_COUNT);  // F3V3 only
PRS1_VALUE_EVENT(PRS1ObstructiveApneaCount, EV_PRS1_OA_COUNT);  // F3V3 only

PRS1_ALARM_EVENT(PRS1DisconnectAlarmEvent, EV_PRS1_DISCONNECT_ALARM);
PRS1_ALARM_EVENT(PRS1ApneaAlarmEvent, EV_PRS1_APNEA_ALARM);
PRS1_ALARM_EVENT(PRS1LowMinuteVentilationAlarmEvent, EV_PRS1_LOW_MV_ALARM);

enum PRS1Mode {
    PRS1_MODE_UNKNOWN = -1,
    PRS1_MODE_CPAPCHECK = 0,    // "CPAP-Check"
    PRS1_MODE_CPAP,             // "CPAP"
    PRS1_MODE_AUTOCPAP,         // "AutoCPAP"
    PRS1_MODE_AUTOTRIAL,        // "Auto-Trial"
    PRS1_MODE_BILEVEL,          // "Bi-Level"
    PRS1_MODE_AUTOBILEVEL,      // "AutoBiLevel"
    PRS1_MODE_ASV,              // "ASV"
    PRS1_MODE_S,                // "S"
    PRS1_MODE_ST,               // "S/T"
    PRS1_MODE_PC,               // "PC"
    PRS1_MODE_ST_AVAPS,         // "S/T - AVAPS"
    PRS1_MODE_PC_AVAPS,         // "PC - AVAPS"
};

// Returns the set of all channels ever reported/supported by the parser for the given chunk.
const QVector<PRS1ParsedEventType> & GetSupportedEvents(const class PRS1DataChunk* chunk);


//********************************************************************************************
// MARK: -
//********************************************************************************************

struct PRS1Waveform;

/*! \class PRS1DataChunk
 *  \brief Representing a chunk of event/summary/waveform data after the header is parsed. */
class PRS1DataChunk
{
public:
    /*
    PRS1DataChunk() {
        fileVersion = 0;
        blockSize = 0;
        htype = 0;
        family = 0;
        familyVersion = 0;
        ext = 255;
        sessionid = 0;
        timestamp = 0;
        
        duration = 0;

        m_filepos = -1;
        m_index = -1;
    }
    */
    PRS1DataChunk(class RawDataDevice & f, class PRS1Loader* loader);
    ~PRS1DataChunk();
    inline int size() const { return m_data.size(); }

    QByteArray m_header;
    QByteArray m_data;
    QByteArray m_headerblock;
    QList<class PRS1ParsedEvent*> m_parsedData;

    QString m_path;
    qint64 m_filepos;  // file offset
    int m_index;  // nth chunk in file
    inline void SetIndex(int index) { m_index = index; }

    // Common fields
    quint8 fileVersion;
    quint16 blockSize;
    quint8 htype;
    quint8 family;
    quint8 familyVersion;
    quint8 ext;
    SessionID sessionid;
    quint32 timestamp;

    // Waveform-specific fields
    quint16 interval_count;
    quint8 interval_seconds;
    int duration;
    QList<PRS1Waveform> waveformInfo;
    
    // V3 normal/non-waveform fields
    QMap<unsigned char, short> hblock;

    QMap<unsigned char, QByteArray> mainblock;
    QMap<unsigned char, QByteArray> hbdata;
    
    // Trailing common fields
    quint8 storedChecksum;  // header checksum stored in file, last byte of m_header
    quint8 calcChecksum;    // header checksum as calculated when parsing
    quint32 storedCrc;      // header + data CRC stored in file, last 2-4 bytes of chunk
    quint32 calcCrc;        // header + data CRC as calculated when parsing

    //! \brief Calculate a simplistic hash to check whether two chunks are identical.
    inline quint64 hash(void) const { return ((((quint64) this->calcCrc) << 32) | this->timestamp); }
    
    //! \brief Parse and return the next chunk from a PRS1 file
    static PRS1DataChunk* ParseNext(class RawDataDevice & f, class PRS1Loader* loader);

    //! \brief Read and parse the next chunk header from a PRS1 file
    bool ReadHeader(class RawDataDevice & f);

    //! \brief Read the chunk's data from a PRS1 file and calculate its CRC, must be called after ReadHeader
    bool ReadData(class RawDataDevice & f);
    
    //! \brief Figures out which Compliance Parser to call, based on device family/version and calls it.
    bool ParseCompliance(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a P25x brick
    bool ParseComplianceF0V23(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a P256x brick
    bool ParseComplianceF0V4(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a x00V brick
    bool ParseComplianceF0V5(void);
    
    //! \brief Parse a single data chunk from a .000 file containing compliance data for a DreamStation 200X brick
    bool ParseComplianceF0V6(void);
    
    //! \brief Figures out which Summary Parser to call, based on device family/version and calls it.
    bool ParseSummary();

    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 2 or 3 device
    bool ParseSummaryF0V23(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 4 device
    bool ParseSummaryF0V4(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 0 CPAP/APAP family version 6 device
    bool ParseSummaryF0V6(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 3 ventilator (family version 0 or 3) device
    bool ParseSummaryF3V03(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 3 ventilator (family version 6) device
    bool ParseSummaryF3V6(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 5 ASV family version 0-2 device
    bool ParseSummaryF5V012(void);
    
    //! \brief Parse a single data chunk from a .001 file containing summary data for a family 5 ASV family version 3 device
    bool ParseSummaryF5V3(void);

    //! \brief Parse a flex setting byte from a .000 or .001 containing compliance/summary data for CPAP/APAP family versions 2, 3, 4, or 5
    void ParseFlexSettingF0V2345(quint8 flex, int prs1mode);
    
    //! \brief Parse a flex setting byte from a .000 or .001 containing compliance/summary data for ASV family versions 0, 1, or 2
    void ParseFlexSettingF5V012(quint8 flex, int prs1mode);
    
    //! \brief Parse an humidifier setting byte from a .000 or .001 containing compliance/summary data for original System One (50-Series) devices: F0V23 and F5V0
    void ParseHumidifierSetting50Series(int humid, bool add_setting=false);

    //! \brief Parse an humidifier setting byte from a .000 or .001 containing compliance/summary data for F0V4 and F5V012 (60-Series) devices
    void ParseHumidifierSetting60Series(unsigned char humid1, unsigned char humid2, bool add_setting=false);

    //! \brief Parse an humidifier setting byte from a .000 or .001 containing compliance/summary data for F3V3 devices (differs from other 60-Series devices)
    void ParseHumidifierSettingF3V3(unsigned char humid1, unsigned char humid2, bool add_setting=false);

    //! \brief Parse humidifier setting bytes from a .000 or .001 containing compliance/summary data for fileversion 3 devices
    void ParseHumidifierSettingV3(unsigned char byte1, unsigned char byte2, bool add_setting=false);

    //! \brief Parse tubing type from a .001 containing summary data for fileversion 3 devices
    void ParseTubingTypeV3(unsigned char type);

    //! \brief Figures out which Event Parser to call, based on device family/version and calls it.
    bool ParseEvents(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 0 CPAP/APAP device
    bool ParseEventsF0V23(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a 60 Series family 0 CPAP/APAP 60 device
    bool ParseEventsF0V4(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a DreamStation family 0 CPAP/APAP device
    bool ParseEventsF0V6(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 3 ventilator family version 0 or 3 device
    bool ParseEventsF3V03(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 3 ventilator family version 6 device
    bool ParseEventsF3V6(void);
    
    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 0 device
    bool ParseEventsF5V0(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 1 device
    bool ParseEventsF5V1(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 2 device
    bool ParseEventsF5V2(void);

    //! \brief Parse a single data chunk from a .002 file containing event data for a family 5 ASV family version 3 device
    bool ParseEventsF5V3(void);

protected:
    class PRS1Loader* loader;
    
    //! \brief Add a parsed event to the chunk
    void AddEvent(class PRS1ParsedEvent* event);

    //! \brief Read and parse the non-waveform header data from a V2 PRS1 file
    bool ReadNormalHeaderV2(class RawDataDevice & f);

    //! \brief Read and parse the non-waveform header data from a V3 PRS1 file
    bool ReadNormalHeaderV3(class RawDataDevice & f);

    //! \brief Read and parse the waveform-specific header data from a PRS1 file
    bool ReadWaveformHeader(class RawDataDevice & f);

    //! \brief Extract the stored CRC from the end of the data of a PRS1 chunk
    bool ExtractStoredCrc(int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF0V23(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF0V45(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF0V6(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF5V012(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF5V3(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF3V03(const unsigned char* data, int size);

    //! \brief Parse a settings slice from a .000 and .001 file
    bool ParseSettingsF3V6(const unsigned char* data, int size);

protected:
    QString DumpEvent(int t, int code, const unsigned char* data, int size);
};


#define DUMP_EVENT() qWarning() << this->sessionid << DumpEvent(t, code, data + pos, size - (pos - startpos)) + "  @ " + QString("0x") + QString::number(startpos-1, 16).toUpper()

enum FlexMode { FLEX_None, FLEX_CFlex, FLEX_CFlexPlus, FLEX_AFlex, FLEX_RiseTime, FLEX_BiFlex, FLEX_PFlex, FLEX_Flex, FLEX_Unknown = -1  };

enum BackupBreathMode { PRS1Backup_Off, PRS1Backup_Auto, PRS1Backup_Fixed };

enum HumidMode { HUMID_Fixed, HUMID_Adaptive, HUMID_HeatedTube, HUMID_Passover, HUMID_Error };


const int PRS1_HTYPE_NORMAL=0;
const int PRS1_HTYPE_INTERVAL=1;


extern const QVector<PRS1ParsedEventType> ParsedEventsF0V23;
extern const QVector<PRS1ParsedEventType> ParsedEventsF0V4;
extern const QVector<PRS1ParsedEventType> ParsedEventsF0V6;

extern const QVector<PRS1ParsedEventType> ParsedEventsF3V0;
extern const QVector<PRS1ParsedEventType> ParsedEventsF3V3;
extern const QVector<PRS1ParsedEventType> ParsedEventsF3V6;

extern const QVector<PRS1ParsedEventType> ParsedEventsF5V0;
extern const QVector<PRS1ParsedEventType> ParsedEventsF5V1;
extern const QVector<PRS1ParsedEventType> ParsedEventsF5V2;
extern const QVector<PRS1ParsedEventType> ParsedEventsF5V3;

#endif // PRS1PARSER_H
