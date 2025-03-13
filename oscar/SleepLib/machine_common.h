/* SleepLib Common Device Header
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef MACHINE_COMMON_H
#define MACHINE_COMMON_H

#include <QHash>
#include <QColor>
#include <QVector>
#include <QVariant>
#include <QString>
#include <QDateTime>
#include <QDebug>
#include <QDate>

using namespace std;

// Do not change these without considering the consequences.. For one the Loader needs changing & version increase
typedef quint32 ChannelID;
typedef quint32 MachineID;
typedef quint32 SessionID;
typedef float EventDataType;
typedef qint16 EventStoreType;
extern QString toHexid(quint32);

//! \brief Exception class for out of Bounds error.. Unused.
class BoundsError {};

//! \brief Exception class for to trap old database versions.
class OldDBVersion {};

const quint32 magic = 0xC73216AB; // Magic number for OSCAR Data Files.. Don't touch!

//const int max_number_event_fields=10;
// This should probably move somewhere else
//! \fn timezoneOffset();
//! \brief Calculate the timezone Offset in milliseconds between system timezone and UTC
qint64 timezoneOffset();


/*! \enum SummaryType
    \brief Calculation/Display method to select from dealing with summary information
  */
enum SummaryType { ST_CNT, ST_SUM, ST_AVG, ST_WAVG, ST_PERC, ST_90P, ST_MIN, ST_MAX, ST_MID, ST_CPH, ST_SPH, ST_FIRST, ST_LAST, ST_HOURS, ST_SESSIONS, ST_SETMIN, ST_SETAVG, ST_SETMAX, ST_SETWAVG, ST_SETSUM, ST_SESSIONID, ST_DATE };

/*! \enum MachineType
    \brief Generalized type of a device. MT_CPAP is any type of xPAP device, MT_OXIMETER any type of Oximeter
    \brief MT_SLEEPSTAGE stage of sleep detector (ZEO importer), MT_JOURNAL for optional notes, MT_POSITION for sleep position detector (Somnopose)
  */
// TODO: This really needs to be a bitmask, since there are increasing numbers of devices that provide
// multiple kinds of data, such as oximetry + motion/position, or sleep stage + oximetry, etc.
//
// Device/loader classes will use the bitmask to identify which data they are capable of importing.
// It may be that we ultimately prefer to have each device identify a primary type instead or in addition.
//
// The channel schema's use of these is probably fine.
//
// Days/Sessions/etc. that currently search for data based on the devices they contain will instead
// need to search for channels with data of that MT type. And anywhere else the code makes decisions
// based on MT.
//
// Unfortunately, this also includes previously imported data, as Session encodes the device's type in
// each file on disk. We might be partially saved by the fact that MT_CPAP and MT_OXIMETER were originally
// 1 and 2, which would only break MT_SLEEPSTAGE and higher.
enum MachineType { MT_UNKNOWN = 0, MT_CPAP, MT_OXIMETER, MT_SLEEPSTAGE, MT_JOURNAL, MT_POSITION, MT_UNCATEGORIZED = 99};
//void InitMapsWithoutAwesomeInitializerLists();

/***** NEVER USED --- 8/2019
// PAP Device Capabilities
const quint32 CAP_Fixed               = 0x0000001;  // Constant PAP
const quint32 CAP_Variable            = 0x0000002;  // Variable Base (EPAP) pressure
const quint32 CAP_BiLevel             = 0x0000004;  // Fixed Pressure Support
const quint32 CAP_Variable_PS         = 0x0000008;  // Pressure support can range
const quint32 CAP_PressureRelief      = 0x0000010;  // Device has a pressure relief mode (EPR; Flex; SmartFlex)
const quint32 CAP_Humidification      = 0x0000020;  // Device has a humidifier attached

// PAP Mode Capabilities
const quint32 PAP_CPAP                    = 0x0001;  // Fixed Pressure PAP
const quint32 PAP_APAP                    = 0x0002;  // Auto Ranging PAP
const quint32 PAP_BiLevelFixed            = 0x0004;  // Fixed BiLevel
const quint32 PAP_BiLevelAutoFixed        = 0x0008;  // Auto BiLevel with Fixed EPAP
const quint32 PAP_BiLevelAutoVariable     = 0x0010;  // Auto BiLevel with full ranging capabilities
const quint32 PAP_ASV_Fixed               = 0x0020;  // ASV with fixed EPAP
const quint32 PAP_ASV_Variable            = 0x0040;  // ASV with full ranging capabilities
const quint32 PAP_SplitNight              = 0x8000;  // Split night capabilities
*****/


/*! \enum CPAPMode
    \brief CPAP Machines mode of operation
  */
enum CPAPMode { //:short
    MODE_UNKNOWN = 0, MODE_CPAP, MODE_APAP, MODE_BILEVEL_FIXED, MODE_BILEVEL_AUTO_FIXED_PS, MODE_BILEVEL_AUTO_VARIABLE_PS, MODE_ASV, MODE_ASV_VARIABLE_EPAP, MODE_AVAPS, MODE_TRILEVEL_AUTO_VARIABLE_PDIFF
};

/*! \enum PRTypes
    \brief Pressure Relief Types, used by CPAP devices
  */
enum PRTypes { //:short
    PR_UNKNOWN = 0, PR_NONE, PR_CFLEX, PR_CFLEXPLUS, PR_AFLEX, PR_BIFLEX, PR_EPR, PR_SMARTFLEX, PR_EASYBREATHE, PR_SENSAWAKE
};
enum PRTimeModes { //:short
    PM_UNKNOWN = 0, PM_RampOnly, PM_FullTime
};


struct MachineInfo {
    MachineInfo() { type = MT_UNKNOWN; version = 0; cap=0; }
    MachineInfo(const MachineInfo & /*copy*/) = default;
    ~MachineInfo() {};

