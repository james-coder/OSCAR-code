/* SleepLib Löwenstein Prisma Loader Header
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PRISMA_LOADER_H
#define PRISMA_LOADER_H
#include "SleepLib/machine_loader.h"
#include "SleepLib/loader_plugins/edfparser.h"
class QDir;

#ifdef UNITTEST_MODE
#define private public
#define protected public
#endif

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation
// BEFORE making a release
const int prisma_data_version = 1;
//
//********************************************************************************************
const QString prisma_class_name = STR_MACH_Prisma;

//********************************************************************************************

// NOTE: Prisma Smart and Prisma Line devices use distinct parameter id-s, but the correct solution
// would be to transform these, into separate enums / parsers.
enum Prisma_Parameters {
    PRISMA_SMART_MODE = 6,
    PRISMA_SMART_PRESSURE = 9,
    PRISMA_SMART_PRESSURE_MAX = 10,
    PRISMA_SMART_PSOFT_MIN = 11,
    PRISMA_SMART_PSOFT = 12,
    PRISMA_SMART_SOFTPAP = 13,
    PRISMA_SMART_APAP_DYNAMIC = 15,
    PRISMA_SMART_HUMIDLEVEL = 16,
    PRISMA_SMART_AUTOSTART = 17,
    PRISMA_SMART_SOFTSTART_TIME_MAX = 18,
    PRISMA_SMART_SOFTSTART_TIME = 19,
    PRISMA_SMART_TUBE_TYPE = 21,
    PRISMA_SMART_PMAXOA = 38,

    PRISMA_LINE_MODE = 1003,
    PRISMA_LINE_TI = 1011,
    PRISMA_LINE_TE = 1012,
    PRISMA_LINE_TARGET_VOLUME = 1016,
    PRISMA_LINE_IPAP_SPEED = 1017,
    PRISMA_LINE_HUMIDLEVEL = 1083,
    PRISMA_LINE_AUTOSTART = 1084,
    PRISMA_LINE_TUBE_TYPE = 1091,
    PRISMA_LINE_BACTERIUMFILTER = 1092,
    PRISMA_LINE_SOFT_PAP_LEVEL = 1123,
    PRISMA_LINE_SOFT_START_PRESS = 1125,
    PRISMA_LINE_SOFT_START_TIME = 1127,
    PRISMA_LINE_EEPAP_MIN = 1138,
    PRISMA_LINE_EEPAP_MAX = 1139,
    PRISMA_LINE_PDIFF_NORM = 1140,
    PRISMA_LINE_PDIFF_MAX = 1141,
    PRISMA_LINE_IPAP_MAX = 1199,
    PRISMA_LINE_IPAP = 1200,
    PRISMA_LINE_EPAP = 1201,
    // PRISMA_LINE_ALARM_LEAK_ACTIVE = 1202,
    // PRISMA_LINE_ALARM_DISCONNECTION_ACTIVE = 1203,
    PRISMA_LINE_APAP_DYNAMIC = 1209,
    PRISMA_LINE_EXTRA_OBSTRUCTION_PROTECTION = 1154, // BiSoft off = 0, BiSoft1 = 2, BiSoft2 = 3, TriLevel = 1
    PRISMA_LINE_AUTO_PDIFF = 1219

};

// NOTE: Modes should be reverse engineered. Based on the samples we had, for now I didn't saw any overlap,
// so I assumed, the mode settings is generic between the two device lines. If you run into an overlap, the
// above mentioned parsers should be introduced. Enum values are coming from the prisma data files.
enum Prisma_Mode {
    // Prisma Smart
    PRISMA_MODE_CPAP = 1,
    PRISMA_MODE_APAP = 2,

    // Prisma Line
    PRISMA_MODE_ACSV = 3,
    PRISMA_MODE_AUTO_S = 9,
    PRISMA_MODE_AUTO_ST = 10,
};

enum Prisma_APAP_Mode {
    PRISMA_APAP_MODE_STANDARD = 1,
    PRISMA_APAP_MODE_DYNAMIC = 2,    
};

enum Prisma_SoftPAP_Mode {
    Prisma_SoftPAP_OFF = 0,
    Prisma_SoftPAP_SLIGHT = 1,
    Prisma_SoftPAP_STANDARD = 2
};

enum Prisma_BiSoft_Mode {
    Prisma_BiSoft_Off = 0,
    Prisma_BiSoft_1 = 2,
    Prisma_BiSoft_2 = 3,
    Prisma_TriLevel = 1,
};

// NOTE: This enum represents a "virtual mode" which combines the main mode of the device with the APAP submode,
// if it makes sense. The reason for this is, that we can see the Standard and Dynamic APAP modes on the statistics
// page. Enum values are internal to the loader. We use -1 to indicate a mode that is not recognized.
enum Prisma_Combined_Mode {
    PRISMA_COMBINED_MODE_CPAP = 1,
    PRISMA_COMBINED_MODE_APAP_STD = 2,
    PRISMA_COMBINED_MODE_APAP_DYN = 3,

    PRISMA_COMBINED_MODE_AUTO_S = 4,
    PRISMA_COMBINED_MODE_AUTO_ST = 5,
    PRISMA_COMBINED_MODE_ACSV = 6,

    PRISMA_COMBINED_MODE_UNKNOWN = -1,
};

enum Prisma_Event_Type {
    PRISMA_EVENT_EPOCH_SEVERE_OBSTRUCTION = 1,
    PRISMA_EVENT_EPOCH_MILD_OBSTRUCTION = 2,
    PRISMA_EVENT_EPOCH_FLOW_LIMITATION = 3,
    PRISMA_EVENT_EPOCH_SNORE = 4,
    PRISMA_EVENT_EPOCH_PERIODIC_BREATHING  = 5,
    PRISMA_EVENT_OBSTRUCTIVE_APNEA = 101,
    PRISMA_EVENT_CENTRAL_APNEA = 102,
    PRISMA_EVENT_APNEA_LEAKAGE = 103,
    PRISMA_EVENT_APNEA_HIGH_PRESSURE = 105,
    PRISMA_EVENT_APNEA_MOVEMENT = 106,
    PRISMA_EVENT_OBSTRUCTIVE_HYPOPNEA= 111,
    PRISMA_EVENT_CENTRAL_HYPOPNEA = 112,
    PRISMA_EVENT_HYPOPNEA_LEAKAGE = 113,
    PRISMA_EVENT_RERA = 121,
    PRISMA_EVENT_SNORE = 131,
    PRISMA_EVENT_ARTIFACT = 141,
    PRISMA_EVENT_FLOW_LIMITATION = 151,
    PRISMA_EVENT_CRITICAL_LEAKAGE = 161,
    PRISMA_EVENT_CS_RESPIRATION = 181,
    PRISMA_EVENT_TIMED_BREATH = 221,
    PRISMA_EVENT_EPOCH_DEEPSLEEP = 261,
};



//********************************************************************************************

// Prisma WMEDF differs from the original EDF, by introducing 8 bit signal data channels.
class WMEDFInfo : public EDFInfo {
    virtual bool ParseSignalData();

  protected:
    qint8 Read8S();
    quint8 Read8U();

};

//********************************************************************************************

class PrismaLoader;
class PrismaEventFile;

/*! \class PrismaImport
 *  \brief Contains the functions to parse a single session... multithreaded */
