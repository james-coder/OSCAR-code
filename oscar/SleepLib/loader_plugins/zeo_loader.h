/* SleepLib ZEO Loader Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef ZEOLOADER_H
#define ZEOLOADER_H

#include "SleepLib/machine_loader.h"

const QString zeo_class_name = "ZEO";
const int zeo_data_version = 2;


/*! \class ZEOLoader
    \brief Unfinished stub for loading ZEO Personal Sleep Coach data
*/
class ZEOLoader : public MachineLoader
{
  public:
    ZEOLoader();
    virtual ~ZEOLoader();

    virtual bool Detect(const QString &path) { Q_UNUSED(path); return false; }  // bypass autoscanner

    virtual int Open(const QString & path) { Q_UNUSED(path); return 0; } // Only for CPAP
    virtual int OpenFile(const QString & filename);
    virtual QStringList getNameFilter() { return QStringList("Zeo CSV File (*.csv)"); }
    static void Register();

    virtual int Version() { return zeo_data_version; }
    virtual const QString &loaderName() { return zeo_class_name; }

    //Machine *CreateMachine();
    virtual MachineInfo newInfo() {
        return MachineInfo(MT_SLEEPSTAGE, 0, zeo_class_name, QObject::tr("Zeo"), QString(), QString(), QString(), QObject::tr("Personal Sleep Coach"), QDateTime::currentDateTime(), zeo_data_version);
    }

    bool openCSV(const QString & filename);
    void closeCSV();
    Session* readNextSession();

  protected:
    QDateTime readDateTime(const QString & text, bool required=true);
    int readInt(const QString & text, bool required=true);

  private:
    QFile file;
    class CSVReader* csv;
    Machine *mach;
    bool invalid_fields;

    void AddEvent(ChannelID channel, qint64 t, EventDataType value);
    void EndEventList(ChannelID channel, qint64 t);
    Session* m_session;
    QHash<ChannelID, EventList*> m_importChannels;
    QHash<ChannelID, EventDataType> m_importLastValue;
};

#endif // ZEOLOADER_H
