/* SleepLib ResMed Loader Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "SleepLib/session.h"
#include "SleepLib/calcs.h"

#include "SleepLib/loader_plugins/resmed_loader.h"
#include "SleepLib/loader_plugins/resmed_EDFinfo.h"

#ifdef DEBUG_EFFICIENCY
#include <QElapsedTimer>  // only available in 4.8 and later
#endif


#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    #define QTCOMBINE insert
    //idmap.insert(hash);
#else
    #define QTCOMBINE unite
    //idmap.unite(hash);
#endif


ChannelID RMS9_EPR, RMS9_EPRLevel, RMS9_Mode, RMS9_SmartStart, RMS9_HumidStatus, RMS9_HumidLevel,
         RMS9_PtAccess, RMS9_Mask, RMS9_ABFilter, RMS9_ClimateControl, RMS9_TubeType, RMAS11_SmartStop,
         RMS9_Temp, RMS9_TempEnable, RMS9_RampEnable, RMAS1x_Comfort, RMAS11_PtView;

ChannelID RMAS1x_EasyBreathe, RMAS1x_RiseEnable, RMAS1x_RiseTime, RMAS1x_Cycle, RMAS1x_Trigger, RMAS1x_TiMax, RMAS1x_TiMin;

const QString STR_ResMed_AirSense10 = "AirSense 10";
const QString STR_ResMed_AirSense11 = "AirSense 11";
const QString STR_ResMed_AirCurve10 = "AirCurve 10";
const QString STR_ResMed_AirCurve11 = "AirCurve 11";
const QString STR_ResMed_S9 = "S9";
const QString STR_UnknownModel = "Resmed ???";

// TODO: See the PRSLoader::LogUnexpectedMessage TODO about generalizing this for other loaders.
void ResmedLoader::LogUnexpectedMessage(const QString & message)
{
    m_importMutex.lock();
    m_unexpectedMessages += message;
    m_importMutex.unlock();
}

#if defined(INCLUDE_AS_MODEL_VERIFICATION)
static const QVector<int> AS11TestedModels {
39420,
39421,
39423,
39463,
39483,
39485, //as11 AutoSet
39491, //ac11 ASV
39494, //ac11 vAuto
39517,
39520,
0};
#endif

ResmedLoader::ResmedLoader() {
#ifndef UNITTEST_MODE
    const QString RMS9_ICON = ":/icons/rms9.png";
    const QString RM10_ICON = ":/icons/airsense10.png";
    const QString RM10C_ICON = ":/icons/aircurve.png";

    m_pixmaps[STR_ResMed_S9] = QPixmap(RMS9_ICON);
    m_pixmap_paths[STR_ResMed_S9] = RMS9_ICON;
    m_pixmaps[STR_ResMed_AirSense10] = QPixmap(RM10_ICON);
    m_pixmap_paths[STR_ResMed_AirSense10] = RM10_ICON;
    m_pixmaps[STR_ResMed_AirCurve10] = QPixmap(RM10C_ICON);
    m_pixmap_paths[STR_ResMed_AirCurve10] = RM10C_ICON;
#endif
    m_type = MT_CPAP;

#ifdef DEBUG_EFFICIENCY
    timeInTimeDelta = timeInLoadBRP = timeInLoadPLD = timeInLoadEVE = 0;
    timeInLoadCSL = timeInLoadSAD = timeInEDFInfo = timeInEDFOpen = timeInAddWaveform = 0;
#endif

    saveCallback = SaveSession;
}

ResmedLoader::~ResmedLoader() { }

bool resmed_initialized = false;
void ResmedLoader::Register()
{
    if (resmed_initialized)
        return;

    qDebug() << "Registering ResmedLoader";
    RegisterLoader(new ResmedLoader());

    resmed_initialized = true;
}

void setupResMedTranslationMap(); // forward
void ResmedLoader::initChannels()
{
    using namespace schema;

//  Channel(ChannelID id, ChanType type, MachineType machtype, ScopeType scope, QString code, QString fullname,
//      QString description, QString label, QString unit, DataType datatype = DEFAULT, QColor = Qt::black, int link = 0);

    Channel * chan = new Channel(RMS9_Mode = 0xe203, SETTING, MT_CPAP,   SESSION,
        "RMS9_Mode", QObject::tr("Mode"), QObject::tr("CPAP Mode"), QObject::tr("Mode"), "", LOOKUP, Qt::green);

    channel.add(GRP_CPAP, chan);

    chan->addOption(0, QObject::tr("CPAP"));
    chan->addOption(1, QObject::tr("APAP"));
    chan->addOption(2, QObject::tr("BiLevel-T"));
    chan->addOption(3, QObject::tr("BiLevel-S"));
    chan->addOption(4, QObject::tr("BiLevel-S/T"));
    chan->addOption(5, QObject::tr("BiLevel-T"));
    chan->addOption(6, QObject::tr("VPAPauto"));
    chan->addOption(7, QObject::tr("ASV"));
    chan->addOption(8, QObject::tr("ASVAuto"));
    chan->addOption(9, QObject::tr("iVAPS"));
    chan->addOption(10, QObject::tr("PAC"));
    chan->addOption(11, QObject::tr("Auto for Her"));
    chan->addOption(16, QObject::tr("Unknown"));        // out of bounds of edf signal

    channel.add(GRP_CPAP, chan = new Channel(RMS9_EPR = 0xe201, SETTING, MT_CPAP,   SESSION,
        "EPR", QObject::tr("EPR"), QObject::tr("ResMed Exhale Pressure Relief"), QObject::tr("EPR"), "", LOOKUP, Qt::green));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("Ramp Only"));
    chan->addOption(2, QObject::tr("Full Time"));
    chan->addOption(3, QObject::tr("Patient???"));

    channel.add(GRP_CPAP, chan = new Channel(RMS9_EPRLevel = 0xe202, SETTING,  MT_CPAP,  SESSION,
        "EPRLevel", QObject::tr("EPR Level"), QObject::tr("Exhale Pressure Relief Level"), QObject::tr("EPR Level"), STR_UNIT_CMH2O, LOOKUP, Qt::blue));

//    RMS9_SmartStart, RMS9_HumidStatus, RMS9_HumidLevel,
//             RMS9_PtAccess, RMS9_Mask, RMS9_ABFilter, RMS9_ClimateControl, RMS9_TubeType,
//             RMS9_Temp, RMS9_TempEnable;

    channel.add(GRP_CPAP, chan = new Channel(RMS9_SmartStart = 0xe204, SETTING, MT_CPAP, SESSION,
        "RMS9_SmartStart", QObject::tr("SmartStart"), QObject::tr("Device auto starts by breathing"), QObject::tr("Smart Start"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(RMS9_HumidStatus = 0xe205, SETTING, MT_CPAP, SESSION,
        "RMS9_HumidStat", QObject::tr("Humid. Status"), QObject::tr("Humidifier Enabled Status"), QObject::tr("Humidifier Status"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(RMS9_HumidLevel = 0xe206, SETTING, MT_CPAP, SESSION,
        "RMS9_HumidLevel", QObject::tr("Humid. Level"), QObject::tr("Humidity Level"), QObject::tr("Humidity Level"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
//  chan->addOption(1, "1");
//  chan->addOption(2, "2");
//  chan->addOption(3, "3");
//  chan->addOption(4, "4");
//  chan->addOption(5, "5");
//  chan->addOption(6, "6");
//  chan->addOption(7, "7");
//  chan->addOption(8, "8");

    channel.add(GRP_CPAP, chan = new Channel(RMS9_Temp = 0xe207, SETTING, MT_CPAP, SESSION,
        "RMS9_Temp", QObject::tr("Temperature"), QObject::tr("ClimateLine Temperature"), QObject::tr("Temperature"), "ºC", INTEGER, Qt::black));


    channel.add(GRP_CPAP, chan = new Channel(RMS9_TempEnable = 0xe208, SETTING, MT_CPAP, SESSION,
        "RMS9_TempEnable", QObject::tr("Temp. Enable"), QObject::tr("ClimateLine Temperature Enable"), QObject::tr("Temperature Enable"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);
    chan->addOption(2, STR_TR_Auto);

    channel.add(GRP_CPAP, chan = new Channel(RMS9_ABFilter= 0xe209, SETTING, MT_CPAP, SESSION,
        "RMS9_ABFilter", QObject::tr("AB Filter"), QObject::tr("Antibacterial Filter"), QObject::tr("Antibacterial Filter"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_No);
    chan->addOption(1, STR_TR_Yes);

    channel.add(GRP_CPAP, chan = new Channel(RMS9_PtAccess= 0xe20A, SETTING, MT_CPAP, SESSION,
        "RMS9_PTAccess", QObject::tr("Pt. Access"), QObject::tr("Essentials"), QObject::tr("Essentials"), "", LOOKUP, Qt::black));

    chan->addOption(0, QObject::tr("Plus"));
    chan->addOption(1, QObject::tr("On"));

    channel.add(GRP_CPAP, chan = new Channel(RMS9_ClimateControl= 0xe20B, SETTING, MT_CPAP, SESSION,
        "RMS9_ClimateControl", QObject::tr("Climate Control"), QObject::tr("Climate Control"), QObject::tr("Climate Control"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Auto);
    chan->addOption(1, QObject::tr("Manual"));

    channel.add(GRP_CPAP, chan = new Channel(RMS9_Mask= 0xe20C, SETTING, MT_CPAP, SESSION,
        "RMS9_Mask", QObject::tr("Mask"), QObject::tr("ResMed Mask Setting"), QObject::tr("Mask"), "", LOOKUP, Qt::black));

    chan->addOption(0, QObject::tr("Pillows"));
    chan->addOption(1, QObject::tr("Full Face"));
    chan->addOption(2, QObject::tr("Nasal"));
    chan->addOption(3, QObject::tr("Unknown"));

    channel.add(GRP_CPAP, chan = new Channel(RMS9_RampEnable = 0xe20D, SETTING, MT_CPAP, SESSION,
        "RMS9_RampEnable", QObject::tr("Ramp"), QObject::tr("Ramp Enable"), QObject::tr("Ramp"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);
    chan->addOption(2, STR_TR_Auto);

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_Comfort = 0xe20E, SETTING, MT_CPAP, SESSION,
        "RMAS1x_Comfort", QObject::tr("Response"), QObject::tr("Response"), QObject::tr("Response"), "", LOOKUP, Qt::black));

    chan->addOption(0, QObject::tr("Standard"));     // This must be verified
    chan->addOption(1, QObject::tr("Soft"));

    channel.add(GRP_CPAP, chan = new Channel(RMAS11_SmartStop = 0xe20F, SETTING, MT_CPAP, SESSION,
        "RMAS11_SmartStop", QObject::tr("SmartStop"), QObject::tr("Device auto stops by breathing"), QObject::tr("Smart Stop"), "", LOOKUP, Qt::black));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(RMAS11_PtView= 0xe210, SETTING, MT_CPAP, SESSION,
        "RMAS11_PTView", QObject::tr("Patient View"), QObject::tr("Patient View"), QObject::tr("Patient View"), "", LOOKUP, Qt::black));

    chan->addOption(0, QObject::tr("Advanced"));
    chan->addOption(1, QObject::tr("Simple"));

    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_RiseEnable = 0xe212, SETTING, MT_CPAP, SESSION,
        "RMAS1x_RiseEnable", QObject::tr("RiseEnable"), QObject::tr("RiseEnable"), QObject::tr("RiseEnable"), "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, "Enabled");

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_RiseTime = 0xe213, SETTING, MT_CPAP, SESSION,
        "RMAS1x_RiseTime", QObject::tr("RiseTime"), QObject::tr("RiseTime"), QObject::tr("RiseTime"), STR_UNIT_milliSeconds, INTEGER, Qt::black));

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_Cycle = 0xe214, SETTING, MT_CPAP, SESSION,
        "RMAS1x_Cycle", QObject::tr("Cycle"), QObject::tr("Cycle"), QObject::tr("Cycle"), "", LOOKUP, Qt::black));
    chan->addOption(0, "Very Low");
    chan->addOption(1, "Low");
    chan->addOption(2, "Med");
    chan->addOption(3, "High");
    chan->addOption(4, "Very High");

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_Trigger = 0xe215, SETTING, MT_CPAP, SESSION,
        "RMAS1x_Trigger", QObject::tr("Trigger"), QObject::tr("Trigger"), QObject::tr("Trigger"), "", LOOKUP, Qt::black));
    chan->addOption(0, "Very Low");
    chan->addOption(1, "Low");
    chan->addOption(2, "Med");
    chan->addOption(3, "High");
    chan->addOption(4, "Very High");

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_TiMax = 0xe216, SETTING, MT_CPAP, SESSION,
        "RMAS1x_TiMax", QObject::tr("TiMax"), QObject::tr("TiMax"), QObject::tr("TiMax"), STR_UNIT_Seconds, DOUBLE, Qt::black));
    chan->addOption(0, "0");

    channel.add(GRP_CPAP, chan = new Channel(RMAS1x_TiMin = 0xe217, SETTING, MT_CPAP, SESSION,
        "RMAS1x_TiMin", QObject::tr("TiMin"), QObject::tr("TiMin"), QObject::tr("TiMin"), STR_UNIT_Seconds, DOUBLE, Qt::black));
    chan->addOption(0, "0");

    // Setup ResMeds signal name translation map
    setupResMedTranslationMap();
}

ChannelID ResmedLoader::CPAPModeChannel() { return RMS9_Mode; }
ChannelID ResmedLoader::PresReliefMode() { return RMS9_EPR; }
ChannelID ResmedLoader::PresReliefLevel() { return RMS9_EPRLevel; }

QHash<ChannelID, QStringList> resmed_codes;

const QString STR_ext_TGT = "tgt";
const QString STR_ext_JSON = "json";
const QString STR_ext_CRC = "crc";

const QString RMS9_STR_datalog = "DATALOG";
const QString RMS9_STR_idfile = "Identification.";
const QString RMS9_STR_strfile = "STR.";

bool ResmedLoader::Detect(const QString & givenpath)
{
    QDir dir(givenpath);

    if (!dir.exists()) {
        return false;
    }

    // ResMed drives contain a folder named "DATALOG".
    if (!dir.exists(RMS9_STR_datalog)) {
        return false;
    }

    // They also contain a file named "STR.edf".
    if (!dir.exists("STR.edf")) {
        return false;
    }

    return true;
}

QHash<QString, QString> parseIdentLine( const QString line, MachineInfo * info);    // forward
void scanProductObject( const QJsonObject product, MachineInfo *info, QHash<QString, QString> *idmap );              // forward
MachineInfo ResmedLoader::PeekInfo(const QString & path)
{
    if (!Detect(path))
        return MachineInfo();

    QFile f(path+"/"+RMS9_STR_idfile+"tgt");

    QFile j(path+"/"+RMS9_STR_idfile+"json");       // Check for AS11 file first, just in case
    if (j.exists() ) {                              // somebody is reusing an SD card w/o re-formatting
        if ( !j.open(QIODevice::ReadOnly)) {
            return MachineInfo();
        }
        if ( f.exists() ) {
            qDebug() << "Old Ident.tgt file is ignored";
        }
        QByteArray identData = j.readAll();
        j.close();
        QJsonDocument identDoc(QJsonDocument::fromJson(identData));
        QJsonObject  identObj = identDoc.object();
        if ( identObj.contains("FlowGenerator") && identObj["FlowGenerator"].isObject()) {
            QJsonObject  flow = identObj["FlowGenerator"].toObject();
            if ( flow.contains("IdentificationProfiles") && flow["IdentificationProfiles"].isObject()) {
                QJsonObject  profiles = flow["IdentificationProfiles"].toObject();
                if ( profiles.contains("Product") && profiles["Product"].isObject()) {
                    QJsonObject  product = profiles["Product"].toObject();
                    MachineInfo info = newInfo();
                    scanProductObject( product, &info, nullptr);
                    return info;
                } else
                    qDebug() << "No Product in Profiles";
            } else
                qDebug() << "No IdentificationProfiles in FlowGenerator";
        } else
            qDebug() << "No FlowGenerator in Identification.json";

        return MachineInfo();

    }
    // Abort if this file is dodgy..
    if (f.exists() ) {
        if ( !f.open(QIODevice::ReadOnly)) {
            return MachineInfo();
        }
        MachineInfo info = newInfo();

        // Parse # entries into idmap.
        while (!f.atEnd()) {
            QString line = f.readLine().trimmed();
            QHash<QString, QString> hash = parseIdentLine( line, & info );
        }

        return info;
    }
    // neither filename exists, return empty info
    return MachineInfo();
}

long event_cnt = 0;

bool parseIdentFile( QString path, MachineInfo * info, QHash<QString, QString> & idmap );        // forward
void backupSTRfiles( const QString strpath, const QString importPath, const QString backupPath,
                        MachineInfo & info, QMap<QDate, STRFile> & STRmap );                    // forward
ResMedEDFInfo * fetchSTRandVerify( QString filename, QString serialNumber );                    // forward

int ResmedLoader::OpenWithCallback(const QString & dirpath, ResDaySaveCallback s)               // alternate for unit testing
{
    ResDaySaveCallback origCallback = saveCallback;
    saveCallback = s;
    int value = Open(dirpath);
    saveCallback = origCallback;
    return value;
}

int ResmedLoader::Open(const QString & dirpath)
{
#ifdef TEST_MACROS_ENABLED
void edfDebugInit();
    edfDebugInit();
#endif
    qDebug() << "Starting ResmedLoader::Open( with " << dirpath << ")";
    QString datalogPath;
    QHash<QString, QString> idmap;  // Temporary device ID properties hash

    QString importPath(dirpath);
    importPath = importPath.replace("\\", "/");

    // Strip off end "/" if any
    if (importPath.endsWith("/")) {
        importPath = importPath.section("/", 0, -2);
    }

    // Strip off DATALOG from importPath, and set newimportPath to the importPath containing DATALOG
    if (importPath.endsWith(RMS9_STR_datalog)) {
        datalogPath = importPath + "/";
        importPath = importPath.section("/", 0, -2);
    } else {
        datalogPath = importPath + "/" + RMS9_STR_datalog + "/";
    }

    // Add separator back
    importPath += "/";

    // Check DATALOG folder exists and is readable
    if (!QDir().exists(datalogPath)) {
        qDebug() << "Missing DATALOG in" << dirpath;
        return -1;
    }

    m_abort = false;
    MachineInfo info = newInfo();

    if ( ! parseIdentFile(importPath, & info, idmap) ) {
        qDebug() << "Failed to parse Identification file";
        return -1;
    }

    qDebug() << "Info:" << info.series << info.model << info.modelnumber << info.serial;
    DEBUGCI  O(info.modelnumber) Q(info.series) Q(info.model) O(info.serial);
#ifdef IDENT_DEBUG
    qDebug() << "IdMap size:" << idmap.size();
    foreach ( QString st , idmap.keys() ) {
        qDebug() << "Key" << st << "Value" << idmap[st];
    }
#endif

    // Abort if no serial number
    if (info.serial.isEmpty()) {
        qDebug() << "ResMed Data card is missing serial number in Indentification.tgt";
        return -1;
    }

    bool compress_backups = p_profile->session->compressBackupData();

    // Early check for STR.edf file, so we can early exit before creating faulty device record.
    // str.edf is the first (primary) file to check, str.edf.gz is the secondary
    QString pripath = importPath + "STR.edf"; // STR.edf file
    QString secpath = pripath + STR_ext_gz;   // STR.edf.gz file
    QString strpath;

    // If compression is enabled, swap primary and secondary paths
    if (compress_backups) {
        strpath = pripath;
        pripath = secpath;
        secpath = strpath;
    }

    // Check if primary path exists
    QFile f(pripath);
    if (f.exists()) {
        strpath = pripath;
    // If no primary file, check for secondary
    } else {
        f.setFileName(secpath);
        strpath = secpath;
        if (!f.exists()) {
            qDebug() << "Missing STR.edf file";
            return -1;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Create device object (unless it's already registered)
    ///////////////////////////////////////////////////////////////////////////////////

    QDate firstImportDay = QDate().fromString("2010-01-01", "yyyy-MM-dd");     // Before Series 8 devices (I think)

    Machine *mach = p_profile->lookupMachine(info.serial, info.loadername);
    if ( mach ) {       // we have seen this device
        qDebug() << "We have seen this machime";
        mach->setInfo( info );                      // update info
        QDate lastDate = mach->LastDay();           // use the last day for this device
        firstImportDay = lastDate;                  // re-import the last day, to  pick up partial days
        QDate purgeDate = mach->purgeDate();
        if (purgeDate.isValid()) {
            firstImportDay = min(firstImportDay, purgeDate);
        }
//      firstImportDay = lastDate.addDays(-1);      // start the day before, to  pick up partial days
//      firstImportDay = lastDate.addDays(1);       // start the day after until we  figure out the purge
    } else {            // Starting from new beginnings - new or purged
        qDebug() << "New device or just purged";
        p_profile->forceResmedPrefs();
        #if defined(INCLUDE_AS_MODEL_VERIFICATION)
        int modelNum = info.modelnumber.toInt();
        if ( modelNum >= 39000 ) {
            if ( ! AS11TestedModels.contains(modelNum) ) {
                QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Device Untested"),
                             QObject::tr("Your ResMed CPAP device (Model %1) has not been tested yet.").arg(info.modelnumber) +"\n\n"+
                             QObject::tr("It seems similar enough to other devices that it might work, but the developers would like a .zip copy of this device's SD card to make sure it works with OSCAR.")
                             ,QMessageBox::Ok);
            }
        }
        #endif
        mach = p_profile->CreateMachine( info );
    }
    QDateTime ignoreBefore = p_profile->session->ignoreOlderSessionsDate();
    bool ignoreOldSessions = p_profile->session->ignoreOlderSessions();

    if (ignoreOldSessions && (ignoreBefore.date() > firstImportDay))
        firstImportDay = ignoreBefore.date();
    qDebug() << "First day to import: " << firstImportDay.toString();

    bool rebuild_from_backups = false;
    bool create_backups = p_profile->session->backupCardData();

    QString backup_path = mach->getBackupPath();

    // Compare QDirs rather than QStrings because separators may be different, especially on Windows.
    // We want to check whether import and backup paths are the same, regardless of variations in the string representations.
    QDir ipath(importPath);
    QDir bpath(backup_path);

    if (ipath == bpath) {
        // Don't create backups if importing from backup folder
        rebuild_from_backups = true;
        create_backups = false;
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Copy the idmap into device objects properties, (overwriting any old values)
    ///////////////////////////////////////////////////////////////////////////////////
    for (auto i=idmap.begin(), idend=idmap.end(); i != idend; i++) {
        mach->info.properties[i.key()] = i.value();
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Create the backup folder structure for storing a copy of everything in..
    // (Unless we are importing from this backup folder)
    ///////////////////////////////////////////////////////////////////////////////////
    QDir dir;
    if (create_backups) {
        if ( ! dir.exists(backup_path)) {
            if ( ! dir.mkpath(backup_path) ) {
                qWarning() << "Could not create ResMed backup directory" << backup_path;
            }
        }

	    // Create the STR_Backup folder if it doesn't exist
	    QString strBackupPath = backup_path + "STR_Backup";
	    if ( ! dir.exists(strBackupPath) )
            if (!dir.mkpath(strBackupPath))
                qWarning() << "Could not create ResMed STR backup directory" << strBackupPath;

	    QString newpath = backup_path + "DATALOG";
	    if ( ! dir.exists(newpath) )
            if (!dir.mkpath(newpath))
                qWarning() << "Could not create ResMed DATALOG backup directory" << newpath;


        // Copy Identification files to backup folder
        QString idfile_ext;
        if (QFile(importPath+RMS9_STR_idfile+STR_ext_TGT).exists()) {
            idfile_ext = STR_ext_TGT;
        }
        else if (QFile(importPath+RMS9_STR_idfile+STR_ext_JSON).exists()) {
            idfile_ext = STR_ext_JSON;
        }
        else {
            idfile_ext = "";       // should never happen...
        }

        QFile backupFile(backup_path + RMS9_STR_idfile + idfile_ext);
        if (backupFile.exists())
            backupFile.remove();
        if ( ! QFile::copy(importPath + RMS9_STR_idfile + idfile_ext, backup_path + RMS9_STR_idfile + idfile_ext))
            qWarning() << "Could not copy" << importPath + RMS9_STR_idfile + idfile_ext << "to backup" << backupFile.fileName();

        backupFile.setFileName(backup_path + RMS9_STR_idfile + STR_ext_CRC);
        if (backupFile.exists())
            backupFile.remove();
        if ( ! QFile::copy(importPath + RMS9_STR_idfile + STR_ext_CRC, backup_path + RMS9_STR_idfile + STR_ext_CRC))
            qWarning() << "Could not copy" << importPath + RMS9_STR_idfile + STR_ext_CRC << "to backup" << backup_path;
    }

    ///////////////////////////////////////////////////////////////////////////////////
    // Open and Process STR.edf files (including those listed in STR_Backup)
    ///////////////////////////////////////////////////////////////////////////////////

    resdayList.clear();

    emit updateMessage(QObject::tr("Locating STR.edf File(s)..."));
    QCoreApplication::processEvents();

    // List all STR.edf backups and tag on latest for processing

    QMap<QDate, STRFile> STRmap;

    if ( ( ! rebuild_from_backups) /* && create_backups */ ) {
        // first we copy any STR_yyyymmdd.edf files and the Backup/STR.edf into STR_Backup and the STRmap
        backupSTRfiles( strpath, importPath, backup_path, info, STRmap );
        //Then we copy the new imported STR.edf into Backup/STR.edf and add it to the STRmap
        QString importFile(importPath+"STR.edf");
        QString backupFile(backup_path + "STR.edf");
        ResMedEDFInfo * stredf = fetchSTRandVerify( importFile, info.serial );
        if ( stredf != nullptr ) {
            bool addToSTRmap = true;
            QDate date = stredf->edfHdr.startdate_orig.date();
            long int days = stredf->GetNumDataRecords();
            qDebug() << importFile.section("/",-3,-1) << "starts at" << date << "for" << days << "ends" << date.addDays(days-1);
            if (STRmap.contains(date)) {        // Keep the longer of the two STR files - or newer if equal!
                qDebug().noquote() << importFile.section("/",-3,-1) << "overlaps" << STRmap[date].filename.section("/",-3,-1) << "for" << days << "days, ends" << date.addDays(days-1);
                if (days >= STRmap[date].days) {
                    qDebug() << "Removing" << STRmap[date].filename.section("/",-3,-1) << "with" << STRmap[date].days << "days from STRmap";
                    STRmap.remove(date);
                } else {
                    qDebug() << "Skipping" << importFile.section("/",-3,-1);
                    qWarning() << "New import str.edf file is shorter than exisiting files - should never happen";
                    delete stredf;
                    addToSTRmap = false;
                }
            }
            if ( addToSTRmap ) {
                if ( compress_backups ) {
                    backupFile += ".gz";
                    if ( QFile::exists( backupFile ) )
                        QFile::remove( backupFile );
                    compressFile(importFile, backupFile);
                }
                else {
                    if ( QFile::exists( backupFile ) )
                        QFile::remove( backupFile );
                    if ( ! QFile::copy(importFile, backupFile) )
                        qWarning() << "Failed to copy" << importFile << "to" << backupFile;
                }
                STRmap[date] = STRFile(backupFile, days, stredf);
                qDebug() << "Adding" << importFile << "to STRmap as" << backupFile;

                // Meh.. these can be calculated if ever needed for ResScan SDcard export
                QFile sourcePath(importPath + "STR.crc");
                if (sourcePath.exists()) {
                    QFile backupFile(backup_path + "STR.crc");
                    if (backupFile.exists())
                        if (!backupFile.remove())
                            qWarning() << "Failed to remove" << backupFile.fileName();
                    if (!QFile::copy(importPath + "STR.crc", backup_path + "STR.crc"))
                        qWarning() << "Failed to copy STR.crc from" << importPath << "to" << backup_path;
                }
            }
        }
    } else {    // get the STR file that is in the BACKUP folder that we are rebuilding from
        qDebug() << "Rebuilding from BACKUP folder";
        ResMedEDFInfo * stredf = fetchSTRandVerify( strpath, info.serial );
        if ( stredf != nullptr ) {
            QDate date = stredf->edfHdr.startdate_orig.date();
            long int days = stredf->GetNumDataRecords();
            qDebug() << strpath.section("/",-2,-1) << "starts at" << date << "for" << days << "ends" << date.addDays(days-1);
            STRmap[date] = STRFile(strpath, days, stredf);
        } else {
           qDebug() << "Failed to open" << strpath;
        }
    } // end if not importing the backup files