class PrismaImport:public ImportTask
{
public:
    PrismaImport(PrismaLoader * l, const MachineInfo& m, SessionID s, QByteArray e, QByteArray d): loader(l), machineInfo(m), sessionid(s), eventData(e), signalData(d) {}
    virtual ~PrismaImport() {};

    //! \brief PrismaImport thread starts execution here.
    virtual void run();

protected:    
    PrismaLoader * loader;
    const MachineInfo & machineInfo;
    SessionID sessionid;
    QByteArray eventData;
    QByteArray signalData;
    qint64 startdate;
    qint64 enddate;
    WMEDFInfo wmedf;
    PrismaEventFile * eventFile;
    Session * session;

    void AddWaveform(ChannelID code, QString edfLabel);
    void AddEvents(ChannelID channel, Prisma_Event_Type eventType) {
        QList<Prisma_Event_Type> eventTypes = { eventType };
        AddEvents(channel, eventTypes);
    }
    void AddEvents(ChannelID channel, QList<Prisma_Event_Type> eventTypes);

};

//********************************************************************************************

/*! \class PrismaLoader
    \brief Löwenstein Prisma Loader Module
    */
class PrismaLoader : public CPAPLoader
{
    Q_OBJECT
    static bool initialized;
  public:
    PrismaLoader();
    virtual ~PrismaLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Load MachineInfo structure.
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans directory path for valid Prisma signature
    virtual int Open(const QString & path);

    //! \brief Returns the database version of this loader
    virtual int Version() { return prisma_data_version; }

    //! \brief Return the loaderName, in this case "Prisma"
    virtual const QString &loaderName() { return prisma_class_name; }

    //! \brief Register this Module to the list of Loaders, so it knows to search for Prisma data.
    static void Register();

    //! \brief Generate a generic MachineInfo structure, with basic Prisma info to be expanded upon.
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, prisma_class_name, QObject::tr("Löwenstein"), QObject::tr("Prisma Smart"), QString(), QString(), QObject::tr(""), QDateTime::currentDateTime(), prisma_data_version);
    }

    virtual QString PresReliefLabel();
    virtual ChannelID CPAPModeChannel();
    virtual ChannelID PresReliefMode();


    //! \brief Called at application init, to set up any custom Prisma Channels
    virtual void initChannels();

    QHash<SessionID, PrismaImport*> sesstasks;

  protected:

    MachineInfo PeekInfoFromConfig(const QString & selectedPath);

    void ImportDataDir(QDir& dataDir, QSet<SessionID>& sessions, QHash<SessionID, QString>& eventFiles, QHash<SessionID, QString>& signalFiles);

    //! \brief Scans the given directories for session data and create an import task for each logical session.
    void ScanFiles(const MachineInfo& info, const QString & path);
};

//********************************************************************************************
class PrismaEvent
{
public:
    PrismaEvent(int endTime, int duration, int pressure, int strength) : m_endTime(endTime), m_duration(duration), m_pressure(pressure), m_strenght(strength) {}
    int endTime() { return m_endTime; }
    int duration() { return m_duration; }
    int strength() { return m_strenght; }
protected:
    int m_endTime;
    int m_duration;
    int m_pressure;
    int m_strenght;
};

class PrismaEventFile
{
public:
    PrismaEventFile(QByteArray &buffer);
    QHash<int, int> getParameters() {return m_parameters; }
    QList<PrismaEvent> getEvents(int eventId) {return m_events.contains(eventId) ? m_events[eventId] : QList<PrismaEvent>(); }

protected:
    QHash<int, int> m_parameters;
    QHash<int, QList<PrismaEvent>> m_events;
};

//********************************************************************************************

class PrismaModelInfo
{
protected:
    QHash<QString,const char*> m_modelNames;

public:
    PrismaModelInfo();
    bool IsTested(const QString &  deviceId) const;
    const char* Name(const QString & deviceId) const;
};

#endif // PRISMA_LOADER_H
