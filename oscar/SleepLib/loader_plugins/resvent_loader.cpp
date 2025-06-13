/* SleepLib Resvent Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

// Turn off for offical release.
#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

//********************************************************************************************
// Please only INCREMENT the resvent_data_version in resvent_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

/*
Resvent     iBreeze 20A     works.
possible machines taht might work
Hoffrichter Point 2 Auto CPAP:
Hoffrichter Trend II Bilevel ST20/30:

SD-CARD Format and information
Folder Name                  Folders containing folders.
THERAPY                             All files on the sdcard are in this folder
THERAPY/RECORD                      all PAP data per day. contains a list of folder by year and month.
                                    example of contents:    202312/  202401/  202402/

THERAPY/LOG                         Logs of device events. like settings per day.  contains a list of folders.
                                    example of contents:    14/  17/

THERAPY/CFGDUP                      ??? ALways empty

THERAPY/RECORD/<Month>              contains a list of days. 01 - 31
                                    example of contents:   01/  02/  03/  04/  05/  06/  07/  08/	09/  10/

THERAPY/LOG/<xxxx>                  contains a list of folders NNNNNNNNN
                                    03260800*
                                    03347200*
                                    03433600*

Folder Name                  Folder containing Files
THERAPY/CONFIG                      configuration Files. all files are name/value pars
                                    Therphy Configuration files
                                        The name format is  T_XXXX where T indicates a Class and XXXX is a specific version
                                        examples C_ST30 F_COPD H_AST30 N_APAP N_CPAP ...
                                    there are other unique file
                                        examples SETTING SYSCFG TCTRL ALARM CHECK.TXT COMFORT
                                    The set of all keys used is all the unique files plus one theraphy  file
                                    TCTRL / VentMode   seems to determine which Therphy COnfiguration to use. need verification

THERAPY/RECORD/<Month>/<day>        Contains a list of files
                                    STAT        Summaries the day; count of events and other info
                                    All the othere file are sessions. starting with 1
                                    ALM<session>
                                    EV<session>
                                        ID=20,DT=1707490206,DR=3,GD=0,
                                    STAT<session>
                                        Name/Value pair session summary counts and other info
                                    Channel Data. two types (25 samples/second) or (2-4 seconds per sample)
                                    W<session>_<chunk> 25 samples per second  Pressure and Flow
                                    P<session>_<chunk> 2-4 seconds per sample other channels..


THERAPY/LOG/<xxxx>/<NNNNNN>         Contains a list of log files.
                                    Each log file has  dateTime followed by the event.
                                    for example
                                    File: 17/04124800
                                        2024/01/02 00:12:25 [STAT MAIN]: close record.
                                        2024/01/02 00:12:27 [STAT MAIN]: sdcard is inserting
                                        2024/01/02 00:16:38 [STAT MAIN]: open record.
                                        2024/01/02 00:18:19 [STAT MAIN]: close record.
                                        2024/01/02 00:18:21 [STAT MAIN]: sdcard is inserting
                                        2024/01/02 00:18:58 comfortRampPress3.00
                                        2024/01/02 00:21:21 [STAT MAIN]: open record.
                                        2024/01/02 04:39:09 [STAT MAIN]: close record.
                                        2024/01/02 04:39:11 [STAT MAIN]: sdcard is inserting
                                        2024/01/02 12:00:00 [STAT MAIN]: sdcard is inserting
                                        2024/01/02 22:30:06 [STAT MAIN]: sdcard is inserting
                                        2024/01/02 22:39:12 comfortMaskType0
                                        2024/01/02 22:42:16 [STAT MAIN]: open record.
                                    File: 17/04211200 - next in order
                                        2024/01/03 00:40:36 [STAT MAIN]: close record.
                                        2024/01/03 00:40:38 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 00:47:16 [STAT MAIN]: open record.
                                        2024/01/03 05:58:40 [STAT MAIN]: close record.
                                        2024/01/03 05:58:42 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 05:58:47 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 12:00:00 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 21:46:10 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 21:46:16 ComfortmaskFitlaunch
                                        2024/01/03 21:46:56 ComfortmaskFitlaunch
                                        2024/01/03 21:48:05 ComfortmaskFitlaunch
                                        2024/01/03 21:49:51 ComfortmaskFitlaunch
                                        2024/01/03 21:51:44 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 21:51:48 ComfortmaskFitlaunch
                                        2024/01/03 22:23:58 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 22:24:13 comfortMaskType2
                                        2024/01/03 22:24:28 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 22:57:38 [STAT MAIN]: open record.
                                        2024/01/03 23:02:52 [STAT MAIN]: close record.
                                        2024/01/03 23:02:54 [STAT MAIN]: sdcard is inserting
                                        2024/01/03 23:08:32 [STAT MAIN]: open record.
                                    Example: of configuration changes.
                                        2023/12/23 05:25:02 settingLanguage2
                                        2023/12/23 05:25:29 settingBrightness1
                                        2023/12/23 05:27:42 ComfortmaskFitlaunch
                                        2023/12/23 05:28:26 comfortRampTime60
                                        2023/12/23 05:28:35 comfortMaskType2
                                        2023/12/23 05:28:48 ComfortmaskFitlaunch
                                        2023/12/23 17:38:43 comfortTubeType0
                                        2023/12/23 17:46:22 comfortiPR3
                                        2023/12/23 22:48:20 comfortiPR1
                                        2023/12/23 22:48:43 comfortiPR2
                                        2023/12/23 22:50:17 comfortiPR3
                                        2023/12/23 22:53:38 comfortAutoStart16
                                        2023/12/23 22:53:43 comfortAutoStart0
                                        2023/12/23 23:14:25 ComfortmaskFitlaunch
                                        2023/12/23 23:15:01 comfortRampPress3.00
                                        2023/12/23 23:15:07 comfortRampTime40
                                        2023/12/24 22:54:10 comfortHumidity2
                                        2023/12/24 23:01:03 ComfortmaskFitlaunch
                                        2023/12/26 21:03:34 comfortiPR2
                                        2023/12/26 21:03:54 comfortRampTime20
                                        2023/12/27 22:13:35 comfortAutoStart1
                                        2023/12/27 22:13:52 comfortRampPress3.50
                                        2023/12/27 22:14:00 comfortRampTime10
                                        2023/12/29 22:34:43 comfortRampPress3.50
                                        2023/12/29 22:35:08 comfortHumidity3
                                        2023/12/30 22:39:25 ComfortmaskFitlaunch
                                        2023/12/30 22:39:47 comfortMaskType0
                                        2023/12/31 22:46:12 comfortMaskType2

*/