#ifdef STR_DEBUG
    qDebug() << "STRmap size is " << STRmap.size();
#endif

    // Now we open the REAL destination STR_Backup, and open the rest for later parsing

    dir.setPath(backup_path + "STR_Backup");
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::Readable);
    QFileInfoList flist = dir.entryInfoList();
    QDate date;
    long int days;
#ifdef STR_DEBUG
    qDebug() << "STR_Backup folder size is " << flist.size();
#endif

    qDebug() << "Add files in STR_Backup to STRmap (unless they are already there)";
    // Add any STR_Backup versions to the file list
    for (auto & fi : flist) {
        QString filename = fi.fileName();
        if ( ! filename.startsWith("STR", Qt::CaseInsensitive))
            continue;
        if ( ! (filename.endsWith("edf.gz", Qt::CaseInsensitive) || filename.endsWith("edf", Qt::CaseInsensitive)))
            continue;
        QString datestr = filename.section("STR-",-1).section(".edf",0,0);  //  +"01";

        ResMedEDFInfo * stredf = fetchSTRandVerify( fi.canonicalFilePath(), info.serial );
        if ( stredf == nullptr )
            continue;

        // Don't trust the filename date, pick the one inside the STR...
        date = stredf->edfHdr.startdate_orig.date();
        days = stredf->GetNumDataRecords();
        if (STRmap.contains(date)) {        // Keep the longer of the two STR files
            qDebug().noquote() << fi.canonicalFilePath().section("/",-3,-1) << "overlaps" << STRmap[date].filename.section("/",-3,-1) << "for" << days << "ends" << date.addDays(days-1);
            if (days <= STRmap[date].days) {
                qDebug() << "Skipping" << fi.canonicalFilePath().section("/",-3,-1);
                delete stredf;
                continue;
            } else {
                qDebug() << "Removing" << STRmap[date].filename.section("/",-3,-1) << "from STRmap";
                STRmap.remove(date);
            }
        }

        qDebug() << "Adding" << fi.canonicalFilePath().section("/", -3,-1) << "starts at" << date << "for" << days << "to STRmap";
        STRmap[date] = STRFile(fi.canonicalFilePath(), days, stredf);
    }       // end for walking the STR_Backup directory
#ifdef STR_DEBUG
    qDebug() << "Finished STRmap size is now " << STRmap.size();
#endif

    ///////////////////////////////////////////////////////////////////////////////////
    // Build a Date map of all records in STR.edf files, populating ResDayList
    ///////////////////////////////////////////////////////////////////////////////////

    if ( ! ProcessSTRfiles(mach, STRmap, firstImportDay) ) {
        qCritical() << "ProcessSTR failed, abandoning this import";
        return -1;
    }

    // We are done with the Parsed STR EDF objects, so delete them
    for (auto it=STRmap.begin(), end=STRmap.end(); it != end; ++it) {
        QString fullname = it.value().filename;
#ifdef STR_DEBUG
        qDebug() << "Deleting edf object of" << fullname;
#endif
        QString datepart = fullname.section("STR-",-1).section(".edf",0,0);
        if (datepart.size() == 6 ) {    // old style name, change to full date
            QFile str(fullname);
            QString newdate = it.key().toString("yyyyMMdd");
            QString newName = fullname.replace(datepart, newdate);
            qDebug() << "Renaming" << it.value().filename << "to" << newName;
              if ( ! str.rename(newName) )
                  qWarning() << "Rename Failed";
        }
        delete it.value().edf;
    }
#ifdef STR_DEBUG
    qDebug() << "Finished STRmap cleanup";
#endif

    ///////////////////////////////////////////////////////////////////////////////////
    // Scan DATALOG files, sort, and import any new sessions
    ///////////////////////////////////////////////////////////////////////////////////

    // First remove a legacy file if present...
    QFile impfile(mach->getDataPath()+"/imported_files.csv");
    if (impfile.exists())
        impfile.remove();

    emit updateMessage(QObject::tr("Cataloguing EDF Files..."));
    QApplication::processEvents();

    if (isAborted())
        return 0;

    qDebug() << "Starting scan of DATALOG";
//  sleep(1);
    dir.setPath(datalogPath);
    ScanFiles(mach, datalogPath, firstImportDay);
    if (isAborted())
        return 0;

    qDebug() << "Finished DATALOG scan";
//  sleep(1);

    // Now at this point we have resdayList populated with processable summary and EDF files data
    // that can be processed in threads..

    emit updateMessage(QObject::tr("Queueing Import Tasks..."));
    QApplication::processEvents();

    for (auto rdi=resdayList.begin(), rend=resdayList.end(); rdi != rend; rdi++) {
        if (isAborted())
            return 0;

        QDate date = rdi.key();

        ResMedDay & resday = rdi.value();
        resday.date = date;

        checkSummaryDay( resday, date, mach );
    }

    sessionCount = 0;
    emit updateMessage(QObject::tr("Importing Sessions..."));

    // Walk down the resDay list
    qDebug() << "About to call runTasks()";
    runTasks();
    qDebug() << "Finshed runTasks() with" << sessionCount << "new sessions";
    int num_new_sessions = sessionCount;


    ////////////////////////////////////////////////////////////////////////////////////
    // Now look for any new summary data that can be extracted from STR.edf records
    ////////////////////////////////////////////////////////////////////////////////////

    emit updateMessage(QObject::tr("Finishing Up..."));
    QApplication::processEvents();

    qDebug() << "About to call finishAddingSessions()";
    finishAddingSessions();
    qDebug() << "Finshed finishedAddingSessions() with" << sessionCount << "new sessions";

#ifdef DEBUG_EFFICIENCY
    {
        qint64 totalbytes = 0;
        qint64 totalns = 0;
        qDebug() << "Performance / Efficiency Information";

        for (auto it = channel_efficiency.begin(), end=channel_efficiency.end(); it != end; it++) {
            ChannelID code = it.key();
            qint64 value = it.value();
            qint64 ns = channel_time[code];
            totalbytes += value;
            totalns += ns;
            double secs = double(ns) / 1000000000.0L;
            QString s = value < 0 ? "saved" : "cost";
            qDebug() << "Time-Delta conversion for " + schema::channel[code].label() + " " + s + " " +
                     QString::number(qAbs(value)) + " bytes and took " + QString::number(secs, 'f', 4) + "s";
        }

        qDebug() << "Total toTimeDelta function usage:" << totalbytes << "in" << double(totalns) / 1000000000.0 << "seconds";

        qDebug() << "Total CPU time in EDF Open" << timeInEDFOpen;
        qDebug() << "Total CPU time in EDF Parser" << timeInEDFInfo;
        qDebug() << "Total CPU time in LoadBRP" << timeInLoadBRP;
        qDebug() << "Total CPU time in LoadPLD" << timeInLoadPLD;
        qDebug() << "Total CPU time in LoadSAD" << timeInLoadSAD;
        qDebug() << "Total CPU time in LoadEVE" << timeInLoadEVE;
        qDebug() << "Total CPU time in LoadCSL" << timeInLoadCSL;
        qDebug() << "Total CPU time in (BRP) AddWaveform" << timeInAddWaveform;
        qDebug() << "Total CPU time in TimeDelta function" << timeInTimeDelta;

        channel_efficiency.clear();
        channel_time.clear();
    }
#endif

//    sessfiles.clear();
//    strsess.clear();
//    strdate.clear();

    qDebug() << "Total Events " << event_cnt;
    qDebug() << "Total new Sessions " << num_new_sessions;

    mach->clearPurgeDate();

    return num_new_sessions;
}   // end Open()

ResMedEDFInfo * fetchSTRandVerify( QString filename, QString serialNumber)
{
    ResMedEDFInfo * stredf = new ResMedEDFInfo();
    if ( ! stredf->Open(filename ) ) {
        qWarning() << "Failed to open" << filename;
        delete stredf;
        return nullptr;
    }
    if ( ! stredf->Parse()) {
        qDebug() << "Faulty STR file" << filename;
        delete stredf;
        return nullptr;
    }

    if (stredf->serialnumber != serialNumber) {
        qDebug() << "Identification.tgt Serial number doesn't match" << filename;
        delete stredf;
        return nullptr;
    }
    return stredf;
}

void StoreSettings(Session * sess, STRRecord & R);  // forward
void ResmedLoader::checkSummaryDay( ResMedDay & resday, QDate date, Machine * mach )
{
    Day * day = p_profile->FindDay(date, MT_CPAP);
    bool reimporting = false;
#ifdef STR_DEBUG
    qDebug() << "Starting checkSummary for" << date.toString();
#endif
    if (day && day->hasMachine(mach)) {
        // Sessions found for this device, check if only summary info
#ifdef STR_DEBUG
        qDebug() << "Sessions already found for this date";
#endif
        if (day->summaryOnly(mach) && (resday.files.size()> 0)) {
            // Note: if this isn't an EDF file, there's really no point doing this here,
            // but the worst case scenario is this session is deleted and reimported.. this just slows things down a bit in that case
            // This day was first imported as a summary from STR.edf, so we now totally want to redo this day
#ifdef STR_DEBUG
            qDebug() << "Summary sessions only - delete them";
#endif
            QList<Session *> sessions = day->getSessions(MT_CPAP);
            for (auto & sess : sessions) {
                day->removeSession(sess);
                delete sess;
            }
        } else if (day->noSettings(mach) && resday.str.date.isValid()) {
            // STR is present now, it wasn't before... we don't need to trash the files, but we do want the official settings.
            // Do it right here
#ifdef STR_DEBUG
            qDebug() << "Date was missing settings, now we have them";
#endif
            for (auto & sess : day->sessions) {
                if (sess->machine() != mach)
                    continue;
#ifdef STR_DEBUG
                qDebug() << "Adding STR.edf information to session" << sess->session();
#endif
                StoreSettings(sess, resday.str);
                sess->setNoSettings(false);
                sess->SetChanged(true);
                sess->StoreSummary();
            }
        } else {
#ifdef STR_DEBUG
            qDebug() << "Have summary and details for this date!";
#endif
            int numPairs = 0;
            for (int i = 0; i <resday.str.maskevents/2; i++)
                if (resday.str.maskon[i] != resday.str.maskoff[i])
                    numPairs++;
            QList<Session *> sessions = day->getSessions(MT_CPAP, true);
            // If we have more sessions that we found in the str file,
            // or if the sessions are for a different device,
            // leave well enough alone and don't re-import the day
            if (sessions.length() >= numPairs || sessions[0]->machine() != mach) {
#ifdef STR_DEBUG
                qDebug() << "No new sessions -- skipping.  Sessions now in day:";
                qDebug() << " i  sessionID    s_first                   from  -  to";
                for (int i=0; i < sessions.length(); i++) {
                    qDebug().noquote() << i << sessions[i]->session()
                             << sessions[i]->first()
                             << QDateTime::fromMSecsSinceEpoch(sessions[i]->first()).toString(" hh:mm:ss")
                             << "-" << QDateTime::fromMSecsSinceEpoch(sessions[i]->last()).toString("hh:mm:ss");
                }
#endif
                return;
            }
            qDebug() << "Maskevent count/2 (modified)" << numPairs << "is greater than the existing MT_CPAP session count" << sessions.length();
            qDebug().noquote() << "Purging and re-importing" << day->date().toString();
            for (auto & sess : sessions) {
                day->removeSession(sess);
                delete sess;
            }
        }
    }

    ResDayTask * rdt = new ResDayTask(this, mach, &resday, saveCallback);
    rdt->reimporting = reimporting;
#ifdef STR_DEBUG
    qDebug() << "in checkSummary, Queue task for" << resday.date.toString();
#endif
    queTask(rdt);
}

