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
#include <QMessageBox>
#include "SleepLib/loader_plugins/bmcDataParsing.h"
#include "SleepLib/loader_plugins/bmc_loader.h"


ChannelID BMC_MODE, BMC_RESLEX, BMC_HUMIDIFIER, BMC_SMARTA, BMC_SMARTC, BMC_SMARTB,
    BMC_AUTO_ON, BMC_AUTO_OFF, BMC_LEAK_ALERT, BMC_AIRTUBE_TYPE, BMC_MASKTYPE,
    BMC_HEATEDTUBE_LEVEL, BMC_RAMPTIME, BMC_RAMPTIME_AUTO,
    BMC_INITIALP, BMC_TREATP, BMC_MANUALP,
    BMC_MIN_APAP, BMC_MAX_APAP, BMC_SENSITIVITY,
    BMC_INITIAL_EPAP, BMC_EPAP, BMC_IPAP, BMC_ISENS, BMC_ESENS, BMC_RISE_TIME, BMC_TI_MIN, BMC_TI_MAX, BMC_BACKUP_RR,
    BMC_MIN_EPAP, BMC_MIN_IPAP, BMC_MAX_IPAP, BMC_SMART_EPAP, BMC_SMART_MIN_EPAP, BMC_SMART_MIN_IPAP, BMC_SMART_MAX_IPAP;

ChannelID BMC_RESLEX_MODE, BMC_RESLEX_PATIENT;


const QDate baseDate(2010 , 1, 1);


/*
  Import Task - This is the primary part of the loader: where sessions are added to OSCAR
    with machine settings, waveforms etc.
  Each day recorded by BMC may have multiple sessions. This method takes in a
  a day and creates a OSCAR session for each session in the recorded day.
*/
void BmcLoaderTask::run()
{
    try
    {
        BmcDateSession bmcDateSession = bmc->ReadDateSession(this->bmcLink->UsrSession.StartTimestamp.date());

        for (int j = 0; j < bmcDateSession.Sessions.length(); j++)
        {
            emit bmcLoader->setProgressValue(this->currentLinkIndex);
            emit bmcLoader->updateMessage(QString("Import day %1 of %2\n%3")
                               .arg(this->currentLinkIndex+1)
                               .arg(this->totalLinksToImport)
                               .arg(this->bmcLink->UsrSession.StartTimestamp.date().toString("yyyy/MM/dd"))
                               );
            QCoreApplication::processEvents();

            BmcSession* bmcSession = bmcDateSession.Sessions.at(j);
            SessionID sessionID = (baseDate.daysTo(this->bmcLink->UsrSession.StartTimestamp.date()) * 64) + j; // leave space for N sessions.

            //We will always try to re-import the last day we import since it might have
            //been imported before noon which means a session could have been in progress.
            //Delete the session if it already exists before importing it again
            auto oscarDay = p_profile->GetDay(this->bmcLink->UsrSession.StartTimestamp.date(), MT_CPAP);
            if (oscarDay && oscarDay->hasMachine(mach))
            {
                auto oscarDaySessions = oscarDay->getSessions(MT_CPAP);
                auto oscarSessionToDelete = oscarDay->find(sessionID, MT_CPAP);
                if (oscarSessionToDelete)
                {
                    oscarDay->removeSession(oscarSessionToDelete);
                }
            }

            if (bmcSession->Waveforms.length() == 0)
                continue;

            //Import the session
            Session* session = new Session(mach, sessionID);

            bmcLoader->setSessionMachineSettings(&bmcDateSession, session);
            bmcLoader->setSessionRespiratoryEvents(bmcSession, session);
            bmcLoader->setSessionWaveforms(bmcSession, session);


            session->really_set_first(bmcSession->StartTimestamp.toMSecsSinceEpoch());
            session->really_set_last(bmcSession->EndTimestamp.addSecs(-1).toMSecsSinceEpoch());

            session->SetChanged(true);
            session->setNoSettings(false);
            session->UpdateSummaries();
            //session->StoreSummary();
            session->Store(mach->getDataPath());
            mach->AddSession(session);

            bmcLoader->sessionsLoaded++;
        }
    }
    catch (std::bad_alloc& e)
    {
        qDebug() << "Bad memory allocation while loading a BMC session";
        QMessageBox::warning(nullptr, QObject::tr("Import Error - Out of Memory"),
                             QObject::tr("Additional memory could not be allocated during the import.")+"\n\n"+
                             QObject::tr("Please try switching to 64-bit OSCAR or setting your preferences to ignore older sessions."),
                             QMessageBox::Ok);
        throw;
    }
    catch (...)
    {
        throw;
    }
}