#include <QCoreApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDebug>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <cmath>

#include "resvent_loader.h"

#ifdef DEBUG_EFFICIENCY
#include <QElapsedTimer>  // only available in 4.8
#endif

// Files WXX_XX contain flow rate and pressure and PXX_XX contain Pressure, IPAP, EPAP, Leak, Vt, MV, RR, Ti, IE, Spo2, PR
// Both files contain a little header of size 0x24 bytes. In offset 0x12 contain the total number of different records in
// the different files of the same type. And later contain the previous describe quantity of description header of size 0x20
// containing the details for every type of record (e.g. sample chunk size).

// each device setting listed in daily left sidebar  "Device settings" must have its ownn channel.

ChannelID RESVENT_iPR, RESVENT_iPRLevel, RESVENT_Mode, RESVENT_SmartStart
    /* examples of setting for resmed renamed for resvent.
     RESVENT_HumidStatus, RESVENT_HumidLevel,
     RESVENT_PtAccess, RESVENT_Mask, RESVENT_ABFilter, RESVENT_ClimateControl, RESVENT_TubeType,
     RESVENT_Temp, RESVENT_TempEnable, RESVENT_RampEnable*/
     ;

ResventLoader::ResventLoader()
{
    const QString RESVENT_ICON = ":/icons/resvent.png";

    QString s = newInfo().series;
    m_pixmap_paths[s] = RESVENT_ICON;
    m_pixmaps[s] = QPixmap(RESVENT_ICON);

    m_type = MT_CPAP;
}
ResventLoader::~ResventLoader()
{
}

const QString kResventTherapyFolder = "THERAPY";
const QString kResventConfigFolder = "CONFIG";
const QString kResventRecordFolder = "RECORD";
const QString kResventLogFolder = "LOG";
const QString kResventSysConfigFilename = "SYSCFG";
constexpr qint64 kDateTimeOffset = 8 * 60 * 60 * 1000; // Offset to GMT
constexpr int kMainHeaderSize = 0x24;
constexpr int kDescriptionHeaderSize = 0x20;
constexpr int kChunkDurationInSecOffset = 0x10;
constexpr int kDescriptionCountOffset = 0x12;
constexpr int kDescriptionSamplesByChunk = 0x1e;
constexpr double kMilliGain = 0.001;
constexpr double kHundredthGain = 0.01;
constexpr double kTenthGain = 0.1;
constexpr double kNoGain = 1.0;

constexpr double kDefaultGain = kHundredthGain ;           // For Flow (rate) and (mask)Pressure - High Resolutions data.
const QDate baseDate(2010 , 1, 1);

double applyGain(QString value, double gain) {
    return (gain*(double)value.toUInt());
}

bool ResventLoader::Detect(const QString & givenpath)
{
    QDir dir(givenpath);

    if (!dir.exists()) {
        return false;
    }

    if (!dir.exists(kResventTherapyFolder)) {
        return false;
    }

    dir.cd(kResventTherapyFolder);
    if (!dir.exists(kResventConfigFolder)) {
        return false;
    }

    if (!dir.cd(kResventRecordFolder)) {
        return false;
    }
    return true;
}

