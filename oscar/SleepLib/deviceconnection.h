/* Device Connection Manager
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef DEVICECONNECTION_H
#define DEVICECONNECTION_H

// TODO: This file will eventually abstract serial port or bluetooth (or other)
// connections to devices. For now it just supports serial ports.

#include <QtSerialPort/QSerialPort>
#include <QHash>
#include <QVariant>

/*
 * Device connection base class
 *
 * Clients obtain a connection instance via DeviceConnectionManager::openConnection().
 * See SerialPortConnection for the only current concrete implementation.
 *
 * See DeviceConnectionManager for the primary interface to device
 * connections.
 */
class DeviceConnection : public QObject
{
    Q_OBJECT

protected:
    // Constructor is protected so that only subclasses and DeviceConnectionManager can call it.
    DeviceConnection(const QString & name, class XmlRecorder* record, class XmlReplay* replay);

    const QString & m_name;  // port/device identifier used to open the connection
    XmlRecorder* m_record;   // nullptr or pointer to recorder instance
    XmlReplay* m_replay;     // nullptr or pointer to replay instance
    bool m_opened;           // true if open() succeeded

    virtual bool open() = 0;
    friend class DeviceConnectionManager;

public:
    // See DeviceConnectionManager::openConnection() to create connections.
    virtual ~DeviceConnection();
    virtual const QString & type() const = 0;
    const QString & name() const { return m_name; }

    typedef DeviceConnection* (*FactoryMethod)(const QString & name, XmlRecorder* record, XmlReplay* replay);
};


/*
 * Device connection manager
 *
 * Principal class used to abstract direct connections to devices,
 * eventually encompassing serial port, Bluetooth, and BLE. This class not
 * only provides an abstraction for the specific connection type (where
 * possible), but it also provides the capability to record and replay
 * connections transparently to clients.
 *
 * Clients obtain the singleton instance via DeviceConnectionManager::getInstance().
 *
 * TODO: Eventually they will be able to connect to signals when a device
 * becomes available or is removed. For now they need to call instance->
 * getAvailableSerialPorts() to poll.
 *
 * When a device becomes available, clients call instance->openSerialPortConnection().
 * TODO: This will eventually probably be openConnection() once Bluetooth is
 * supported.
 *
 * To enable recording and replay of connections, call instance->record()
 * and/or instance->replay(), which will cause all subsequent connections to
 * be recorded or replayed, respectively. Passing nullptr to record() or
 * replay() will turn off recording/replaying for subsequent connections.
 * This allows an application to record or replay connection data
 * transparently to client code that assumes it is talking directly to a
 * real device.
 */
class DeviceConnectionManager : public QObject
{
    Q_OBJECT

private:
    // See getInstance() for creating/using the device connection manager.
    DeviceConnectionManager();
    
    XmlRecorder* m_record;  // nullptr or pointer to recorder instance
    XmlReplay* m_replay;    // nullptr or pointer to replay instance

    QList<class SerialPortInfo> m_serialPorts;  // currently available serial ports
    void reset() {  // clear state
        m_serialPorts.clear();
    }

    QHash<QString,DeviceConnection*> m_connections;  // currently open connections

public:
    //! \brief Obtain pointer to global DeviceConnectionManager instance, creating it if necessary.
    static DeviceConnectionManager & getInstance();

    //! \brief Open a connection to a device, returning an instance of the appropriate type, or nullptr if the connection couldn't be opened.
    class DeviceConnection* openConnection(const QString & type, const QString & name);

    //! \brief Open a serial port connection (convenience function, hopefully temporary), returning nullptr if the connection couldn't be opened.
    static class SerialPortConnection* openSerialPortConnection(const QString & portName);  // temporary

    //! \brief Return the list of currently available serial ports.
    QList<class SerialPortInfo> getAvailableSerialPorts();
    
    // TODO: method to start a polling thread that maintains the list of ports
    // TODO: emit signal when new port is detected (or removed)

    //! \brief Record all subsequent device activity to the given file, and subsequent connections to separate files alongside it. Passing nullptr turns off recording.
    void record(class QFile* stream);
    
    // Record all subsequent device activity to the given string. Primarily for testing; connection recordings are not supported.
    void record(QString & string);

    //! \brief Replay the activity previously recorded in the given file, allowing for some simple variation in the order of API calls. Passing nullptr turns off replay.
    void replay(class QFile* stream);

    // Replay the activity represented by the given string. Primarily for testing; connection replay is not supported.
    void replay(const QString & string);


    // DeviceConnection subclasses registration, not intended for client use.
protected:
    static QHash<QString,DeviceConnection::FactoryMethod> & factories();
public:
    static bool registerClass(const QString & type, DeviceConnection::FactoryMethod factory);
    static class DeviceConnection* createInstance(const QString & type);

    // Currently public only so that connections can deregister themselves.
    // Eventually this could move to protected if that gets handled by the
    // DeviceConnection destructor and DeviceConnection is declared a friend.
    void connectionClosed(DeviceConnection* conn);
};


/*
 * Serial port connection class
 *
 * This class provides functionality equivalent to QSerialPort, but
 * specifically represents an opened connection rather than the port itself.
 * (See the SerialPort class for the QSerialPort equivalent.) This class
 * also provides support for recording and replay of an opened serial port
 * connection.
 *
 * TODO: This class may eventually be internal to DeviceConnection, if its
 * interface shares enough in common with Bluetooth and/or BLE.
 */
