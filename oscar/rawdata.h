/* QIODevice wrapper for reading raw binary data
 *
 * Copyright (c) 2021-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef RAWDATA_H
#define RAWDATA_H

#include <QIODevice>
#include <QString>

// Wrap an arbitrary QIODevice with a name (and TODO: endian-aware decoding functions),
// passing through requests to the underlying device.
class RawDataDevice : public QIODevice
{
    Q_OBJECT
  public:
    RawDataDevice(QIODevice & device, QString name);
    virtual ~RawDataDevice();

  public:
    virtual bool isSequential() const;

    virtual bool open(QIODevice::OpenMode mode);
    virtual void close();
    
    virtual qint64 size() const;
    virtual bool seek(qint64 pos);

    virtual bool canReadLine() const;

    virtual bool waitForReadyRead(int msecs);

  protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    virtual qint64 writeData(const char *data, qint64 len);

    QIODevice & m_device;
    QString m_name;
  public:
    QString name() const { return m_name; }
  private:
    void syncTextMode();

  protected slots:
    void onAboutToClose();
    void onChannelReadyRead(int);
    void onReadChannelFinished();
    void onReadyRead();

  public:
    // TODO: add get/set endian, read16/read32/reads16/reads32, tests
};


// Convenience class for wrapping files, using their canonical path as the device name.
class RawDataFile : public RawDataDevice
{
    Q_OBJECT
  public:
    RawDataFile(class QFile & file);
};

#endif // RAWDATA_H