void ResventLoader::readAllConfigFiles(const QString & path ,QMap<QString,QString>& hash ) {
    const auto configPath = path + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventConfigFolder + QDir::separator() ;
    static const QStringList uniqueFiles({ "ALARM" , "COMFORT" , "CHECK.TXT" , "SETTING" , "VERSION" , "SYSCFG" , "TCTRL" });
    DEBUGFC;
    for (const QString& fileName : uniqueFiles) {
        QString filepath = configPath+fileName;
        readConfigFile(filepath, hash);
    }
    int papMode = configSettings.value("VentMode").toInt();
    // CPAP
    if (papMode == RESVENT_DEVICE_CPAP) {
        readConfigFile(configPath+"N_CPAP", hash);
        myCPAPMode = MODE_CPAP;
        myRESVENT_PAP_MODE = RESVENT_PAP_CPAP;
    // APAP
    } else if (papMode == RESVENT_DEVICE_APAP) {
        readConfigFile(configPath+"N_APAP", hash);
        myCPAPMode = MODE_APAP;
        myRESVENT_PAP_MODE = RESVENT_PAP_APAP ;
    // S(30)
    } else if (papMode == RESVENT_DEVICE_S30) {
        readConfigFile(configPath+"N_S30", hash);
        myCPAPMode = MODE_BILEVEL_FIXED;
        myRESVENT_PAP_MODE = RESVENT_PAP_S30;
    // Auto S(30)
    } else if (papMode == RESVENT_DEVICE_AUTO_S30) {
        readConfigFile(configPath+"N_AS30", hash);
        myCPAPMode = MODE_BILEVEL_AUTO_FIXED_PS;
        myRESVENT_PAP_MODE = RESVENT_PAP_AUTO_S30 ; 
    // S/T(30)
    } else if (papMode == RESVENT_DEVICE_ST30) {
        readConfigFile(configPath+"N_ST30", hash);
        // No CPAPMode for x/T Modes
        myCPAPMode = MODE_BILEVEL_FIXED;
        myRESVENT_PAP_MODE = RESVENT_PAP_ST30;
    // Auto S/T(30)
    } else if (papMode == RESVENT_DEVICE_AUTO_ST30) {
        readConfigFile(configPath+"N_AST30", hash);
        // No CPAPMode for x/T Modes
        myCPAPMode = MODE_BILEVEL_AUTO_FIXED_PS;
        myRESVENT_PAP_MODE = RESVENT_PAP_AUTO_ST30 ; 
    // T(30)
    } else if (papMode == RESVENT_DEVICE_T30) {
        readConfigFile(configPath+"N_T30", hash);
        // No CPAPMode for T
        myCPAPMode = MODE_BILEVEL_FIXED;
        myRESVENT_PAP_MODE = RESVENT_PAP_T30;
    // PC
    } else if (papMode == RESVENT_DEVICE_PC) {
        readConfigFile(configPath+"N_PC", hash);
        // No CPAP Mode for PC
        myCPAPMode = MODE_BILEVEL_FIXED;
        myRESVENT_PAP_MODE = RESVENT_PAP_PC;
    }
}

void ResventLoader::readConfigFile(const QString & configFile, QMap<QString,QString>& hash) {
    //DEBUGFC O(QFileInfo(configFile).fileName());
    if (!QFile::exists(configFile)) {
        qDebug() << "Resvent Data card has no file" << configFile;
    } else {
        QFile f(configFile);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        f.seek(4);
        while (!f.atEnd()) {
            QString line = f.readLine().trimmed();
            const auto elems = line.split("=");
            Q_ASSERT(elems.size() == 2);
            QString key=elems[0].simplified();
            QString value=elems[1].simplified();
            auto it = hash.insert(key,value);
            DEBUGFC O(QFileInfo(configFile).fileName())  O(key) O(value);
            Q_UNUSED(it);
            /*
            auto it = hash.insert(key,value);
            if (it != hash.end() ) {
                QString oldKey=it.key().simplified();
                QString old=it.value().simplified();
                IF (it.value() != old ) { DEBUGFC O(QFileInfo(configFile).fileName())  O(oldKey) O(old) O("==>") O(key) O(value); }
            } else {
                DEBUGFC O(QFileInfo(configFile).fileName())  O(key) O(value);
            }
            */
        }
    }
}

MachineInfo ResventLoader::PeekInfo(const QString & path)
{
    if (!Detect(path)) {
        return MachineInfo();
    }
    readAllConfigFiles( path , configSettings);

    MachineInfo info = newInfo();
    for (auto it = configSettings.begin(); it != configSettings.end(); ++it) {
        QString key = it.key(),  value = it.value();
        if (key == "models") {
            info.model = value;
            if ( (info.model.contains("Point", Qt::CaseInsensitive))  ||
                 (info.model.contains("Trend", Qt::CaseInsensitive)) ){
                info.brand = "Hoffrichter";
            } else {
                info.brand = "Resvent";
            }
        } else if (key == "sn") {
            info.serial = value;
        } else if (key == "num") {
            info.version = value.toInt();
        }
        info.type = MachineType::MT_CPAP;
    }
    return info;
}

QVector<QDate> ResventLoader::GetSessionsDate(const QString& dirpath) {
    QVector<QDate> sessions_date;

    const auto records_path = dirpath + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventRecordFolder;
    QDir records_folder(records_path);
    const auto year_month_folders = records_folder.entryList(QStringList(), QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name);
    std::for_each(year_month_folders.cbegin(), year_month_folders.cend(), [&](const QString& year_month_folder_name){
        if (year_month_folder_name.length() != 6) {
            return;
        }

        const int year = std::stoi(year_month_folder_name.left(4).toStdString());
        const int month = std::stoi(year_month_folder_name.right(2).toStdString());

        const auto year_month_folder_path = records_path + QDir::separator() + year_month_folder_name;
        QDir year_month_folder(year_month_folder_path);
        const auto session_folders = year_month_folder.entryList(QStringList(), QDir::Dirs|QDir::NoDotAndDotDot, QDir::Name);
        std::for_each(session_folders.cbegin(), session_folders.cend(), [&](const QString& day_folder){
            const auto day = std::stoi(day_folder.toStdString());

            sessions_date.push_back(QDate(year, month, day));
        });
    });
    return sessions_date;
}