///////////////////////////////////////////////////////////////////////////////////////////
// Sorted EDF files that need processing into date records according to ResMed noon split
///////////////////////////////////////////////////////////////////////////////////////////
int ResmedLoader::ScanFiles(Machine * mach, const QString & datalog_path, QDate firstImport)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
#endif

    bool create_backups = p_profile->session->backupCardData();
    QString backup_path = mach->getBackupPath();

    if (datalog_path == (backup_path + RMS9_STR_datalog + "/")) {
        // Don't create backups if importing from backup folder
        create_backups = false;
    }


    ///////////////////////////////////////////////////////////////////////////////////////
    // Generate list of files for later processing
    ///////////////////////////////////////////////////////////////////////////////////////
    qDebug() << "Generating list of EDF files";
    qDebug() << "First Import date is " << firstImport;

#ifdef DEBUG_EFFICIENCY
    time.start();
#endif

    QDir dir(datalog_path);

    // First list any EDF files in DATALOG folder - Series 9 devices
    QStringList filter;
    filter << "*.edf";
    dir.setNameFilters(filter);
    QFileInfoList EDFfiles = dir.entryInfoList();

    // Scan through all folders looking for EDF files, skip any already imported and peek inside to get durations

    dir.setNameFilters(QStringList());
    dir.setFilter(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    QString filename;
    bool ok;

    QFileInfoList dirlist = dir.entryInfoList();
    int dirlistSize = dirlist.size();

//  QDateTime ignoreBefore = p_profile->session->ignoreOlderSessionsDate();
//  bool ignoreOldSessions = p_profile->session->ignoreOlderSessions();

    // Scan for any sub folders and create files lists
    for (int i = 0; i < dirlistSize ; i++) {
        const QFileInfo & fi = dirlist.at(i);
        filename = fi.fileName();

        int len = filename.length();
        if (len == 4) {                    // This is a year folder in BackupDATALOG
            filename.toInt(&ok);
            if ( ! ok ) {
                qDebug() << "Skipping directory - bad 4-letter name" << filename;
                continue;
            }
        } else if (len == 8) {        // test directory date
            QDate dirDate = QDate().fromString(filename, "yyyyMMdd");
            if (dirDate < firstImport) {
#ifdef SESSION_DEBUG
                qDebug() << "Skipping directory - ignore before " << filename;
#endif
                continue;
            }
        } else {
            qDebug() << "Skipping directory - bad name size " << filename;
            continue;
        }
        // Get file lists under this directory
        dir.setPath(fi.canonicalFilePath());
        dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        dir.setSorting(QDir::Name);

        // Append all files to one big QFileInfoList
        EDFfiles.append(dir.entryInfoList());
    }
#ifdef DEBUG_EFFICIENCY
    qDebug() << "Generating EDF files list took" << time.elapsed() << "ms";
#endif
    qDebug() << "EDFfiles list size is " << EDFfiles.size();

    ////////////////////////////////////////////////////////////////////////////////////////
    // Scan through EDF files, Extracting EDF Durations, and skipping already imported files
    // Check for duplicates along the way from compressed/uncompressed files
    ////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_EFFICIENCY
    time.start();
#endif
    QString datestr;
    QDateTime datetime;
    QDate date;
    int totalfiles = EDFfiles.size();
    qDebug() << "Scanning " << totalfiles << " EDF files";

    // Calculate number of files for progress bar for this stage
    int pbarFreq = totalfiles / 50;
    if (pbarFreq < 1) // stop a divide by zero
        pbarFreq = 1;

    emit setProgressValue(0);
    emit setProgressMax(totalfiles);
    QCoreApplication::processEvents();

    qDebug() << "Starting EDF duration scan pass";
    for (int i=0; i < totalfiles; ++i) {
        if (isAborted())
            return 0;

        const QFileInfo & fi = EDFfiles.at(i);

        // Update progress bar
        if ((i % pbarFreq) == 0) {
            emit setProgressValue(i);
            QCoreApplication::processEvents();
        }

        // Forget about it if it can't be read.
        if (!fi.isReadable()) {
            qWarning() << fi.fileName() << "is unreadable and has been ignored";
            continue;
        }

        // Skip empty files
        if (fi.size() == 0) {
            qWarning() << fi.fileName() << "is empty and has been ignored";
            continue;
        }

        filename = fi.fileName();

        datestr = filename.section("_", 0, 1);
        datetime = QDateTime().fromString(datestr,"yyyyMMdd_HHmmss");
        date = datetime.date();
        // ResMed splits days at noon and now so do we, so all times before noon
        // go to the previous day
        if (datetime.time().hour() < 12) {
            date = date.addDays(-1);
        }

        if (date < firstImport) {
#ifdef SESSION_DEBUG
            qDebug() << "Skipping file - ignore before " << filename;
#endif
            continue;
        }

        // Chop off the .gz component if it exists, it's not needed at this stage
        if (filename.endsWith(STR_ext_gz)) {
            filename.chop(3);
        }
        QString fullpath = fi.filePath();

        QString newpath = create_backups ? Backup(fullpath, backup_path) : fullpath;

        // Accept only .edf and .edf.gz files
        if (filename.right(4).toLower() != ("."+STR_ext_EDF))
            continue;

//        QString ext = key.section("_", -1).section(".",0,0).toUpper();
//        EDFType type = lookupEDFType(ext);

        // Find or create ResMedDay object for this date
        auto rd = resdayList.find(date);
        if (rd == resdayList.end()) {
            rd = resdayList.insert(date, ResMedDay(date));
            rd.value().date = date;

            // We have data files without STR.edf record... the user MAY be planning on importing from another backup
            // later which could cause problems if we don't deal with it.
            // Best solution I can think of is import and tag the day No Settings and skip the day from overview.
        }
        ResMedDay & resday = rd.value();

        if ( ! resday.files.contains(filename)) {
            resday.files[filename] = newpath;
        }
    }
#ifdef DEBUG_EFFICIENCY
    qDebug() << "Scanning EDF files took" << time.elapsed() << "ms";
#endif

    qDebug() << "resdayList size is " << resdayList.size();

    return resdayList.size();
}           // end of scanFiles

QString ResmedLoader::Backup(const QString & fullname, const QString & backup_path)
{
    QDir dir;
    QString filename, yearstr, newname, oldname;

    bool compress = p_profile->session->compressBackupData();

    bool ok;
    bool gz = (fullname.right(3).toLower() == STR_ext_gz);  // Input file is a .gz?


    filename = fullname.section("/", -1);
    if (gz) {
        filename.chop(3);
    }

    yearstr = filename.left(4);
    yearstr.toInt(&ok, 10);

    if ( ! ok) {
        qDebug() << "Invalid EDF filename given to ResMedLoader::Backup()" << fullname;
        return "";
    }

    QString newpath = backup_path + "DATALOG" + "/" + yearstr;
    if ( ! dir.exists(newpath) )
        dir.mkpath(newpath);

    newname = newpath+"/"+filename;

    QString tmpname = newname;

    QString newnamegz = newname + STR_ext_gz;
    QString newnamenogz = newname;

    newname = compress ? newnamegz : newnamenogz;

    // First make sure the correct backup exists in the right place
    // Allow for second import of newer version of EVE and CSL edf files
    // But don't try to copy onto itself (as when rebuilding CPAP data from backup)
    // Compare QDirs rather than QStrings to handle variations in separators, etc.

    QFile nf(newname);
    QFile of(fullname);
    QFileInfo nfi(nf);
    QFileInfo ofi(of);
    QDir nfdir = nfi.dir();
    QDir ofdir = ofi.dir();

    if (nfdir != ofdir) {
        if (QFile::exists(newname)) // remove existing backup
            QFile::remove(newname);
        if (compress) {
            // If input file is already compressed.. copy it to the right location, otherwise compress it
            if (gz) {
                if (!QFile::copy(fullname, newname))
                    qWarning() << "unable to copy" << fullname << "to" << newname;
            }
            else
                compressFile(fullname, newname);
        } else {
            // If inputs a gz, uncompress it, otherwise copy is raw
            if (gz)
                uncompressFile(fullname, newname);
            else {
                if (!QFile::copy(fullname, newname))
                    qWarning() << "unable to copy" << fullname << "to" << newname;
            }
        }
    }

    // Now the correct backup is in place, we can trash any unneeded backup
    if (compress) {
        // Remove any uncompressed duplicate
        if (QFile::exists(newnamenogz))
            QFile::remove(newnamenogz);
    } else {
        // Delete the compressed copy
        if (QFile::exists(newnamegz))
            QFile::remove(newnamegz);
    }

    // Used to store it under Backup\Datalog
    // Remove any traces from old backup directory structure
    if (nfdir != ofdir) {
        oldname = backup_path + RMS9_STR_datalog + "/" + filename;
        if (QFile::exists(oldname))
            QFile::remove(oldname);
        if (QFile::exists(oldname + STR_ext_gz))
            QFile::remove(oldname + STR_ext_gz);
    }
    return newname;
}


// This function parses a list of STR files and creates a date ordered map of individual records
bool ResmedLoader::ProcessSTRfiles(Machine *mach, QMap<QDate, STRFile> & STRmap, QDate firstImport)
{
    bool AS_eleven = (mach->info.modelnumber.toInt() >= 39000);

//  QDateTime ignoreBefore = p_profile->session->ignoreOlderSessionsDate();
//  bool ignoreOldSessions = p_profile->session->ignoreOlderSessions();

    qDebug() << "Starting ProcessSTRfiles";

    int totalRecs = 0;          // Count the STR days
    for (auto it=STRmap.begin(), end=STRmap.end(); it != end; ++it) {
        STRFile & file = it.value();
        ResMedEDFInfo & str = *file.edf;
        int days = str.GetNumDataRecords();
        totalRecs += days;
#ifdef STR_DEBUG
        qDebug() << "STR file is" << file.filename.section("/", -3, -1);
        qDebug() << "First day" << QDateTime::fromMSecsSinceEpoch(str.startdate, EDFInfo::localNoDST).date().toString() << "for" << days << "days";
#endif
    }

    emit updateMessage(QObject::tr("Parsing STR.edf records..."));
    emit setProgressMax(totalRecs);
    QCoreApplication::processEvents();

    int currentRec = 0;

    // Walk through all the STR files in the STRmap
    for (auto it=STRmap.begin(), end=STRmap.end(); it != end; ++it) {
        STRFile & file = it.value();
        ResMedEDFInfo & str = *file.edf;

        QDate date = str.edfHdr.startdate_orig.date(); // each STR.edf record starts at 12 noon
        int size = str.GetNumDataRecords();
        QDate lastDay = date.addDays(size-1);

#ifdef STR_DEBUG
        QString & strfile = file.filename;
        qDebug() << "Processing" << strfile.section("/", -3, -1) << date.toString() << "for" << size << "days";
        qDebug() << "Last day is" << lastDay;
#endif

        if ( lastDay < firstImport ) {
#ifdef STR_DEBUG
            qDebug() << "LastDay before firstImport, skipping" << strfile.section("/", -3, -1);
#endif
            continue;
        }

        // ResMed and their consistent naming and spacing... :/
        EDFSignal *maskon = str.lookupLabel("Mask On");     // Series 9 devices
        if (!maskon) {
            maskon = str.lookupLabel("MaskOn");             // Series 1x devices
        }
        EDFSignal *maskoff = str.lookupLabel("Mask Off");
        if (!maskoff) {
             maskoff = str.lookupLabel("MaskOff");
        }
        EDFSignal *maskeventcount = str.lookupLabel("Mask Events");
        if ( ! maskeventcount) {
             maskeventcount = str.lookupLabel("MaskEvents");
        }
        if ( !maskon || !maskoff || !maskeventcount ) {
            qCritical() << "Corrupt or untranslated STR.edf file";
            return false;
        }

        EDFSignal *sig = nullptr;

        // For each data record, representing 1 day each
        for (int rec = 0; rec < size; ++rec, date = date.addDays(1)) {
            emit setProgressValue(++currentRec);
            QCoreApplication::processEvents();

            if (date < firstImport) {
#ifdef SESSION_DEBUG
                qDebug() << "Skipping" << date.toString() << "Before" << firstImport.toString();
#endif
                continue;
            }

//      This is not what we want to check, we must look at this day in the database files...
//      Found the place in checkSummaryDay to compare session count with maskevents divided by 2
#ifdef SESSION_DEBUG
            qDebug() << "ResdayList size is" << resdayList.size();
#endif
//             auto rit = resdayList.find(date);
//             if (rit != resdayList.end()) {
//                 // Already seen this record.. should check if the data is the same, but meh.
//                 // At least check the maskeventcount to see if it changed...
//                 if ( maskeventcount->dataArray[0] != rit.value().str.maskevents ) {
//                     qDebug() << "Mask events don't match, purge" << rit.value().date.toString();
// //                  purge...
//                 }
// // #ifdef SESSION_DEBUG
//                 qDebug() << "Skipping" << date.toString() << "Already saw this one";
// // #endif
//                 continue;
//             } // else {
//     //          qWarning() << date.toString() << "is missing from resdayList - FIX THIS";
//     //          continue;
//     //      }

            int recstart = rec * maskon->sampleCnt;

            bool validday = false;
            for (int s = 0; s < maskon->sampleCnt; ++s) {
                qint32 on = maskon->dataArray[recstart + s];
                qint32 off = maskoff->dataArray[recstart + s];

                if (((on >= 0) && (off >= 0)) && (on != off)) {// ignore very short on-off times
                        validday=true;
                }
            }
            if ( ! validday) {
                // There are no mask on/off events, so this STR day is useless.
                qDebug() << "Skipping" << date.toString() << "No mask events";
                continue;
            }

#ifdef STR_DEBUG
            qDebug() << "Adding" << date.toString() << "to resdayLisyt b/c we have STR record";
#endif
            auto rit = resdayList.insert(date, ResMedDay(date));

#ifdef STR_DEBUG
            qDebug() << "Setting up STRRecord for" << date.toString();
#endif
            STRRecord &R = rit.value().str;

            uint noonstamp = QDateTime(date,QTime(12,0,0), EDFInfo::localNoDST).toSecsSinceEpoch();
            R.date = date;

            // skipday = false;

            // For every mask on, there will be a session within 1 minute either way
            // We can use that for data matching
            // Scan the mask on/off events by minute
            R.maskon.resize(maskon->sampleCnt);
            R.maskoff.resize(maskoff->sampleCnt);
            int lastOn = -1;
            int lastOff = -1;
            for (int s = 0; s < maskon->sampleCnt; ++s) {
                qint32 on = maskon->dataArray[recstart + s];    // these on/off times are minutes since noon
                qint32 off = maskoff->dataArray[recstart + s];  // we want the actual time in seconds
                if ( (on > 24*60) || (off > 24*60) ) {
                    qWarning().noquote() << "Mask times are out of range. Possible SDcard corruption" << "date" << date << "on" << on << "off" <<off;
                    continue;
                }
                if ( on > 0 ) {       // convert them to seconds since midnight
                    lastOn = s;
                    R.maskon[s] = (noonstamp + (on * 60));
                } else
                    R.maskon[s] = 0;
                if ( off > 0 ) {
                    lastOff = s;
                    R.maskoff[s] = (noonstamp + (off * 60));
                } else
                    R.maskoff[s] = 0;
            }

            // two conditions that need dealing with, mask running at noon start, and finishing at noon start..
            // (Sessions are forcibly split by resmed.. why the heck don't they store it that way???)
            if ((R.maskon[0]==0) && (R.maskoff[0]>0)) {
                R.maskon[0] = noonstamp;
            }
            if ( (lastOn >= 0) && (lastOff >= 0) ) {
                if ((R.maskon[lastOn] > 0) && (R.maskoff[lastOff] == 0)) {
                    R.maskoff[lastOff] = QDateTime(date,QTime(12,0,0), EDFInfo::localNoDST).addDays(1).toSecsSinceEpoch() - 1;
                }
            }

            R.maskevents = maskeventcount->dataArray[rec];

            CPAPMode mode = MODE_UNKNOWN;

            #if 0
            // OSCAR Therapy modes
            0 MODE_UNKNOWN = 0
            1 MODE_CPAP
            2 MODE_APAP
            3 MODE_BILEVEL_FIXED
            4 MODE_BILEVEL_AUTO_FIXED_PS
            5 MODE_BILEVEL_AUTO_VARIABLE_PS
            6 MODE_ASV
            7 MODE_ASV_VARIABLE_EPAP
            8 MODE_AVAPS
            9 MODE_TRILEVEL_AUTO_VARIABLE_PDIFF

            AS/AC 10 therapy modes characteristics      OSCAR MODES
                11  Her                                 MODE_APAP
                10  PAC what ever that is               MODE_UNKNOWN
                9                                       MODE_AVAPS
                8   vpap adapt variable epap            MODE_ASV_VARIABLE_EPAP
                7   vpap adapt                          MODE_ASV
                6   vpap auto (Min EPAP, Max IPAP, PS)  MODE_BILEVEL_AUTO_FIXED_PS
                5                                       MODE_BILEVEL_FIXED
                4                                       MODE_BILEVEL_FIXED
                3   vpap auto (Min EPAP, Max IPAP, PS)  MODE_BILEVEL_FIXED
                2                                       MODE_BILEVEL_FIXED
                1                                       MODE_APAP
                0   Unkown                              MODE_CPAP

            // Therapy Modes for Resmed version 10 and prior
            //  0;    // CPAP
            //  1;    // APAP
            //  2;    // Bilevel-T
            //  3;    // BiLevel-S
            //  4;    // BiLevel-S/T ??
            //  5;    // MODE_BILEVEL_FIXED  BiLevel-S/T
            //  6;    // ASVauto is displayed
            //  7;    // ASV
            //  8;    // ASVauto
            //  9;    // IVAPS   MODE_AVAPS  BiLevel-T
            //  10;   // PAC
            //  11;   // Auto for Her
            //  12;   // ASVauto  ??
			
            // Therapy Modes for Resmed version 11
            //  0;    CPAP
            //  1;    AirSenseAutoSet APAP		7 9
			//        ResCan Displays TherapyMode AutoSet
			//        		EPR FULLTIME
			//              EPR Enable
			//              EPR Patient Enable
			//              EPR Level
			//              Ramp Enable
			//              Ramp Time
			//              Patient View
			//              Resonse
			//        OSCAR displays    Min {Min} Max {max}
            //  2;    AirSenseAutoSet APAP her
			//  	  Mapped by previous developers.
            //  3;    AirSenseAutoSet CPAP 	
			//        ResCan Displays TherapyMode CPAP
			//              Set Pressure
			//              EPR Level
			//              Ramp Time
			//              Patient View
			//        OSCAR displays    PS {PS} over {MinEPAP}-{MAXIPAP}
            //  4;    AirCurveVauto BiLevel-S
            //  5;
            //  6;    AirCurveASV  ASV	
			//        ResCan Displays TherapyMode ASV
			//        		EPAP and MixMaxPs
			//              Ramp Enable
			//              Ramp Time
			//              Patient View
			//        OSCAR displays    EPAP {v} PS{minPS}-{maxPS} (cmH20)
            //  7;    AirCurveVauto ASVauto
			//        ResCan Displays TherapyMode ASVauto
			//              MinMaxEPAP
			//              MixMaxPs
			//              Ramp Enable
			//              Ramp Time
			//              Patient View
			//        OSCAR displays    MinEPAP {v} MaxIPAP {v} PS{minPS}-{maxPS} (cmH20)
            //  8;    AirCurveVauto vAuto
			//        ResCan Displays TherapyMode vAuto
			//              Ramp Enable
			//              Ramp Time
			//              Patient View
			//              Pressure Support (PS)
			//              MinEPAP
			//              MaxIPAP
			//              MixMaxPs
			//              MinMaxTI
			//        OSCAR displays    PS {PS} over {MinEPAP}-{MAXIPAP}


            // Therapy Modes for Resmed version 11 - for device therapy
            //  AC11 Mode  AC11 Therapy              AC10 Therapy       OSCAR Theriapy
            //  0;         ???
            //  1;         AirSenseAutoSet APAP	     => 1  APAP         => MODE_APAP
            //  2;         AirSenseAutoSet APAP her  => 11 APAPher      => MODE_APAP
            //  3;         AirSenseAutoSet CPAP 	 => 0  CPAP         => MODE_CPAP 
            //  4;         AirCurveVauto BiLevel-S   => 3  BiLevel-S    => MODE_BILEVEL_FIXED
            //  5;         ???
            //  6;         AirCurveASV  ASV			 => 7  ASV          => MODE_ASV
            //  7;         AirCurveVauto ASVauto     => 8  ASVauto      => MODE_ASV_VARIABLE_EPAP
            //  8;         AirCurveVauto vAuto       => 6  vAuto        => MODE_BILEVEL_FIXED
            //  ?;         ???                       => 16 Unknown      => MODE_UNKNOWN

            #endif

            if ((sig = str.lookupSignal(CPAP_Mode))) {
                int mod = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                R.rms9_mode = mod;
                // Convert AirCurve 11 and AirSense 11 to appropiate resmed 10 modes.
                // map Resmed 11 devices theraphy mode back to Previous Resmed 10 modes
                // EPR is pressure recovery not pressure settings
                if ( AS_eleven  ) {         // translate AS11 mode values back to S9 / AS10 values

                    switch ( mod ) {
                    case 0:                     // AS11 CPAP
                        R.rms9_mode = 16;       //Unknown - have not seen this machine
                        break;
                    case 1:                     // AS11 Autoset - APAP
                        R.rms9_mode = 1;        // still APAP
                        break;
                    case 2:                     // AS11 AutoSet  - APAP for Her
                        R.rms9_mode = 11;       //make it look like AS10 A4Her
                        break;
                    case 3:                     // AS11 Autoset  CPAP with adjustable EPR.h
                        R.rms9_mode = 0;        // make it be CPAP
                        break;
                    case 4:
                        R.rms9_mode = 3;        //MODE_BILEVEL_FIXED  BiLevel-S  EPAP{n/a} IPAP{n/a} (cmH2O)
                        break;
                    case 5:	       			    // never seen this one
                        R.rms9_mode = 16;       //Unknown - have not seen this machine
                        break;
                    case 6:					    // Added for ASV WHat therapy mode
                        R.rms9_mode = 7;        // make it be ASV
                        break;
                    case 7:					    // AC11 ASV ASVauto
                        R.rms9_mode = 8;        // make it be AC10 ASVauto
                        break;
                    case 8:					    // AC11 vAuto  MODE_BILEVEL_AUTO_FIXED_PS
                        R.rms9_mode = 6;
                        break;
                    default:
                        R.rms9_mode = 16;   // unknown for now
                        break;
                    };
                    // DEBUGCI QQ(serialNumber,mach->info.serial) QQ(Resmed10Mode,R.rms9_mode) QQ(Resmed11Mode,mod) QQ(model,mach->info.modelnumber) ;
                }
                // Map Resmed theraphy mode to OSCAR therapy modes
                // for all modes prior to AirCurve 11
                int RMS9_mode = R.rms9_mode;
                switch ( RMS9_mode ) {
                case 11:
                    mode = MODE_APAP; // For her is a special apap
                    break;
                case 10:
                    mode = MODE_UNKNOWN;    // it's PAC, whatever that is
                    break;
                case 9:
                    mode = MODE_AVAPS;
                    break;
                case 8:    // mod 8 == vpap adapt variable epap
                    mode = MODE_ASV_VARIABLE_EPAP;
                    break;
                case 7:    // mod 7 == vpap adapt
                    mode = MODE_ASV;
                    break;
                case 6:    // mod 6 == vpap auto (Min EPAP, Max IPAP, PS)
                    mode = MODE_BILEVEL_AUTO_FIXED_PS;
                    break;
                case 5: // 4,5 are S/T types...
                case 4:
                case 3:    // mod 3 == vpap s fixed pressure (EPAP, IPAP, No PS)
                    mode = MODE_BILEVEL_FIXED;
                    break;
                case 2:
                    mode = MODE_BILEVEL_FIXED;
                    break;
                case 1:
                    mode = MODE_APAP; // mod 1 == apap
                    break;
                case 0:
                    mode = MODE_CPAP; // mod 0 == cpap
                    break;
                default:
                    mode = MODE_UNKNOWN;
                }
                //DEBUGCI QQ(serialNumber,mach->info.serial) QQ(OSCARMode,mode) QQ(Resmed10Mode,R.rms9_mode) QQ(model,mach->info.modelnumber) ;
                R.mode = mode;

                // Settings.CPAP.Starting Pressure
                if ((R.rms9_mode == 0) && (sig = str.lookupLabel("S.C.StartPress"))) {
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                // Settings.Adaptive Starting Pressure?
                if ( (R.rms9_mode == 1) && ((sig = str.lookupLabel("S.AS.StartPress")) || (sig = str.lookupLabel("S.A.StartPress"))) ) {
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                // mode 11 = APAP for her?
                if ( (R.rms9_mode == 11) && (sig = str.lookupLabel("S.AFH.StartPress"))) {
                        R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((R.mode == MODE_BILEVEL_FIXED) && (sig = str.lookupLabel("S.BL.StartPress"))) {
                    // Bilevel Starting Pressure
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if (((R.mode == MODE_ASV) || (R.mode == MODE_ASV_VARIABLE_EPAP) ||
                     (R.mode == MODE_BILEVEL_AUTO_FIXED_PS)) && (sig = str.lookupLabel("S.VA.StartPress"))) {
                    // Bilevel Starting Pressure
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
            }

// Collect the staistics
            if ((sig = str.lookupLabel("Mask Dur")) || (sig = str.lookupLabel("Duration"))) {
                R.maskdur = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("Leak Med")) || (sig = str.lookupLabel("Leak.50"))) {
                float gain = sig->gain * 60.0;
                R.leak50 = EventDataType(sig->dataArray[rec]) * gain;
            }
            if ((sig = str.lookupLabel("Leak Max"))|| (sig = str.lookupLabel("Leak.Max"))) {
                float gain = sig->gain * 60.0;
                R.leakmax = EventDataType(sig->dataArray[rec]) * gain;
            }
            if ((sig = str.lookupLabel("Leak 95")) || (sig = str.lookupLabel("Leak.95"))) {
                float gain = sig->gain * 60.0;
                R.leak95 = EventDataType(sig->dataArray[rec]) * gain;
            }
            if ((sig = str.lookupLabel("RespRate.50")) || (sig = str.lookupLabel("RR Med"))) {
                R.rr50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("RespRate.Max")) || (sig = str.lookupLabel("RR Max"))) {
                R.rrmax = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("RespRate.95")) || (sig = str.lookupLabel("RR 95"))) {
                R.rr95 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("MinVent.50")) || (sig = str.lookupLabel("Min Vent Med"))) {
                R.mv50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("MinVent.Max")) || (sig = str.lookupLabel("Min Vent Max"))) {
                R.mvmax = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("MinVent.95")) || (sig = str.lookupLabel("Min Vent 95"))) {
                R.mv95 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("TidVol.50")) || (sig = str.lookupLabel("Tid Vol Med"))) {
                R.tv50 = EventDataType(sig->dataArray[rec]) * (sig->gain*1000.0);
            }
            if ((sig = str.lookupLabel("TidVol.Max")) || (sig = str.lookupLabel("Tid Vol Max"))) {
                R.tvmax = EventDataType(sig->dataArray[rec]) * (sig->gain*1000.0);
            }
            if ((sig = str.lookupLabel("TidVol.95")) || (sig = str.lookupLabel("Tid Vol 95"))) {
                R.tv95 = EventDataType(sig->dataArray[rec]) * (sig->gain*1000.0);
            }

            if ((sig = str.lookupLabel("MaskPress.50")) || (sig = str.lookupLabel("Mask Pres Med"))) {
                R.mp50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("MaskPress.Max")) || (sig = str.lookupLabel("Mask Pres Max"))) {
                R.mpmax = EventDataType(sig->dataArray[rec]) * sig->gain ;
            }
            if ((sig = str.lookupLabel("MaskPress.95")) || (sig = str.lookupLabel("Mask Pres 95"))) {
                R.mp95 = EventDataType(sig->dataArray[rec]) * sig->gain ;
            }

            if ((sig = str.lookupLabel("TgtEPAP.50")) || (sig = str.lookupLabel("Exp Pres Med"))) {
                R.tgtepap50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("TgtEPAP.Max")) || (sig = str.lookupLabel("Exp Pres Max"))) {
                R.tgtepapmax = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("TgtEPAP.95")) || (sig = str.lookupLabel("Exp Pres 95"))) {
                R.tgtepap95 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }

            if ((sig = str.lookupLabel("TgtIPAP.50")) || (sig = str.lookupLabel("Insp Pres Med"))) {
                R.tgtipap50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("TgtIPAP.Max")) || (sig = str.lookupLabel("Insp Pres Max"))) {
                R.tgtipapmax = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("TgtIPAP.95")) || (sig = str.lookupLabel("Insp Pres 95"))) {
                R.tgtipap95 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }

            if ((sig = str.lookupLabel("I:E Med"))) {
                R.ie50 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("I:E Max"))) {
                R.iemax = EventDataType(sig->dataArray[rec]) * sig->gain;
            }
            if ((sig = str.lookupLabel("I:E 95"))) {
                R.ie95 = EventDataType(sig->dataArray[rec]) * sig->gain;
            }

