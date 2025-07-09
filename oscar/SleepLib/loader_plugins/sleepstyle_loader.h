/* SleepLib Fisher & Paykel SleepStyle Loader Implementation
 *
 * Copyright (c) 2020-2025 The Oscar Team (info@oscar-team.org)
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SLEEPSTYLE_LOADER_H
#define SLEEPSTYLE_LOADER_H

#include <QMultiMap>
#include "SleepLib/machine.h"
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the following value when making changes to this loaders implementation.
//
const int sleepstyle_data_version = 2;
//
//********************************************************************************************

/*! \class SleepStyle
    \brief F&P SleepStyle customized device object
    */
class SleepStyle: public CPAP
{
  public:
    SleepStyle(Profile *, MachineID id = 0);
    virtual ~SleepStyle();
};


const int sleepstyle_load_buffer_size = 1024 * 1024;

extern ChannelID SS_SensAwakeLevel, SS_EPR, SS_EPRLevel, SS_Ramp, SS_Humidity;

const QString sleepstyle_class_name = STR_MACH_SleepStyle;

/*! \class SleepStyleLoader
    \brief Loader for Fisher & Paykel SleepStyle data
    This is only relatively recent addition and still needs more work
    */

class SleepStyleLoader : public CPAPLoader
{
  Q_OBJECT
  public:
    SleepStyleLoader();
    virtual ~SleepStyleLoader();

    //! \brief Detect if the given path contains a valid Folder structure
    virtual bool Detect(const QString & path);

    //! \brief Scans path for F&P SleepStyle data signature, and Loads any new data
    virtual int Open(const QString & path);

    int OpenMachine(Machine *mach, const QString & path, const QString & ssPath);

    bool OpenSummary(Machine *mach, const QString & path);
    bool OpenDetail(Machine *mach, const QString & path);
//    bool OpenFLW(Machine *mach, const QString & filename);
    bool OpenRealTime(Machine *mach, const QString & fname, const QString & filename);

    //! \brief Returns SleepLib database version of this F&P SleepStyle loader
    virtual int Version() { return sleepstyle_data_version; }

    //! \brief Returns the device class name of this CPAP device, "SleepStyle"
    virtual const QString & loaderName() { return sleepstyle_class_name; }

    // ! \brief Creates a device object, indexed by serial number
    //Machine *CreateMachine(QString serial);

    QString getSerialPath () {return serialPath;}
    void setSerialPath (QString sp) {serialPath = sp;}
    bool backupData (Machine * mach, const QString & path);

    SessionID findSession (SessionID sid);

    void initChannels();

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_CPAP, 0, sleepstyle_class_name, QObject::tr("Fisher & Paykel"), QString(), QString(), QString(), QObject::tr("SleepStyle"), QDateTime::currentDateTime(), sleepstyle_data_version);
    }

    //! \brief Registers this MachineLoader with the master list, so F&P Icon data can load
    static void Register();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Now for some CPAPLoader overrides
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    virtual QString presRelType() { return QObject::tr("EPR"); }
    virtual QString PresReliefLabel() { return QObject::tr("EPR: "); }
    virtual ChannelID PresReliefMode();
    virtual ChannelID PresReliefLevel();
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  protected:
//    QDateTime readFPDateTime(quint8 *data);

    QString last;
    QHash<QString, Machine *> MachList;
    QMap<SessionID, Session *> Sessions;
    QMultiMap<QDate, Session *> SessDate;

    QString serialPath; // fully qualified path to  the input data, ...SDCard.../FPHCARE/ICON/serial
//    QString serial;    // Serial number
    bool rebuild_from_backups = false;
    bool create_backups = true;
    bool calc_leaks = true;
    float lpm4, lpm20;  // Leak per minute at 4 and 20 cmH20

    unsigned char *m_buffer;
};

#endif // SLEEPSTYLE_LOADER_H