void ResventLoader::UpdateEvents(EventType event_type, const QMap<EventType, QVector<EventData>>& events, Session* session) {
    static QMap<EventType, unsigned int> mapping {{EventType::ObstructiveApnea, CPAP_Obstructive},
                                                 {EventType::CentralApnea, CPAP_ClearAirway},
                                                 {EventType::Hypopnea, CPAP_Hypopnea},
                                                 {EventType::FlowLimitation, CPAP_FlowLimit},
                                                 {EventType::RERA, CPAP_RERA},
                                                 {EventType::PeriodicBreathing, CPAP_PB},
                                                 {EventType::Snore, CPAP_Snore}};
    const auto it_events = events.find(event_type);
    const auto it_mapping = mapping.find(event_type);
    if (it_events == events.cend() || it_mapping == mapping.cend()) {
        return;
    }

    EventList* event_list  = session->AddEventList(it_mapping.value(), EVL_Event);
    std::for_each(it_events.value().cbegin(), it_events.value().cend(), [&](const EventData& event_data){
        event_list->AddEvent(event_data.date_time.toMSecsSinceEpoch() + kDateTimeOffset - timezoneOffset(), event_data.duration);
    });
}

QString ResventLoader::GetSessionFolder(const QString& dirpath, const QDate& session_date) {
    const auto year_month_folder = QString("%1%2").arg(session_date.year()).arg(session_date.month(),2,10,QLatin1Char('0'));
    const auto day_folder = QString("%1").arg(session_date.day(),2,10,QLatin1Char('0')) ;
    const auto session_folder_path = dirpath + QDir::separator() + kResventTherapyFolder + QDir::separator() + kResventRecordFolder + QDir::separator() + year_month_folder + QDir::separator() + day_folder;
    return session_folder_path;
}

bool ResventLoader::VerifyEvent(EventData& eventData) {
    switch (eventData.type) {
        case EventType::FlowLimitation:
        case EventType::Hypopnea:
        case EventType::ObstructiveApnea:       // OA
        case EventType::CentralApnea:           // CA and same clear airway.
            // adjust time of event to be after the event ends rather than when the event starts.
            eventData.date_time = eventData.date_time.addMSecs(eventData.duration*1000);
            break;
        case EventType::RERA:
            eventData.duration = 0 ;    // duration is large and suppress duration display of eariler OA events.
            break;
        case EventType::PeriodicBreathing:
        case EventType::Snore:
        default:
            break;
    }
    return true;
}
// ResventLoader::
void ResventLoader::LoadEvents(const QString& session_folder_path, Session* session, const ResVentUsageData& usage ) {
    const auto event_file_path = session_folder_path + QDir::separator() + "EV" + usage.number;
    // Oscar (resmed) plots events at end.

    QMap<EventType, QVector<EventData>> events;
    QFile f(event_file_path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    f.seek(4);
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed(); // ID=20,DT=1692022874,DR=6,GD=0,

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        const auto elems = line.split(",", Qt::SkipEmptyParts);
#else
        const auto elems = line.split(",", QString::SkipEmptyParts);
#endif
        if (elems.size() != 4) {
            continue;
        }

        const auto event_type_elems = elems.at(0).split("=");
        const auto date_time_elems = elems.at(1).split("=");
        const auto duration_elems = elems.at(2).split("=");

        Q_ASSERT(event_type_elems.size() == 2);
        Q_ASSERT(date_time_elems.size() == 2);
        Q_ASSERT(duration_elems.size() == 2);
        auto event_type = static_cast<EventType>(std::stoi(event_type_elems[1].toStdString()));
        auto duration = std::stoi(duration_elems[1].toStdString());
        auto date_time = QDateTime::fromSecsSinceEpoch(std::stoi(date_time_elems[1].toStdString()));

        EventData eventData({event_type, date_time, duration});
        VerifyEvent(eventData);
        events[event_type].push_back(eventData);
    }

    static QVector<EventType> mapping {EventType::ObstructiveApnea,
                                      EventType::CentralApnea,
                                      EventType::Hypopnea,
                                      EventType::FlowLimitation,
                                      EventType::RERA,
                                      EventType::PeriodicBreathing,
                                      EventType::Snore};

    std::for_each(mapping.cbegin(), mapping.cend(), [&](EventType event_type){
        UpdateEvents(event_type, events, session);
    });
}

template <typename T>
T read_from_file(QFile& f) {
    T data{};
    f.read(reinterpret_cast<char*>(&data), sizeof(T));
    return data;
}

struct WaveFileData {
    unsigned int wave_event_id;
    QString file_base_name;
    unsigned int sample_rate_offset;
    unsigned int start_offset;
};