// Collect the pressure settings

            if ((sig = str.lookupSignal(CPAP_IPAP))) {
                R.ipap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_EPAP))) {
                R.epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if (R.mode == MODE_AVAPS) {
                if ((sig = str.lookupLabel("S.i.StartPress"))) {
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.EPAP"))) {
                    R.min_epap = R.max_epap = R.epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.EPAPAuto"))) {
                    R.epapAuto = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.MinPS"))) {
                    R.min_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.MinEPAP"))) {
                    R.min_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.MaxEPAP"))) {
                    R.max_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.i.MaxPS"))) {
                    R.max_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ( (R.epap >= 0) && (R.epapAuto == 0) ) {
                    R.max_ipap = R.epap + R.max_ps;
                    R.min_ipap = R.epap + R.min_ps;
                } else {
                    R.max_ipap = R.max_epap + R.max_ps;
                    R.min_ipap = R.min_epap + R.min_ps;
                }
                qDebug() << "AVAPS mode; Ramp" << R.s_RampPressure << "Fixed EPAP" << ((R.epapAuto == 0) ? "True" : "False") <<
                            "EPAP" << R.epap << "Min EPAP" << R.min_epap <<
                            "Max EPAP" << R.max_epap << "Min PS" << R.min_ps << "Max PS" << R.max_ps <<
                            "Min IPAP" << R.min_ipap << "Max_IPAP" << R.max_ipap;
            }
            if (R.mode == MODE_ASV) {
                if ((sig = str.lookupLabel("S.AV.StartPress"))) {
                    R.s_RampPressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AV.EPAP"))) {
                    R.min_epap = R.max_epap = R.epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AV.MinPS"))) {
                    R.min_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AV.MaxPS"))) {
                    R.max_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    R.max_ipap = R.epap + R.max_ps;
                    R.min_ipap = R.epap + R.min_ps;
                }
            }
            if (R.mode == MODE_ASV_VARIABLE_EPAP) {
                if ((sig = str.lookupLabel("S.AA.StartPress"))) {
                    EventDataType sp = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    R.s_RampPressure = sp;
                }
                if ((sig = str.lookupLabel("S.AA.MinEPAP"))) {
                    R.min_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AA.MaxEPAP"))) {
                    R.max_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AA.MinPS"))) {
                    R.min_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.AA.MaxPS"))) {
                    R.max_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    R.max_ipap = R.max_epap + R.max_ps;
                    R.min_ipap = R.min_epap + R.min_ps;
                }
            }
            if ( (R.rms9_mode == 11) && (sig = str.lookupLabel("S.AFH.MaxPress")) ) {
                R.max_pressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            else if ((sig = str.lookupSignal(CPAP_PressureMax))) {
                R.max_pressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ( (R.rms9_mode == 11) && (sig = str.lookupLabel("S.AFH.MinPress")) ) {
                R.min_pressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            else if ((sig = str.lookupSignal(CPAP_PressureMin))) {
                R.min_pressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(RMS9_SetPressure))) {
                R.set_pressure = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_EPAPHi))) {
                R.max_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_EPAPLo))) {
                R.min_epap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_IPAPHi))) {
                R.max_ipap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_IPAPLo))) {
                R.min_ipap = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupSignal(CPAP_PS))) {
                R.ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }

            // Okay, problem here: THere are TWO PSMin & MAX dataArrays on the 36037 with the same string
            // One is for ASV mode, and one is for ASVAuto
            int psvar = (mode == MODE_ASV_VARIABLE_EPAP) ? 1 : 0;

            if ((sig = str.lookupLabel("Max PS", psvar))) {
                R.max_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("Min PS", psvar))) {
                R.min_ps = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }

            if (mode == MODE_ASV_VARIABLE_EPAP) {
                R.min_ipap = R.min_epap + R.min_ps;
                R.max_ipap = R.max_epap + R.max_ps;
            } else if (mode == MODE_ASV) {
                R.min_ipap = R.epap + R.min_ps;
                R.max_ipap = R.epap + R.max_ps;
            }

// Collect the other settings

            if ((sig = str.lookupLabel("S.AS.Comfort"))) {
                R.s_Comfort = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_Comfort--;
            }

            EventDataType epr = -1, epr_level = -1;
            bool a1x = false;       // AS-10 or AS-11
            if ((mode == MODE_CPAP) || (mode == MODE_APAP) ) {
                if ((sig = str.lookupSignal(RMS9_EPR))) {
                    epr= EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) epr--;
                }
                if ((sig = str.lookupSignal(RMS9_EPRLevel))) {
                    epr_level= EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((sig = str.lookupLabel("S.EPR.EPRType"))) {
                    a1x = true;
                    epr = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    epr += 1;
                    if ( AS_eleven ) epr--;
                }
                int epr_on=0, clin_epr_on=0;
                if ((sig = str.lookupLabel("S.EPR.EPREnable"))) { // first check devices opinion
                    a1x = true;
                    epr_on = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) epr_on--;
                }
                if (epr_on && (sig = str.lookupLabel("S.EPR.ClinEnable"))) {
                    a1x = true;
                    clin_epr_on = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) clin_epr_on--;
                }
                if (a1x && !(epr_on && clin_epr_on)) {
                    epr = 0;
                    epr_level = 0;
                }
            }

            if ((epr >= 0) && (epr_level >= 0)) {
                R.epr_level = epr_level;
                R.epr = epr;
            } else {
                if (epr >= 0) {
                    static bool warn=false;
                    if (!warn) { // just nag once
                        qDebug() << "If you can read this, please tell the developers you found a ResMed with EPR but no EPR_LEVEL so he can remove this warning";
//                        sleep(1);
                        warn = true;
                    }

                    R.epr = (epr > 0) ? 1 : 0;
                    R.epr_level = epr;
                } else if (epr_level >= 0) {
                    R.epr_level = epr_level;
                    R.epr = (epr_level > 0) ? 1 : 0;
                }
            }

            if ((sig = str.lookupLabel("AHI"))) {
                R.ahi = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("AI"))) {
                R.ai = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("HI"))) {
                R.hi = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("UAI"))) {
                R.uai = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("CAI"))) {
                R.cai = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("OAI"))) {
                R.oai = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("CSR"))) {
                R.csr = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }

            if ((sig = str.lookupLabel("S.RampTime"))) {
                R.s_RampTime = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("S.RampEnable"))) {
                R.s_RampEnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_RampEnable--;
                if ( R.s_RampEnable == 2 ) R.s_RampTime = -1;
            }
            if ((sig = str.lookupLabel("S.EPR.ClinEnable"))) {
                R.s_EPR_ClinEnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_EPR_ClinEnable--;
            }
            if ((sig = str.lookupLabel("S.EPR.EPREnable"))) {
                R.s_EPREnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_EPREnable--;
            }

            if ((sig = str.lookupLabel("S.ABFilter"))) {
                R.s_ABFilter = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_ABFilter--;
            }

            if ((sig = str.lookupLabel("S.ClimateControl"))) {
                R.s_ClimateControl = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_ClimateControl--;
            }

            if ((sig = str.lookupLabel("S.Mask"))) {
                R.s_Mask = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) {
                    if ( R.s_Mask < 2 || R.s_Mask > 4 )
                        R.s_Mask = 4;   // unknown mask type
                    else
                        R.s_Mask -= 2;  // why be consistent?
                }
            }
            if ((sig = str.lookupLabel("S.PtAccess"))) {
                if ( AS_eleven ) {
                    R.s_PtView = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    R.s_PtView--;
                } else {
                    R.s_PtAccess = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
            }
            if ((sig = str.lookupLabel("S.SmartStart"))) {
                R.s_SmartStart = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_SmartStart--;
//              qDebug() << "SmartStart is set to" << R.s_SmartStart;
            }
            if ((sig = str.lookupLabel("S.SmartStop"))) {
                R.s_SmartStop = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_SmartStop--;
                //qDebug() << "SmartStop is set to" << R.s_SmartStop;
            }
            if ((sig = str.lookupLabel("S.HumEnable"))) {
                R.s_HumEnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_HumEnable--;
            }
            if ((sig = str.lookupLabel("S.HumLevel"))) {
                R.s_HumLevel = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("S.TempEnable"))) {
                R.s_TempEnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                if ( AS_eleven ) R.s_TempEnable--;
            }
            if ((sig = str.lookupLabel("S.Temp"))) {
                R.s_Temp = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((sig = str.lookupLabel("S.Tube"))) {
                R.s_Tube = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
            }
            if ((R.rms9_mode >= 2) && (R.rms9_mode <= 5)) {     // S, ST, or T modes
                //qDebug() << "BiLevel Mode found" << R.rms9_mode;

                QString sigprefix("S.") ;
                if ( AS_eleven ) sigprefix.append("S.");

                if (R.rms9_mode == 3) {     // S mode only
                    QString signame =QString("%1%2").arg(sigprefix).arg("EasyBreathe");
                    if ((sig = str.lookupLabel(signame))) {
                        R.s_EasyBreathe = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                        if ( AS_eleven ) --R.s_EasyBreathe;
                    }
                }

                QString signame =QString("%1%2").arg(sigprefix).arg("RiseEnable");
                if ((sig = str.lookupLabel(signame))) {
                    R.s_RiseEnable = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) --R.s_RiseEnable;
                }
                signame =QString("%1%2").arg(sigprefix).arg("RiseTime");
                if ((sig = str.lookupLabel(signame))) {
                 R.s_RiseTime = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                if ((R.rms9_mode ==3) || (R.rms9_mode ==4)) {       // S or ST mode
                    QString signame =QString("%1%2").arg(sigprefix).arg("Cycle");
                    if ((sig = str.lookupLabel(signame))) {
                        R.s_Cycle = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                        if ( AS_eleven ) --R.s_Cycle;
                    }
                    signame =QString("%1%2").arg(sigprefix).arg("Trigger");
                    if ((sig = str.lookupLabel(signame))) {
                        R.s_Trigger = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                        if ( AS_eleven ) --R.s_Trigger;
                    }
                    signame =QString("%1%2").arg(sigprefix).arg("TiMax");
                    if ((sig = str.lookupLabel(signame))) {
                        R.s_TiMax = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    }
                    signame =QString("%1%2").arg(sigprefix).arg("TiMin");
                    if ((sig = str.lookupLabel(signame))) {
                        R.s_TiMin = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    }
             }
            }
            if (R.rms9_mode == 6) {     // vAuto mode
                //qDebug() << "vAuto mode found" << 6;
                QString sigprefix("S.") ;
                if ( AS_eleven ) sigprefix.append("VA.");
                QString signame =QString("%1%2").arg(sigprefix).arg("Cycle");
                if ((sig = str.lookupLabel(signame))) {
                    R.s_Cycle = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) --R.s_Cycle;
                }
                signame =QString("%1%2").arg(sigprefix).arg("Trigger");
                if ((sig = str.lookupLabel(signame))) {
                    R.s_Trigger = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                    if ( AS_eleven ) --R.s_Trigger;
                }
                signame =QString("%1%2").arg(sigprefix).arg("TiMax");
                if ((sig = str.lookupLabel(signame))) {
                    R.s_TiMax = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
                signame =QString("%1%2").arg(sigprefix).arg("TiMin");
                if ((sig = str.lookupLabel(signame))) {
                    R.s_TiMin = EventDataType(sig->dataArray[rec]) * sig->gain + sig->offset;
                }
            }
            if ( R.min_pressure == 0 ) {
                qDebug() << "Min Pressure is zero on" << date.toString();
            }
#ifdef STR_DEBUG
            qDebug() << "Finished" << date.toString();
#endif
        }
#ifdef STR_DEBUG
        qDebug() << "Finished" << strfile;