    MachineInfo(MachineType type, quint32 cap, QString loadername, QString brand, QString model, QString modelnumber, QString serial, QString series, QDateTime lastimported, int version, QDate purgeDate = QDate()) :
        type(type), cap(cap), loadername(loadername), brand(brand), model(model), modelnumber(modelnumber), serial(serial), series(series), lastimported(lastimported), version(version), purgeDate(purgeDate) {}

    MachineType type;
    quint32 cap;
    QString loadername;
    QString brand;
    QString model;
    QString modelnumber;
    QString serial;
    QString series;
    QDateTime lastimported;
    int version;
    QDate purgeDate;

    //! \brief List of text device properties, like brand, model, etc...
    QHash<QString, QString> properties;
};


//extern map<ChannelID,QString> DefaultMCShortNames;
//extern map<ChannelID,QString> DefaultMCLongNames;
//extern map<PRTypes,QString> PressureReliefNames;
//extern map<CPAPMode,QString> CPAPModeNames;

/*! \enum MCDataType
    \brief Data Types stored by Profile/Preferences objects, etc..
    */

enum MCDataType
{ MC_bool = 0, MC_int, MC_long, MC_float, MC_double, MC_string, MC_datetime };

extern ChannelID AllAhiChannels;
extern QVector<ChannelID> ahiChannels;

extern ChannelID NoChannel, SESSION_ENABLED, CPAP_SummaryOnly;
extern ChannelID CPAP_IPAP, CPAP_IPAPLo, CPAP_IPAPHi, CPAP_EPAP, CPAP_EPAPLo, CPAP_EPAPHi, CPAP_EEPAP, CPAP_EEPAPLo, CPAP_EEPAPHi,
       CPAP_Pressure, CPAP_PS, CPAP_PSMin, CPAP_PSMax,
       CPAP_Mode, CPAP_AHI,
       CPAP_PressureMin, CPAP_PressureMax, CPAP_Ramp, CPAP_RampTime, CPAP_RampPressure, CPAP_Obstructive,
       CPAP_Hypopnea, CPAP_AllApnea,
       CPAP_ClearAirway, CPAP_Apnea, CPAP_PB, CPAP_CSR, CPAP_LeakFlag, CPAP_ExP, CPAP_NRI, CPAP_VSnore,
       CPAP_VSnore2,
       CPAP_RERA, CPAP_PressurePulse, CPAP_FlowLimit, CPAP_SensAwake, CPAP_FlowRate, CPAP_MaskPressure,
       CPAP_MaskPressureHi,
       CPAP_RespEvent, CPAP_Snore, CPAP_MinuteVent, CPAP_RespRate, CPAP_TidalVolume, CPAP_PTB, CPAP_Leak,
       CPAP_LeakMedian, CPAP_LeakTotal, CPAP_MaxLeak, CPAP_FLG, CPAP_IE, CPAP_Te, CPAP_Ti, CPAP_TgMV,
       CPAP_UserFlag1, CPAP_UserFlag2, CPAP_UserFlag3, /*CPAP_BrokenSummary, CPAP_BrokenWaveform,*/ CPAP_RDI,
       CPAP_PresReliefMode, CPAP_PresReliefLevel, CPAP_Test1, CPAP_Test2,
       CPAP_PressureSet, CPAP_IPAPSet, CPAP_EPAPSet;

extern ChannelID RMS9_E01, RMS9_E02, RMS9_SetPressure, RMS9_MaskOnTime;
extern ChannelID CPAP_LargeLeak, PRS1_BND,
       PRS1_FlexMode, PRS1_FlexLevel, PRS1_HumidStatus, PRS1_HumidLevel, PRS1_HumidTargetTime, PRS1_MaskResistLock,
       CPAP_HumidSetting,
       PRS1_MaskResistSet, PRS1_HoseDiam, PRS1_AutoOn, PRS1_AutoOff, PRS1_MaskAlert, PRS1_ShowAHI;

extern ChannelID INTELLIPAP_Unknown1, INTELLIPAP_Unknown2, INTP_SnoreFlag;

//extern ChannelID SS_SenseAwakeLevel, SS_EPR, SS_EPRLevel, SS_Ramp;

extern ChannelID OXI_Pulse, OXI_SPO2, OXI_Perf, OXI_PulseChange, OXI_SPO2Drop, OXI_Plethy;

extern ChannelID Journal_Notes, Journal_Weight, Journal_BMI, Journal_ZombieMeter, Bookmark_Start,
       Bookmark_End, Bookmark_Notes, LastUpdated;

extern ChannelID ZEO_SleepStage, ZEO_ZQ, ZEO_TotalZ, ZEO_TimeToZ, ZEO_TimeInWake, ZEO_TimeInREM,
       ZEO_TimeInLight, ZEO_TimeInDeep, ZEO_Awakenings,
       ZEO_AlarmReason, ZEO_SnoozeTime, ZEO_WakeTone, ZEO_WakeWindow, ZEO_AlarmType, ZEO_MorningFeel,
       ZEO_FirmwareVersion,
       ZEO_FirstAlarmRing, ZEO_LastAlarmRing, ZEO_FirstSnoozeTime, ZEO_LastSnoozeTime, ZEO_SetAlarmTime,
       ZEO_RiseTime;

extern ChannelID POS_Orientation, POS_Inclination, POS_Movement;

extern ChannelID BMC_PressureWave, BMC_FlowAbnormality, BMC_IE_Ratio;

const QString GRP_CPAP = "CPAP";
const QString GRP_POS = "POS";
const QString GRP_OXI = "OXI";
const QString GRP_JOURNAL = "JOURNAL";
const QString GRP_SLEEP = "SLEEP";


#endif // MACHINE_COMMON_H