/*
  Constructor. Tell OSCAR where to get icons for each machine series.
*/

BmcLoader::BmcLoader()
{
    const QString BMC_ICON = ":/icons/bmc.png";

    QString s = newInfo().series;
    m_pixmap_paths[s] = BMC_ICON;
    m_pixmaps[s] = QPixmap(BMC_ICON);
    m_type = MT_CPAP;
}

bool bmc_initialized = false;


//Given a created session, we add the BMC machine settings to the OSCAR session
void BmcLoader::setSessionMachineSettings(BmcDateSession* bmcSession, Session* oscarSession)
{
    BmcMachineSettings machineSettings = bmcSession->MacineSettings;

    oscarSession->settings[BMC_MODE] = (int)machineSettings.Mode;

    if (machineSettings.Mode == BmcMode::CPAP)
    {
        oscarSession->settings[CPAP_Mode] = (int)CPAPMode::MODE_CPAP;
        oscarSession->settings[CPAP_Pressure] = machineSettings.CPAP_TreatP;

        oscarSession->settings[BMC_SMARTC] = machineSettings.CPAP_SmartC;
        oscarSession->settings[BMC_INITIALP] = machineSettings.CPAP_InitialP;
        oscarSession->settings[BMC_TREATP] = machineSettings.CPAP_TreatP;
        oscarSession->settings[BMC_MANUALP] = machineSettings.CPAP_ManualP;

    }

    if (machineSettings.Mode == BmcMode::AutoCPAP)
    {
        oscarSession->settings[CPAP_Mode] = (int)CPAPMode::MODE_APAP;
        oscarSession->settings[CPAP_PressureMin] = machineSettings.APAP_MinAPAP;
        oscarSession->settings[CPAP_PressureMax] = machineSettings.APAP_MaxAPAP;

        oscarSession->settings[BMC_SMARTA] = machineSettings.APAP_SmartA ? 1 : 0;
        oscarSession->settings[BMC_INITIALP] = machineSettings.APAP_IntialP;
        oscarSession->settings[BMC_MIN_APAP] = machineSettings.APAP_MinAPAP;
        oscarSession->settings[BMC_MAX_APAP] = machineSettings.APAP_MaxAPAP;
        oscarSession->settings[BMC_SENSITIVITY] = machineSettings.APAP_Sensitivity;

    }

    if (machineSettings.Mode == BmcMode::S)
    {
        oscarSession->settings[CPAP_Mode] = (int)CPAPMode::MODE_BILEVEL_FIXED;
        oscarSession->settings[CPAP_EPAP] = machineSettings.S_EPAP;
        oscarSession->settings[CPAP_IPAP] = machineSettings.S_IPAP;
        oscarSession->settings[CPAP_PS] = 0;

        oscarSession->settings[BMC_INITIAL_EPAP] = machineSettings.S_InitialEPAP;
        oscarSession->settings[BMC_EPAP] = machineSettings.S_EPAP;
        oscarSession->settings[BMC_IPAP] = machineSettings.S_IPAP;
        oscarSession->settings[BMC_ISENS] = machineSettings.S_ISENS;
        oscarSession->settings[BMC_ESENS] = machineSettings.S_ESENS;
        oscarSession->settings[BMC_RISE_TIME] = machineSettings.S_RiseTime;
        oscarSession->settings[BMC_TI_MIN] = machineSettings.S_TiMin;
        oscarSession->settings[BMC_TI_MAX] = machineSettings.S_TiMax;
        oscarSession->settings[BMC_BACKUP_RR] = machineSettings.S_BackupRR ? 1 : 0;
    }

    if (machineSettings.Mode == BmcMode::AutoS)
    {
        oscarSession->settings[CPAP_Mode] = (int)CPAPMode::MODE_BILEVEL_AUTO_FIXED_PS;
        oscarSession->settings[CPAP_EPAPLo] = machineSettings.AutoS_MinEPAP;
        oscarSession->settings[CPAP_IPAPHi] = machineSettings.AutoS_MaxIPAP;
        oscarSession->settings[CPAP_PS] = 0;

        oscarSession->settings[BMC_SMARTB] = machineSettings.AutoS_SmartB ? 1 : 0;
        oscarSession->settings[BMC_INITIAL_EPAP] = machineSettings.AutoS_InitialEPAP;
        oscarSession->settings[BMC_MIN_EPAP] = machineSettings.AutoS_MinEPAP;
        oscarSession->settings[BMC_MIN_IPAP] = machineSettings.AutoS_MinIPAP;
        oscarSession->settings[BMC_MAX_IPAP] = machineSettings.AutoS_MaxIPAP;
        oscarSession->settings[BMC_ISENS] = machineSettings.AutoS_ISENS;
        oscarSession->settings[BMC_ESENS] = machineSettings.AutoS_ESENS;
        oscarSession->settings[BMC_RISE_TIME] = machineSettings.AutoS_RiseTime;
    }

    //Comfort settings common to all machines
    if (machineSettings.RampTimeMinutes == 0xff)
        oscarSession->settings[BMC_RAMPTIME_AUTO] = 0;
    else
        oscarSession->settings[BMC_RAMPTIME] = machineSettings.RampTimeMinutes;

    oscarSession->settings[BMC_RESLEX] = machineSettings.Reslex;
    oscarSession->settings[BMC_RESLEX_PATIENT] = machineSettings.ReslexPatient ? 1 : 0;
    oscarSession->settings[BMC_AUTO_ON] = machineSettings.AutoOn;
    oscarSession->settings[BMC_AUTO_OFF] = machineSettings.AutoOff;
    oscarSession->settings[BMC_HUMIDIFIER] = machineSettings.HumidifierLevel;
    oscarSession->settings[BMC_MASKTYPE] = (int)machineSettings.MaskType;
    oscarSession->settings[BMC_AIRTUBE_TYPE] = (int)machineSettings.AirTubeType;
    oscarSession->settings[BMC_LEAK_ALERT] = machineSettings.LeakAlert;

    if (machineSettings.AirTubeType == BmcAirTubeType::Heated15mm || machineSettings.AirTubeType == BmcAirTubeType::Heated22mm){
        oscarSession->settings[BMC_HEATEDTUBE_LEVEL] = machineSettings.HeatedTubeLevel;
    }

    oscarSession->settings[BMC_RESLEX_MODE] = 0;

}

