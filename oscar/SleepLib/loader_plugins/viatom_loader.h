/* SleepLib Viatom Loader Header
 *
 * Copyright (c) 2020-2024 The OSCAR Team
 * (Initial importer written by dave madden <dhm@mersenne.com>)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef VIATOMLOADER_H
#define VIATOMLOADER_H

#include "SleepLib/machine_loader.h"

const QString viatom_class_name = "Viatom";
const int viatom_data_version = 2;


/*! \class ViatomLoader
    \brief Unfinished stub for loading Viatom Sleep Ring / Wrist Pulse Oximeter data
*/
class ViatomLoader : public MachineLoader
{
  public:
    ViatomLoader() { m_type = MT_OXIMETER; }
    virtual ~ViatomLoader() { }

    virtual bool Detect(const QString & path);

    virtual int Open(const QString & path) { Q_UNUSED(path); return 0; } // Only for CPAP
    virtual int Open(const QStringList & paths);
    Session* ParseFile(const QString & filename, bool *existing=0);

    static void Register();

    virtual int Version() { return viatom_data_version; }
    virtual const QString &loaderName() { return viatom_class_name; }

    virtual MachineInfo newInfo() {
        return MachineInfo(MT_OXIMETER, 0, viatom_class_name, QObject::tr("Viatom"), QString(), QString(), QString(), QObject::tr("Viatom Software"), QDateTime::currentDateTime(), viatom_data_version);
    }

    virtual QStringList getNameFilter();

  //Machine *CreateMachine();

  protected:
    int OpenFile(const QString & filename);
    void SaveSessionToDatabase(Session* session);

    void AddEvent(ChannelID channel, qint64 t, EventDataType value);
    void EndEventList(ChannelID channel, qint64 t);

    Session* ParseFileViatom(const QString & filename, bool *existing);
    Session* ParseFilePOD2(const QString & filename, bool *existing);


    Machine* m_mach;
    Session* m_session;
    qint64 m_step;
    QHash<ChannelID, EventList*> m_importChannels;
    QHash<ChannelID, EventDataType> m_importLastValue;
  private:
};

class ViatomPOD2File{

public:
    struct Record{
         unsigned char spo2;
         unsigned char hr;
         unsigned char unk1;//Always 0 from my device.
         unsigned char pi; //Perfusion index. Two digits.  Divide by decimal 10 for value in %. Example: pi= 0x0E = 14 = 1.4%
         unsigned char unk2;//Always 0 from my device.
         unsigned char unk3;//Always 0xC0 from my device.
    };
    ViatomPOD2File(QFile & file);
    virtual ~ViatomPOD2File() = default;

    virtual bool ParseHeader();
    virtual QList<Record> ReadData();
    SessionID sessionid() const { return m_sessionid; }
    quint64 timestamp() const { return m_timestamp; }
    int duration() const { return m_duration; }
    QDateTime getPOD2FilenameTimestamp();

protected:
    static const int RECORD_SIZE = 6;
    QFile & m_file;
    int m_sig;
    quint64 m_timestamp;
    int m_duration;
    int m_record_count;
    int m_resolution;
    SessionID m_sessionid;
};

class ViatomFile
{
public:
    struct Record
    {
        unsigned char spo2;
        unsigned char hr;
        unsigned char oximetry_invalid;
        unsigned char motion;
        unsigned char vibration;
    };
    ViatomFile(QFile & file);
    virtual ~ViatomFile() = default;

    virtual bool ParseHeader();
    virtual QList<Record> ReadData();
    SessionID sessionid() const { return m_sessionid; }
    quint64 timestamp() const { return m_timestamp; }
    int duration() const { return m_duration; }
    QDateTime getFilenameTimestamp();

protected:
    static const int RECORD_SIZE = 5;
    QFile & m_file;
    int m_sig;
    quint64 m_timestamp;
    int m_duration;
    int m_record_count;
    int m_resolution;
    SessionID m_sessionid;
};

class O2RingS : public ViatomFile
{
public:
    O2RingS(QFile & file);
    ~O2RingS() = default;
    bool ParseHeader();
    QList<Record> ReadData();
};

#endif // VIATOMLOADER_H
