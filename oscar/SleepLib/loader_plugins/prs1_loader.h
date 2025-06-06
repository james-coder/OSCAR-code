/* SleepLib PRS1 Loader Header
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PRS1LOADER_H
#define PRS1LOADER_H
#include "SleepLib/machine_loader.h"

#if defined(UNITTEST_MODE) || defined(UNITTEST_MODE_PRS1)
#define private public
#define protected public
#endif

//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation
// BEFORE making a release
const int prs1_data_version = 21;
//
//********************************************************************************************
const QString prs1_class_name = STR_MACH_PRS1;

QString ts(qint64 msecs);

/*! \struct PRS1Waveform
    \brief Used in PRS1 Waveform Parsing */
struct PRS1Waveform {
    PRS1Waveform(quint16 i, quint8 f) {
        interleave = i;
        sample_format = f;
    }
    quint16 interleave;
    quint8 sample_format;
};

class PRS1DataChunk;
class PRS1ParsedEvent;

class PRS1Loader;

/*! \class PRS1Import
 *  \brief Contains the functions to parse a single session... multithreaded */
class PRS1Import:public ImportTask
{
public:
    PRS1Import(PRS1Loader * l, SessionID s, int base): loader(l), sessionid(s), m_sessionid_base(base) {
        summary = nullptr;
        compliance = nullptr;
        session = nullptr;
        m_currentSliceInitialized = false;
    }
    virtual ~PRS1Import();

    //! \brief PRS1Import thread starts execution here.
    virtual void run();

    PRS1DataChunk * compliance;
    PRS1DataChunk * summary;
    QMap<qint64,PRS1DataChunk *> m_event_chunks;
    QList<PRS1DataChunk *> waveforms;
    QList<PRS1DataChunk *> oximetry;


    QList<QString> m_wavefiles;
    QList<QString> m_oxifiles;

    //! \brief Imports .000 files for bricks.
    bool ImportCompliance();

    //! \brief Imports the .001 summary file.
    bool ImportSummary();

    //! \brief Imports the .002 event file(s).
    bool ImportEvents();

    //! \brief Reads the .005 or .006 waveform file(s).
    QList<PRS1DataChunk *> ReadWaveformData(QList<QString> & files, const char* label);

    //! \brief Coalesce contiguous .005 or .006 waveform chunks from the file into larger chunks for import.
    QList<PRS1DataChunk *> CoalesceWaveformChunks(QList<PRS1DataChunk *> & allchunks);

    //! \brief Takes the parsed list of Flow/MaskPressure waveform chunks and adds them to the database
    void ImportWaveforms();

    //! \brief Takes the parsed list of oximeter waveform chunks and adds them to the database.
    void ImportOximetry();

    //! \brief Adds a single channel of continuous oximetry data to the database, splitting on any missing samples.
    void ImportOximetryChannel(ChannelID channel, QByteArray & data, quint64 ti, qint64 dur);


protected:
    Session * session;
    PRS1Loader * loader;
    SessionID sessionid;
    QHash<ChannelID,EventList*> m_importChannels;  // map channel ID to the session's current EventList*

    int summary_duration;
    int m_sessionid_base;  // base for inferring session ID from filename

    //! \brief Translate the PRS1-specific device mode to the importable vendor-neutral enum.
    CPAPMode importMode(int mode);
    //! \brief Parse all the chunks in a single device session
    bool ParseSession(void);

    //! \brief Cache a single slice from a summary or compliance chunk.
    void AddSlice(qint64 chunk_start, PRS1ParsedEvent* e);
    QVector<SessionSlice> m_slices;

    //! \brief Import a single event from a data chunk.
    void ImportEvent(qint64 t, PRS1ParsedEvent* event);
    // State that needs to persist between individual events:
    EventDataType m_currentPressure;
    bool m_calcPSfromSet;

    //! \brief Advance the current mask-on slice if needed and update import data structures accordingly.
    bool UpdateCurrentSlice(PRS1DataChunk* chunk, qint64 t);
    bool m_currentSliceInitialized;
    QVector<SessionSlice>::const_iterator m_currentSlice;
    qint64 m_statIntervalStart, m_prevIntervalStart;
    QList<PRS1ParsedEvent*> m_lastIntervalEvents;
    qint64 m_lastIntervalEnd;
    EventDataType m_intervalPressure;

    //! \brief Write out any pending end-of-slice events.
    void FinishSlice();
    //! \brief Record the beginning timestamp of a new stat interval, and do related housekeeping.
    void StartNewInterval(qint64 t);
    //! \brief Identify statistical events that are reported at the end of an interval.
    bool IsIntervalEvent(PRS1ParsedEvent* e);