//Given a created session, we add the BMC respiratory events to the OSCAR session
void BmcLoader::setSessionRespiratoryEvents(BmcSession* bmcSession, Session* oscarSession)
{
    EventList* oscarOsaList = oscarSession->AddEventList(CPAP_Obstructive, EVL_Event);
    EventList* oscarCsaList = oscarSession->AddEventList(CPAP_ClearAirway, EVL_Event);
    EventList* oscarHypList = oscarSession->AddEventList(CPAP_Hypopnea, EVL_Event);

    for (auto & bmcEvent : bmcSession->RespiratoryEvents)
    {
        switch (bmcEvent.EventType)
        {
            case BmcRespiratoryEventType::OSA: oscarOsaList->AddEvent(bmcEvent.EndTime.toMSecsSinceEpoch(), bmcEvent.DurationSeconds); break;
            case BmcRespiratoryEventType::CSA: oscarCsaList->AddEvent(bmcEvent.EndTime.toMSecsSinceEpoch(), bmcEvent.DurationSeconds); break;
            case BmcRespiratoryEventType::HYP: oscarHypList->AddEvent(bmcEvent.EndTime.toMSecsSinceEpoch(), bmcEvent.DurationSeconds); break;
            default: qDebug() << "Unknown BMC respiratory event type not added to OSCAR";

        }
    }
}