EventList* ResventLoader::GetEventList(const QString& name, Session* session, float sample_rate) {
    if (name == "Press") {
        return session->AddEventList(CPAP_Pressure, EVL_Event, kHundredthGain);
    }
    else if (name == "IPAP") {
        return session->AddEventList(CPAP_IPAP, EVL_Event, kHundredthGain);
    }
    else if (name == "EPAP") {
        return session->AddEventList(CPAP_EPAP, EVL_Event, kHundredthGain);
    }
    else if (name == "Leak") {
        return session->AddEventList(CPAP_Leak, EVL_Event , kTenthGain);
    }
    else if (name == "Vt") {
        return session->AddEventList(CPAP_TidalVolume, EVL_Event , kNoGain);
    }
    else if (name == "MV") {
        return session->AddEventList(CPAP_MinuteVent, EVL_Event , kHundredthGain);
    }
    else if (name == "RR") {
        return session->AddEventList(CPAP_RespRate, EVL_Event, kTenthGain);
    }
    else if (name == "Ti") {
        return session->AddEventList(CPAP_Ti, EVL_Event, kMilliGain);
    }
    else if (name == "I:E") {
        return session->AddEventList(CPAP_IE, EVL_Event, kMilliGain);
    } else if (name == "SpO2" || name == "PR") {
        // Not present
        return nullptr;
    } else if (name == "Pressure") {
        return session->AddEventList(CPAP_MaskPressure, EVL_Waveform, kDefaultGain, 0.0, 0.0, 0.0, 1000.0 / sample_rate);
    }
    else if (name == "Flow") {
        return session->AddEventList(CPAP_FlowRate, EVL_Waveform, kDefaultGain, 0.0, 0.0, 0.0, 1000.0 / sample_rate);
    }
    else {
        // Not supported
        Q_ASSERT(false);
        return nullptr;
    }
}

QString ReadDescriptionName(QFile& f) {
    constexpr int kNameSize = 9;
    QVector<char> name(kNameSize);

    const auto readed = f.read(name.data(), kNameSize - 1);
    Q_ASSERT(readed == kNameSize - 1);

    return QString(name.data());
}

void ResventLoader::ReadWaveFormsHeaders(QFile& f, QVector<ChunkData>& wave_forms, Session* session, const ResVentUsageData& usage) {
    f.seek(kChunkDurationInSecOffset);
    const auto chunk_duration_in_sec = read_from_file<uint16_t>(f);
    f.seek(kDescriptionCountOffset);
    const auto description_count = read_from_file<uint16_t>(f);
    wave_forms.resize(description_count);

    for (unsigned int i = 0; i < description_count; i++) {
        const auto description_header_offset = kMainHeaderSize + i * kDescriptionHeaderSize;
        f.seek(description_header_offset);
        const auto name = ReadDescriptionName(f);
        f.seek(description_header_offset + kDescriptionSamplesByChunk);
        const auto samples_by_chunk = read_from_file<uint16_t>(f);

        wave_forms[i].sample_rate = 1.0 * samples_by_chunk / chunk_duration_in_sec;
        wave_forms[i].event_list = GetEventList(name, session, wave_forms[i].sample_rate);
        wave_forms[i].samples_by_chunk = samples_by_chunk;
        wave_forms[i].start_time = usage.start_time.toMSecsSinceEpoch();
    }
}

void ResventLoader::LoadOtherWaveForms(const QString& session_folder_path, Session* session, const ResVentUsageData& usage) {
    QDir session_folder(session_folder_path);

    const auto wave_files = session_folder.entryList(QStringList() << "P" + usage.number + "_*", QDir::Files, QDir::Name);

    QVector<ChunkData> wave_forms;
    bool initialized = false;
    std::for_each(wave_files.cbegin(), wave_files.cend(), [&](const QString& wave_file){
        // P01_ file
        QFile f(session_folder_path + QDir::separator() + wave_file);
        f.open(QIODevice::ReadOnly);

        if (!initialized) {
            ReadWaveFormsHeaders(f, wave_forms, session, usage);
            initialized = true;
        }
        f.seek(kMainHeaderSize + wave_forms.size() * kDescriptionHeaderSize);

        std::vector<qint16> chunk(std::max_element(wave_forms.cbegin(), wave_forms.cend(), [](const ChunkData& lhs, const ChunkData& rhs){
                                      return lhs.samples_by_chunk < rhs.samples_by_chunk;
                                  })->samples_by_chunk);
        while (!f.atEnd()) {
            for (int i = 0; i < wave_forms.size(); i++) {
                const auto& wave_form = wave_forms[i].event_list;
                const auto samples_by_chunk_actual = wave_forms[i].samples_by_chunk;
                auto& start_time_current = wave_forms[i].start_time;
                auto& total_samples_by_chunk = wave_forms[i].total_samples_by_chunk;
                const auto sample_rate = wave_forms[i].sample_rate;

                const auto duration = samples_by_chunk_actual * 1000.0 / sample_rate;
                const auto readed = f.read(reinterpret_cast<char*>(chunk.data()), chunk.size() * sizeof(qint16));
                if (wave_form) {
                    const auto readed_elements = readed / sizeof(qint16);
                    if (readed_elements != samples_by_chunk_actual) {
                        std::fill(std::begin(chunk) + readed_elements, std::end(chunk), 0);
                    }

                    int offset = 0;
             std::for_each(chunk.cbegin(), chunk.cend(), [&](const qint16& value){
                        wave_form->AddEvent(start_time_current + offset + kDateTimeOffset - timezoneOffset(), value);
                        offset += 1000.0 / sample_rate;
                    });
                }

                start_time_current += duration;
                total_samples_by_chunk += samples_by_chunk_actual;
            }
        }
    });

    QVector<qint16> chunk;
    for (int i = 0; i < wave_forms.size(); i++) {
        const auto& wave_form = wave_forms[i];
        const auto expected_samples = usage.start_time.msecsTo(usage.end_time) / 1000.0 * wave_form.sample_rate;
        if (wave_form.total_samples_by_chunk < expected_samples) {
            chunk.resize(expected_samples - wave_form.total_samples_by_chunk);
            if (wave_form.event_list) {
                int offset = 0;
                std::for_each(chunk.cbegin(), chunk.cend(), [&](const qint16& value){
                    wave_form.event_list->AddEvent(wave_form.start_time + offset + kDateTimeOffset - timezoneOffset(), value );
                    offset += 1000.0 / wave_form.sample_rate;
                });
            }
        }
    }
}