#endif
    }
    qDebug() << "Finished ProcessSTR";
    return true;
}

    ///////////////////////////////////////////////////////////////////////////////////
    // Parse Identification.tgt file (containing serial number and device information)
    ///////////////////////////////////////////////////////////////////////////////////
// QHash<QString, QString> parseIdentLine( const QString line, MachineInfo * info); // forward
// void scanProductObject( QJsonObject product, MachineInfo *info, QHash<QString, QString> *idmap);                 // forward
bool parseIdentFile( QString path, MachineInfo * info, QHash<QString, QString> & idmap ) {
    QString filename = path + RMS9_STR_idfile + STR_ext_TGT;
    QFile f(filename);
    QFile j(path + RMS9_STR_idfile + STR_ext_JSON);

    if (j.exists() ) {      // chose the AS11 file if both exist
        if ( !j.open(QIODevice::ReadOnly)) {
            return false;
        }
        QByteArray identData = j.readAll();
        j.close();
        QJsonDocument identDoc(QJsonDocument::fromJson(identData));
        QJsonObject identObj(identDoc.object());
        if ( identObj.contains("FlowGenerator") && identObj["FlowGenerator"].isObject()) {
            QJsonObject flow = identObj["FlowGenerator"].toObject();
            if ( flow.contains("IdentificationProfiles") && flow["IdentificationProfiles"].isObject()) {
                QJsonObject profiles = flow["IdentificationProfiles"].toObject();
                if ( profiles.contains("Product") && profiles["Product"].isObject()) {
                    QJsonObject product = profiles["Product"].toObject();
// passed in        MachineInfo info = newInfo();
                    scanProductObject( product, info, &idmap);
                    return true;
                }
            }
        }
        return false;
    }
    // Abort if this file is dodgy..
    if (f.exists() ) {
        if ( !f.open(QIODevice::ReadOnly)) {
            return false;
        }
        qDebug() << "Parsing Identification File " << filename;
    //  emit updateMessage(QObject::tr("Parsing Identification File"));
    //  QApplication::processEvents();

        // Parse # entries into idmap.
        while (!f.atEnd()) {
           QString line = f.readLine().trimmed();
           QHash<QString, QString> hash = parseIdentLine( line, info );
           idmap.QTCOMBINE(hash);
        }

        f.close();
        return true;
    }
    return false;
}

void scanProductObject( QJsonObject product, MachineInfo *info, QHash<QString, QString> *idmap) {
    QHash<QString, QString> hash1, hash2, hash3;
    if (product.contains("SerialNumber")) {
        info->serial = product["SerialNumber"].toString();
        hash1["SerialNumber"] = product["SerialNumber"].toString();
        if (idmap)
            idmap->QTCOMBINE(hash1);
    }
    if (product.contains("ProductCode")) {
        info->modelnumber = product["ProductCode"].toString();
        hash2["ProductCode"] = info->modelnumber;
        if (idmap)
            idmap->QTCOMBINE(hash2);
    }
    if (product.contains("ProductName")) {
        info->model = product["ProductName"].toString();
        hash3["ProductName"] = info->model;
        if (idmap)
            idmap->QTCOMBINE(hash3);
        int idx = info->model.indexOf("11");
        info->series = info->model.left(idx+2);
    }
}

void backupSTRfiles( const QString strpath, const QString importPath, const QString backupPath,
                    MachineInfo & info, QMap<QDate, STRFile> & STRmap )
{
    Q_UNUSED(strpath);
    qDebug() << "Starting backupSTRfiles during new IMPORT";
    QDir dir;
//  Qstring strBackupPath(backupPath+"STR_Backup");
    QStringList strfiles;
    // add Backup/STR.edf - make sure it ends up in the STRmap
    strfiles.push_back(backupPath+"STR.edf");

    // Just in case we are importing from a Backup folder in a different Profile, process OSCAR backup structures
    QString strBackupPath(importPath + "STR_Backup");
    dir.setPath(strBackupPath);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::Readable);
    QFileInfoList flist = dir.entryInfoList();

    // Add any STR_Backup versions to the file list
    for (auto & fi : flist) {       // this is empty if imprting from an SD card
        QString filename = fi.fileName();
        if ( ! filename.startsWith("STR", Qt::CaseInsensitive))
            continue;
        if ( ! (filename.endsWith("edf.gz", Qt::CaseInsensitive) || filename.endsWith("edf", Qt::CaseInsensitive)))
            continue;
        strfiles.push_back(fi.canonicalFilePath());
    }
#ifdef STR_DEBUG
    qDebug() << "STR file list size is" << strfiles.size();
#endif

    // Now copy any of these files to the Backup folder adding the file date to the file name
    // and put it into the STRmap structure
    for (auto & filename : strfiles) {
        QDate date;
        long int days;
        ResMedEDFInfo * stredf = fetchSTRandVerify( filename, info.serial );
        if ( stredf == nullptr )
            continue;
        date = stredf->edfHdr.startdate_orig.date();
        days = stredf->GetNumDataRecords();
        if (STRmap.contains(date)) {
            qDebug() << "STRmap already contains" << date.toString("yyyy-MM-dd") << "for" << STRmap[date].days << "ending" << date.addDays(STRmap[date].days-1);
            qDebug() << filename.section("/",-2,-1) << "has" << days << "ending" << date.addDays(days-1);
            if ( days <= STRmap[date].days ) {
                qDebug() << "Skipping" << filename.section("/",-2,-1) << "Keeping" << STRmap[date].filename.section("/",-2,-1);
                delete stredf;
                continue;
            } else {
                qDebug() << "Dropping" << STRmap[date].filename.section("/", -2, -1) << "Keeping" << filename.section("/",-2,-1);
                delete STRmap[date].edf;
                STRmap.remove(date);    // new one gets added after we know its new name
            }
        }
        // now create the new backup name
        QString newname = "STR-"+date.toString("yyyyMMdd")+"."+STR_ext_EDF;
        QString backupfile = backupPath+"/STR_Backup/"+newname;

        QString gzfile = backupfile + STR_ext_gz;
        QString nongzfile = backupfile;

        bool compress_backups = p_profile->session->compressBackupData();
        backupfile = compress_backups ? gzfile : nongzfile;

        STRmap[date] = STRFile(backupfile, days, stredf);
        qDebug() << "Adding" << filename.section("/",-3,-1) << "with" << days << "days as" << backupfile.section("/", -3, -1) << "to STRmap";

        if ( QFile::exists(backupfile)) {
            QFile::remove(backupfile);
        }
#ifdef STR_DEBUG
        qDebug() << "Copying" << filename.section("/",-3,-1) << "to" << backupfile.section("/",-3,-1);
#endif
        if (filename.endsWith(STR_ext_gz,Qt::CaseInsensitive)) {    // we have a compressed file
            if (compress_backups) {                 // fine, copy it to backup folder
                if (!QFile::copy(filename, backupfile))
                    qWarning() << "Failed to copy" << filename << "to" << backupfile;
            } else {                                // oops, uncompress it to the backup folder
                uncompressFile(filename, backupfile);
            }
        } else {                                    // file is not compressed
            if (compress_backups) {                 // so compress it into the backup folder
                compressFile(filename, backupfile);
            } else {                                // and that's OK, just copy it over
                if (!QFile::copy(filename, backupfile))
                    qWarning() << "Failed to copy" << filename << "to" << backupfile;
            }
        }

        // Remove any duplicate compressed/uncompressed backup file
        if (compress_backups)
            QFile::exists(nongzfile) && QFile::remove(nongzfile);
        else
            QFile::exists(gzfile) && QFile::remove(gzfile);
    }   // end for walking the STR files list
#ifdef STR_DEBUG
    qDebug() << "STRmap has" << STRmap.size() << "entries";
#endif
    qDebug() << "Finished backupSTRfiles during new IMPORT";
}

QHash<QString, QString> parseIdentLine( const QString line, MachineInfo * info)
{
    QHash<QString, QString> hash;

    if (!line.isEmpty()) {
        QString key = line.section(" ", 0, 0).section("#", 1);
        QString value = line.section(" ", 1);

        if (key == "SRN") { // Serial Number
            info->serial = value;

        } else if (key == "PNA") {  // Product Name
            value.replace("_"," ");

            if (value.contains(STR_ResMed_AirSense10)) {
        //      value.replace(STR_ResMed_AirSense10, "");
                info->series = STR_ResMed_AirSense10;
            } else if (value.contains(STR_ResMed_AirCurve10)) {
        //      value.replace(STR_ResMed_AirCurve10, "");
                info->series = STR_ResMed_AirCurve10;
            } else {    // it will be a Series 9, and might not contain (STR_ResMed_S9))
                value.replace("("," ");     // might sometimes have a double space...
                value.replace(")","");
                if ( ! value.startsWith(STR_ResMed_S9)) {
                    value.replace(STR_ResMed_S9, "");
                    value.insert(0, " ");               // There's proablely a better way than this
                    value.insert(0, STR_ResMed_S9);     // two step way to put "S9 " at the start
                }
                info->series = STR_ResMed_S9;
        //      value.replace(STR_ResMed_S9, "");
            }
        //  if (value.contains("Adapt", Qt::CaseInsensitive)) {
        //      if (!value.contains("VPAP")) {
        //          value.replace("Adapt", QObject::tr("VPAP Adapt"));
        //      }
        //  }
            info->model = value.trimmed();
        } else if (key == "PCD") { // Product Code
            info->modelnumber = value;
        }
        hash[key] = value;
    }

    return hash;
}

EDFType lookupEDFType(const QString & filename)
{
    QString text = filename.section("_", -1).section(".",0,0).toUpper();
    if (text == "EVE") {
        return EDF_EVE;
    } else if (text =="BRP") {
        return EDF_BRP;
    } else if (text == "PLD") {
        return EDF_PLD;
    } else if ((text == "SAD") || (text == "SA2")){
        return EDF_SAD;
    } else if (text == "CSL") {
        return EDF_CSL;
    } else if (text == "AEV") {
        return EDF_AEV;
    } else return EDF_UNKNOWN;
}


///////////////////////////////////////////////////////////////////////////////
// Looks inside an EDF or EDF.gz and grabs the start and duration
///////////////////////////////////////////////////////////////////////////////
EDFduration getEDFDuration(const QString & filename)
{
//    qDebug() << "getEDFDuration called for" << filename;

    QString ext = filename.section("_", -1).section(".",0,0).toUpper();

    if ((ext == "EVE") || (ext == "CSL")) {		// don't even try with Annotation-only edf files
        EDFduration dur(0, 0, filename);
        dur.type = lookupEDFType(filename);
        qDebug() << "File ext is" << ext;
        dumpEDFduration(dur);
        return dur;
    }

    bool ok1, ok2;
    int num_records;
    double rec_duration;
    QDateTime startDate;

//  We will just look at the header part of the edf file here
    if (!filename.endsWith(".gz", Qt::CaseInsensitive)) {
        QFile file(filename);
        if (!file.open(QFile::ReadOnly))
            return EDFduration(0, 0, filename);

        if (!file.seek(0xa8)) {
            file.close();
            return EDFduration(0, 0, filename);
        }

        QByteArray bytes = file.read(16).trimmed();
//      We'll fix the xx85 problem below
//      startDate = QDateTime::fromString(QString::fromLatin1(bytes, 16), "dd.MM.yyHH.mm.ss");
//      getStartDT ought to be named getStartNoDST ... TODO someday
        startDate = EDFInfo::getStartDT(QString::fromLatin1(bytes,16));

        if (!file.seek(0xec)) {
            file.close();
            return EDFduration(0, 0, filename);
        }

        bytes = file.read(8).trimmed();
        num_records = bytes.toInt(&ok1);
        bytes = file.read(8).trimmed();
        rec_duration = bytes.toDouble(&ok2);

        file.close();
    } else {
        gzFile f = gzopen(filename.toLatin1(), "rb");
        if (!f)
            return EDFduration(0, 0, filename);

        // Decompressed header and data block
        if (!gzseek(f, 0xa8, SEEK_SET)) {
            gzclose(f);
            return EDFduration(0, 0, filename);
        }
        char datebytes[17] = {0};
        gzread(f, (char *)&datebytes, 16);
        QString str = QString(QString::fromLatin1(datebytes,16)).trimmed();
//      startDate = QDateTime::fromString(str, "dd.MM.yyHH.mm.ss");
        startDate = EDFInfo::getStartDT(str);

        if (!gzseek(f, 0xec-0xa8-16, SEEK_CUR)) { // 0xec
            gzclose(f);
            return EDFduration(0, 0, filename);
        }

        char cbytes[9] = {0};
        gzread(f, (char *)&cbytes, 8);
        str = QString(cbytes).trimmed();
        num_records = str.toInt(&ok1);

        gzread(f, (char *)&cbytes, 8);
        str = QString(cbytes).trimmed();
        rec_duration = str.toDouble(&ok2);

        gzclose(f);
    }

    QDate d2 = startDate.date();

    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        startDate.setDate(d2);
    }
    if ( (! startDate.isValid()) || ( startDate > QDateTime::currentDateTime()) ) {
        qDebug() << "Invalid date time retreieved parsing EDF duration for" << filename;
        qDebug() << "Time zone(Utc) is" << startDate.timeZone().abbreviation(QDateTime::currentDateTimeUtc());
        qDebug() << "Time zone is" << startDate.timeZone().abbreviation(QDateTime::currentDateTime());
        return EDFduration(0, 0, filename);
    }

    if (!(ok1 && ok2))
        return EDFduration(0, 0, filename);

    quint32 start = startDate.toSecsSinceEpoch();
    quint32 end = start + rec_duration * num_records;

    QString filedate = filename.section("/",-1).section("_",0,1);
//  QDateTime dt2 = QDateTime::fromString(filedate, "yyyyMMdd_hhmmss");
    d2 = QDate::fromString( filedate.left(8), "yyyyMMdd");
    QTime t2 = QTime::fromString( filedate.right(6), "hhmmss");
    QDateTime dt2 = QDateTime( d2, t2, EDFInfo::localNoDST );
    quint32 st2 = dt2.toSecsSinceEpoch();

    start = qMin(st2, start);	// They should be the same, usually

    if (end < start)
        end = qMax(st2, start);

    EDFduration dur(start, end, filename);

    dur.type = lookupEDFType(filename);

    return dur;
}


void GuessPAPMode(Session *sess)
{
    if (sess->channelDataExists(CPAP_Pressure)) {
        // Determine CPAP or APAP?
        EventDataType min = sess->Min(CPAP_Pressure);
        EventDataType max = sess->Max(CPAP_Pressure);
        if ((max-min)<0.1) {
            sess->settings[CPAP_Mode] = MODE_CPAP;
            sess->settings[CPAP_Pressure] = qRound(max * 10.0)/10.0;
            // early call.. It's CPAP mode
        } else {
            // Ramp is ugly - but this is a bad way to test for it
            if (sess->length() > 1800000L) {  // half an hour
            }
            sess->settings[CPAP_Mode] = MODE_APAP;
            sess->settings[CPAP_PressureMin] = qRound(min * 10.0)/10.0;
            sess->settings[CPAP_PressureMax] = qRound(max * 10.0)/10.0;
        }

    } else if (sess->eventlist.contains(CPAP_IPAP)) {
        sess->settings[CPAP_Mode] = MODE_BILEVEL_AUTO_VARIABLE_PS;
       // Determine BiPAP or ASV
    }

}

void StoreSummaryStatistics(Session * sess, STRRecord & R)
{
    if (R.mode >= 0) {
        if (R.mode == MODE_CPAP) {
        } else if (R.mode == MODE_APAP) {
        }
    }

    if (R.leak50 >= 0) {
//      sess->setp95(CPAP_Leak, R.leak95);
//      sess->setp50(CPAP_Leak, R.leak50);
        sess->setMax(CPAP_Leak, R.leakmax);
    }

    if (R.rr50 >= 0) {
//      sess->setp95(CPAP_RespRate, R.rr95);
//      sess->setp50(CPAP_RespRate, R.rr50);
        sess->setMax(CPAP_RespRate, R.rrmax);
    }

    if (R.mv50 >= 0) {
//      sess->setp95(CPAP_MinuteVent, R.mv95);
//      sess->setp50(CPAP_MinuteVent, R.mv50);
        sess->setMax(CPAP_MinuteVent, R.mvmax);
    }

    if (R.tv50 >= 0) {
//      sess->setp95(CPAP_TidalVolume, R.tv95);
//      sess->setp50(CPAP_TidalVolume, R.tv50);
        sess->setMax(CPAP_TidalVolume, R.tvmax);
    }

    if (R.mp50 >= 0) {
//      sess->setp95(CPAP_MaskPressure, R.mp95);
//      sess->seTTtp50(CPAP_MaskPressure, R.mp50);
        sess->setMax(CPAP_MaskPressure, R.mpmax);
    }

    if (R.oai > 0) {
        sess->setCph(CPAP_Obstructive, R.oai);
        sess->setCount(CPAP_Obstructive, R.oai * sess->hours());
    }
    if (R.hi > 0) {
        sess->setCph(CPAP_Hypopnea, R.hi);
        sess->setCount(CPAP_Hypopnea, R.hi * sess->hours());
    }
    if (R.cai > 0) {
        sess->setCph(CPAP_ClearAirway, R.cai);
        sess->setCount(CPAP_ClearAirway, R.cai * sess->hours());
    }
    if (R.uai > 0) {
        sess->setCph(CPAP_Apnea, R.uai);
        sess->setCount(CPAP_Apnea, R.uai * sess->hours());
    }
    if (R.csr > 0) {
        sess->setCph(CPAP_CSR, R.csr);
        sess->setCount(CPAP_CSR, R.csr * sess->hours());
    }
}