//Given a created session, we add the BMC waveforms for the session to the OSCAR session
void BmcLoader::setSessionWaveforms(BmcSession* bmcSession, Session* oscarSession)
{
    auto wPressure = oscarSession->AddEventList(CPAP_Pressure, EVL_Event, 0.5, 0.0, 0.0, 0.0, 1000);
    auto wIPAP = oscarSession->AddEventList(CPAP_EPAP, EVL_Event, 0.5, 0.0, 0.0, 0.0, 1000);
    auto wEPAP = oscarSession->AddEventList(CPAP_IPAP, EVL_Event, 0.5, 0.0, 0.0, 0.0, 1000);

    auto wFlow = oscarSession->AddEventList(CPAP_FlowRate, EVL_Waveform, 0.1, 0.0, 0.0, 0.0, 1000/25.0);
    auto wPressureWave = oscarSession->AddEventList(BMC_PressureWave, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, 1000/25.0);
    auto wFlowAbnormality = oscarSession->AddEventList(BMC_FlowAbnormality, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, 1000/25.0);

    auto wLeak = oscarSession->AddEventList(CPAP_Leak, EVL_Event, 0.1, 0.0, 0.0, 0.0, 1000);
    auto wTidalVolume = oscarSession->AddEventList(CPAP_TidalVolume, EVL_Event, 1.0, 0.0, 0.0, 0.0, 1000);
    auto wMinuteVentilation = oscarSession->AddEventList(CPAP_MinuteVent, EVL_Event, 0.1, 0.0, 0.0, 0.0, 1000);
    auto wRespiratoryRate = oscarSession->AddEventList(CPAP_RespRate, EVL_Event, 1.0, 0.0, 0.0, 0.0, 1000);
    auto wIEValue = oscarSession->AddEventList(CPAP_IE, EVL_Event, 0.001, 0.0, 0.0, 0.0, 1000);
    auto wIERatio = oscarSession->AddEventList(BMC_IE_Ratio, EVL_Event, 0.1, 0.0, 0.0, 0.0, 1000);


    auto wInspTime = oscarSession->AddEventList(CPAP_Ti, EVL_Event, 0.001, 0.0, 0.0, 0.0, 1000);
    auto wExpTime = oscarSession->AddEventList(CPAP_Te, EVL_Event, 0.001, 0.0, 0.0, 0.0, 1000);


    for (auto & bmcWaveform : bmcSession->Waveforms)
    {
        qint64 timestamp = bmcWaveform.Timestamp.toMSecsSinceEpoch();

        wFlow->AddWaveform(timestamp, bmcWaveform.Raw.Flow, 25, 1000);
        wPressureWave->AddWaveform(timestamp, bmcWaveform.Raw.PressureWave, 25, 1000);
        wFlowAbnormality->AddWaveform(timestamp, bmcWaveform.Raw.FlowAbnormality, 25, 1000);

        wPressure->AddEvent(timestamp, bmcWaveform.Raw.IPAP);
        wIPAP->AddEvent(timestamp, bmcWaveform.Raw.IPAP);
        wEPAP->AddEvent(timestamp, bmcWaveform.Raw.EPAP);
        wLeak->AddEvent(timestamp, bmcWaveform.Raw.Leak);
        wTidalVolume->AddEvent(timestamp, bmcWaveform.Raw.TidalVolume);
        wMinuteVentilation->AddEvent(timestamp, bmcWaveform.Raw.MinuteVentilation);
        wRespiratoryRate->AddEvent(timestamp, bmcWaveform.Raw.RespiratoryRate);
        wIEValue->AddEvent(timestamp, (qint16)(1000*(1000 - bmcWaveform.Raw.IERatioMapped) / (double)bmcWaveform.Raw.IERatioMapped)); //Map the percentage IE Ratio to a IE value e.g. 1:2 for 33%
        wIERatio->AddEvent(timestamp, bmcWaveform.Raw.IERatioMapped);

        //We add the inspiration and expiration waveform so that OSCAR doesn't try to calculcate them
        //OSCAR searched for peaks and throughs in the flow waveform, but since BMC's flow waveform
        //doesn't have a zero crossing (i.e. there is a y-offset), it can't do so reliably.
        //Instead, we can accurately calculate the I and E times from two parameters BMC does record:
        //the respiratory rate and the IE ratio

        if (bmcWaveform.RespiratoryRate > 0)
        {
            double respTime = (60.0 / bmcWaveform.RespiratoryRate);
            double inspTime = respTime * (bmcWaveform.Raw.IERatioMapped / 1000.0);
            double expTime = respTime * ((1000 - bmcWaveform.Raw.IERatioMapped) / 1000.0);
            wInspTime->AddEvent(timestamp, (qint16)(inspTime * 1000));
            wExpTime->AddEvent(timestamp, (qint16)(expTime * 1000));
        }

    }
}