void ResventLoader::LoadWaveForms(const QString& session_folder_path, Session* session, const ResVentUsageData& usage) {
    QDir session_folder(session_folder_path);

    const auto wave_files = session_folder.entryList(QStringList() << "W" + usage.number + "_*", QDir::Files, QDir::Name);

    QVector<ChunkData> wave_forms;
    bool initialized = false;
    std::for_each(wave_files.cbegin(), wave_files.cend(), [&](const QString& wave_file){
        // W01_ file
        QFile f(session_folder_path + QDir::separator() + wave_file);
        f.open(QIODevice::ReadOnly);

        if (!initialized) {
            ReadWaveFormsHeaders(f, wave_forms, session, usage);
            initialized = true;
        }
        f.seek(kMainHeaderSize + wave_forms.size() * kDescriptionHeaderSize);

        QVector<qint16> chunk(std::max_element(wave_forms.cbegin(), wave_forms.cend(), [](const ChunkData& lhs, const ChunkData& rhs){
                                  return lhs.samples_by_chunk < rhs.samples_by_chunk;
                              })->samples_by_chunk);
        while (!f.atEnd()) {
            for (int i = 0; i < wave_forms.size(); i++) {
                const auto& wave_form = wave_forms[i].event_list;
                const auto samples_by_chunk_actual = wave_forms[i].samples_by_chunk;
                auto& start_time_current = wave_forms[i].start_time;
                auto& total_samples_by_chunk = wave_forms[i].total_samples_by_chunk;
                const auto sample_rate = wave_forms[i].sample_rate;

                const auto duration = samples_by_chunk_actual * 1000.0 / sample_rate;
                const auto readed = f.read(reinterpret_cast<char*>(chunk.data()), chunk.size() * sizeof(qint16));
                if (wave_form) {
                    const auto readed_elements = readed / sizeof(qint16);
                    if (readed_elements != samples_by_chunk_actual) {
                        std::fill(std::begin(chunk) + readed_elements, std::end(chunk), 0);
                    }
                    wave_form->AddWaveform(start_time_current + kDateTimeOffset - timezoneOffset(), chunk.data(), samples_by_chunk_actual, duration);
                }
                start_time_current += duration;
                total_samples_by_chunk += samples_by_chunk_actual;
            }
        }
    });

    QVector<qint16> chunk;
    for (int i = 0; i < wave_forms.size(); i++) {
        const auto& wave_form = wave_forms[i];
        const auto expected_samples = usage.start_time.msecsTo(usage.end_time) / 1000.0 * wave_form.sample_rate;
        if (wave_form.total_samples_by_chunk < expected_samples) {
            chunk.resize(expected_samples - wave_form.total_samples_by_chunk);
            if (wave_form.event_list) {
                const auto duration = chunk.size() * 1000.0 / wave_form.sample_rate;
                wave_form.event_list->AddWaveform(wave_form.start_time + kDateTimeOffset - timezoneOffset(), chunk.data(), chunk.size(), duration);
            }
        }
    }
}
// The following modes are specific to the resvent device. there could change based on manufactures differences
// See resmed loader for more examples.
// enum RESVENT_PAP_MODE  now in header file.

enum RESVENT_iPR_MODE {
    RESVENT_iPR_OFF , RESVENT_iPR_ON , RESVENT_iPR_RAMP
};