    //! \brief Import a single data chunk from a .002 file containing event data.
    bool ImportEventChunk(PRS1DataChunk* event);
    //! \brief Create all supported channels (except for on-demand ones that only get created if an event appears).
    void CreateEventChannels(const PRS1DataChunk* event);
    //! \brief Get the EventList* for the import channel, creating it if necessary.
    EventList* GetImportChannel(ChannelID channel);
    //! \brief Import a single event to a channel, creating the channel if necessary.
    void AddEvent(ChannelID channel, qint64 t, float value, float gain);
};

/*! \class PRS1Loader
    \brief Philips Respironics System One Loader Module
    */
class PRS1Loader : public CPAPLoader
{
    Q_OBJECT
  public:
    PRS1Loader();
    virtual ~PRS1Loader();
    
    //! \brief Peek into PROP.TXT or properties.txt at given path, and return it as a normalized key/value hash
    bool PeekProperties(const QString & filename, QHash<QString,QString> & props);
    
    //! \brief Peek into PROP.TXT or properties.txt at given path, and use it to fill MachineInfo structure
    bool PeekProperties(MachineInfo & info, const QString & path);

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Wrapper for PeekProperties that creates the MachineInfo structure.
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans directory path for valid PRS1 signature
    virtual int Open(const QString & path);

    //! \brief Returns the database version of this loader
    virtual int Version() { return prs1_data_version; }

    //! \brief Return the loaderName, in this case "PRS1"
    virtual const QString &loaderName() { return prs1_class_name; }

    //! \brief Parse a PRS1 summary/event/waveform file and break into invidivual session or waveform chunks
    QList<PRS1DataChunk *> ParseFile(const QString & path);

    //! \brief Register this Module to the list of Loaders, so it knows to search for PRS1 data.
    static void Register();

    //! \brief Generate a generic MachineInfo structure, with basic PRS1 info to be expanded upon.
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, prs1_class_name, QObject::tr("Philips Respironics"), QString(), QString(), QString(), QObject::tr("System One"), QDateTime::currentDateTime(), prs1_data_version);
    }


    virtual QString PresReliefLabel();
    //! \brief Returns the PRS1 specific code for Pressure Relief Mode
    virtual ChannelID PresReliefMode();
    //! \brief Returns the PRS1 specific code for Pressure Relief Setting
    virtual ChannelID PresReliefLevel();
    //! \brief Returns the PRS1 specific code for PAP mode
    virtual ChannelID CPAPModeChannel();

    //! \brief Returns the PRS1 specific code for Humidifier Connected
    virtual ChannelID HumidifierConnected();
    //! \brief Returns the PRS1 specific code for Humidifier Level
    virtual ChannelID HumidifierLevel();

    //! \brief Called at application init, to set up any custom PRS1 Channels
    void initChannels();


    QHash<SessionID, PRS1Import*> sesstasks;

  protected:
    //! \brief Returns the path of the P-Series folder (whatever case) if present on the card
    QString GetPSeriesPath(const QString & path);

    //! \brief Returns the path for each device detected on an SD card, from oldest to newest
    QStringList FindMachinesOnCard(const QString & cardPath);

    //! \brief Opens the SD folder structure for this device, scans for data files and imports any new sessions
    int OpenMachine(const QString & path);

    //! \brief Finds the P0,P1,... session paths and property pathname and returns the base (10 or 16) of the session filenames
    int FindSessionDirsAndProperties(const QString & path, QStringList & paths, QString & propertyfile);

    //! \brief Reads the model number from the property file, evaluates its capabilities, and returns true if the device is supported
    bool CreateMachineFromProperties(QString propertyfile);

    //! \brief Scans the given directories for session data and create an import task for each logical session.
    void ScanFiles(const QStringList & paths, int sessionid_base);
    
    //! \brief PRS1 Data files can store multiple sessions, so store them in this list for later processing.
    QHash<SessionID, Session *> new_sessions;
    
    //! \brief DS2 key derivation is very slow, but keys are reused in multiple files, so we cache the derived keys.
    QHash<QByteArray, QByteArray> m_keycache;
};


//********************************************************************************************

class PRS1ModelInfo
{
protected:
    QHash<int, QHash<int, QStringList>> m_testedModels;
    QHash<QString,const char*> m_modelNames;
    QSet<QString> m_bricks;
    
public:
    PRS1ModelInfo();
    bool IsSupported(const QHash<QString,QString> & properties) const;
    bool IsSupported(int family, int familyVersion) const;
    bool IsTested(const QHash<QString,QString> & properties) const;
    bool IsTested(const QString & modelNumber, int family, int familyVersion) const;
    bool IsBrick(const QString & model) const;
    const char* Name(const QString & model) const;
};


#endif // PRS1LOADER_H
