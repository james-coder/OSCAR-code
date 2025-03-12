#ifndef VREMLOADER_H
#define VREMLOADER_H
// Your header file content goes here
#include "SleepLib/machine_loader.h"

#ifdef UNITTEST_MODE
#define private public
#define protected public
#endif // VREMLOADER_H
//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation
// BEFORE making a release
const int vREM_data_version = 1;
//
//********************************************************************************************
const QString vREM_class_name = "vREM";

class VREMLoader;

struct vREMData {
    QString id;
    qint64 start_time{};
    qint64 end_time{};
    QString max_pressure;
    QString min_pressure;
    QString ramp_pressure;
    QString rampTime;
    QString flex;
    QString flex_level;
    QString humidifier;
    QString humidifier_level;
    QString mask_type;
    QString mode;
};
class VREMLoader : public CPAPLoader
{
  Session * session;
    Q_OBJECT
  public:
    VREMLoader();
    virtual ~VREMLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Wrapper for PeekProperties that creates the MachineInfo structure.
    virtual MachineInfo PeekInfo(const QString & path);

    //! \brief Scans directory path for valid vREM signature
    virtual int Open(const QString & path);

    //! \brief Returns the database version of this loader
    virtual int Version() { return vREM_data_version; }

    //! \brief Return the loaderName, in this case "vREM"
    virtual const QString &loaderName() { return vREM_class_name; }

    //! \brief Register this Module to the list of Loaders, so it knows to search for vREM data.
    static void Register();

    // //! \brief Generate a generic MachineInfo structure, with basic vREM info to be expanded upon.
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, vREM_class_name, QObject::tr("vREM"), QString(), QString(), QString(), QObject::tr("vREM one"), QDateTime::currentDateTime(), vREM_data_version);
    }
    virtual QString PresReliefLabel();
    //! \brief Returns the vREM specific code for Pressure Relief Mode
    virtual ChannelID PresReliefMode();
    //! \brief Returns the vREM specific code for Pressure Relief Setting
    virtual ChannelID PresReliefLevel();
    //! \brief Returns the vREM specific code for PAP mode
    virtual ChannelID CPAPModeChannel();
    void initChannels();
  protected:
    //! \brief Returns the path of the vREM folder (whatever case) if present on the card
    QString GetvREMPath(const QString & path);

    //! \brief Returns the path for each device detected on an SD card, from oldest to newest
    QStringList FindMachinesOnCardvREM(const QString & cardPath);

    //! \brief Opens the SD folder structure for this device, scans for data files and imports any new sessions
    int OpenMachinevREM(const QString & path);

    //! \brief Finds the P0,P1,... session paths and property pathname and returns the base (10 or 16) of the session filenames
    void FindSessionDirsAndPropertiesvREM(const QString & path, QString & propertyfile,  QStringList & Odata);

    //! \brief Reads the model number from the property file, evaluates its capabilities, and returns true if the device is supported
    bool CreateMachineFromPropertiesvREM(QString propertyfile);

    int OscarDataParser(QStringList Odata,Machine* machine, QVector<vREMData> &data);

    //! \brief vREM Data files can store multiple sessions, so store them in this list for later processing.
    QHash<SessionID, Session *> new_sessions;
    
    //! \brief DS2 key derivation is very slow, but keys are reused in multiple files, so we cache the derived keys.
    QHash<QByteArray, QByteArray> m_keycache;
};
#endif // VREMLOADER_H