void ResventLoader::LoadStats(const ResVentUsageData& /*usage_data*/, Session* session ) {
    // these should be set once per day or once per sessio or once per oscar sessionFull mode
    session->settings[CPAP_Mode] = myCPAPMode ;
    if (myRESVENT_PAP_MODE == RESVENT_PAP_APAP) {
        session->settings[CPAP_PressureMin] = applyGain(configSettings.value("PMin"),kHundredthGain);
        session->settings[CPAP_PressureMax] = applyGain(configSettings.value("PMax"),kHundredthGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_CPAP) {
        session->settings[CPAP_Pressure] = applyGain(configSettings.value("Press"),kHundredthGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_S30) {
        session->settings[CPAP_EPAP] = applyGain(configSettings.value("EPAP"),kHundredthGain);
        session->settings[CPAP_IPAP] = applyGain(configSettings.value("IPAP"),kHundredthGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_AUTO_S30){
        session->settings[CPAP_EPAPLo] = applyGain(configSettings.value("EPAPMin"),kHundredthGain);
        session->settings[CPAP_IPAPHi] = applyGain(configSettings.value("IPAPMax"),kHundredthGain);
        session->settings[CPAP_PS] = applyGain(configSettings.value("PS"),kHundredthGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_ST30){
        session->settings[CPAP_EPAP] = applyGain(configSettings.value("EPAP"),kHundredthGain);
        session->settings[CPAP_IPAP] = applyGain(configSettings.value("IPAP"),kHundredthGain);
        session->settings[CPAP_RespRate] = applyGain(configSettings.value("TitraTime"),kNoGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_AUTO_ST30) {
        session->settings[CPAP_EPAPLo] = applyGain(configSettings.value("EPAPMin"),kHundredthGain);
        session->settings[CPAP_IPAPHi] = applyGain(configSettings.value("IPAPMax"),kHundredthGain);
        session->settings[CPAP_PS] = applyGain(configSettings.value("PS"),kHundredthGain);
        session->settings[CPAP_RespRate] = applyGain(configSettings.value("TitraTime"),kNoGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_T30) {
        session->settings[CPAP_EPAPLo] = applyGain(configSettings.value("EPAP"),kHundredthGain);
        session->settings[CPAP_IPAPHi] = applyGain(configSettings.value("IPAP"),kHundredthGain);
        session->settings[CPAP_RespRate] = applyGain(configSettings.value("TitraTime"),kNoGain);
    } else if (myRESVENT_PAP_MODE == RESVENT_PAP_PC) {
        session->settings[CPAP_EPAPLo] = applyGain(configSettings.value("EPAP"),kHundredthGain);
        session->settings[CPAP_IPAPHi] = applyGain(configSettings.value("IPAP"),kHundredthGain);
        session->settings[CPAP_RespRate] = applyGain(configSettings.value("TitraTime"),kNoGain);
    }

    session->settings[RESVENT_Mode] = myRESVENT_PAP_MODE;
    int level = (configSettings.value("iPR").toInt()) ;
    session->settings[RESVENT_iPR] =  level<=0?RESVENT_iPR_OFF:RESVENT_iPR_ON ; // defined in options.
    session->settings[RESVENT_iPRLevel] = level;
}

ResVentUsageData ResventLoader::ReadUsage(const QString& session_folder_path, const QString& usage_number) {
    ResVentUsageData usage_data;
    usage_data.number = usage_number;

    const auto session_stat_path = session_folder_path + QDir::separator() + "STAT" + usage_number;
    if (!QFile::exists(session_stat_path)) {
        qDebug() << "Resvent Data card has no " << session_stat_path;
        return usage_data;
    }
    QFile f(session_stat_path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    f.seek(4);
    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();

        const auto elems = line.split("=");
        Q_ASSERT(elems.size() == 2);

        if (elems[0] == "secStart") {
            usage_data.start_time = QDateTime::fromSecsSinceEpoch(std::stoi(elems[1].toStdString()));
        }
        else if (elems[0] == "secUsed") {
            usage_data.end_time = QDateTime::fromSecsSinceEpoch(usage_data.start_time.toSecsSinceEpoch() + std::stoi(elems[1].toStdString()));
        }
        else if (elems[0] == "cntAHI") {
            usage_data.countAHI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntOAI") {
            usage_data.countOAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntCAI") {
            usage_data.countCAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntAI") {
            usage_data.countAI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntHI") {
            usage_data.countHI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntRERA") {
            usage_data.countRERA = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntSNI") {
            usage_data.countSNI = std::stoi(elems[1].toStdString());
        }
        else if (elems[0] == "cntBreath") {
            usage_data.countBreath = std::stoi(elems[1].toStdString());
        }
    }

    return usage_data;
}

QVector<ResVentUsageData> ResventLoader::GetDifferentUsage(const QString& session_folder_path) {
    QDir session_folder(session_folder_path);

    const auto stat_files = session_folder.entryList(QStringList() << "STAT*", QDir::Files, QDir::Name);

    QVector<ResVentUsageData> usage_data;
    std::for_each(stat_files.cbegin(), stat_files.cend(), [&](const QString& stat_file){
        if (stat_file.size() != 6) {
            return;
        }

        auto usageData = ReadUsage(session_folder_path, stat_file.right(2));

        usage_data.push_back(usageData);
    });
    return usage_data;
}

int ResventLoader::LoadSession(const QString& dirpath, const QDate& session_date, Machine* machine) {
    // Handles one day - all OSCAR sessions for a  day.
    const auto session_folder_path = GetSessionFolder(dirpath, session_date);

    const auto different_usage = GetDifferentUsage(session_folder_path);
    int base = 0;
    // Session ID must be unique.
    // SessioId is defined as an unsigned int. Oscar however has a problem .
    // if the most signifigant bit is set then Oscat misbehaves.
    // typically with signed vs unsigned int issues.
    // so sessionID has an implicit limit of a max postive value of of signed int.
    // sessionID  be must a unique positive ingteger ei. <= (2**31 -1)  (2,147,483,647)
    //
    SessionID sessionId = (baseDate.daysTo(session_date)) * 64; // leave space for N sessions.
    for (auto usage : different_usage)
    {
        if (machine->SessionExists(sessionId)) {
            // session alreadt exists
            //return base;
            continue;
        }
        Session* session = new Session(machine, sessionId++);
        session->SetChanged(true);
        session->really_set_first(usage.start_time.toMSecsSinceEpoch() + kDateTimeOffset - timezoneOffset());
        session->really_set_last(usage.end_time.toMSecsSinceEpoch() + kDateTimeOffset - timezoneOffset());
        LoadStats(usage, session);
        LoadWaveForms(session_folder_path, session, usage);
        LoadOtherWaveForms(session_folder_path, session, usage);
        LoadEvents(session_folder_path, session, usage);

        session->UpdateSummaries();
        session->Store(machine->getDataPath());
        machine->AddSession(session);
        ++base;
    };
    return base;
}


///////////////////////////////////////////////////////////////////////////////////////////
// Sorted EDF files that need processing into date records according to ResMed noon split
///////////////////////////////////////////////////////////////////////////////////////////
int ResventLoader::Open(const QString & dirpath)
{
    const auto machine_info = PeekInfo(dirpath);


    // Abort if no serial number
    if (machine_info.serial.isEmpty()) {
        qDebug() << "Resvent Data card has no valid serial number in " << kResventSysConfigFilename;
        return -1;
    }

    const auto sessions_date = GetSessionsDate(dirpath);
    int progress =  0;
    emit setProgressMax(1+sessions_date.size());    // add one to include Save in progress.
    emit setProgressValue(progress);
    QCoreApplication::processEvents();

    Machine *machine = p_profile->CreateMachine(machine_info);


    int new_sessions = 0;
    // do for each day found.
    std::for_each(sessions_date.cbegin(), sessions_date.cend(), [&](const QDate& session_date){
        new_sessions += LoadSession(dirpath, session_date, machine);
        emit setProgressValue(++progress);
        QCoreApplication::processEvents();
    });


    machine->Save();

    emit setProgressValue(++progress);
    QCoreApplication::processEvents();
    return new_sessions;
}

void ResventLoader::initChannels()
{
    // Remember there must a one-to-one coresponce between channelId and ChannelName.
    // channel XX: the ID of xX:  RESVENT_Mode must be a unique channelId and
    // channel xx: the Name of XX: "RESVENT_Mode must be a unique channel Name.
    // Copied from resmed
    using namespace schema;
    int RESVENT_CHANNELS = 0xe8A0;

    Channel * chan = new Channel(RESVENT_Mode = RESVENT_CHANNELS , SETTING, MT_CPAP,   SESSION,
        "RESVENT_Mode", QObject::tr("Mode"), QObject::tr("CPAP Mode"), QObject::tr("Mode"), "", LOOKUP, Qt::green);
    channel.add(GRP_CPAP, chan);

    // These should be resvent loader names. must start at 0 and increment
    chan->addOption(RESVENT_PAP_CPAP, STR_TR_CPAP);    // strings have already been translated
    chan->addOption(RESVENT_PAP_APAP, STR_TR_APAP);    // strings have already been translated1
    chan->addOption(RESVENT_PAP_S30, STR_TR_BIPAP);
    chan->addOption(RESVENT_PAP_AUTO_S30, STR_TR_BIPAP);
    chan->addOption(RESVENT_PAP_ST30, STR_TR_STASV);
    chan->addOption(RESVENT_PAP_AUTO_ST30, STR_TR_STASV);
    chan->addOption(RESVENT_PAP_T30, STR_TR_STASV);
    chan->addOption(RESVENT_PAP_AUTO_ST30, STR_TR_STASV);// strings have already been translated
    chan->addOption(RESVENT_PAP_PC, STR_TR_STASV);

    channel.add(GRP_CPAP, chan = new Channel(RESVENT_iPR = RESVENT_CHANNELS+1 , SETTING, MT_CPAP,   SESSION,
        "iPR", QObject::tr("iPR"), QObject::tr("Resvent Exhale Pressure Relief"), QObject::tr("iPR"), "", LOOKUP, Qt::green));

    chan->addOption(RESVENT_iPR_OFF, STR_TR_Off);   // strings have already been translated
    chan->addOption(RESVENT_iPR_ON, STR_TR_On); // strings have already been translated
    // This is a resmed option include as example. chan->addOption(RESVENT_iPR_RAMP, QObject::tr("Ramp Only"));

    channel.add(GRP_CPAP, chan = new Channel(RESVENT_iPRLevel = RESVENT_CHANNELS+2 , SETTING,  MT_CPAP,  SESSION,
        "iPRLevel", QObject::tr("iPR Level"), QObject::tr("Exhale Pressure Relief Level"), QObject::tr("iPR Level"),
        STR_UNIT_CMH2O, LOOKUP, Qt::blue));
}

ChannelID ResventLoader::PresReliefMode() {
    return RESVENT_iPR;
}
ChannelID ResventLoader::PresReliefLevel() {
    return RESVENT_iPRLevel;
}
ChannelID ResventLoader::CPAPModeChannel() {
    return RESVENT_Mode;
}

bool resvent_initialized = false;
void ResventLoader::Register()
{
    if (resvent_initialized) { return; }

    qDebug() << "Registering ResventLoader";
    RegisterLoader(new ResventLoader());

    resvent_initialized = true;
}