void StoreSettings(Session * sess, STRRecord & R)
{
    if (R.mode >= 0) {
        sess->settings[CPAP_Mode] = R.mode;
        sess->settings[RMS9_Mode] = R.rms9_mode;
        if ( R.min_pressure == 0 )
            qDebug() << "Min Pressure is zero, R.mode is" << R.mode;
        if (R.mode == MODE_CPAP) {
            if (R.set_pressure >= 0) sess->settings[CPAP_Pressure] = R.set_pressure;
        } else if (R.mode == MODE_APAP) {
            if (R.min_pressure >= 0) sess->settings[CPAP_PressureMin] = R.min_pressure;
            if (R.max_pressure >= 0) sess->settings[CPAP_PressureMax] = R.max_pressure;
        } else if (R.mode == MODE_BILEVEL_FIXED) {
            if (R.epap >= 0) sess->settings[CPAP_EPAP] = R.epap;
            if (R.ipap >= 0) sess->settings[CPAP_IPAP] = R.ipap;
            if (R.ps >= 0) sess->settings[CPAP_PS] = R.ps;
            if (R.s_Cycle >= 0) sess->settings[ RMAS1x_Cycle ] = R.s_Cycle;
            if (R.s_Trigger >= 0) sess->settings[ RMAS1x_Trigger ] = R.s_Trigger;
            if (R.s_TiMax >= 0) sess->settings[ RMAS1x_TiMax ] = R.s_TiMax;
            if (R.s_TiMin >= 0) sess->settings[ RMAS1x_TiMin ] = R.s_TiMin;
            if (R.s_EasyBreathe >= 0) sess->settings[ RMAS1x_EasyBreathe ] = R.s_EasyBreathe;
            if (R.s_RiseEnable >= 0) sess->settings[ RMAS1x_RiseEnable ] = R.s_RiseEnable;
            if (R.s_RiseTime >= 0) sess->settings[ RMAS1x_RiseTime ] = R.s_RiseTime;
        } else if (R.mode == MODE_BILEVEL_AUTO_FIXED_PS) {
            if (R.min_epap >= 0) sess->settings[CPAP_EPAPLo] = R.min_epap;
            if (R.max_ipap >= 0) sess->settings[CPAP_IPAPHi] = R.max_ipap;
            if (R.ps >= 0) sess->settings[CPAP_PS] = R.ps;
            if (R.s_Cycle >= 0) sess->settings[ RMAS1x_Cycle ] = R.s_Cycle;
            if (R.s_Trigger >= 0) sess->settings[ RMAS1x_Trigger ] = R.s_Trigger;
            if (R.s_TiMax >= 0) sess->settings[ RMAS1x_TiMax ] = R.s_TiMax;
            if (R.s_TiMin >= 0) sess->settings[ RMAS1x_TiMin ] = R.s_TiMin;
        } else if (R.mode == MODE_ASV) {
            if (R.epap >= 0) sess->settings[CPAP_EPAP] = R.epap;
            if (R.min_ps >= 0) sess->settings[CPAP_PSMin] = R.min_ps;
            if (R.max_ps >= 0) sess->settings[CPAP_PSMax] = R.max_ps;
            if (R.max_ipap >= 0) sess->settings[CPAP_IPAPHi] = R.max_ipap;
        } else if (R.mode == MODE_ASV_VARIABLE_EPAP) {
            if (R.max_epap >= 0) sess->settings[CPAP_EPAPHi] = R.max_epap;
            if (R.min_epap >= 0) sess->settings[CPAP_EPAPLo] = R.min_epap;
            if (R.max_ipap >= 0) sess->settings[CPAP_IPAPHi] = R.max_ipap;
            if (R.min_ipap >= 0) sess->settings[CPAP_IPAPLo] = R.min_ipap;
            if (R.min_ps >= 0) sess->settings[CPAP_PSMin] = R.min_ps;
            if (R.max_ps >= 0) sess->settings[CPAP_PSMax] = R.max_ps;
        } else {
            qDebug() << "Setting session pressures for R.mode" << R.mode;
            if (R.set_pressure > 0) sess->settings[CPAP_Pressure] = R.set_pressure;
            if (R.min_pressure > 0) sess->settings[CPAP_PressureMin] = R.min_pressure;
            if (R.max_pressure > 0) sess->settings[CPAP_PressureMax] = R.max_pressure;
            if (R.max_epap > 0) sess->settings[CPAP_EPAPHi] = R.max_epap;
            if (R.min_epap > 0) sess->settings[CPAP_EPAPLo] = R.min_epap;
            if (R.max_ipap > 0) sess->settings[CPAP_IPAPHi] = R.max_ipap;
            if (R.min_ipap > 0) sess->settings[CPAP_IPAPLo] = R.min_ipap;
            if (R.min_ps > 0) sess->settings[CPAP_PSMin] = R.min_ps;
            if (R.max_ps > 0) sess->settings[CPAP_PSMax] = R.max_ps;
            if (R.ps > 0) sess->settings[CPAP_PS] = R.ps;
            if (R.epap > 0) sess->settings[CPAP_EPAP] = R.epap;
            if (R.ipap > 0) sess->settings[CPAP_IPAP] = R.ipap;
        }
    }

    if (R.epr >= 0) {
        sess->settings[RMS9_EPR] = (int)R.epr;
        if (R.epr > 0) {
            if (R.epr_level >= 0) {
                sess->settings[RMS9_EPRLevel] = (int)R.epr_level;
            }
        }
    }

    if (R.s_RampEnable >= 0) {
        sess->settings[RMS9_RampEnable] = R.s_RampEnable;

        if (R.s_RampEnable >= 1) {
            if (R.s_RampTime >= 0) {
                sess->settings[CPAP_RampTime] = R.s_RampTime;
            }
            if (R.s_RampPressure >= 0) {
                sess->settings[CPAP_RampPressure] = R.s_RampPressure;
            }
        }
    }

    if (R.s_SmartStart >= 0) {
        sess->settings[RMS9_SmartStart] = R.s_SmartStart;
    }
    if (R.s_SmartStop >= 0) {
        sess->settings[RMAS11_SmartStop] = R.s_SmartStop;
    }
    if (R.s_ABFilter >= 0) {
        sess->settings[RMS9_ABFilter] = R.s_ABFilter;
    }
    if (R.s_ClimateControl >= 0) {
        sess->settings[RMS9_ClimateControl] = R.s_ClimateControl;
    }
    if (R.s_Mask >= 0) {
        sess->settings[RMS9_Mask] = R.s_Mask;
    }
    if (R.s_PtAccess >= 0) {
        sess->settings[RMS9_PtAccess] = R.s_PtAccess;
    }

    if (R.s_PtView >= 0) {
        sess->settings[RMAS11_PtView] = R.s_PtView;
    }

    if (R.s_HumEnable >= 0) {
        sess->settings[RMS9_HumidStatus] = (short)R.s_HumEnable;
        if ((R.s_HumEnable >= 1) && (R.s_HumLevel >= 0)) {
            sess->settings[RMS9_HumidLevel] = (short)R.s_HumLevel;
        }
    }
    if (R.s_TempEnable >= 0) {
        sess->settings[RMS9_TempEnable] = (short)R.s_TempEnable;
        if ((R.s_TempEnable >= 1) && (R.s_Temp >= 0)){
            sess->settings[RMS9_Temp] = (short)R.s_Temp;
        }
    }
    if (R.s_Comfort >= 0) {
        sess->settings[RMAS1x_Comfort] = R.s_Comfort;
    }
}

struct OverlappingEDF {
    quint32 start;
    quint32 end;
    QMultiMap<quint32, QString> filemap;    // key is start time, value is filename
    Session * sess;
};

void ResDayTask::run()
{
#ifdef SESSION_DEBUG
    qDebug() << "Processing STR and edf files for" << resday->date;
#endif
    if (resday->files.size() == 0) { // No EDF files???
        if (( ! resday->str.date.isValid()) || (resday->str.date > QDate::currentDate()) ) {
            // This condition should be impossible, but just in case something gets fudged up elsewhere later
            qDebug() << "No edf files in resday" << resday->date << "and the str date is inValid";
            return;
        }
        // Summary only day, create sessions for each mask-on/off pair and tag them summary only
        STRRecord & R = resday->str;
#ifdef SESSION_DEBUG
        qDebug() << "Creating summary-only sessions for" << resday->date;
#endif
        for (int i=0;i<resday->str.maskon.size();++i) {
            quint32 maskon = resday->str.maskon[i];
            quint32 maskoff = resday->str.maskoff[i];
/**
            QTime noon(12,00,00);
            QDateTime daybegin(resday->date,noon); // Beginning of ResMed day
            quint32 dayend = daybegin.addDays(1).addMSecs(-1).toSecsSinceEpoch(); // End of ResMed day
            if ( (maskon > dayend) ||
                 (maskoff > dayend) ) {
                qWarning() << "mask time in future" << resday->date << daybegin << dayend << "maskon" << maskon << "maskoff" << maskoff;
                continue;
            }
**/
            if (((maskon>0) && (maskoff>0)) && (maskon != maskoff)) {   //ignore very short sessions
                Session * sess = new Session(mach, maskon);
                sess->set_first(quint64(maskon) * 1000L);
                sess->set_last(quint64(maskoff) * 1000L);
                StoreSettings(sess, R);         // Process the STR.edf settings
                StoreSummaryStatistics(sess, R);  // We want the summary information too

                sess->setSummaryOnly(true);
                sess->SetChanged(true);

//                loader->sessionMutex.lock();          // This chunk moved into SaveSession below
//                sess->Store(mach->getDataPath());
//                mach->AddSession(sess);
//                loader->sessionCount++;
//                loader->sessionMutex.unlock();
////              delete sess;

                save(loader, sess);                     // This is aliased to SaveSession - unless testing
            }
        }
//        qDebug() << "Finished summary processing for" << resday->date;
        return;
    }

    // sooo... at this point we have
    // resday record populated with correct STR.edf settings for this date
    // files list containing unsorted EDF files that match this day
    // guaranteed no sessions for this day for this device.

    // Need to check overlapping files in session candidates

    QList<OverlappingEDF> overlaps;

    int maskOnSize = resday->str.maskon.size();
    if (resday->str.date.isValid()) {
        //First populate Overlaps with Mask ON/OFF events
        for (int i=0; i < maskOnSize; ++i) {
//            if ( (resday->str.maskon[i] > QDateTime::currentDateTime().toSecsSinceEpoch()) ||
//                 (resday->str.maskoff[i] > QDateTime::currentDateTime().toSecsSinceEpoch()) ) {
//                qWarning() << "mask time in future" << resday->date << "now" << QDateTime::currentDateTime().toSecsSinceEpoch() << "maskon" << resday->str.maskon[i] << "maskoff" << resday->str.maskoff[i];
//                continue;
//            }
/*
            QTime noon(12,00,00);
            QDateTime daybegin(resday->date,noon); // Beginning of ResMed day
            quint32 dayend = daybegin.addDays(1).addMSecs(-1).toSecsSinceEpoch(); // End of ResMed day
            if ( (resday->str.maskon[i] > dayend) ||
                 (resday->str.maskoff[i] > dayend) ) {
                qWarning() << "mask time in future" << resday->date << "daybegin:" << daybegin << "dayend:" << dayend << "maskon" << resday->str.maskon[i] << "maskoff" << resday->str.maskoff[i];
                continue;
            }
*/
            if (((resday->str.maskon[i]>0) || (resday->str.maskoff[i]>0))
                    && (resday->str.maskon[i] != resday->str.maskoff[i]) ) {
                OverlappingEDF ov;
                ov.start = resday->str.maskon[i];
                ov.end = resday->str.maskoff[i];
                ov.sess = nullptr;
                overlaps.append(ov);
            }
        }
    }
#ifdef STR_DEBUG
    if (overlaps.size() > 0)
        qDebug().noquote() << "Created" << overlaps.size() << "sessionGroups from STR record for" << resday->str.date.toString();
#endif

    QMap<quint32, QString> EVElist, CSLlist;
    for (auto f_itr=resday->files.begin(), fend=resday->files.end(); f_itr!=fend; ++f_itr) {
        const QString & filename = f_itr.key();
        const QString & fullpath = f_itr.value();
//        QString ext = filename.section("_", -1).section(".",0,0).toUpper();
        EDFType type = lookupEDFType(filename);

        QString datestr = filename.section("_", 0, 1);
//      QDateTime filetime = QDateTime().fromString(datestr,"yyyyMMdd_HHmmss");
        QDate d2 = QDate::fromString( datestr.left(8), "yyyyMMdd");
        QTime t2 = QTime::fromString( datestr.right(6), "hhmmss");
        QDateTime filetime = QDateTime( d2, t2, EDFInfo::localNoDST );

        quint32 filetime_t = filetime.toSecsSinceEpoch();
        if (type == EDF_EVE) {      // skip the EVE and CSL files, b/c they often cover all sessions
            EVElist[filetime_t] = filename;
            continue;
        } else if (type == EDF_CSL) {
            CSLlist[filetime_t] = filename;
            continue;
        }
        bool added = false;
        for (auto & ovr : overlaps) {
            if ((filetime_t >= (ovr.start)) && (filetime_t < ovr.end)) {
                ovr.filemap.insert(filetime_t, filename);
                added = true;
                break;
            }
        }
        if ( ! added) {    // Didn't get a hit, look at the EDF files duration and check for an overlap
            EDFduration dur = getEDFDuration(fullpath);
/**
            QTime noon(12,00,00);
            QDateTime daybegin(resday->date,noon); // Beginning of ResMed day
            quint32 dayend = daybegin.addDays(1).addMSecs(-1).toSecsSinceEpoch(); // End of ResMed day
              if ((dur.start > (dayend)) ||
                  (dur.end > (dayend)) ) {
                  qWarning() << "Future Date in" << fullpath << "dayend" << dayend << "dur.start" << dur.start << "dur.end" << dur.end;
                  continue;           // skip this file
            }
**/
            for (int i=overlaps.size()-1; i>=0; --i) {
                OverlappingEDF & ovr = overlaps[i];
                if ((ovr.start < dur.end) && (dur.start < ovr.end)) {
                    ovr.filemap.insert(filetime_t, filename);
                    added = true;
#ifdef SESSION_DEBUG
                    qDebug() << "Adding" << filename << "to overlap" << i;
                    qDebug() << "Overlap starts:" << ovr.start << "ends:" << ovr.end;
                    qDebug() << "File time starts:" << dur.start << "ends:" << dur.end;
#endif
                    // Expand ovr's scope -- I think this is necessary!! (PO)
                    // YES! when the STR file is missing, there are no mask on/off entries
                    // and the edf files are not always created at the same time
                    ovr.start = min(ovr.start, dur.start);
                    ovr.end = max(ovr.end, dur.end);
//                  if ( (dur.start < ovr.start) || (dur.end > ovr.end) )
//                      qDebug() << "Should have expanded overlap" << i << "for" << filename;
                    break;
                }
            }       // end for walk existing overlap entries
            if ( ! added ) {
                if (dur.start != dur.end) { // Didn't fit it in anywhere, create a new Overlap entry/session
                    OverlappingEDF ov;
                    ov.start = dur.start;
                    ov.end = dur.end;
                    ov.filemap.insert(filetime_t, filename);
#ifdef SESSION_DEBUG
                    qDebug() << "Creating overlap for" << filename << "missing STR record";
                    qDebug() << "Starts:" << dur.start << "Ends:" << dur.end;
#endif
                    overlaps.append(ov);
                } else {
#ifdef SESSION_DEBUG
                    qDebug() << "Skipping zero duration file" << filename;
#endif
                }
            }           // end create a new overlap entry
        }           // end check for file overlap
    }           // end for walk resday files list

    // Create an ordered map and see how far apart the sessions really are.
    QMap<quint32, OverlappingEDF> mapov;
    for (auto & ovr : overlaps) {
        mapov[ovr.start] = ovr;
    }

// We are not going to merge close sessions - gaps can be useful markers for users
//     // Examine the gaps in between to see if we should merge sessions
//     for (auto oit=mapov.begin(), oend=mapov.end(); oit != oend; ++oit) {
//         // Get next in line
//         auto next_oit = oit+1;
//         if (next_oit != mapov.end()) {
//             OverlappingEDF & A = oit.value();
//             OverlappingEDF & B = next_oit.value();
//             int gap = B.start - A.end;
//             if (gap < 60) {     // TODO see if we should use the prefs value here... ???
// //                qDebug() << "Only a" << gap << "s sgap between ResMed sessions on" << resday->date.toString();
//             }
//         }
//     }

    if (overlaps.size()==0) {
        qDebug() << "No sessionGroups  for" << resday->date << "FINSIHED";
        return;
    }

    // Now overlaps is populated with zero or more individual session groups of EDF files (zero because of sucky summary only days)
    for (auto & ovr : overlaps) {
        if (ovr.filemap.size() == 0)
            continue;
        Session * sess = new Session(mach, ovr.start);
//      Do not set the session times according to Mask on/off times
//      The LoadXXX edf routines will update them with recording start and durations
//      sess->set_first(quint64(ovr.start)*1000L);
//      sess->set_last(quint64(ovr.end)*1000L);
        ovr.sess = sess;

        for (auto mit=ovr.filemap.begin(), mend=ovr.filemap.end(); mit != mend; ++mit) {
            const QString & filename = mit.value();
            const QString & fullpath = resday->files[filename];
            EDFType type = lookupEDFType(filename);

#ifdef SESSION_DEBUG
            sess->session_files.append(filename);
#endif
            switch (type) {
            case EDF_BRP:
                loader->LoadBRP(sess, fullpath);
                break;
            case EDF_PLD:
                loader->LoadPLD(sess, fullpath);
                break;
            case EDF_SAD:
            case EDF_SA2:
                loader->LoadSAD(sess, fullpath);
                break;
            case EDF_EVE:
            case EDF_CSL:
            case EDF_AEV:   // this is in the 36039 - must figure out what to do with it
                break;
            default:
                qWarning() << "Unrecognized file type for" << filename;
            }
        }       // end for each edf file in the sessionGroup

        // Turns out there is only one or sometimes two EVE's per day, and they store data for the whole day
        // So we have to extract Annotations data and apply it for all sessions
        for (auto eit=EVElist.begin(), eveend=EVElist.end(); eit != eveend; ++eit) {
            const QString & fullpath = resday->files[eit.value()];
            loader->LoadEVE(ovr.sess, fullpath);
        }
        for (auto eit=CSLlist.begin(), cslend=CSLlist.end(); eit != cslend; ++eit) {
            const QString & fullpath = resday->files[eit.value()];
            loader->LoadCSL(ovr.sess, fullpath);
        }

        if (EVElist.size() == 0) {
            sess->AddEventList(CPAP_Obstructive, EVL_Event);
            sess->AddEventList(CPAP_ClearAirway, EVL_Event);
            sess->AddEventList(CPAP_Apnea, EVL_Event);
            sess->AddEventList(CPAP_Hypopnea, EVL_Event);
        }
        sess->setSummaryOnly(false);
        sess->SetChanged(true);

        if (sess->length() == 0) {
            // we want empty sessions even though they are crap
            qDebug() << "Session" << sess->session()
                << "["+QDateTime::fromSecsSinceEpoch(sess->session()).toString("MMM dd, yyyy hh:mm:ss")+"]"
                << "has zero duration" << QString("Start: %1").arg(sess->realFirst(),0,16) << QString("End: %1").arg(sess->realLast(),0,16);
        }
        if (sess->length() < 0) {
            // we want empty sessions even though they are crap
            qDebug() << "Session" << sess->session()
                << "["+QDateTime::fromSecsSinceEpoch(sess->session()).toString("MMM dd, yyyy hh:mm:ss")+"]"
                << "has negative duration";
            qDebug() << QString("Start: %1").arg(sess->realFirst(),0,16) << QString("End: %1").arg(sess->realLast(),0,16);
        }

        if (resday->str.date.isValid()) {
            STRRecord & R = resday->str;

            // Claim this session
            R.sessionid = sess->session();

            // Save maskon time in session setting so we can use it later to avoid doubleups.
            //sess->settings[RMS9_MaskOnTime] = R.maskon;

    #ifdef SESSION_DEBUG
            sess->session_files.append("STR.edf");
    #endif
            StoreSettings(sess, R);

        } else { // No corresponding STR.edf record, but we have EDF files
   #ifdef STR_DEBUG
            qDebug() << "EDF files without STR record" << resday->date.toString();
   #endif
            bool foundprev = false;
            loader->sessionMutex.lock();

            auto it=p_profile->daylist.find(resday->date); // should exist already to be here
            auto begin = p_profile->daylist.begin();
            while (it!=begin) {
                --it;
                Day * day = it.value();
                bool hasmachine = day && day->hasMachine(mach);

                if ( ! hasmachine)
                    continue;

                QList<Session *> sessions = day->getSessions(MT_CPAP);

                if (sessions.size() > 0) {
                    Session *chksess = sessions[0];
                    sess->settings = chksess->settings;
                    foundprev = true;
                    break;
                }
            }
            loader->sessionMutex.unlock();
            sess->setNoSettings(true);

            if (!foundprev) {
                // We have no Summary or Settings data... we need to do something to indicate this, and detect the mode
                if (sess->channelDataExists(CPAP_Pressure)) {
                    qDebug() << "Guessing the PAP mode...";
                    GuessPAPMode(sess);
                }
            }
        }           // end else no STR record for these edf files

        sess->UpdateSummaries();
#ifdef SESSION_DEBUG
        qDebug() << "Adding session" << sess->session()
            << "["+QDateTime::fromSecsSinceEpoch(sess->session()).toString("MMM dd, yyyy hh:mm:ss")+"]";
#endif

        // Save is not threadsafe? (meh... it seems to be)
       // loader->saveMutex.lock();
       // loader->saveMutex.unlock();

//        if ( (QDateTime::fromSecsSinceEpoch(sess->session()) > QDateTime::currentDateTime()) ||
        if ( (sess->realFirst() == 0) || (sess->realLast() == 0) )
            qWarning().noquote() << "Skipping future or absent date session:" << sess->session()
                << "["+QDateTime::fromSecsSinceEpoch(sess->session()).toString("MMM dd, yyyy hh:mm:ss")+"]"
                << "\noriginal date is" << resday->date.toString()
                << "session realFirst" << sess->realFirst() << "realLast" << sess->realLast();
        else
            save(loader, sess);

        // Free the memory used by this session
        sess->TrashEvents();
//      delete sess;
    }   // end for-loop walking the overlaps (file groups per session
}

void ResmedLoader::SaveSession(ResmedLoader* loader, Session* sess)
{
    Machine* mach = sess->machine();

    loader->sessionMutex.lock();         // AddSession definitely ain't threadsafe.
    if ( ! sess->Store(mach->getDataPath()) ) {
        qWarning() << "Failed to store session" << sess->session();
    }
    if ( ! mach->AddSession(sess) ) {
        qWarning() << "Session" << sess->session() << "was not addded";
    }
    loader->sessionCount++;
    loader->sessionMutex.unlock();
}

