/* SleepLib Prisma Loader Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include <cmath>

#include "SleepLib/schema.h"
#include "SleepLib/importcontext.h"
#include "prisma_loader.h"
#include "SleepLib/session.h"
#include "SleepLib/calcs.h"
#include "rawdata.h"


#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../thirdparty/miniz.h"

#define PRISMA_SMART_CONFIG_FILE "config.pscfg"
#define PRISMA_LINE_CONFIG_FILE "config.pcfg"
#define PRISMA_LINE_THERAPY_FILE "therapy.pdat"

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the prisma_data_version in prisma_loader.h when making changes to this
// loader that change loader behaviour or modify channels.
//********************************************************************************************

// parameters
ChannelID Prisma_Mode = 0, Prisma_SoftPAP = 0, Prisma_BiSoft = 0, Prisma_PSoft = 0, Prisma_PSoft_Min = 0, Prisma_AutoStart = 0, Prisma_Softstart_Time = 0, Prisma_Softstart_TimeMax = 0, Prisma_Softstart_Pressure = 0, Prisma_TubeType = 0, Prisma_PMaxOA = 0, Prisma_HumidifierLevel = 0;

// waveforms
ChannelID Prisma_ObstructLevel = 0, Prisma_rMVFluctuation = 0, Prisma_rRMV= 0, Prisma_PressureMeasured = 0, Prisma_FlowFull = 0, Prisma_EEPAP = 0;

// events
ChannelID Prisma_Artifact = 0, Prisma_CriticalLeak = 0, Prisma_DeepSleep = 0, Prisma_TimedBreath = 0;

// epoch events
// The device evaluates 2 minute long epochs, and catagorizes them based on the events that occur during that epoch.
//   eSO: Severe Obstruction
//   eMO: Mild Obstruction
//    eS: Snore
//    eF: Flattening/Flow limitation
ChannelID Prisma_eSO = 0, Prisma_eMO = 0, Prisma_eS = 0, Prisma_eF = 0;

// this "virtual" config setting is used to indicate, if the selected Prisma Mode is not fully supported.
ChannelID Prisma_Warning = 0;

QString PrismaLoader::PresReliefLabel() { return QString("SoftPAP: "); }
ChannelID PrismaLoader::PresReliefMode() { return Prisma_SoftPAP; }
ChannelID PrismaLoader::CPAPModeChannel() { return Prisma_Mode; }

//********************************************************************************************

bool WMEDFInfo::ParseSignalData() {
    int bytes = 0;
    for (auto & sig : edfsignals) {
        if (sig.reserved == "#1") {
            bytes += 1 * sig.sampleCnt;
        }
        else if (sig.reserved == "#2") {
            bytes += 2 * sig.sampleCnt;
        }
    }
    // allocate the arrays for the signal values
    edfHdr.num_data_records = (fileData.size() - edfHdr.num_header_bytes) / bytes;

    for (auto & sig : edfsignals) {
        long samples = sig.sampleCnt * edfHdr.num_data_records;
        if (edfHdr.num_data_records <= 0) {
            sig.dataArray = nullptr;
            continue;
        }
        sig.dataArray = new qint16 [samples];
    }
    for (int recNo = 0; recNo < edfHdr.num_data_records; recNo++) {
        for (auto & sig : edfsignals) {
            for (int j=0;j<sig.sampleCnt;j++) {
                // The reserved field indicates if the channel is 8 or 16 bit.
                // 8bit channels can be both signed and unsigned
                if (sig.reserved == "#1") {
                    if (sig.digital_minimum >= 0)
                    {
                        sig.dataArray[recNo*sig.sampleCnt+j]=(qint16)Read8U();
                    }
                    else
                    {
                        sig.dataArray[recNo*sig.sampleCnt+j]=(qint16)Read8S();
                    }
                } else if (sig.reserved == "#2") {
                    qint16 t=Read16();
                    sig.dataArray[recNo*sig.sampleCnt+j]=t;
                }
            }
        }
    }
    return true;
}

qint8 WMEDFInfo::Read8S()
{
    if ((pos + 1) > datasize) {
        eof = true;
        return 0;
    }
    qint8 res = *(qint8 *)&signalPtr[pos];
    pos += 1;
    return res;
}

quint8 WMEDFInfo::Read8U()
{
    if ((pos + 1) > datasize) {
        eof = true;
        return 0;
    }
    quint8 res = *(quint8 *)&signalPtr[pos];
    pos += 1;
    return res;
}

//********************************************************************************************

void PrismaImport::run()
{
    qDebug() << "PRISMA IMPORT" << sessionid;

    if (!wmedf.Open(signalData)) {
        qWarning() << "Signal file open failed";
        return;
    }

    if (!wmedf.Parse()) {
        qWarning() << "Signal file parsing failed";
        return;
    }

    eventFile = new PrismaEventFile(eventData);

    startdate = qint64(wmedf.edfHdr.startdate_orig.toSecsSinceEpoch()) * 1000L;
    enddate = startdate + wmedf.GetDuration() * qint64(wmedf.GetNumDataRecords()) * 1000;

    session = loader->context()->CreateSession(sessionid);
    session->really_set_first(startdate);
    session->really_set_last(enddate);

    // TODO AXT: set physical limits from a config file
    session->setPhysMax(CPAP_Pressure, 20);
    session->setPhysMin(CPAP_Pressure, 4);
    session->setPhysMax(CPAP_IPAP, 20);
    session->setPhysMin(CPAP_IPAP, 4);
    session->setPhysMax(CPAP_EPAP, 20);
    session->setPhysMin(CPAP_EPAP, 4);

    // set session parameters
    auto parameters = eventFile->getParameters();

    // TODO AXT: extract
    if (parameters.contains(PRISMA_SMART_MODE)) {
        switch(parameters[PRISMA_SMART_MODE]) {
            case PRISMA_MODE_CPAP:
                session->settings[CPAP_Mode] = (int)MODE_CPAP;
                session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_CPAP;
            break;
            case PRISMA_MODE_APAP:
                session->settings[CPAP_Mode] = (int)MODE_APAP;
                switch (parameters[PRISMA_SMART_APAP_DYNAMIC])
                {
                    case PRISMA_APAP_MODE_STANDARD:
                        session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_STD;
                    break;
                    case PRISMA_APAP_MODE_DYNAMIC:
                        session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_DYN;
                    break;
                }

            break;
        }
        session->settings[CPAP_PressureMin] = parameters[PRISMA_SMART_PRESSURE] / 100.0;
        session->settings[CPAP_PressureMax] = parameters[PRISMA_SMART_PRESSURE_MAX] / 100.0;
        session->settings[Prisma_SoftPAP] = parameters[PRISMA_SMART_SOFTPAP];
        session->settings[Prisma_PSoft] = parameters[PRISMA_SMART_PSOFT] / 100.0;
        session->settings[Prisma_PSoft_Min] = parameters[PRISMA_SMART_PSOFT_MIN] / 100.0;
        session->settings[Prisma_AutoStart] = parameters[PRISMA_SMART_AUTOSTART];

        session->settings[Prisma_Softstart_Time] = parameters[PRISMA_SMART_SOFTSTART_TIME];
        session->settings[Prisma_Softstart_TimeMax] = parameters[PRISMA_SMART_SOFTSTART_TIME_MAX];

        if (parameters.contains(PRISMA_SMART_TUBE_TYPE)) {
            session->settings[Prisma_TubeType] = parameters[PRISMA_SMART_TUBE_TYPE] / 10.0;
        }

        session->settings[Prisma_PMaxOA] = parameters[PRISMA_SMART_PMAXOA] / 100;

        // TODO
        // session->settings[Prisma_HumidifierLevel] = parameters[PRISMA_SMART_HUMIDLEVEL];
    }

    bool found = true;
    if (parameters.contains(PRISMA_LINE_MODE)) {

        if (parameters[PRISMA_LINE_MODE] == PRISMA_MODE_AUTO_ST ||
            parameters[PRISMA_LINE_MODE] == PRISMA_MODE_AUTO_S) {

            if (parameters[PRISMA_LINE_EXTRA_OBSTRUCTION_PROTECTION] != 1) {
                if (parameters[PRISMA_LINE_AUTO_PDIFF] == 1) {
                    session->settings[CPAP_Mode] = (int)MODE_BILEVEL_AUTO_VARIABLE_PS;
                }else{
                    session->settings[CPAP_Mode] = (int)MODE_BILEVEL_AUTO_FIXED_PS;
                }
            }else{
                session->settings[CPAP_Mode] = (int)MODE_TRILEVEL_AUTO_VARIABLE_PDIFF;
            }

            session->settings[Prisma_BiSoft] = parameters[PRISMA_LINE_EXTRA_OBSTRUCTION_PROTECTION];
            session->settings[CPAP_EEPAPLo] = parameters[PRISMA_LINE_EEPAP_MIN] / 100.0;
            session->settings[CPAP_EEPAPHi] = parameters[PRISMA_LINE_EEPAP_MAX] / 100.0;
            session->settings[CPAP_EPAP] = parameters[PRISMA_LINE_EPAP] / 100.0;
            session->settings[CPAP_IPAP] = parameters[PRISMA_LINE_IPAP] / 100.0;
            session->settings[CPAP_IPAPHi] = parameters[PRISMA_LINE_IPAP_MAX] / 100.0;
            session->settings[CPAP_PSMin] = parameters[PRISMA_LINE_PDIFF_NORM] / 100.0;
            session->settings[CPAP_PSMax] = parameters[PRISMA_LINE_PDIFF_MAX] / 100.0;
            session->settings[Prisma_Softstart_Pressure] = parameters[PRISMA_LINE_SOFT_START_PRESS] / 100.0;
            session->settings[Prisma_Softstart_Time] = parameters[PRISMA_LINE_SOFT_START_TIME];
            session->settings[Prisma_AutoStart] = parameters[PRISMA_LINE_AUTOSTART];
            if (parameters.contains(PRISMA_SMART_TUBE_TYPE)) {
                session->settings[Prisma_TubeType] = parameters[PRISMA_LINE_TUBE_TYPE] / 10.0;
            }
            // Indicate partial support
            session->settings[Prisma_Warning] = 2;
        }

        switch(parameters[PRISMA_LINE_MODE]) {
            case PRISMA_MODE_AUTO_ST:
                // TODO AXT
                // Was not sure which mode this should be mapped, maybe we need to intorudce new modes
                // Setting/parameter mapping should be reviewed and tested
                // session->settings[CPAP_Mode] = (int)MODE_BILEVEL_AUTO_VARIABLE_PS; ???
                session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_AUTO_ST;
            break;

            case PRISMA_MODE_AUTO_S:
                session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_AUTO_S;
            break;

            case PRISMA_MODE_ACSV:
                // TODO AXT: its possible that based on PDIFF setting we should choose between MODE_ASV
                // and MODE_ASV_VARIABLE_EPAP here
                session->settings[CPAP_Mode] = (int)MODE_ASV;
                session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_ACSV;
                session->settings[CPAP_EEPAPLo] = parameters[PRISMA_LINE_EEPAP_MIN] / 100.0;
                session->settings[CPAP_EEPAPHi] = parameters[PRISMA_LINE_EEPAP_MAX] / 100.0;
                session->settings[CPAP_EPAP] = parameters[PRISMA_LINE_EPAP] / 100.0;
                session->settings[CPAP_IPAP] = parameters[PRISMA_LINE_IPAP] / 100.0;
                session->settings[CPAP_IPAPHi] = parameters[PRISMA_LINE_IPAP_MAX] / 100.0;
                session->settings[CPAP_PSMin] = parameters[PRISMA_LINE_PDIFF_NORM] / 100.0;
                session->settings[CPAP_PSMax] = parameters[PRISMA_LINE_PDIFF_MAX] / 100.0;
                session->settings[Prisma_Softstart_Time] = parameters[PRISMA_LINE_SOFT_START_TIME];
                session->settings[Prisma_Softstart_Pressure] = parameters[PRISMA_LINE_SOFT_START_PRESS] / 100.0;
                session->settings[Prisma_AutoStart] = parameters[PRISMA_LINE_AUTOSTART];
                if (parameters.contains(PRISMA_SMART_TUBE_TYPE)) {
                    session->settings[Prisma_TubeType] = parameters[PRISMA_LINE_TUBE_TYPE] / 10.0;
                }
                // Indicate partial support
                session->settings[Prisma_Warning] = 2;
            break;

            case PRISMA_MODE_APAP:
                session->settings[CPAP_Mode] = (int)MODE_APAP;
                switch (parameters[PRISMA_LINE_APAP_DYNAMIC])
                {
                    case PRISMA_APAP_MODE_STANDARD:
                        session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_STD;
                    break;
                    case PRISMA_APAP_MODE_DYNAMIC:
                        session->settings[Prisma_Mode] = (int)PRISMA_COMBINED_MODE_APAP_DYN;
                    break;
                }
                session->settings[CPAP_PressureMin] = parameters[PRISMA_LINE_EPAP] / 100.0;
                session->settings[CPAP_PressureMax] = parameters[PRISMA_LINE_IPAP] / 100.0;
                session->settings[Prisma_AutoStart] = parameters[PRISMA_LINE_AUTOSTART];
                session->settings[Prisma_SoftPAP] = parameters[PRISMA_LINE_SOFT_PAP_LEVEL];
                session->settings[Prisma_Softstart_Time] = parameters[PRISMA_LINE_SOFT_START_TIME];
                session->settings[Prisma_Softstart_Pressure] = parameters[PRISMA_LINE_SOFT_START_PRESS] / 100.0;
                if (parameters.contains(PRISMA_SMART_TUBE_TYPE)) {
                    session->settings[Prisma_TubeType] = parameters[PRISMA_LINE_TUBE_TYPE] / 10.0;
                }
            break;

            default:
                found = false;
            break;
        }


        if (!found) {
            // Indicate mode not supported
            session->settings[Prisma_Warning] = 1;
        }




    }

    // add waveforms
    // common waveforms, these exists on all prisma devices
    AddWaveform(CPAP_MaskPressure, QString("Pressure"));
    AddWaveform(CPAP_FlowRate, QString("RespFlow"));
    AddWaveform(CPAP_Leak, QString("LeakFlowBreath"));
    AddWaveform(Prisma_ObstructLevel, QString("ObstructLevel"));

    // prisma smart
    // waweforms specific for prisma smart / soft devices
    AddWaveform(CPAP_EPAP, QString("EPAP"));
    AddWaveform(CPAP_IPAP, QString("IPAP"));
    AddWaveform(Prisma_rMVFluctuation, QString("rMVFluctuation"));
    AddWaveform(Prisma_rRMV, QString("rRMV"));
    AddWaveform(Prisma_PressureMeasured, QString("PressureMeasured"));
    AddWaveform(Prisma_FlowFull, QString("FlowFull"));

    // The CPAPPressure exitst but is not used
    // AddWaveform(CPAP_Pressure, QString("CPAPPressure"));

    // prisma line
    AddWaveform(CPAP_EPAP, QString("EPAPsoll"));
    AddWaveform(CPAP_IPAP, QString("IPAPsoll"));
    AddWaveform(CPAP_EEPAP, QString("EEPAPsoll"));

    // Channels that exist on varous Prisma Line devices, but are not handled yet

    // AddWaveform(CPAP_RespRate, "BreathFrequency");
    // AddWaveform(CPAP_TidalVolume, "BreathVolume");
    // AddWaveform(CPAP_IE, "InspExpirRel");
    // AddWaveform(OXI_Pulse, "HeartFrequency");
    // AddWaveform(OXI_SPO2, "SpO2");
    // AddWaveform(CPAP_LeakTotal, QString("TotalLeakage"));
    // 20A, 25ST
    //  MV.txt
    //  rAMV.txt
    // 25S:
    //  rMVFluctuation.txt
    //  RSBI.txt
    //  TotalLeakage.txt


    // add signals
    AddEvents(CPAP_Obstructive, PRISMA_EVENT_OBSTRUCTIVE_APNEA);
    AddEvents(CPAP_ClearAirway, PRISMA_EVENT_CENTRAL_APNEA);
    AddEvents(CPAP_Apnea, { PRISMA_EVENT_APNEA_LEAKAGE, PRISMA_EVENT_APNEA_HIGH_PRESSURE, PRISMA_EVENT_APNEA_MOVEMENT});
    AddEvents(CPAP_Hypopnea, { PRISMA_EVENT_OBSTRUCTIVE_HYPOPNEA, PRISMA_EVENT_CENTRAL_HYPOPNEA});
    AddEvents(CPAP_RERA, PRISMA_EVENT_RERA);
    AddEvents(CPAP_VSnore, PRISMA_EVENT_SNORE);
    AddEvents(CPAP_CSR, PRISMA_EVENT_CS_RESPIRATION);
    AddEvents(CPAP_FlowLimit, PRISMA_EVENT_FLOW_LIMITATION);

    AddEvents(Prisma_Artifact, PRISMA_EVENT_ARTIFACT);
    AddEvents(Prisma_CriticalLeak, PRISMA_EVENT_CRITICAL_LEAKAGE);
    AddEvents(Prisma_eSO, PRISMA_EVENT_EPOCH_SEVERE_OBSTRUCTION);
    AddEvents(Prisma_eMO, PRISMA_EVENT_EPOCH_MILD_OBSTRUCTION);
    AddEvents(Prisma_eF, PRISMA_EVENT_EPOCH_FLOW_LIMITATION);
    AddEvents(Prisma_eS, PRISMA_EVENT_EPOCH_SNORE);
    AddEvents(Prisma_DeepSleep, PRISMA_EVENT_EPOCH_DEEPSLEEP);
    AddEvents(Prisma_TimedBreath, PRISMA_EVENT_TIMED_BREATH);

    session->SetChanged(true);
    loader->context()->AddSession(session);
}

void PrismaImport::AddWaveform(ChannelID code, QString edfLabel)
{
    EDFSignal * es = wmedf.lookupLabel(edfLabel);
    if (es != nullptr) {
        qint64 duration = wmedf.GetNumDataRecords() * wmedf.GetDuration() * 1000L;
        long recs = es->sampleCnt * wmedf.GetNumDataRecords();

        double rate = double(duration) / double(recs);
        EventList *a = session->AddEventList(code, EVL_Waveform, es->gain, es->offset, 0, 0, rate);
        a->setDimension(es->physical_dimension);
        a->AddWaveform(startdate, es->dataArray, recs, duration);

        session->setPhysMin(code, es->physical_minimum);
        session->setPhysMax(code, es->physical_maximum);
    }
}

void PrismaImport::AddEvents(ChannelID channel, QList<Prisma_Event_Type> eventTypes)
{
    EventList *eventList = nullptr;
    for (auto eventType : eventTypes) {
        QList<PrismaEvent> events = eventFile->getEvents(eventType);
        for (auto event: events) {
            if (eventList == nullptr) {
                eventList = session->AddEventList(channel, EVL_Event, 1.0, 0.0, 0.0, 0.0, 0.0, true);
            }
            eventList ->AddEvent(startdate + event.endTime(), event.duration(), event.strength());
        }
    }
    session->AddEventList(channel, EVL_Event);
}

//********************************************************************************************
PrismaEventFile::PrismaEventFile(QByteArray &buffer) {
    QDomDocument dom;
    dom.setContent(buffer);

    QDomElement root = dom.documentElement();
    QDomNodeList  deviceEventNodelist = root.elementsByTagName("DeviceEvent");
    for(int i=0; i < deviceEventNodelist.count(); i++)
    {
        QDomElement node=deviceEventNodelist.item(i).toElement();
        int eventId = node.attribute("DeviceEventID").toInt();
        if (eventId == 0) {
            int parameterId = node.attribute("ParameterID").toInt();
            int value = node.attribute("NewValue").toInt();
            m_parameters[parameterId] = value;
        }
    }

    QDomNodeList  respEventNodelist = root.elementsByTagName("RespEvent");
    for(int i=0; i < respEventNodelist.count(); i++)
    {
        QDomElement node=respEventNodelist.item(i).toElement();
        int eventId = node.attribute("RespEventID").toInt();
        const int time_quantum = 10;
        int endTime  = node.attribute("EndTime").toInt() * 1000 / time_quantum;
        int duration = node.attribute("Duration").toInt() / time_quantum;
        int pressure = node.attribute("Pressure").toInt();
        int strength = node.attribute("Strength").toInt();
        m_events[eventId].append(PrismaEvent(endTime, duration, pressure, strength));
    }
    qDebug() << "SIZE" << m_events.size();
}

//********************************************************************************************

// NOTE: was created for PrismaSmart, should be extended to support PrismaLines as they stabilize
struct PrismaTestedModel
{
    QString deviceId;
    const char* name;
};

static const PrismaTestedModel s_PrismaTestedModels[] = {
    { "0x92", "Prisma Smart" },
    { "0x91", "Prisma Soft" },
    { "", ""}
};

PrismaModelInfo s_PrismaModelInfo;

PrismaModelInfo::PrismaModelInfo ()
{
    for (int i = 0; !s_PrismaTestedModels[i].deviceId.isEmpty(); i++) {
        const PrismaTestedModel & model = s_PrismaTestedModels[i];
        m_modelNames[model.deviceId] = model.name;
    }
}

bool PrismaModelInfo::IsTested(const QString &  deviceId) const
{
    return m_modelNames.contains(deviceId);
};

const char* PrismaModelInfo::Name(const QString & deviceId) const
{
    const char* name;
    if (m_modelNames.contains(deviceId)) {
        name = m_modelNames[deviceId];
    } else {
        name = "Unknown Model";
    }
    return name;
};

//********************************************************************************************

PrismaLoader::PrismaLoader()
{
    m_type = MT_CPAP;
}

PrismaLoader::~PrismaLoader()
{
}

bool PrismaLoader::Detect(const QString & selectedPath)
{
    QFile prismaSmartConfigFile(selectedPath + QDir::separator() + PRISMA_SMART_CONFIG_FILE);
    QFile prismaLineConfigFile(selectedPath + QDir::separator() + PRISMA_LINE_CONFIG_FILE);
    return prismaSmartConfigFile.exists() || prismaLineConfigFile.exists();
}

int PrismaLoader::Open(const QString & selectedPath)
{
    if (m_ctx == nullptr) {
        qWarning() << "PrismaLoader::Open() called without a valid m_ctx object present";
        return 0;
    }
    Q_ASSERT(m_ctx);

    qDebug() << "Prisma opening" << selectedPath;


    QFile prismaSmartConfigFile(selectedPath + QDir::separator() + PRISMA_SMART_CONFIG_FILE);
    QFile prismaLineConfigFile(selectedPath + QDir::separator() + PRISMA_LINE_CONFIG_FILE);

    MachineInfo info = PeekInfoFromConfig(selectedPath);
    if (info.type == MT_UNKNOWN) {
        emit deviceIsUnsupported(info);
        return -1;
    }
    m_ctx->CreateMachineFromInfo(info);

    if (!s_PrismaModelInfo.IsTested(info.modelnumber)) {
        qDebug() << info.modelnumber << "untested";
        emit deviceIsUntested(info);
    }

    m_abort = false;
    emit setProgressValue(0);
    emit updateMessage(QObject::tr("Getting Ready..."));
    QCoreApplication::processEvents();

    emit updateMessage(QObject::tr("Backing Up Files..."));
    QCoreApplication::processEvents();


    QString backupPath = context()->GetBackupPath() + selectedPath.section("/", -1);
    if (QDir::cleanPath(selectedPath).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(selectedPath, backupPath);
    }

    emit updateMessage(QObject::tr("Scanning Files..."));
    QCoreApplication::processEvents();

    if (prismaSmartConfigFile.exists()) // TODO AXT || !configFile.isReadable() fails
    {
        // TODO AXT extract
        char out[12];
        int serialInDecimal;
        sscanf(info.serial.toLocal8Bit().data() , "%x", &serialInDecimal);
        snprintf(out, 12, "%010d", serialInDecimal);

        ScanFiles(info, selectedPath + QDir::separator() + out);
    }
    else if (prismaLineConfigFile.exists())
    {
        // TODO AXT: this is just a quick hack to load the zipped therapy files for the
        // Prisma Line devices. This should be extracted into a loader class, like the
        // PrismaEventFile. If this extraction is done, then loading the machine info
        // could become much easier.
        QSet<SessionID> sessions;
        QHash<SessionID, QString> eventFiles;
        QHash<SessionID, QString> signalFiles;


        QFile prismaLineTherapyFile(selectedPath + QDir::separator() + PRISMA_LINE_THERAPY_FILE);
        if (!prismaLineTherapyFile.exists()) { // TODO AXT || !configFile.isReadable() fails
            qDebug() << "Prisma line therapy file error" << prismaLineTherapyFile.fileName();
            return 0;
        }
        if (!prismaLineTherapyFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Prisma line therapy file not readable" << prismaLineTherapyFile.fileName();
            return 0;
        }
        QByteArray therapyData = prismaLineTherapyFile.readAll();
        prismaLineTherapyFile.close();

        mz_bool status;
        mz_zip_archive zip_archive;
        mz_zip_archive_file_stat file_stat;

        memset(&zip_archive, 0, sizeof(zip_archive));

        status = mz_zip_reader_init_mem(&zip_archive, (const void*)therapyData.constData(), therapyData.size(), 0);

        if (!status)
        {
            qDebug() <<  "mz_zip_reader_init_file() failed!";
            return 0;
        }

        int n = mz_zip_reader_get_num_files(&zip_archive);
        for (int i = 0; i < n; ++i) {
          if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
              qDebug() << "mz_zip_reader_file_stat() failed!";
              mz_zip_reader_end(&zip_archive);
              return 0;
          }
          qDebug() << file_stat.m_filename;

          QString fileName(file_stat.m_filename);
          if (fileName.contains("event_") && fileName.endsWith(".xml")) {
              SessionID sid = fileName.mid(fileName.size()-4-6,6).toLong();
              sessions += sid;
              eventFiles[sid] = fileName;
          }
          if (fileName.contains("signal_") && fileName.endsWith(".wmedf")) {
              SessionID sid = fileName.mid(fileName.size()-6-6,6).toLong();
              sessions += sid;
              signalFiles[sid] = fileName;
          }
        }
        qDebug() << sessions;

        for(auto & sid : sessions) {
            size_t uncomp_size_events;
            void *extract_events = mz_zip_reader_extract_file_to_heap(&zip_archive, eventFiles[sid].toLocal8Bit(), &uncomp_size_events, 0);
            QByteArray eventData((const char*)extract_events, uncomp_size_events);
            free(extract_events);
            size_t uncomp_size_signals;
            void *extract_signals = mz_zip_reader_extract_file_to_heap(&zip_archive, signalFiles[sid].toLocal8Bit(), &uncomp_size_signals, 0);
            QByteArray signalData((const char*)extract_signals, uncomp_size_signals);
            free(extract_signals);
            queTask(new PrismaImport(this, info,  sid, eventData, signalData));
        }
        mz_zip_reader_end(&zip_archive);
    } else {
        qDebug() << "Prisma config file error" << selectedPath;
        return 0;
    }


    int tasks = countTasks();
    qDebug() << "Task count " << tasks;
    emit updateMessage(QObject::tr("Importing Sessions..."));
    QCoreApplication::processEvents();

    runTasks(AppSetting->multithreading());

    m_ctx->FlushUnexpectedMessages();

    return tasks;
}

MachineInfo PrismaLoader::PeekInfo(const QString & selectedPath)
{
    qDebug() << "PeekInfo " << selectedPath;
    if (!Detect(selectedPath))
        return MachineInfo();

    return PeekInfoFromConfig(selectedPath + QDir::separator() + PRISMA_SMART_CONFIG_FILE);
}

MachineInfo PrismaLoader::PeekInfoFromConfig(const QString & selectedPath)
{
    QFile prismaSmartConfigFile(selectedPath + QDir::separator() + PRISMA_SMART_CONFIG_FILE);
    QFile prismaLineConfigFile(selectedPath + QDir::separator() + PRISMA_LINE_CONFIG_FILE);

    // TODO AXT, extract into ConfigFile class
    if (prismaSmartConfigFile.exists()) {
        if (!prismaSmartConfigFile.open(QIODevice::ReadOnly)) {
            return MachineInfo();
        }
        MachineInfo info = newInfo();
        QByteArray configData = prismaSmartConfigFile.readAll();
        prismaSmartConfigFile.close();

        QJsonDocument configDoc(QJsonDocument::fromJson(configData));
        QJsonObject  configObj = configDoc.object();
        QJsonObject  devObj = configObj["dev"].toObject();
        info.modelnumber=configObj["devid"].toString();
        info.model = s_PrismaModelInfo.Name(info.modelnumber);
        info.serial = devObj["sn"].toString();
        info.series = devObj["hwversion"].toString();
        // TODO AXT load propserties here,
        // we should use these to set the physical limits of the device
        info.properties["cica"] = "mica";
        return info;
    } else if (prismaLineConfigFile.exists()) {
        // TODO AXT prismaLine machine info loader not supported at all at this time
        // first extract the therapy data loader in PrismaLoader::Open(), it will help
        // to solve this issue too
        if (!prismaLineConfigFile.open(QIODevice::ReadOnly)) {
            return MachineInfo();
        }
        MachineInfo info = newInfo();
        prismaLineConfigFile.close();
        // info.modelnumber=42     error: conversion from 'int' to 'QChar' is ambiguous
        info.modelnumber="42";
        info.model = "Unknown PrismaLine";
        info.serial = "0x42424242";
        // TODO AXT load props
        info.properties["cica"] = "mica";
        return info;
    }
    return MachineInfo();
}

void PrismaLoader::ImportDataDir(QDir& dataDir, QSet<SessionID>& sessions, QHash<SessionID, QString>& eventFiles, QHash<SessionID, QString>& signalFiles) {
    dataDir.setFilter(QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks);
    dataDir.setSorting(QDir::Name);

    if (dataDir.exists()) {
        for (auto & inputFile : dataDir.entryInfoList()) {
            QString fileName = inputFile.fileName().toLower();
            if (fileName.startsWith("event_") && fileName.endsWith(".xml")) {
                SessionID sid = fileName.mid(6,fileName.size()-4-6).toLong();
                sessions += sid;
                eventFiles[sid] = inputFile.canonicalFilePath();
            }
            if (inputFile.fileName().toLower().startsWith("signal_") && inputFile.fileName().toLower().endsWith(".wmedf")) {
                SessionID sid = fileName.mid(7,fileName.size()-6-7).toLong();
                sessions += sid;
                signalFiles[sid] = inputFile.canonicalFilePath();
            }
        }
    }
}

// TODO AXT PrismaSmart specific, extract it into a parser class with the config files
void PrismaLoader::ScanFiles(const MachineInfo& info, const QString & machinePath)
{
    Q_ASSERT(m_ctx);
    qDebug() << "SCANFILES" << machinePath;

    QDir machineDir(machinePath);
    machineDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks);
    machineDir.setSorting(QDir::Name);
    QFileInfoList dayListing = machineDir.entryInfoList();

    QSet<SessionID> sessions;
    QHash<SessionID, QString> eventFiles;
    QHash<SessionID, QString> signalFiles;

    qint64 ignoreBefore = m_ctx->IgnoreSessionsOlderThan().toMSecsSinceEpoch()/1000;
    bool ignoreOldSessions = m_ctx->ShouldIgnoreOldSessions();

    qDebug() << "INFO  " << ignoreBefore << " " << ignoreOldSessions;

    for (auto & dayDirInfo : dayListing) {
        QDir dayDir(dayDirInfo.canonicalFilePath());
        dayDir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::NoSymLinks);
        dayDir.setSorting(QDir::Name);
        QFileInfoList subDirs = dayDir.entryInfoList();

        if (subDirs.size() == 0) {
            ImportDataDir(dayDir, sessions, eventFiles, signalFiles);
        } else {
            for (auto & dataDirInfo: subDirs) {
                QDir dataDir(dataDirInfo.canonicalFilePath());
                ImportDataDir(dataDir, sessions, eventFiles, signalFiles);
            }
        }
    }

    for(auto & sid : sessions) {
        QByteArray eventData;
        QByteArray signalData;

        QFile efile(eventFiles[sid]);
        if(efile.open(QIODevice::ReadOnly)) {
            eventData = efile.readAll();
            efile.close();
        }

        QFile sfile(signalFiles[sid]);
        if(sfile.open(QIODevice::ReadOnly)) {
            signalData = sfile.readAll();
            sfile.close();
        }

        queTask(new PrismaImport(this, info,  sid, eventData, signalData));
    }
}

using namespace schema;

void PrismaLoader::initChannels()
{
    Channel * chan = nullptr;
    channel.add(GRP_CPAP, chan = new Channel(Prisma_Mode=0xe400, SETTING,  MT_CPAP,  SESSION,
        "PrismaMode", QObject::tr("Mode"),
        QObject::tr("PAP Mode"),
        QObject::tr("Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(PRISMA_COMBINED_MODE_UNKNOWN, QObject::tr("UNKNOWN"));
    chan->addOption(PRISMA_COMBINED_MODE_CPAP, QObject::tr("CPAP"));
    chan->addOption(PRISMA_COMBINED_MODE_APAP_STD, QObject::tr("APAP (std)"));
    chan->addOption(PRISMA_COMBINED_MODE_APAP_DYN, QObject::tr("APAP (dyn)"));
    chan->addOption(PRISMA_COMBINED_MODE_AUTO_S, QObject::tr("Auto S"));
    chan->addOption(PRISMA_COMBINED_MODE_AUTO_ST, QObject::tr("Auto S/T"));
    chan->addOption(PRISMA_COMBINED_MODE_ACSV, QObject::tr("AcSV"));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_SoftPAP=0xe401, SETTING,  MT_CPAP,  SESSION,
        "Prisma_SoftPAP",
        QObject::tr("SoftPAP Mode"),
        QObject::tr("Pressure relief during exhalation"),
        QObject::tr("SoftPAP Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(Prisma_SoftPAP_OFF, QObject::tr("Off"));
    chan->addOption(Prisma_SoftPAP_SLIGHT, QObject::tr("Slight"));
    chan->addOption(Prisma_SoftPAP_STANDARD, QObject::tr("Standard"));

    channel.add(GRP_CPAP, new Channel(Prisma_PSoft=0xe402, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PSoft",
        QObject::tr("Softstart pressure"),
        QObject::tr("Pressure during soft start period"),
        QObject::tr("PSoft"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_PSoft_Min=0xe403, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PSoft_Min",
        QObject::tr("Softstart minimum pressure"),
        QObject::tr("Minimum pressure during soft start period"),
        QObject::tr("PSoftMin"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_AutoStart=0xe404, SETTING,  MT_CPAP,  SESSION,
        "Prisma_AutoStart",
        QObject::tr("Auto start"),
        QObject::tr("Automatically turn on the device by breathing"),
        QObject::tr("Auto start"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, new Channel(Prisma_Softstart_Time=0xe405, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Softstart_Time",
        QObject::tr("Softstart time"),
        QObject::tr("Lenght of soft start period"),
        QObject::tr("Softstart time"),
        STR_UNIT_Minutes, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_Softstart_TimeMax=0xe406, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Softstart_TimeMax",
        QObject::tr("Soft start maximum time"),
        QObject::tr("Maximum lenght of soft start period"),
        QObject::tr("Soft start max. time"),
        STR_UNIT_Minutes, LOOKUP, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_Softstart_Pressure=0xe407, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Softstart_Pressure",
        QObject::tr("Soft start pressure"),
        QObject::tr("Pressure during soft start period"),
        QObject::tr("Soft start pressure"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_PMaxOA=0xe408, SETTING,  MT_CPAP,  SESSION,
        "Prisma_PMaxOA",
        QObject::tr("PMaxOA"),
        QObject::tr("PMaxOA"),
        QObject::tr("PMaxOA"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(CPAP_EEPAPLo=0xe409, SETTING,  MT_CPAP,  SESSION,
        "CPAP_EEPAPLo",
        QObject::tr("EEPAPMin"),
        QObject::tr("Lower End Expiratory Pressure"),
        QObject::tr("EEPAPMin"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(CPAP_EEPAPHi=0xe40a, SETTING,  MT_CPAP,  SESSION,
        "CPAP_EEPAPHi",
        QObject::tr("EEPAPMax"),
        QObject::tr("Higher End Expiratory Pressure"),
        QObject::tr("EEPAPMax"),
        STR_UNIT_CMH2O, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_HumidifierLevel=0xe40b, SETTING,  MT_CPAP,  SESSION,
        "Prisma_HumidLevel",
        QObject::tr("Humidifier level"),
        QObject::tr("Humidifier level"),
        QObject::tr("Humidifier level"),
        "", DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(Prisma_TubeType=0xe40c, SETTING,  MT_CPAP,  SESSION,
        "Prisma_TubeType",
        QObject::tr("Tube type"),
        QObject::tr("Tube type"),
        QObject::tr("Tube type"),
        STR_UNIT_CM, DEFAULT, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_Warning=0xe40d, SETTING,  MT_CPAP,  SESSION,
        "Prisma_Warning",
        QObject::tr("Warning"),
        QObject::tr("Warning"),
        QObject::tr("Warning"),
        "", LOOKUP, Qt::green));
    chan->addOption(1, "Mode is not supported yet, please send sample data.");
    chan->addOption(2, "Mode partially supported, please send sample data.");


    channel.add(GRP_CPAP, chan = new Channel(Prisma_ObstructLevel=0xe440, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_ObstructLevel",
        QObject::tr("Obstruction level"),
        QObject::tr("Obstruction level in percentage"),
        QObject::tr("Obstruction level"),
        STR_UNIT_Percentage, DEFAULT, QColor("light purple")));
    chan->setUpperThreshold(100);
    chan->setLowerThreshold(0);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_rMVFluctuation=0xe441, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_rMVFluctuation",
        QObject::tr("rRMVFluctuation"),
        QObject::tr("Relative respiratory minute volume fluctuation"),
        QObject::tr("rRMVFluctuation"),
        STR_UNIT_Unknown, DEFAULT, QColor("light purple")));
    chan->setUpperThreshold(16);
    chan->setLowerThreshold(0);

    channel.add(GRP_CPAP, new Channel(Prisma_rRMV=0xe442, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_rRMV",
        QObject::tr("rRMV"),
        QObject::tr("Relative respiratory minute volume"),
        QObject::tr("rRMV"),
        STR_UNIT_Unknown, DEFAULT, QColor("light purple")));

    channel.add(GRP_CPAP, new Channel(Prisma_PressureMeasured=0xe443, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_PressureMeasured",
        QObject::tr("Measured pressure"),
        QObject::tr("Measured pressure"),
        QObject::tr("Measured pressure"),
        STR_UNIT_CMH2O, DEFAULT, QColor("black")));

    channel.add(GRP_CPAP, new Channel(Prisma_FlowFull=0xe444, WAVEFORM,  MT_CPAP,   SESSION,
        "Prisma_FlowFull",
        QObject::tr("Full flow"),
        QObject::tr("Full flow"),
        QObject::tr("Full flow"),
        STR_UNIT_Unknown, DEFAULT, QColor("black")));

    //Note: removed the channel, but keeping this code here, because of the channel id allocation, maybe we will bring it back in the future
    //channel.add(GRP_CPAP, new Channel(Prisma_SPRStatus=0xe445, WAVEFORM,  MT_CPAP,   SESSION,
    //    "Prisma_SPRStatus",
    //    QObject::tr("SPRStatus"),
    //    QObject::tr("SPRStatus"),
    //    QObject::tr("SPRStatus"),
    //    STR_UNIT_Unknown, DEFAULT, QColor("black")));


    channel.add(GRP_CPAP, new Channel(Prisma_Artifact=0xe446, SPAN,  MT_CPAP,   SESSION,
        "Prisma_Artifact",
        QObject::tr("Artefact"),
        QObject::tr("Irregularity in measured data, that doesn't represents a breathing event (e.g swallowing, coughing, or speaking)"),
        QObject::tr("ART"),
        STR_UNIT_Percentage, DEFAULT, QColor("salmon")));

    channel.add(GRP_CPAP, new Channel(Prisma_CriticalLeak = 0xe447, SPAN,  MT_CPAP,   SESSION,
        "Prisma_CriticalLeak",
        QObject::tr("CriticalLeak"),
        QObject::tr("Mask leakage is above a critical treshold"),
        QObject::tr("CL"),
        STR_UNIT_EventsPerHour, DEFAULT, QColor("orchid")));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eMO = 0xe448, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eMO",
        QObject::tr("eMO"),
        QObject::tr("Epoch (2 mins) with Mild Obstruction"),
        QObject::tr("eMO"),
        STR_UNIT_Percentage, DEFAULT, QColor("orange")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eSO = 0xe449, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eSO",
        QObject::tr("eSO"),
        QObject::tr("Epoch (2 mins) with Severe Obstruction"),
        QObject::tr("eSO"),
        STR_UNIT_Percentage, DEFAULT, QColor("red")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eS = 0xe44a, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eS",
        QObject::tr("eS"),
        QObject::tr("Epoch (2 mins) with Snoring"),
        QObject::tr("eS"),
        STR_UNIT_Percentage, DEFAULT, QColor("light green")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_eF = 0xe44b, SPAN,  MT_CPAP,   SESSION,
        "Prisma_eFL",
        QObject::tr("eFL"),
        QObject::tr("Epoch (2 mins) with Flow Limitation"),
        QObject::tr("eFL"),
        STR_UNIT_Percentage, DEFAULT, QColor("yellow")));
    chan->setEnabled(false);

    channel.add(GRP_CPAP, chan = new Channel(Prisma_DeepSleep = 0xe44c, SPAN,  MT_CPAP,   SESSION,
        "Prisma_DS",
        QObject::tr("Deep Sleep"),
        QObject::tr("Deep sleep, stable respiration"),
        QObject::tr("DS"),
        STR_UNIT_Percentage, DEFAULT, QColor("light blue")));
    chan->setEnabled(false);


    channel.add(GRP_CPAP, chan = new Channel(Prisma_TimedBreath = 0xe44d, FLAG,  MT_CPAP,   SESSION,
        "Prisma_TB",
        QObject::tr("Timed breath"),
        QObject::tr("Machine Initiated Breath"),
        QObject::tr("TB"),
        STR_UNIT_Percentage, DEFAULT, QColor("purple")));

    channel.add(GRP_CPAP, chan = new Channel(Prisma_BiSoft=0xe44e, SETTING,  MT_CPAP,  SESSION,
        "Prisma_BiSoft",
        QObject::tr("BiSoft Mode"),
        QObject::tr("BiSoft Mode"),
        QObject::tr("BiSoft Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(Prisma_BiSoft_Off, QObject::tr("Off"));
    chan->addOption(Prisma_BiSoft_1, QObject::tr("BiSoft 1"));
    chan->addOption(Prisma_BiSoft_2, QObject::tr("BiSoft 2"));
    chan->addOption(Prisma_TriLevel, QObject::tr("TriLevel"));

}

bool PrismaLoader::initialized = false;

void PrismaLoader::Register()
{
    if (initialized) { return; }

    qDebug() << "Registering PrismaLoader";
    RegisterLoader(new PrismaLoader());
    initialized = true;
}