class SerialPortConnection : public DeviceConnection
{
    Q_OBJECT

private:
    QSerialPort m_port;  // physical port used by connection
    void checkResult(bool ok, class XmlReplayEvent & event) const;
    void checkResult(qint64 len, XmlReplayEvent & event) const;
    void checkError(XmlReplayEvent & event) const;
    void close();

private slots:
    void onReadyRead();

signals:
    // The readyRead() signal is emitted with the same semantics as QSerialPort::readyRead().
    void readyRead();

protected:
    SerialPortConnection(const QString & name, XmlRecorder* record, XmlReplay* replay);
    virtual bool open();

public:
    // See DeviceConnectionManager::openConnection() or openSerialPortConnection() to create connections.
    virtual ~SerialPortConnection();
    
    // See QSerialPort for semantics of the below functions.
    bool setBaudRate(qint32 baudRate, QSerialPort::Directions directions = QSerialPort::AllDirections);
    bool setDataBits(QSerialPort::DataBits dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    bool clear(QSerialPort::Directions directions = QSerialPort::AllDirections);
    qint64 bytesAvailable() const;
    qint64 read(char *data, qint64 maxSize);
    qint64 write(const char *data, qint64 maxSize);
    bool flush();

    // Subclass registration with DeviceConnectionManager, not intended for client use.
public:
    static DeviceConnection* createInstance(const QString & name, XmlRecorder* record, XmlReplay* replay);
    static const QString TYPE;
    static const bool registered;
    virtual const QString & type() const { return TYPE; }
};


/*
 * SerialPort temporary class for legacy compatibility
 *
 * This class is a temporary drop-in replacement for QSerialPort for code
 * that currently assumes serial port connectivity. Using this class instead
 * of QSerialPort allows for recording and replay of connection data.
 *
 * See QSerialPort documentation for interface details. See
 * DeviceConnectionManager::record() and replay() for enabling recording
 * and replay.
 *
 * See SerialPortConnection for implementation details.
 */
class SerialPort : public QObject
{
    Q_OBJECT
    
private:
    SerialPortConnection* m_conn;
    QString m_portName;

private slots:
    void onReadyRead();

signals:
    void readyRead();

public:
    SerialPort();
    virtual ~SerialPort();

    void setPortName(const QString &name);
    bool open(QIODevice::OpenMode mode);
    bool setBaudRate(qint32 baudRate, QSerialPort::Directions directions = QSerialPort::AllDirections);
    bool setDataBits(QSerialPort::DataBits dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    bool clear(QSerialPort::Directions directions = QSerialPort::AllDirections);
    qint64 bytesAvailable() const;
    qint64 read(char *data, qint64 maxSize);
    qint64 write(const char *data, qint64 maxSize);
    bool flush();
    void close();
};


/*
 * SerialPortInfo temporary class for legacy compatibility
 *
 * This class is a temporary drop-in replacement for QSerialPortInfo for
 * code that currently assumes serial port connectivity. Using this class
 * instead of QSerialPortInfo allows for recording and replay of port
 * scanning.
 *
 * See QSerialPortInfo documentation for interface details. See
 * DeviceConnectionManager::record() and replay() for enabling recording
 * and replay.
 *
 * TODO: This class's functionality may either become internal to
 * DeviceConnection or may be moved to a generic port info class that
 * supports Bluetooth and BLE as well as serial. Such a class might then be
 * used instead of port "name" between DeviceConnectionManager and clients.
 */
class QXmlStreamWriter;
class QXmlStreamReader;

class SerialPortInfo
{
public:
    static QList<SerialPortInfo> availablePorts();
    SerialPortInfo(const SerialPortInfo & other);
    SerialPortInfo(const QString & data);
    SerialPortInfo();

    inline QString portName() const { return m_info["portName"].toString(); }
    inline QString systemLocation() const { return m_info["systemLocation"].toString(); }
    inline QString description() const { return m_info["description"].toString(); }
    inline QString manufacturer() const { return m_info["manufacturer"].toString(); }
    inline QString serialNumber() const { return m_info["serialNumber"].toString(); }

    inline quint16 vendorIdentifier() const { return m_info["vendorIdentifier"].toInt(); }
    inline quint16 productIdentifier() const { return m_info["productIdentifier"].toInt(); }

    inline bool hasVendorIdentifier() const { return m_info.contains("vendorIdentifier"); }
    inline bool hasProductIdentifier() const { return m_info.contains("productIdentifier"); }

    inline bool isNull() const { return m_info.isEmpty(); }

    operator QString() const;
    friend class QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const SerialPortInfo & info);
    friend class QXmlStreamReader & operator>>(QXmlStreamReader & xml, SerialPortInfo & info);
    bool operator==(const SerialPortInfo & other) const;
    SerialPortInfo& operator=(const SerialPortInfo & other) = default;

protected:
    SerialPortInfo(const class QSerialPortInfo & other);
    QHash<QString,QVariant> m_info;

    friend class DeviceConnectionManager;
};

#endif // DEVICECONNECTION_H