bool matchSignal(ChannelID ch, const QString & name);		// forward
bool ResmedLoader::LoadCSL(Session *sess, const QString & path)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
    time.start();
#endif

    //QString filename = path.section(-2, -1); generates error: conversion from ‘int’ to ‘QChar’ is ambiguous
    // Filename is only used for display purposes.
    ResMedEDFInfo edf;
    QString filename = path.section("/",-2, -1);
    if ( ! edf.Open(path) ) {
        qDebug() << "LoadCSL failed to open" << filename;
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfopentime = time.elapsed();
    time.start();
#endif

    if (!edf.Parse()) {
        qDebug() << "LoadCSL failed to parse" << filename;
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfparsetime = time.elapsed();
    time.start();
#endif

    // Always create CSR event list so that overview always finds something
    EventList *CSR = sess->AddEventList(CPAP_CSR, EVL_Event);

    // Allow for empty sessions..
    qint64 csr_starts = 0;

    // Process event annotation records
//  qDebug() << "File has " << edf.annotations.size() << "annotation vectors";
//  int vec = 1;
    for (auto annoVec = edf.annotations.begin(); annoVec != edf.annotations.end(); annoVec++ ) {
//      qDebug() << "Vector " << vec++ << " has " << annoVec->size() << " annotations";
        for (auto anno = annoVec->begin(); anno != annoVec->end(); anno++ ) {
//          qDebug() << "Offset: " << anno->offset << " Duration: " << anno->duration << " Text: " << anno->text;
            qint64 tt = edf.startdate + qint64(anno->offset*1000L);

            if ( ! anno->text.isEmpty()) {
                if (anno->text == "CSR Start") {
                    csr_starts = tt;
                } else if (anno->text == "CSR End") {
//                    if ( ! CSR) {
//                        CSR = sess->AddEventList(CPAP_CSR, EVL_Event);
//                    }
                    if (csr_starts > 0) {
                        if (sess->checkInside(csr_starts)) {
                            CSR->AddEvent(tt, double(tt - csr_starts) / 1000.0);
                        }
                        csr_starts = 0;
                    } else {
                        qWarning() << "Split csr event flag in " << edf.filename;
                    }
                } else if (anno->text != "Recording starts") {
                    qWarning() << "Unobserved ResMed CSL annotation field: " << anno->text;
                }
            }
        }
    }

    if (csr_starts > 0) {
        qDebug() << "Unfinished csr event in " << edf.filename;
    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    timeInLoadCSL += time.elapsed();
    timeInEDFOpen += edfopentime;
    timeInEDFInfo += edfparsetime;
    timeMutex.unlock();
#endif

    return true;
}

bool ResmedLoader::LoadEVE(Session *sess, const QString & path)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
    time.start();
#endif
    ResMedEDFInfo edf;
    QString filename = path.section("/",-2, -1);
    if ( ! edf.Open(path) ) {
        qDebug() << "LoadEVE failed to open" << filename;
        return false;
    }
#ifdef DEBUG_EFFICIENCY
    int edfopentime = time.elapsed();
    time.start();
#endif

    if (!edf.Parse()) {
        qDebug() << "LoadEVE failed to parse" << filename;
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfparsetime = time.elapsed();
    time.start();
#endif

    // Notes: Event records have useless duration record.
    // Do not update session start / end times because they are needed to determine if events belong in this session or not...

    EventList *OA = nullptr, *HY = nullptr, *CA = nullptr, *UA = nullptr, *RE = nullptr;

    // Allow for empty sessions..

    // Create some EventLists
    OA = sess->AddEventList(CPAP_Obstructive, EVL_Event);
    HY = sess->AddEventList(CPAP_Hypopnea, EVL_Event);
    UA = sess->AddEventList(CPAP_Apnea, EVL_Event);

    // Process event annotation records
//  qDebug() << "File has " << edf.annotations.size() << "annotation vectors";
//  int vec = 1;
    for (auto annoVec = edf.annotations.begin(); annoVec != edf.annotations.end(); annoVec++ ) {
//      qDebug() << "Vector " << vec++ << " has " << annoVec->size() << " annotations";
        for (auto anno = annoVec->begin(); anno != annoVec->end(); anno++ ) {
            qint64 tt = edf.startdate + qint64(anno->offset*1000L);
//          qDebug() << "Offset: " << anno->offset << " Duration: " << anno->duration << " Text: " << anno->text;
//          qDebug() << "Time: " << (tt/1000L). << " Duration: " << anno->duration << " Text: " << anno->text;

            if ( ! anno->text.isEmpty()) {
                if (matchSignal(CPAP_Obstructive, anno->text)) {
                    if (sess->checkInside(tt))
                        OA->AddEvent(tt, anno->duration);
                } else if (matchSignal(CPAP_Hypopnea, anno->text)) {
                    if (sess->checkInside(tt))
                        HY->AddEvent(tt, anno->duration); // Hyponeas may not have any duration!
                } else if (matchSignal(CPAP_Apnea, anno->text)) {
                    if (sess->checkInside(tt))
                        UA->AddEvent(tt, anno->duration);
                } else if (matchSignal(CPAP_RERA, anno->text)) {
                    // Not all devices have it, so only create it when necessary..
                    if ( ! RE)
                        RE = sess->AddEventList(CPAP_RERA, EVL_Event);
                    if (sess->checkInside(tt))
                        RE->AddEvent(tt, anno->duration);
                } else if (matchSignal(CPAP_ClearAirway, anno->text)) {
                    // Not all devices have it, so only create it when necessary..
                    if ( ! CA)
                        CA = sess->AddEventList(CPAP_ClearAirway, EVL_Event);
                    if (sess->checkInside(tt))
                        CA->AddEvent(tt, anno->duration);
                } else if (anno->text == "SpO2 Desaturation") {    // Used in 28509
                        continue;                               // ignored for now
                } else {
                    if (anno->text != "Recording starts") {
                        qDebug() << "Unobserved ResMed annotation field: " << anno->text;
                    }
                }
            }

        }
    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    timeInLoadEVE += time.elapsed();
    timeInEDFOpen += edfopentime;
    timeInEDFInfo += edfparsetime;
    timeMutex.unlock();
#endif

    return true;
}

bool ResmedLoader::LoadBRP(Session *sess, const QString & path)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
    time.start();
#endif
    ResMedEDFInfo edf;
    QString filename = path.section("/",-2, -1);
    if ( ! edf.Open(path) ) {
        qDebug() << "LoadBRP failed to open" << filename;
        return false;
    }
#ifdef DEBUG_EFFICIENCY
    int edfopentime = time.elapsed();
    time.start();
#endif
    if (!edf.Parse()) {
#ifdef EDF_DEBUG
        qDebug() << "LoadBRP failed to parse" << filename.section("/", -2, -1);
#endif
        return false;
    }
#ifdef DEBUG_EFFICIENCY
    int edfparsetime = time.elapsed();
    time.start();
    int AddWavetime = 0;
    QTime time2;
#endif
    sess->updateFirst(edf.startdate);

    qint64 duration = edf.GetNumDataRecords() * edf.GetDurationMillis();
    sess->updateLast(edf.startdate + duration);

    for (auto & es : edf.edfsignals) {
        long recs = es.sampleCnt * edf.GetNumDataRecords();
        if (recs < 0)
            continue;
        ChannelID code;

        if (matchSignal(CPAP_FlowRate, es.label)) {
            code = CPAP_FlowRate;
            es.gain *= 60.0;
            es.physical_minimum *= 60.0;
            es.physical_maximum *= 60.0;
            es.physical_dimension = "L/M";

        } else if (matchSignal(CPAP_MaskPressureHi, es.label)) {
            code = CPAP_MaskPressureHi;

        } else if (matchSignal(CPAP_RespEvent, es.label)) {
            code = CPAP_RespEvent;

//      } else if (es.label == "TrigCycEvt.40ms") {         // we need a real code for this signal
//          code = CPAP_TriggerEvent;                       // Well, it got folded into RespEvent
//          continue;

        } else if (es.label != "Crc16") {
            qDebug() << "Unobserved ResMed BRP Signal " << es.label;
            continue;
        } else
            continue;

        if (code) {
            double rate = double(duration) / double(recs);
            EventList *a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
            a->setDimension(es.physical_dimension);
#ifdef DEBUG_EFFICIENCY
            time2.start();
#endif
            a->AddWaveform(edf.startdate, es.dataArray, recs, duration);
#ifdef DEBUG_EFFICIENCY
            AddWavetime+= time2.elapsed();
#endif

            EventDataType min = a->Min();
            EventDataType max = a->Max();

            // Cap to physical dimensions, because there can be ram glitches/whatever that throw really big outliers.
            if (min < es.physical_minimum)
                min = es.physical_minimum;
            if (max > es.physical_maximum)
                max = es.physical_maximum;

            sess->updateMin(code, min);
            sess->updateMax(code, max);
            sess->setPhysMin(code, es.physical_minimum);
            sess->setPhysMax(code, es.physical_maximum);
        }
    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    timeInLoadBRP += time.elapsed();
    timeInEDFOpen += edfopentime;
    timeInEDFInfo += edfparsetime;
    timeInAddWaveform += AddWavetime;
    timeMutex.unlock();
#endif

    return true;
}

// Load SAD Oximetry Signals
bool ResmedLoader::LoadSAD(Session *sess, const QString & path)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
    time.start();
#endif

    ResMedEDFInfo edf;
    QString filename = path.section("/",-2, -1);
    if ( ! edf.Open(path) ) {
        qDebug() << "LoadSAD failed to open" << filename;
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfopentime = time.elapsed();
    time.start();
#endif

    if (!edf.Parse()) {
#ifdef EDF_DEBUG
        qDebug() << "LoadSAD failed to parse" << filename.section("/", -2, -1);
#endif
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfparsetime = time.elapsed();
    time.start();
#endif

    sess->updateFirst(edf.startdate);
    qint64 duration = edf.GetNumDataRecords() * edf.GetDurationMillis();
    sess->updateLast(edf.startdate + duration);

    for (auto & es : edf.edfsignals) {
        //qDebug() << "SAD:" << es.label << es.digital_maximum << es.digital_minimum << es.physical_maximum << es.physical_minimum;
        long recs = es.sampleCnt * edf.GetNumDataRecords();
        ChannelID code;

        bool hasdata = false;

        for (int i = 0; i < recs; ++i) {
            if (es.dataArray[i] != -1) {
                hasdata = true;
                break;
            }
        }
        if (!hasdata)
            continue;

        if (matchSignal(OXI_Pulse, es.label)) {
            code = OXI_Pulse;
            ToTimeDelta(sess, edf, es, code, recs, duration);
            sess->setPhysMax(code, 180);
            sess->setPhysMin(code, 18);
        } else if (matchSignal(OXI_SPO2, es.label)) {
            code = OXI_SPO2;
            es.physical_minimum = 60;
            ToTimeDelta(sess, edf, es, code, recs, duration);
            sess->setPhysMax(code, 100);
            sess->setPhysMin(code, 60);
        } else if (es.label != "Crc16") {
            qDebug() << "Unobserved ResMed SAD Signal " << es.label;
        }
    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    timeInLoadSAD += time.elapsed();
    timeInEDFOpen += edfopentime;
    timeInEDFInfo += edfparsetime;
    timeMutex.unlock();
#endif
    return true;
}


bool ResmedLoader::LoadPLD(Session *sess, const QString & path)
{
#ifdef DEBUG_EFFICIENCY
    QTime time;
    time.start();
#endif
    ResMedEDFInfo edf;
    QString filename = path.section("/",-2, -1);
    if ( ! edf.Open(path) ) {
        qDebug() << "LoadPLD failed to open" << filename;
        return false;
    }
#ifdef DEBUG_EFFICIENCY
    int edfopentime = time.elapsed();
    time.start();
#endif

    if (!edf.Parse()) {
#ifdef EDF_DEBUG
        qDebug() << "LoadPLD failed to parse" << filename.section("/", -2, -1);
#endif
        return false;
    }

#ifdef DEBUG_EFFICIENCY
    int edfparsetime = time.elapsed();
    time.start();
#endif

    // Is it safe to assume the order does not change here?
    enum PLDType { MaskPres = 0, TherapyPres, ExpPress, Leak, RR, Vt, Mv, SnoreIndex, FFLIndex, U1, U2 };

    qint64 duration = edf.GetNumDataRecords() * edf.GetDurationMillis();
    sess->updateFirst(edf.startdate);
    sess->updateLast(edf.startdate + duration);
    QString t;
    int emptycnt = 0;
    EventList *a = nullptr;
//  double rate;
    long samples;
    ChannelID code;
    bool square = AppSetting->squareWavePlots();
    // The following is a hack to skip the multiple uses of Ti and Te by Resmed for signal labels
    // It should be replaced when code in resmed_info class changes the labels to be unique
    bool found_Ti_code = false;
    bool found_Te_code = false;

    QDateTime sessionStartDT = QDateTime:: fromMSecsSinceEpoch(sess->first());
//  bool forceDebug = (sessionStartDT > QDateTime::fromString("2021-02-26 12:00:00", "yyyy-MM-dd HH:mm:ss")) &&
//                      (sessionStartDT < QDateTime::fromString("2021-02-27 12:00:00", "yyyy-MM-dd HH:mm:ss"));
    bool forceDebug = false;

    for (auto & es : edf.edfsignals) {
        a = nullptr;
        samples = es.sampleCnt * edf.GetNumDataRecords();

        if (samples <= 0)
            continue;

//      rate = double(duration) / double(samples);

        //qDebug() << "EVE:" << es.digital_maximum << es.digital_minimum << es.physical_maximum << es.physical_minimum << es.gain;
        if (forceDebug) {
            qDebug() << "Session" << sessionStartDT.toString() << filename.section("/", -2, -1) << "signal" << es.label;
            qDebug() << "\tSecond/rec:" << edf.GetDurationMillis()/1000 << "Samples/rec:" << es.sampleCnt;
        }
        if (matchSignal(CPAP_Snore, es.label)) {
            code = CPAP_Snore;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_Pressure, es.label)) {
            code = CPAP_Pressure;
//          es.physical_maximum = 25;
//          es.physical_minimum = 4;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, true);
        } else if (matchSignal(CPAP_IPAP, es.label)) {
            code = CPAP_IPAP;
//          es.physical_maximum = 25;
//          es.physical_minimum = 4;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, true);
        } else if (matchSignal(CPAP_EPAP, es.label)) { // Expiratory Pressure
            code = CPAP_EPAP;
//          es.physical_maximum = 25;
//          es.physical_minimum = 4;

            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, true);
        }  else if (matchSignal(CPAP_MinuteVent,es.label)) {
            code = CPAP_MinuteVent;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_RespRate, es.label)) {
            code = CPAP_RespRate;
//          a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
//          a->AddWaveform(edf.startdate, es.dataArray, samples, duration);
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_TidalVolume, es.label)) {
            code = CPAP_TidalVolume;
            es.physical_dimension = "mL";
            es.gain *= 1000.0;
            es.physical_maximum *= 1000.0;
            es.physical_minimum *= 1000.0;
//          es.digital_maximum*=1000.0;
//          es.digital_minimum*=1000.0;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_Leak, es.label)) {
            code = CPAP_Leak;
            es.gain *= 60.0;
            es.physical_maximum *= 60.0;
            es.physical_minimum *= 60.0;
//          es.digital_maximum*=60.0;
//          es.digital_minimum*=60.0;
            es.physical_dimension = "L/M";
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, true);
            sess->setPhysMax(code, 120.0);
            sess->setPhysMin(code, 0);
        } else if (matchSignal(CPAP_FLG, es.label)) {
            code = CPAP_FLG;
            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_MaskPressure, es.label)) {
            code = CPAP_MaskPressure;
//          es.physical_maximum = 25;
//          es.physical_minimum = 4;

            ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
        } else if (matchSignal(CPAP_IE, es.label)) { //I:E ratio
            code = CPAP_IE;
            es.gain /= 100.0;
            es.physical_maximum /= 100.0;
            es.physical_minimum /= 100.0;
//          qDebug() << "IE Gain, Max, Min" << es.gain << es.physical_maximum << es.physical_minimum;
//          qDebug() << "IE count, data..." << es.sampleCnt << es.dataArray[0] << es.dataArray[1] << es.dataArray[2] << es.dataArray[3] << es.dataArray[4];
//          a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
//          a->AddWaveform(edf.startdate, es.dataArray, samples, duration);
//  Fix ToTimeDelta to store inverse of edf data - also fix labels and tool tip
//            ToTimeDelta(sess,edf,es, code,samples,duration,0,0, square);
        } else if (matchSignal(CPAP_Ti, es.label)) {
            code = CPAP_Ti;
            // There are TWO of these with the same label on 36037, 36039, 36377 and others
            // Also 37051 has R5Ti.2s and Ti.2s. We use R5Ti.2s and ignore the Ti.2s
            if ( found_Ti_code )
                continue;
            found_Ti_code = true;
//          a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
//          a->AddWaveform(edf.startdate, es.dataArray, samples, duration);
            ToTimeDelta(sess,edf,es, code,samples,duration,0,0, square);
        } else if (matchSignal(CPAP_Te, es.label)) {
            code = CPAP_Te;
            // There are TWO of these with the same label on my VPAP Adapt 36037
            if ( found_Te_code )
                continue;
            found_Te_code = true;
//          a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
//          a->AddWaveform(edf.startdate, es.dataArray, samples, duration);
            ToTimeDelta(sess,edf,es, code,samples,duration,0,0, square);
        } else if (matchSignal(CPAP_TgMV, es.label)) {
            code = CPAP_TgMV;
//          a = sess->AddEventList(code, EVL_Waveform, es.gain, es.offset, 0, 0, rate);
//          a->AddWaveform(edf.startdate, es.dataArray, samples, duration);
            ToTimeDelta(sess,edf,es, code,samples,duration,0,0, square);
        } else if (es.label == "Va") {  // Signal used in 36039... What to do with it???
            a = nullptr;                // We'll skip it for now
        } else if (es.label == "AlvMinVent.2s") {  // Signal used in 28509... What to do with it???
            a = nullptr;                // We'll skip it for now
        } else if (es.label == "CLRatio.2s") {  // Signal used in 28509... What to do with it???
            a = nullptr;                // We'll skip it for now
        } else if (es.label == "TRRatio.2s") {  // Signal used in 28509... What to do with it???
            a = nullptr;                // We'll skip it for now
        } else if (es.label == "") { // What the hell resmed??
        // these empty lables should be changed in resmed_EDFInfo to something unique
            if (emptycnt == 0) {
                code = RMS9_E01;
//                ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
            } else if (emptycnt == 1) {
                code = RMS9_E02;
//                ToTimeDelta(sess, edf, es, code, samples, duration, 0, 0, square);
            } else {
                qDebug() << "Unobserved Empty Signal " << es.label;
            }

            emptycnt++;
        }  else if (es.label != "Crc16") {
            qDebug() << "Unobserved ResMed PLD Signal " << es.label;
            a = nullptr;
        }

        if (a) {
            sess->updateMin(code, a->Min());
            sess->updateMax(code, a->Max());
            sess->setPhysMin(code, es.physical_minimum);
            sess->setPhysMax(code, es.physical_maximum);
            a->setDimension(es.physical_dimension);
        }

    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    timeInLoadPLD += time.elapsed();
    timeInEDFOpen += edfopentime;
    timeInEDFInfo += edfparsetime;
    timeMutex.unlock();
#endif

    return true;
}

// Convert EDFSignal data to OSCAR's Time-Delta Event format
EventList * buildEventList( EventStoreType est, EventDataType t_min, EventDataType t_max, EDFSignal &es,
        EventDataType *min, EventDataType *max, double tt, EventList *el, Session * sess, ChannelID code );		// forward
void ResmedLoader::ToTimeDelta(Session *sess, ResMedEDFInfo &edf, EDFSignal &es, ChannelID code,
                               long samples, qint64 duration, EventDataType t_min, EventDataType t_max, bool square)
{
    using namespace schema;
    ChannelList channel;

//  QDateTime sessionStartDT = QDateTime:: fromMSecsSinceEpoch(sess->first());
//  bool forceDebug = (sessionStartDT > QDateTime::fromString("2021-02-26 12:00:00", "yyyy-MM-dd HH:mm:ss")) &&
//                      (sessionStartDT < QDateTime::fromString("2021-02-27 12:00:00", "yyyy-MM-dd HH:mm:ss"));
    bool forceDebug = false;

    if (t_min == t_max) {
        t_min = es.physical_minimum;
        t_max = es.physical_maximum;
    }

#ifdef DEBUG_EFFICIENCY
    QElapsedTimer time;
    time.start();
#endif

    double rate = (duration / samples); // milliseconds per record
    double tt = edf.startdate;

    EventStoreType c=0, last;

    int startpos = 0;

//  There's no reason to skip the first 40 seconds of slow data
//  Reduce that to 10 seconds, to allow presssures to stabilise
    if ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) {
        startpos = 5; // Shave the first 10 seconds of pressure data
        tt += rate * startpos;
    }
