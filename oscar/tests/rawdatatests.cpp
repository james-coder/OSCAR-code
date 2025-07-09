/* Raw Data Unit Tests
 *
 * Copyright (c) 2021-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "rawdatatests.h"
#include "rawdata.h"

#include <QBuffer>

// Check QIODevice interface for consistency.
void RawDataTests::testQIODeviceInterface()
{
    // Create sample data.
    static const int DATA_SIZE = 256;
    QByteArray data(DATA_SIZE, 0);
    for (int i = 0; i < data.size(); i++) {
        data[i] = (DATA_SIZE-1) - i;
    }
    QBuffer qio(&data);

    // Create raw data wrapper.
    RawDataDevice raw_instance(qio, "sample");
    Q_ASSERT(raw_instance.name() == "sample");
    QIODevice & raw(raw_instance);  // cast to its generic interface for accurate testing


    // Connect signals for testing.
    _RawDataTestSignalSink sink;
    connect(&raw, SIGNAL(channelReadyRead(int)), &sink, SLOT(onChannelReadyRead(int)));
    connect(&raw, SIGNAL(readyRead()), &sink, SLOT(onReadyRead()));
    connect(&raw, SIGNAL(readChannelFinished()), &sink, SLOT(onReadChannelFinished()));
    connect(&raw, SIGNAL(aboutToClose()), &sink, SLOT(onAboutToClose()));


    // Open
    Q_ASSERT(raw.isOpen() == qio.isOpen());
    Q_ASSERT(raw.isReadable() == qio.isReadable());
    Q_ASSERT(raw.isWritable() == qio.isWritable());
    Q_ASSERT(raw.isWritable() == false);
    Q_ASSERT(raw.isSequential() == qio.isSequential());
    Q_ASSERT(raw.openMode() == qio.openMode());

    Q_ASSERT(raw.open(QIODevice::ReadWrite) == false);
    Q_ASSERT(raw.open(QIODevice::ReadOnly) == true);
    Q_ASSERT(raw.isOpen() == qio.isOpen());
    Q_ASSERT(raw.isReadable() == qio.isReadable());
    Q_ASSERT(raw.isWritable() == qio.isWritable());
    Q_ASSERT(raw.isWritable() == false);
    Q_ASSERT(raw.isSequential() == qio.isSequential());
    Q_ASSERT(raw.openMode() == qio.openMode());


    // waitForReadyRead and ready signals
    Q_ASSERT(raw.waitForReadyRead(10000) == false);
    //Q_ASSERT(sink.m_channelReadyRead != -1);
    //Q_ASSERT(sink.m_readyRead == true);


    // Channels
    Q_ASSERT(raw.readChannelCount() == qio.readChannelCount());
    for (int i = 0; i < raw.readChannelCount(); i++) {
        raw.setCurrentReadChannel(i);
        Q_ASSERT(raw.currentReadChannel() == i);
        Q_ASSERT(raw.currentReadChannel() == qio.currentReadChannel());
    }


    // Text mode
    // Text mode is pretty awful, it just drops all \x0D, even without a trailing \x0A.
    Q_ASSERT(raw.isTextModeEnabled() == false);
    Q_ASSERT(raw.isTextModeEnabled() == qio.isTextModeEnabled());
    raw.setTextModeEnabled(true);
    Q_ASSERT(raw.isTextModeEnabled() == true);
    raw.peek(1);  // force a sync of text mode
    Q_ASSERT(raw.isTextModeEnabled() == qio.isTextModeEnabled());
    raw.setTextModeEnabled(false);
    raw.peek(1);  // force a sync of text mode
    Q_ASSERT(raw.isTextModeEnabled() == qio.isTextModeEnabled());


    // seek/pos/getChar/ungetChar/readAll/atEnd
    // skip() is 5.10 or later, so we don't use or test it
    char ch=-1;
    int pos = raw.pos();
    Q_ASSERT(raw.pos() == qio.pos() - 1);  // peek (above) only retracts raw's position after reading qio
    Q_ASSERT(raw.getChar(&ch) == true);
    Q_ASSERT(raw.pos() == qio.pos());
    raw.ungetChar(ch);
    Q_ASSERT(raw.pos() == pos);
    Q_ASSERT(raw.pos() == qio.pos() - 1);  // ungetChar only affects raw's buffer/position
    Q_ASSERT(ch == data[0]);

    Q_ASSERT(raw.size() == qio.size());

    Q_ASSERT(raw.seek(16) == true);
    Q_ASSERT(raw.pos() == 16);
    Q_ASSERT(raw.pos() == qio.pos());
    Q_ASSERT(raw.atEnd() == qio.atEnd());
    
    
    // Check boundary conditions at end of device.
    Q_ASSERT(raw.seek(255) == true);
    Q_ASSERT(raw.getChar(&ch) == true);
    Q_ASSERT(raw.pos() == qio.pos());
    Q_ASSERT(raw.atEnd() == true);
    Q_ASSERT(raw.atEnd() == qio.atEnd());
    Q_ASSERT(raw.bytesAvailable() == qio.bytesAvailable());
    raw.ungetChar(ch);
    Q_ASSERT(raw.atEnd() == false);
    Q_ASSERT(raw.atEnd() != qio.atEnd());
    Q_ASSERT(raw.bytesAvailable() == qio.bytesAvailable() + 1);
    
    Q_ASSERT(raw.reset() == true);
    Q_ASSERT(raw.pos() == 0);
    Q_ASSERT(raw.pos() == qio.pos());
    QByteArray all = raw.readAll();
    Q_ASSERT(all == data);
    Q_ASSERT(raw.atEnd() == qio.atEnd());
    Q_ASSERT(raw.bytesAvailable() == qio.bytesAvailable());


    // canReadLine
    Q_ASSERT(raw.canReadLine() == qio.canReadLine());
    raw.seek(255 - 0x0A);
    Q_ASSERT(raw.canReadLine() == true);
    Q_ASSERT(raw.canReadLine() == qio.canReadLine());
    Q_ASSERT(raw.getChar(&ch) == true);
    Q_ASSERT(ch == 0x0A);
    Q_ASSERT(raw.canReadLine() == false);
    Q_ASSERT(raw.canReadLine() == qio.canReadLine());
    raw.ungetChar(ch);
    Q_ASSERT(raw.canReadLine() == true);
    Q_ASSERT(raw.canReadLine() != qio.canReadLine());


    // readLine x2
    Q_ASSERT(raw.reset() == true);
    Q_ASSERT(raw.canReadLine() == qio.canReadLine());

    char line[DATA_SIZE+1];  // plus trailing null
    int length = raw.readLine(line, sizeof(line));
    pos = raw.pos();
    raw.reset();
    char line2[DATA_SIZE+1];  // plus trailing null
    int length2 = qio.readLine(line2, sizeof(line2));
    Q_ASSERT(length == length2);
    Q_ASSERT(strcmp(line, line2) == 0);
    
    raw.reset();
    
    QByteArray raw_readLine = raw.readLine();
    raw.reset();
    Q_ASSERT(raw_readLine == qio.readLine());


    // read & peek x2
    Q_ASSERT(raw.reset() == true);
    
    length = raw.read(line, 128);
    Q_ASSERT(length == 128);
    Q_ASSERT(raw.pos() == 128);
    Q_ASSERT(raw.pos() == qio.pos());
    Q_ASSERT(memcmp(data.constData(), line, 128) == 0);

    Q_ASSERT(raw.pos() == 128);
    length2 = raw.peek(line2, 128);
    Q_ASSERT(raw.pos() == 128);
    Q_ASSERT(length == 128);
    Q_ASSERT(raw.pos() == qio.pos() - length);  // peek only retracts raw's position after reading qio
    Q_ASSERT(memcmp(data.constData()+128, line2, 128) == 0);

    raw.reset();

    QByteArray raw_read = raw.read(128);
    Q_ASSERT(length == 128);
    Q_ASSERT(raw.pos() == 128);
    Q_ASSERT(raw.pos() == qio.pos());
    Q_ASSERT(raw_read == data.mid(0, 128));

    Q_ASSERT(raw.pos() == 128);
    QByteArray raw_peek = raw.peek(128);
    Q_ASSERT(raw.pos() == 128);
    Q_ASSERT(length == 128);
    Q_ASSERT(raw.pos() == qio.pos() - 128);  // peek only retracts raw's position after reading qio
    Q_ASSERT(raw_peek == data.mid(128, 128));

    raw.reset();


    // Transactions
    // These exist solely within raw and don't pass through to the underlying device.
    Q_ASSERT(raw.isTransactionStarted() == false);
    raw.startTransaction();
    Q_ASSERT(raw.isTransactionStarted() == true);
    raw_peek = raw.read(128);
    Q_ASSERT(raw.pos() == 128);
    raw.rollbackTransaction();
    Q_ASSERT(raw.isTransactionStarted() == false);
    Q_ASSERT(raw.pos() == 0);
    raw.startTransaction();
    Q_ASSERT(raw.isTransactionStarted() == true);
    raw_read = raw.read(128);
    Q_ASSERT(raw.pos() == 128);
    raw.commitTransaction();
    Q_ASSERT(raw.isTransactionStarted() == false);
    Q_ASSERT(raw.pos() == 128);


    // Close
    raw.close();
    Q_ASSERT(raw.isOpen() == qio.isOpen());
    Q_ASSERT(sink.m_aboutToClose);
    //Q_ASSERT(sink.m_readChannelFinished);


    // Unimplemented/untested:
    // bytesToWrite
    // currentWriteChannel
    // setCurentWriteChannel
    // putChar
    // waitForBytesWritten
    // write x3
    // writeChannelCount
    // bytesWritten signal
    // channelBytesWritten signal
}

_RawDataTestSignalSink::_RawDataTestSignalSink() : QObject() {
    m_channelReadyRead = -1;
    m_readyRead = false;
    m_readChannelFinished = false;
    m_aboutToClose = false;
}

void _RawDataTestSignalSink::onAboutToClose()
{
    m_aboutToClose = true;
}

void _RawDataTestSignalSink::onChannelReadyRead(int channel)
{
    m_channelReadyRead = channel;
}

void _RawDataTestSignalSink::onReadChannelFinished()
{
    m_readChannelFinished = true;
}

void _RawDataTestSignalSink::onReadyRead()
{
    m_readyRead = true;
}


// TODO: Test sequential devices when we have a test case.
// TODO: Test waitForReadySignal when we have a test case.
// TODO: Test readyRead/channelReadyRead/onReadChannelFinished signals when we have a test case.
