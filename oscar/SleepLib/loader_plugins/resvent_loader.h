/* SleepLib Resvent Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef RESVENT_LOADER_H
#define RESVENT_LOADER_H

#include <QVector>
#include <QFile>
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"
class EventList;

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int resvent_data_version = 2;
//
//********************************************************************************************

const QString resvent_class_name = "Resvent";

enum class EventType {
    //UsageSec = 1,
    //UnixStart = 2,
    ObstructiveApnea = 17,
    CentralApnea = 18,
    Hypopnea = 19,
    FlowLimitation = 20,
    RERA = 21,
    PeriodicBreathing = 22,
    Snore = 23
};

struct EventData {
    EventType type;
    QDateTime date_time;
    int duration;
};


struct ResVentUsageData {
    QString number{};
    QDateTime start_time{};
    QDateTime end_time{};
    qint32 countAHI = 0;
    qint32 countOAI = 0;
    qint32 countCAI = 0;
    qint32 countAI = 0;
    qint32 countHI = 0;
    qint32 countRERA = 0;
    qint32 countSNI = 0;
    qint32 countBreath = 0;
    qint32 PressureMin = 0;
    qint32 PressureMax = 0;
};

struct ChunkData {
    EventList* event_list;
    uint16_t samples_by_chunk;
    qint64 start_time;
    int total_samples_by_chunk;
    float sample_rate;
};


class ResVentUsageData2 {
public:
    ResVentUsageData2();
    virtual ~ResVentUsageData2();
    QString number{};
    QDateTime start_time{};
    QDateTime end_time{};
    qint32 countAHI = 0;
    qint32 countOAI = 0;
    qint32 countCAI = 0;
    qint32 countAI = 0;
    qint32 countHI = 0;
    qint32 countRERA = 0;
    qint32 countSNI = 0;
    qint32 countBreath = 0;
};

/*! \class ResventLoader
    \brief Importer for Resvent iBreezer and Hoffrichter Point 3
    */
class ResventLoader : public CPAPLoader
{
    Q_OBJECT
public:
    ResventLoader();
    virtual ~ResventLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Look up machine model information of ResMed file structure stored at path
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans for ResMed SD folder structure signature, and loads any new data if found
    virtual int Open(const QString &);

    //! \brief Returns the version number of this Resvent loader
    virtual int Version() { return resvent_data_version; }

    //! \brief Returns the Machine class name of this loader. ("Resvent")
    virtual const QString &loaderName() { return resvent_class_name; }

    //! \brief Register the ResmedLoader with the list of other machine loaders
    static void Register();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, resvent_class_name, QObject::tr("Resvent"), QString(), QString(), QString(), QObject::tr("iBreeze"), QDateTime::currentDateTime(), resvent_data_version);
    }

    virtual void initChannels();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    virtual QString PresReliefLabel() { return QObject::tr("IPR: "); }

    virtual ChannelID PresReliefMode();
    virtual ChannelID PresReliefLevel();
    virtual ChannelID CPAPModeChannel();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
    // OSCAR CPAP MODE
    enum CPAPMode myCPAPMode = MODE_UNKNOWN;

    //Resvent Loader Mode
    //These must start at zero. and increment. order is required for initChannels function
    enum RESVENT_PAP_MODE {
        RESVENT_PAP_CPAP = 0  
        , RESVENT_PAP_APAP = 1
        , RESVENT_PAP_S30 = 2
        , RESVENT_PAP_AUTO_S30 = 3
        , RESVENT_PAP_ST30 = 4
        , RESVENT_PAP_AUTO_ST30 = 5
        , RESVENT_PAP_T30 = 6
        , RESVENT_PAP_PC = 7
    };
    enum RESVENT_PAP_MODE myRESVENT_PAP_MODE = RESVENT_PAP_CPAP;

    // Revent Device Mode. value defined in file CONFIG/TCTRL VentMOde
    enum RESVENT_DEVICE_MODE{
        RESVENT_DEVICE_CPAP = 1,
        RESVENT_DEVICE_APAP = 3,
        RESVENT_DEVICE_S30 = 10,
        RESVENT_DEVICE_AUTO_S30 = 11,
        RESVENT_DEVICE_ST30 = 12,
        RESVENT_DEVICE_AUTO_ST30 = 13,
        RESVENT_DEVICE_T30 = 14,
        RESVENT_DEVICE_PC = 15
    };

    QMap<QString,QString> configSettings ;

    void readAllConfigFiles(const QString & homePath ,QMap<QString,QString>& ) ;
    void readConfigFile(const QString & configFile, QMap<QString,QString>&);
    QVector<QDate> GetSessionsDate(const QString& dirpath);
    QString GetSessionFolder(const QString& dirpath, const QDate& session_date);
    int LoadSession(const QString& dirpath, const QDate& session_date, Machine* machine);
    void LoadEvents(const QString& session_folder_path, Session* session, const ResVentUsageData& usage );
    void UpdateEvents(EventType event_type, const QMap<EventType, QVector<EventData>>& events, Session* session);
    bool VerifyEvent(EventData& eventData);
    EventList* GetEventList(const QString& name, Session* session, float sample_rate = 0.0) ;
    void ReadWaveFormsHeaders(QFile& f, QVector<ChunkData>& wave_forms, Session* session, const ResVentUsageData& usage);
    void LoadOtherWaveForms(const QString& session_folder_path, Session* session, const ResVentUsageData& usage);
    void LoadWaveForms(const QString& session_folder_path, Session* session, const ResVentUsageData& usage);
    void LoadStats(const ResVentUsageData& /*usage_data*/, Session* session );
    ResVentUsageData ReadUsage(const QString& session_folder_path, const QString& usage_number);
    QVector<ResVentUsageData> GetDifferentUsage(const QString& session_folder_path);
};

#endif // RESVENT_LOADER_H