//****************************************************************************************
//* All below are implementations of virtual methods of MachineLoader and derived CpapLoader
//****************************************************************************************

/*
  Base Class Implementation. Register this loader with OSCAR
*/

void BmcLoader::Register()
{
    if (bmc_initialized) { return; }

    qDebug() << "Registering BMC Loader";
    RegisterLoader(new BmcLoader());

    bmc_initialized = true;
}

/*
  Base Class Implementation. Checks if a path contains data that this loader can import
*/
bool BmcLoader::Detect(const QString & givenpath)
{
    QDir dir(givenpath);

    if (!dir.exists()) {
        return false;
    }

    bool hasBmcData = BmcData::DirectoryHasBmcData(givenpath);

    return hasBmcData;
}

/*
  Base Class Implementation. While importing data, the machine's info and icon is displayed
*/
MachineInfo BmcLoader::PeekInfo(const QString & path)
{
    if (!Detect(path)) {
        return MachineInfo();
    }

    BmcData bmc(path);
    auto bmcMachineInfo = bmc.ReadMachineInfo();

    MachineInfo info = newInfo();
    info.type = MachineType::MT_CPAP;
    info.brand = "BMC";
    info.model = bmcMachineInfo.Model;
    info.modelnumber = bmcMachineInfo.Model;
    info.series = "BMC";
    info.serial = bmcMachineInfo.SerialNumber;
    info.version = bmc_version;

    return info;
}