// Likewise for the values that the device computes for us, but 20 seconds
    if ( (code == CPAP_MinuteVent) || (code == CPAP_RespRate) || (code == CPAP_TidalVolume) ||
         (code == CPAP_Ti) || (code == CPAP_Te) || (code == CPAP_IE) ) {
        startpos = 10; // Shave the first 20 seconds of computed data
        tt += rate * startpos;
    }

    qint16 *sptr = es.dataArray;
    qint16 *eptr = sptr + samples;
    sptr += startpos;

    EventDataType min = t_max, max = t_min, tmp;

    EventList *el = nullptr;

    if (forceDebug)
        qDebug() << "Code:" << QString::number(code, 16) << "Samples:" << samples;
    long sampleCounter=startpos;

    if (samples > startpos + 1) {
        // Prime last with a good starting value
        do {
            last = *sptr++;
            sampleCounter++;
            tmp = EventDataType(last) * es.gain;
            if ((tmp >= t_min) && (tmp <= t_max)) {
                min = tmp;
                max = tmp;
                el = sess->AddEventList(code, EVL_Event, es.gain, es.offset, 0, 0);
                if (forceDebug)
//                  qDebug() << "New EventList:" << channel.channels[code]->code() << QDateTime::fromMSecsSinceEpoch(tt).toString();
                    qDebug() << "New EventList:" << QString::number(code, 16) << QDateTime::fromMSecsSinceEpoch(tt).toString();

                el->AddEvent(tt, last);

                if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                    qDebug() << "First Event:" << tmp << QDateTime::fromMSecsSinceEpoch(tt).toString() << "Pos:" << (sptr-1) - es.dataArray;
                tt += rate;
                break;
            }
            tt += rate;
        } while (sptr < eptr);

        if ( ! el) {
            qWarning() << "No eventList for" << QDateTime::fromMSecsSinceEpoch(sess->first()).toString() << "code"
//                          << channel.channels[code]->code();
                            << QString::number(code, 16);
#ifdef DEBUG_EFFICIENCY
            timeMutex.lock();
            timeInTimeDelta += time.elapsed();
            timeMutex.unlock();
#endif
            return;
        }

        if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
             qDebug() << "Before loop to buildEventList" << el->count() << "Last:" << last*es.gain << "Next:" << (*sptr)*es.gain <<
                "Pos:" << sptr - es.dataArray << QDateTime::fromMSecsSinceEpoch(tt).toString();
        for (; sptr < eptr; sptr++) {
            sampleCounter++;
            c = *sptr;
            if (c == -1  && t_min>=0 ) {
                // -1 is Null value
                //DEBUGCI NAME(code) Q(last) Q(c) QQ(cnt,sampleCounter) QQ(max,samples) O((sampleCounter == samples)) DATETIME((qint64)tt);
                Q_UNUSED(sampleCounter);
                c=last; //reset the current sample to the last one to skip over using it.
                // sometimes the resmed devices create this condition.
                // this change stops false error values from deing displayed.
            }
            if (last != c) {
                if (square) {
                    if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                        qDebug() << "Before square call to buildEventList" << el->count();
                    el = buildEventList( last, t_min, t_max, es, &min, &max, tt, el, sess, code );
                    if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                        qDebug() << "After square call to buildEventList" << el->count();
                }
                if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                    qDebug() << "Before call to buildEventList" << el->count() << "Cur:" << c*es.gain << "Last:" << last*es.gain << "Pos:" << sptr - es.dataArray <<
                        QDateTime::fromMSecsSinceEpoch(tt).toString();
                el = buildEventList( c, t_min, t_max, es, &min, &max, tt, el, sess, code );
                if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                    qDebug() << "After call to buildEventList" << el->count();
            }
            tt += rate;
            last = c;
        }

        tmp = EventDataType(c) * es.gain;
        if ((tmp >= t_min) && (tmp <= t_max)) {
            el->AddEvent(tt, c);
            if (forceDebug && ((code == CPAP_Pressure) || (code == CPAP_IPAP) || (code == CPAP_EPAP)) )
                qDebug() << "Last Event:" << tmp << QDateTime::fromMSecsSinceEpoch(tt).toString() << "Pos:" << (sptr-1) - es.dataArray;
        } else {
            if (!(c == -1  && t_min>=0 )) {
                qDebug() << "Failed to add last event - Code:" << QString::number(code, 16) << "Value:" << tmp <<
                    QDateTime::fromMSecsSinceEpoch(tt).toString() << "Pos:" << (sptr-1) - es.dataArray;
            }
        }

        sess->updateMin(code, min);
        sess->updateMax(code, max);
        sess->setPhysMin(code, es.physical_minimum);
        sess->setPhysMax(code, es.physical_maximum);
        sess->updateLast(tt);
        if (forceDebug)
//          qDebug() << "EventList:" << channel.channels[code]->code() << QDateTime::fromMSecsSinceEpoch(tt).toString() << "Size" << el->count();
            qDebug() << "EventList:" << QString::number(code, 16) << QDateTime::fromMSecsSinceEpoch(tt).toString() << "Size" << el->count();
    } else {
        qWarning() << "not enough records for EventList" << QDateTime::fromMSecsSinceEpoch(sess->first()).toString() << "code"
//                          << channel.channels[code]->code();
                            << QString::number(code, 16);
    }

#ifdef DEBUG_EFFICIENCY
    timeMutex.lock();
    if (el != nullptr) {
        qint64 t = time.nsecsElapsed();
        int cnt = el->count();
        int bytes = cnt * (sizeof(EventStoreType) + sizeof(quint32));
        int wvbytes = samples * (sizeof(EventStoreType));
        auto it = channel_efficiency.find(code);

        if (it == channel_efficiency.end()) {
            channel_efficiency[code] = wvbytes - bytes;
            channel_time[code] = t;
        } else {
            it.value() += wvbytes - bytes;
            channel_time[code] += t;
        }
    }
    timeInTimeDelta += time.elapsed();
    timeMutex.unlock();
#endif
}       // end ResMedLoader::ToTimeDelta

EventList * buildEventList( EventStoreType est, EventDataType t_min, EventDataType t_max, EDFSignal &es,
        EventDataType *min, EventDataType *max, double tt, EventList *el, Session * sess, ChannelID code )
{
    using namespace schema;
    ChannelList channel;
//  QDateTime sessionStartDT = QDateTime:: fromMSecsSinceEpoch(sess->first());
//  bool forceDebug = (sessionStartDT > QDateTime::fromString("2021-02-26 12:00:00", "yyyy-MM-dd HH:mm:ss")) &&
//                      (sessionStartDT < QDateTime::fromString("2021-02-27 12:00:00", "yyyy-MM-dd HH:mm:ss"));
    bool forceDebug = false;


    EventDataType tmp = EventDataType(est) * es.gain;

    if ((tmp >= t_min) && (tmp <= t_max)) {
        if (tmp < *min)
            *min = tmp;

        if (tmp > *max)
            *max = tmp;

        el->AddEvent(tt, est);
    } else {
//        if ( tmp > 0 )
            qDebug() << "Code:" << QString::number(code, 16) <<"Value:" << tmp << "Out of range:\n\t t_min:" <<
                t_min << "t_max:" << t_max << "EL count:" << el->count();
        // Out of bounds value, start a new eventlist
        // But first drop a closing value that repeats the last one
        el->AddEvent(tt, el->raw(el->count() - 1));
        if (el->count() > 1) {
            // that should be in session, not the eventlist.. handy for debugging though
            el->setDimension(es.physical_dimension);

            el = sess->AddEventList(code, EVL_Event, es.gain, es.offset, 0, 0);
            if (forceDebug)
//              qDebug() << "New EventList:" << channel.channels[code]->code() << QDateTime::fromMSecsSinceEpoch(tt).toString();
                qDebug() << "New EventList:" << QString::number(code, 16) << QDateTime::fromMSecsSinceEpoch(tt).toString();
        } else {
            el->clear(); // reuse the object
            if (forceDebug)
//              qDebug() << "Clear EventList:" << channel.channels[code]->code() << QDateTime::fromMSecsSinceEpoch(tt).toString();
                qDebug() << "Clear EventList:" << QString::number(code, 16) << QDateTime::fromMSecsSinceEpoch(tt).toString();
        }
    }
    return el;
}

// Check if given string matches any alternative signal names for this channel
bool matchSignal(ChannelID ch, const QString & name)
{
    auto channames = resmed_codes.find(ch);
    if (channames == resmed_codes.end()) {
        return false;
    }

    for (auto & string : channames.value()) {
        // Using starts with, because ResMed is very lazy about consistency
        if (name.startsWith(string, Qt::CaseInsensitive)) {
                return true;
        }
    }
    return false;
}

void setupResMedTranslationMap()
{
    ////////////////////////////////////////////////////////////////////////////
    // Translation lookup table for non-english devices
    // Also combine S9, AS10, and AS11 variants
    ////////////////////////////////////////////////////////////////////////////

    // Only put the first part, enough to be identifiable, because ResMed likes
    // to crop short the signal names
    // Read this from a table?

    resmed_codes.clear();

    // BRP file
    resmed_codes[CPAP_FlowRate] = QStringList{ "Flow", "Flow.40ms" };
    resmed_codes[CPAP_MaskPressureHi] = QStringList{ "Mask Pres", "Press.40ms" };
//  resmed_codes[CPAP_TriggerEvent] = QStringList{ "TrigCycEvt.40ms" };             // AC10 VAuto and -S
    resmed_codes[CPAP_RespEvent] = QStringList {"Resp Event", "TrigCycEvt.40ms" };  // S9 VPAPS and STA-IVAPS call it RespEvent

    // PLD File
    resmed_codes[CPAP_MaskPressure] = QStringList { "Mask Pres", "MaskPress.2s" };
//  resmed_codes[CPAP_RespEvent] = QStringList {"Resp Event" };
    resmed_codes[CPAP_Pressure] = QStringList { "Therapy Pres", "Press.2s" };   // Un problemo... IPAP also uses Press.2s.. check the mode :/


    // STR signals
    resmed_codes[CPAP_IPAP] = QStringList { "Insp Pres", "IPAP", "S.BL.IPAP" , "S.S.IPAP" };
    resmed_codes[CPAP_EPAP] = QStringList { "Exp Pres", "EprPress.2s", "EPAP", "S.BL.EPAP", "EPRPress.2s" , "S.S.EPAP" };
    resmed_codes[CPAP_EPAPHi] = QStringList { "Max EPAP" };
    resmed_codes[CPAP_EPAPLo] = QStringList { "Min EPAP", "S.VA.MinEPAP" };
    resmed_codes[CPAP_IPAPHi] = QStringList { "Max IPAP", "S.VA.MaxIPAP" };
    resmed_codes[CPAP_IPAPLo] = QStringList { "Min IPAP" };
    resmed_codes[CPAP_PS] = QStringList { "PS", "S.VA.PS" };
    resmed_codes[CPAP_PSMin] = QStringList { "Min PS" };
    resmed_codes[CPAP_PSMax] = QStringList { "Max PS" };
    resmed_codes[CPAP_Leak] = QStringList { "Leak", "Leck", "Fuites", "Fuite", "Fuga", "\xE6\xBC\x8F\xE6\xB0\x94", "Lekk", "Läck","LÃ¤ck", "Leak.2s", "Sızıntı" };
    resmed_codes[CPAP_RespRate] = QStringList {  "RR", "AF", "FR", "RespRate.2s" };
    resmed_codes[CPAP_MinuteVent] = QStringList { "MV", "VM", "MinVent.2s" };
    resmed_codes[CPAP_TidalVolume] = QStringList { "Vt", "VC", "TidVol.2s" };
    resmed_codes[CPAP_IE] = QStringList { "I:E", "IERatio.2s" };
    resmed_codes[CPAP_Snore] = QStringList { "Snore", "Snore.2s" };
    resmed_codes[CPAP_FLG] = QStringList { "FFL Index", "FlowLim.2s" };
    resmed_codes[CPAP_Ti] = QStringList { "Ti", "B5ITime.2s" };
    resmed_codes[CPAP_Te] = QStringList { "Te", "B5ETime.2s" };
    resmed_codes[CPAP_TgMV] = QStringList { "TgMV", "TgtVent.2s" };
    resmed_codes[OXI_Pulse] = QStringList { "Pulse", "Puls", "Pouls", "Pols", "Pulse.1s", "Nabiz" };
    resmed_codes[OXI_SPO2] = QStringList { "SpO2", "SpO2.1s" };
    resmed_codes[CPAP_Obstructive] = QStringList { "Obstructive apnea" };
    resmed_codes[CPAP_Hypopnea] = QStringList { "Hypopnea" };
    resmed_codes[CPAP_Apnea] = QStringList { "Apnea" };
    resmed_codes[CPAP_RERA] = QStringList { "Arousal" };
    resmed_codes[CPAP_ClearAirway] = QStringList { "Central apnea" };
    resmed_codes[CPAP_Mode] = QStringList { "Mode", "Modus", "Funktion", "\xE6\xA8\xA1\xE5\xBC\x8F", "Mod" };
    resmed_codes[RMS9_SetPressure] = QStringList { "Set Pressure", "Eingest. Druck", "Ingestelde druk", "\xE8\xAE\xBE\xE5\xAE\x9A\xE5\x8E\x8B\xE5\x8A\x9B", "Pres. prescrite", "Inställt tryck", "InstÃ¤llt tryck", "S.C.Press", "Basıncı Ayarl" };
    resmed_codes[RMS9_EPR] = QStringList { "EPR", "\xE5\x91\xBC\xE6\xB0\x94\xE9\x87\x8A\xE5\x8E\x8B\x28\x45\x50" };
    resmed_codes[RMS9_EPRLevel] = QStringList { "EPR Level", "EPR-Stufe", "EPR-niveau", "\x45\x50\x52\x20\xE6\xB0\xB4\xE5\xB9\xB3", "Niveau EPR", "EPR-nivå", "EPR-nivÃ¥", "S.EPR.Level", "EPR Düzeyi" };
    resmed_codes[CPAP_PressureMax] = QStringList { "Max Pressure", "Max. Druck", "Max druk", "\xE6\x9C\x80\xE5\xA4\xA7\xE5\x8E\x8B\xE5\x8A\x9B", "Pression max.", "Max tryck", "S.AS.MaxPress", "S.A.MaxPress", "Azami Basınç" };
    resmed_codes[CPAP_PressureMin] = QStringList { "Min Pressure", "Min. Druck", "Min druk", "\xE6\x9C\x80\xE5\xB0\x8F\xE5\x8E\x8B\xE5\x8A\x9B", "Pression min.", "Min tryck", "S.AS.MinPress", "S.A.MinPress", "Min Basınç" };

    //resmed_codes[RMS9_EPR].push_back("S.EPR.EPRType");
}


// don't really need this anymore, but perhaps it's useful info for reference
//    Resmed_Model_Map = {
//        { "S9 Escape",      { 36001, 36011, 36021, 36141, 36201, 36221, 36261, 36301, 36361 } },
//        { "S9 Escape Auto", { 36002, 36012, 36022, 36302, 36362 } },
//        { "S9 Elite",       { 36003, 36013, 36023, 36103, 36113, 36123, 36143, 36203, 36223, 36243, 36263, 36303, 36343, 36363 } },
//        { "S9 Autoset",     { 36005, 36015, 36025, 36105, 36115, 36125, 36145, 36205, 36225, 36245, 36265, 36305, 36325, 36345, 36365 } },
//        { "S9 AutoSet CS",  { 36100, 36110, 36120, 36140, 36200, 36220, 36360 } },
//        { "S9 AutoSet 25",  { 36106, 36116, 36126, 36146, 36206, 36226, 36366 } },
//        { "S9 AutoSet for Her", { 36065 } },
//        { "S9 VPAP S",      { 36004, 36014, 36024, 36114, 36124, 36144, 36204, 36224, 36284, 36304 } },
//        { "S9 VPAP Auto",   { 36006, 36016, 36026 } },
//        { "S9 VPAP Adapt",  { 36037, 36007, 36017, 36027, 36367 } },
//        { "S9 VPAP ST",     { 36008, 36018, 36028, 36108, 36148, 36208, 36228, 36368 } },
//        { "S9 VPAP ST 22",  { 36118, 36128 } },
//        { "S9 VPAP ST-A",   { 36039, 36159, 36169, 36379 } },
//      //S8 Series
//        { "S8 Escape",      { 33007 } },
//        { "S8 Elite II",    { 33039 } },
//        { "S8 Escape II",   { 33051 } },
//        { "S8 Escape II AutoSet", { 33064 } },
//        { "S8 AutoSet II",  { 33129 } },
//    };
//
// Return the model name matching the supplied model number.
// const QString & lookupModel(quint16 model)
// {
//
//     for (auto it=Resmed_Model_Map.begin(),end = Resmed_Model_Map.end(); it != end; ++it) {
//         QList<quint16> & list = it.value();
//         for (auto val : list) {
//             if (val == model) {
//                 return it.key();
//             }
//         }
//     }
//     return STR_UnknownModel;
// }

////////////////////////////////////////////////////////////////////////////////////////////////
// Model number information
//    36003, 36013, 36023, 36103, 36113, 36123, 36143, 36203,
//    36223, 36243, 36263, 36303, 36343, 36363 S9 Elite Series
//    36005, 36015, 36025, 36105, 36115, 36125, 36145, 36205,
//    36225, 36245, 36265, 36305, 36325, 36345, 36365 S9 AutoSet Series
//    36065 S9 AutoSet for Her
//    36001, 36011, 36021, 36141, 36201, 36221, 36261, 36301,
//    36361 S9 Escape
//    36002, 36012, 36022, 36302, 36362 S9 Escape Auto
//    36004, 36014, 36024, 36114, 36124, 36144, 36204, 36224,
//    36284, 36304 S9 VPAP S (+ H5i, + Climate Control)
//    36006, 36016, 36026 S9 VPAP AUTO (+ H5i, + Climate Control)

//    36007, 36017, 36027, 36367
//    S9 VPAP ADAPT (+ H5i, + Climate
//    Control)
//    36008, 36018, 36028, 36108, 36148, 36208, 36228, 36368 S9 VPAP ST (+ H5i, + Climate Control)
//    36100, 36110, 36120, 36140, 36200, 36220, 36360 S9 AUTOSET CS
//    36106, 36116, 36126, 36146, 36206, 36226, 36366 S9 AUTOSET 25
//    36118, 36128 S9 VPAP ST 22
//    36039, 36159, 36169, 36379 S9 VPAP ST-A
//    24921, 24923, 24925, 24926, 24927 ResMed Power Station II (RPSII)
//    33030 S8 Compact
//    33001, 33007, 33013, 33036, 33060 S8 Escape
//    33032 S8 Lightweight
//    33033 S8 AutoScore
//    33048, 33051, 33052, 33053, 33054, 33061 S8 Escape II
//    33055 S8 Lightweight II
//    33021 S8 Elite
//    33039, 33045, 33062, 33072, 33073, 33074, 33075 S8 Elite II
//    33044 S8 AutoScore II
//    33105, 33112, 33126 S8 AutoSet (including Spirit & Vantage)
//    33128, 33137 S8 Respond
//    33129, 33141, 33150 S8 AutoSet II
//    33136, 33143, 33144, 33145, 33146, 33147, 33148 S8 AutoSet Spirit II
//    33138 S8 AutoSet C
//    26101, 26121 VPAP Auto 25
//    26119, 26120 VPAP S
//    26110, 26122 VPAP ST
//    26104, 26105, 26125, 26126 S8 Auto 25
//    26102, 26103, 26106, 26107, 26108, 26109, 26123, 26127 VPAP IV
//    26112, 26113, 26114, 26115, 26116, 26117, 26118, 26124 VPAP IV ST


