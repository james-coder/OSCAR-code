/* QIODevice wrapper for reading raw binary data
 *
 * Copyright (c) 2021-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "rawdata.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>


RawDataFile::RawDataFile(QFile & file)
    : RawDataDevice(file, QFileInfo(file).canonicalFilePath())
{
}


RawDataDevice::RawDataDevice(QIODevice & device, QString name)
    : m_device(device), m_name(name)
{
    connect(&m_device, SIGNAL(channelReadyRead(int)), this, SLOT(onChannelReadyRead(int)));
    connect(&m_device, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(&m_device, SIGNAL(readChannelFinished()), this, SLOT(onReadChannelFinished()));
    connect(&m_device, SIGNAL(aboutToClose()), this, SLOT(onAboutToClose()));
    if (m_device.isOpen()) {
        open(m_device.openMode());
    }
}

RawDataDevice::~RawDataDevice()
{
    disconnect(&m_device, SIGNAL(channelReadyRead(int)), this, SLOT(onChannelReadyRead(int)));
    disconnect(&m_device, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    disconnect(&m_device, SIGNAL(readChannelFinished()), this, SLOT(onReadChannelFinished()));
    disconnect(&m_device, SIGNAL(aboutToClose()), this, SLOT(onAboutToClose()));
}

void RawDataDevice::onAboutToClose()
{
    emit aboutToClose();
}

void RawDataDevice::onChannelReadyRead(int channel)
{
    qWarning() << "RawDataDevice::onChannelReadyRead untested";
    emit channelReadyRead(channel);
}

void RawDataDevice::onReadChannelFinished()
{
    qWarning() << "RawDataDevice::onReadChannelFinished untested";
    emit readChannelFinished();
}

void RawDataDevice::onReadyRead()
{
    qWarning() << "RawDataDevice::onReadyRead untested";
    emit readyRead();
}

bool RawDataDevice::waitForReadyRead(int msecs)
{
    return m_device.waitForReadyRead(msecs);
}

bool RawDataDevice::open(QIODevice::OpenMode mode)
{
    bool ok = false;
    if (mode & QIODevice::WriteOnly) {
        // RawDataDevice is intended only for importing external data formats.
        // Use QDataStream for writing/reading internal data.
        // TODO: Revisit this if we wrap device connections in a RawDataDevice.
        qWarning() << "RawDataDevice does not support writing. Use QDataStream.";
    } else {
        if (m_device.openMode() == mode) {
            ok = QIODevice::open(mode);  // If the device is already opened, mark the raw device as opened.
        } else if (m_device.open(mode)) {
            mode = m_device.openMode();  // Copy over any flags set by the device, e.g. unbuffered.
            ok = QIODevice::open(mode);
        }
    }
    setErrorString(m_device.errorString());
    return ok;
}

void RawDataDevice::close()
{
    m_device.close();
    QIODevice::close();
    setErrorString(m_device.errorString());
}

void RawDataDevice::syncTextMode(void)
{
    // Sadly setTextModeEnabled() isn't virtual in QIODevice,
    // so we have to sync the setting before read/write/peek.
    if (isTextModeEnabled() != m_device.isTextModeEnabled()) {
        m_device.setTextModeEnabled(isTextModeEnabled());
    }
}

qint64 RawDataDevice::readData(char *data, qint64 maxSize)
{
    syncTextMode();
    qint64 result = m_device.read(data, maxSize);  // note that readData is also used by peek, so pos may diverge
    setErrorString(m_device.errorString());
    return result;
}

qint64 RawDataDevice::writeData(const char */*data*/, qint64 /*len*/)
{
    syncTextMode();
    // This method is required in order to create a concrete instance of QIODevice,
    // but we should never be writing raw data.
    qWarning() << name() << "writing not supported";
    setErrorString("RawDataDevice does not support writing.");
    return -1;
}

bool RawDataDevice::seek(qint64 pos)
{
    bool ok = m_device.seek(pos);
    if (ok) {
        QIODevice::seek(pos);
        setErrorString(m_device.errorString());
    }
    return ok;
}

bool RawDataDevice::isSequential() const
{
    bool is_sequential = m_device.isSequential();
    Q_ASSERT(is_sequential == false);  // Before removing this, add tests to RawDataTests to confirm that sequential devices work!
    return is_sequential;
}

bool RawDataDevice::canReadLine() const
{
    return m_device.canReadLine() || QIODevice::canReadLine();
}

qint64 RawDataDevice::size() const
{
    return m_device.size();
}