/*
  Base Class Implementation. Create all the settings that will be displayed to the user in "Device Settings"
*/
void BmcLoader::initChannels()
{
    using namespace schema;

    int BMC_CHANNEL_IDX = 0xe930;

    int channelIdx = BMC_CHANNEL_IDX;

    //Mode
    //---------------------------------------------------------------------------
    Channel * chan = new Channel(BMC_MODE = channelIdx++ , SETTING, MT_CPAP, SESSION,
            "BMC_Mode", QObject::tr("BMC Mode"), QObject::tr("BMC Mode"), QObject::tr("BMC Mode"), "", LOOKUP, Qt::green);
    channel.add(GRP_CPAP, chan);
    chan->addOption(0, QObject::tr("CPAP"));
    chan->addOption(1, QObject::tr("AutoCPAP"));
    chan->addOption(2, QObject::tr("S"));
    chan->addOption(3, QObject::tr("S/T"));
    chan->addOption(4, QObject::tr("T"));
    chan->addOption(5, QObject::tr("Titration"));
    chan->addOption(6, QObject::tr("AutoS"));
    chan->addOption(7, QObject::tr("Unknown"));


    channel.add(GRP_CPAP, chan = new Channel(BMC_RESLEX = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "Reslex", QObject::tr("Reslex"), QObject::tr("BMC Reslex is an exhalation pressure relief feature"), QObject::tr("Reslex"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("1"));
    chan->addOption(2, QObject::tr("2"));
    chan->addOption(3, QObject::tr("3"));
    chan->addOption(4, QObject::tr("Patient"));


    channel.add(GRP_CPAP, chan = new Channel(BMC_RESLEX_MODE = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "ReslexMode", QObject::tr("Reslex Mode"), QObject::tr("Reslex Mode"), QObject::tr("Reslex Mode"), "", LOOKUP, Qt::green));
    chan->addOption(0, "Full Time");

    channel.add(GRP_CPAP, chan = new Channel(BMC_HUMIDIFIER = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "Humidifier", QObject::tr("Humidifier"), QObject::tr("Humidifier"), QObject::tr("Humidifier"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("1"));
    chan->addOption(2, QObject::tr("2"));
    chan->addOption(3, QObject::tr("3"));
    chan->addOption(4, QObject::tr("4"));
    chan->addOption(5, QObject::tr("5"));
    chan->addOption(6, STR_TR_Auto);

    channel.add(GRP_CPAP, chan = new Channel(BMC_SMARTA = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "SmartA", QObject::tr("SmartA"), QObject::tr("SmartA"), QObject::tr("SmartA"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_SMARTB = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "SmartB", QObject::tr("SmartB"), QObject::tr("SmartB"), QObject::tr("SmartB"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_SMARTC = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "SmartC", QObject::tr("SmartC"), QObject::tr("SmartC"), QObject::tr("SmartC"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_AUTO_ON = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "AutoOn", QObject::tr("Auto On"), QObject::tr("Auto On"), QObject::tr("Auto On"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_AUTO_OFF = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "AutoOff", QObject::tr("Auto Off"), QObject::tr("Auto Off"), QObject::tr("Auto Off"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_LEAK_ALERT = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "LeakAlert", QObject::tr("Leak Alert"), QObject::tr("Leak Alert"), QObject::tr("Leak Alert"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_AIRTUBE_TYPE = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "AirTubeType", QObject::tr("Air Tube Type"), QObject::tr("Air Tube Type"), QObject::tr("Air Tube Type"), "", LOOKUP, Qt::green));
    chan->addOption(0, QObject::tr("Normal 22mm"));
    chan->addOption(1, QObject::tr("Normal 15mm"));
    chan->addOption(2, QObject::tr("Heated 22mm"));
    chan->addOption(3, QObject::tr("Heated 22mm"));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MASKTYPE = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "MaskType", QObject::tr("Mask"), QObject::tr("Mask"), QObject::tr("Mask"), "", LOOKUP, Qt::green));
    chan->addOption(0, QObject::tr("Full Face"));
    chan->addOption(1, QObject::tr("Nasal"));
    chan->addOption(2, QObject::tr("Nasal Pillows"));
    chan->addOption(3, QObject::tr("Unknown"));


    channel.add(GRP_CPAP, chan = new Channel(BMC_HEATEDTUBE_LEVEL = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "HeatedTubeLevel", QObject::tr("Heated Tube Level"), QObject::tr("Heated Tube Level"), QObject::tr("Heated Tube Level"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("1"));
    chan->addOption(2, QObject::tr("2"));
    chan->addOption(3, QObject::tr("3"));
    chan->addOption(4, QObject::tr("4"));
    chan->addOption(5, QObject::tr("5"));
    chan->addOption(6, STR_TR_Auto);


    channel.add(GRP_CPAP, chan = new Channel(BMC_RAMPTIME = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcRampTime", QObject::tr("BmcRampTime"), QObject::tr("Ramp Time "), QObject::tr("Ramp Time "), STR_UNIT_Minutes, INTEGER, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_RAMPTIME_AUTO = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcRampAuto", QObject::tr("BmcRampAuto"), QObject::tr("Ramp Time "), QObject::tr("Ramp Time "), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Auto);


    channel.add(GRP_CPAP, chan = new Channel(BMC_INITIALP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "InitialP", QObject::tr("InitialP"), QObject::tr("Initial P"), QObject::tr("Initial P"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_TREATP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "TreatP", QObject::tr("TreatP"), QObject::tr("Treat P"), QObject::tr("Treat P"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MANUALP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "ManualP", QObject::tr("ManualP"), QObject::tr("Manual P"), QObject::tr("Manual P"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MIN_APAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "MinAPAP", QObject::tr("Min APAP"), QObject::tr("Min APAP"), QObject::tr("Min APAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MAX_APAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "MaxAPAP", QObject::tr("Max APAP"), QObject::tr("Max APAP"), QObject::tr("Max APAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_SENSITIVITY = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "Sensitivity", QObject::tr("Sensitivity"), QObject::tr("Sensitivity"), QObject::tr("Sensitivity"), "", INTEGER, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_INITIAL_EPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "InitialEPAP", QObject::tr("Initial EPAP"), QObject::tr("Initial EPAP"), QObject::tr("Initial EPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_EPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcEPAP", QObject::tr("EPAP"), QObject::tr("EPAP"), QObject::tr("EPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_IPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcIPAP", QObject::tr("IPAP"), QObject::tr("IPAP"), QObject::tr("IPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_ISENS = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "ISens", QObject::tr("ISens"), QObject::tr("I Sens"), QObject::tr("I Sens"), "", INTEGER, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_ESENS = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "ESens", QObject::tr("ESens"), QObject::tr("E Sens"), QObject::tr("E Sens"), "", INTEGER, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_RISE_TIME = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "RiseTime", QObject::tr("RiseTime"), QObject::tr("Rise Time"), QObject::tr("Rise Time"), STR_UNIT_Seconds, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_TI_MIN = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "TiMin", QObject::tr("TiMin"), QObject::tr("Ti Min"), QObject::tr("Ti Min"), STR_UNIT_Seconds, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_TI_MAX = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "TiMax", QObject::tr("TiMax"), QObject::tr("Ti Max"), QObject::tr("Ti Max"), STR_UNIT_Seconds, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_BACKUP_RR = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BackupRR", QObject::tr("BackupRR"), QObject::tr("Backup RR"), QObject::tr("Backup RR"), "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(BMC_MIN_EPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "MinEPAP", QObject::tr("MinEPAP"), QObject::tr("Min EPAP"), QObject::tr("Min EPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MIN_IPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "MinIPAP", QObject::tr("MinIPAP"), QObject::tr("Min IPAP"), QObject::tr("Min IPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MAX_IPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcMaxIPAP", QObject::tr("MaxIPAP"), QObject::tr("Max IPAP"), QObject::tr("Max IPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));

    channel.add(GRP_CPAP, chan = new Channel(BMC_MAX_IPAP = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "BmcMaxIPAP", QObject::tr("MaxIPAP"), QObject::tr("Max IPAP"), QObject::tr("Max IPAP"), STR_UNIT_CMH2O, DOUBLE, Qt::green));


    channel.add(GRP_CPAP, chan = new Channel(BMC_RESLEX_PATIENT = channelIdx++, SETTING, MT_CPAP,   SESSION,
            "ReslexAvailability", QObject::tr("Reslex Availability"), QObject::tr("Reslex setting can be restricted to only clinician menu or may be made available for the user to change"), QObject::tr("Reslex Availability"), "", LOOKUP, Qt::green));
    chan->addOption(0, QObject::tr("Clinician"));
    chan->addOption(1, QObject::tr("Patient"));
}

/*
  Base Class Implementation. Returns various channels and names for mapping in OSCAR.
*/

QString BmcLoader::PresReliefLabel() { return QString("Reslex"); }
ChannelID BmcLoader::PresReliefMode() { return BMC_RESLEX_MODE; }
ChannelID BmcLoader::PresReliefLevel() { return BMC_RESLEX; }
ChannelID BmcLoader::CPAPModeChannel() { return BMC_MODE; }

/*
  Base Class Implementation. "Open" is called to import all data.
  The loader must create the machine, add new sessions with settings, waveforms etc to the profile
  Perform a backup of the SD card if enabled
  The methodology was changed to allow for multithreaded importing so the importing of sessions
  is now added as tasks which are all run by OSCAR's machine class instance.
*/
int BmcLoader::Open(const QString & dirpath)
{
    this->sessionsLoaded = 0;


    //#region Open the BMC data files and ready out
    //******************************************************************************
    QCoreApplication::processEvents();
    emit updateMessage(QObject::tr("Reading data..."));
    QCoreApplication::processEvents();

    const auto machine_info = PeekInfo(dirpath);
    BmcData bmc(dirpath);
    bmc.ReadData();

    //******************************************************************************
    //#endregion



    //#region Find or create the OSCAR machine and determine the date to import from
    //******************************************************************************

    QCoreApplication::processEvents();
    emit updateMessage(QObject::tr("Find sessions to import..."));
    QCoreApplication::processEvents();

    QDate firstImportDay = QDate(2000,1,1);

    Machine *mach = p_profile->lookupMachine(machine_info.serial, machine_info.loadername);
    if ( mach ) {       // we have seen this device
        qDebug() << "We have imported data for this machine before";
        mach->setInfo( machine_info );                      // update info
        QDate lastDate = mach->LastDay();           // use the last day for this device
        firstImportDay = lastDate;                  // re-import the last day, to  pick up partial days
        QDate purgeDate = mach->purgeDate();
        if (purgeDate.isValid()) {
            firstImportDay = min(firstImportDay, purgeDate);
        }
    } else {            // Starting from new beginnings - new or purged
        qDebug() << "We haven't imported data for this machine before";
        mach = p_profile->CreateMachine( machine_info );
    }
    QDateTime ignoreBefore = p_profile->session->ignoreOlderSessionsDate();
    bool ignoreOldSessions = p_profile->session->ignoreOlderSessions();

    if (ignoreOldSessions && (ignoreBefore.date() > firstImportDay))
        firstImportDay = ignoreBefore.date();
    qDebug() << "First day to import: " << firstImportDay.toString();
    //******************************************************************************
    //#endregion


    //#region Create backup
    //******************************************************************************

    emit updateMessage(QObject::tr("Creating data backup..."));
    QCoreApplication::processEvents();

    QString backupPath = mach->getBackupPath();
    QDir backupDir(backupPath);
    if (backupDir.exists(backupPath))
        backupDir.removeRecursively();

    backupDir.mkpath(backupPath);

    copyPath(dirpath, backupPath);

    //******************************************************************************
    //#endregion


    //#region Determine the set of sessions to import and set the progress bar values
    //******************************************************************************

    QList<BmcDataLink> linksToImport;
    for (auto & link : bmc.SessionLinks)
    {
        if (link.UsrSession.StartTimestamp.date() >= firstImportDay){
            linksToImport.append(link);
        }
    }

    emit updateMessage(QObject::tr("Starting import..."));
    emit setProgressMax(linksToImport.length());    // add one to include Save in progress.
    emit setProgressValue(0);
    QCoreApplication::processEvents();

    //32-bit users import an entire SD card of data may get a bad memory allocation exception
    //while OSCAR tries to store the session.
    /*if (linksToImport.length() > 10 && QSysInfo::WordSize == 32){
        QMessageBox::warning(nullptr, QObject::tr("Import Warning"),
                             QObject::tr("You are about to import a large number of sessions and may run out of memory.")+"\n\n"+
                             QObject::tr("Please try switching to 64-bit OSCAR or setting your preferences to ignore older sessions."),
                             QMessageBox::Ok);
    }*/


    //******************************************************************************
    //#endregion

    // For each BMC session, create and queue an import task that will transform
    // our BMC session into an OSCAR session.
    for (int i = 0; i < linksToImport.length(); i++){

        BmcDataLink *link;
        link = (BmcDataLink*)&linksToImport.at(i);

        queTask(new BmcLoaderTask(this, mach, &bmc, link, linksToImport.length(), i));
    }


    runTasks();

    //When everything is imported, we can finish up and return the number of new sessions imported.

    mach->Save();

    QCoreApplication::processEvents();

    return this->sessionsLoaded;
}

