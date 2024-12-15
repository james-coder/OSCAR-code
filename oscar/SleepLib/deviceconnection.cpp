/* Device Connection Manager
 *
 * Copyright (c) 2020-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnection.h"
#include "xmlreplay.h"
#include "version.h"
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>


static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}


// MARK: -
// MARK: Device connection manager

/*
 * DeviceRecorder/DeviceReplay subclasses of XmlRecorder/XmlReplay
 *
 * Used by DeviceConnectionManager to record its activity, such as
 * port scanning and connection opening/closing.
 */
class DeviceRecorder : public XmlRecorder
{
public:
    static const QString TAG;

    DeviceRecorder(class QFile * file) : XmlRecorder(file, DeviceRecorder::TAG) { m_xml->writeAttribute("oscar", getVersion().toString()); }
    DeviceRecorder(QString & string) : XmlRecorder(string, DeviceRecorder::TAG) { m_xml->writeAttribute("oscar", getVersion().toString()); }
};
const QString DeviceRecorder::TAG = "devicereplay";

class DeviceReplay : public XmlReplay
{
public:
    DeviceReplay(class QFile * file) : XmlReplay(file, DeviceRecorder::TAG) {}
    DeviceReplay(QXmlStreamReader & xml) : XmlReplay(xml, DeviceRecorder::TAG) {}
};

void DeviceConnectionManager::record(QFile* stream)
{
    if (m_record) {
        delete m_record;
    }
    if (stream) {
        m_record = new DeviceRecorder(stream);
    } else {
        // nullptr turns off recording
        m_record = nullptr;
    }
}

void DeviceConnectionManager::record(QString & string)
{
    if (m_record) {
        delete m_record;
    }
    m_record = new DeviceRecorder(string);
}

void DeviceConnectionManager::replay(const QString & string)
{
    QXmlStreamReader xml(string);
    reset();
    if (m_replay) {
        delete m_replay;
    }
    m_replay = new DeviceReplay(xml);
}

void DeviceConnectionManager::replay(QFile* file)
{
    reset();
    if (m_replay) {
        delete m_replay;
    }
    if (file) {
        m_replay = new DeviceReplay(file);
    } else {
        // nullptr turns off replay
        m_replay = nullptr;
    }
}


// Return singleton instance of DeviceConnectionManager, creating it if necessary.
DeviceConnectionManager & DeviceConnectionManager::getInstance()
{
    static DeviceConnectionManager instance;
    return instance;
}

// Protected constructor
DeviceConnectionManager::DeviceConnectionManager()
    : m_record(nullptr), m_replay(nullptr)
{
}

DeviceConnection* DeviceConnectionManager::openConnection(const QString & type, const QString & name)
{
    if (!factories().contains(type)) {
        qWarning() << "Unknown device connection type:" << type;
        return nullptr;
    }
    if (m_connections.contains(name)) {
        qWarning() << "connection to" << name << "already open";
        return nullptr;
    }

    // Recording/replay (if any) is handled by the connection.
    DeviceConnection* conn = factories()[type](name, m_record, m_replay);
    if (conn) {
        if (conn->open()) {
            m_connections[name] = conn;
        } else {
            qWarning().noquote() << "unable to open" << type << "connection to" << name;
            delete conn;
            conn = nullptr;
        }
    } else {
        qWarning() << "unable to create" << type << "connection to" << name;
    }
    return conn;
}

// Called by connections to deregister themselves.
void DeviceConnectionManager::connectionClosed(DeviceConnection* conn)
{
    Q_ASSERT(conn);
    const QString & type = conn->type();
    const QString & name = conn->name();

    if (m_connections.contains(name)) {
        if (m_connections[name] == conn) {
            m_connections.remove(name);
        } else {
            qWarning() << "connection to" << name << "not created by openConnection!";
        }
    } else {
        qWarning() << type << "connection to" << name << "missing";
    }
}

// Temporary convenience function for code that still supports only serial ports.
SerialPortConnection* DeviceConnectionManager::openSerialPortConnection(const QString & portName)
{
    return dynamic_cast<SerialPortConnection*>(getInstance().openConnection(SerialPortConnection::TYPE, portName));
}


QHash<QString,DeviceConnection::FactoryMethod> & DeviceConnectionManager::factories()
{
    static QHash<QString,DeviceConnection::FactoryMethod> s_factories;
    return s_factories;
}

bool DeviceConnectionManager::registerClass(const QString & type, DeviceConnection::FactoryMethod factory)
{
    if (factories().contains(type)) {
        qWarning() << "Connection class already registered for type" << type;
        return false;
    }
    factories()[type] = factory;
    return true;
}

// Since there are relatively few connection types, don't bother with a CRTP
// parent class. Instead, this macro defines the factory method, and the
// subclass will need to declare createInstance() and TYPE manually.
#define REGISTER_DEVICECONNECTION(type, T) \
const QString T::TYPE = type; \
const bool T::registered = DeviceConnectionManager::registerClass(T::TYPE, T::createInstance); \
DeviceConnection* T::createInstance(const QString & name, XmlRecorder* record, XmlReplay* replay) { return static_cast<DeviceConnection*>(new T(name, record, replay)); }


// MARK: -
// MARK: Device manager events

// See XmlReplayEvent discussion of complex data types above.
class GetAvailableSerialPortsEvent : public XmlReplayBase<GetAvailableSerialPortsEvent>
{
public:
    QList<SerialPortInfo> m_ports;

protected:
    virtual void write(QXmlStreamWriter & xml) const
    {
        xml << m_ports;
    }
    virtual void read(QXmlStreamReader & xml)
    {
        xml >> m_ports;
    }
};
REGISTER_XMLREPLAYEVENT("getAvailableSerialPorts", GetAvailableSerialPortsEvent);


QList<SerialPortInfo> DeviceConnectionManager::getAvailableSerialPorts()
{
    XmlReplayLock lock(this, m_replay);
    GetAvailableSerialPortsEvent event;

    if (!m_replay) {
        // Query the actual hardware present.
        for (auto & info : QSerialPortInfo::availablePorts()) {
            event.m_ports.append(SerialPortInfo(info));
        }
    } else {
        auto replayEvent = m_replay->getNextEvent<GetAvailableSerialPortsEvent>();
        if (replayEvent) {
            event.m_ports = replayEvent->m_ports;
        } else {
            // If there are no replay events available, reuse the most recent state.
            event.m_ports = m_serialPorts;
        }
    }
    m_serialPorts = event.m_ports;

    event.record(m_record);
    return event.m_ports;
}


// MARK: -
// MARK: Serial port info

/*
 * This class is both a drop-in replacement for QSerialPortInfo and
 * supports XML serialization for the GetAvailableSerialPortsEvent above.
 */

SerialPortInfo::SerialPortInfo(const QSerialPortInfo & other)
{
    if (other.isNull() == false) {
        m_info["portName"] = other.portName();
        m_info["systemLocation"] = other.systemLocation();
        m_info["description"] = other.description();
        m_info["manufacturer"] = other.manufacturer();
        m_info["serialNumber"] = other.serialNumber();
        if (other.hasVendorIdentifier()) {
            m_info["vendorIdentifier"] = other.vendorIdentifier();
        }
        if (other.hasProductIdentifier()) {
            m_info["productIdentifier"] = other.productIdentifier();
        }
    }
}

SerialPortInfo::SerialPortInfo(const SerialPortInfo & other)
    : m_info(other.m_info)
{
}

SerialPortInfo::SerialPortInfo(const QString & data)
{
    QXmlStreamReader xml(data);
    xml.readNextStartElement();
    xml >> *this;
}

SerialPortInfo::SerialPortInfo()
{
}

// TODO: This method is a temporary wrapper that mimics the QSerialPortInfo interface until we begin refactoring.
QList<SerialPortInfo> SerialPortInfo::availablePorts()
{
    return DeviceConnectionManager::getInstance().getAvailableSerialPorts();
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const SerialPortInfo & info)
{
    xml.writeStartElement("serial");
    if (info.isNull() == false) {
        xml.writeAttribute("portName", info.portName());
        xml.writeAttribute("systemLocation", info.systemLocation());
        xml.writeAttribute("description", info.description());
        xml.writeAttribute("manufacturer", info.manufacturer());
        xml.writeAttribute("serialNumber", info.serialNumber());
        if (info.hasVendorIdentifier()) {
            xml.writeAttribute("vendorIdentifier", hex(info.vendorIdentifier()));
        }
        if (info.hasProductIdentifier()) {
            xml.writeAttribute("productIdentifier", hex(info.productIdentifier()));
        }
    }
    xml.writeEndElement();
    return xml;
}

QXmlStreamReader & operator>>(QXmlStreamReader & xml, SerialPortInfo & info)
{
    //if (xml.atEnd() == false && xml.isStartElement() && xml.name() == "serial") 
    // error: result of comparison against a string literal is unspecified 
    if (xml.atEnd() == false && xml.isStartElement() && xml.name() == QStringLiteral("serial")) {
        for (auto & attribute : xml.attributes()) {
            QString name = attribute.name().toString();
            QString value = attribute.value().toString();
            if (name == "vendorIdentifier" || name == "productIdentifier") {
                bool ok;
                quint16 id = value.toUInt(&ok, 0);
                if (ok) {
                    info.m_info[name] = id;
                } else {
                    qWarning() << "invalid" << name << "value" << value;
                }
            } else {
                info.m_info[name] = value;
            }
        }
    } else {
        qWarning() << "no <serial> tag";
    }
    xml.skipCurrentElement();
    return xml;
}

SerialPortInfo::operator QString() const
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml << *this;
    return out;
}

bool SerialPortInfo::operator==(const SerialPortInfo & other) const
{
    return m_info == other.m_info;
}


// MARK: -
// MARK: Device connection base classes and events

/*
 * Event recorded in the Device Connection Manager XML stream that indicates
 * a connection was opened (or attempted). On success, a ConnectionEvent
 * (see below) will begin the connection's substream.
 */
class OpenConnectionEvent : public XmlReplayBase<OpenConnectionEvent>
{
public:
    OpenConnectionEvent() {}
    OpenConnectionEvent(const QString & type, const QString & name)
    {
        set("type", type);
        set("name", name);
    }
    virtual const QString id() const { return m_values["name"]; }
};
REGISTER_XMLREPLAYEVENT("openConnection", OpenConnectionEvent);

/*
 * Event created when a connection is successfully opened, used as the
 * enclosing tag for the connection substream.
 */
class ConnectionEvent : public XmlReplayBase<ConnectionEvent>
{
public:
    ConnectionEvent() { Q_ASSERT(false); }  // Implement if we ever support string-based substreams
    ConnectionEvent(const OpenConnectionEvent & trigger)
    {
        copy(trigger);
    }
    virtual const QString id() const
    {
        QString time = m_time.toString("yyyyMMdd.HHmmss.zzz");
        return m_values["name"] + "-" + time;
    }
};
REGISTER_XMLREPLAYEVENT("connection", ConnectionEvent);

/*
 * ConnectionRecorder/ConnectionReplay subclasses of XmlRecorder/XmlReplay
 *
 * Used by DeviceConnection subclasses to record their activity, such as
 * configuration and data sent and received.
 */
class ConnectionRecorder : public XmlRecorder
{
public:
    ConnectionRecorder(XmlRecorder* parent, const ConnectionEvent& event) : XmlRecorder(parent, event.id(), event.tag())
    {
        Q_ASSERT(m_xml);
        event.writeTag(*m_xml);
        m_xml->writeAttribute("oscar", getVersion().toString());
    }
};

class ConnectionReplay : public XmlReplay
{
public:
    ConnectionReplay(XmlReplay* parent, const ConnectionEvent& event) : XmlReplay(parent, event.id(), event.tag()) {}
};


// Device connection base class
DeviceConnection::DeviceConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : m_name(name), m_record(record), m_replay(replay), m_opened(false)
{
}

DeviceConnection::~DeviceConnection()
{
}

/*
 * Generic get/set events
 */
class SetValueEvent : public XmlReplayBase<SetValueEvent>
{
public:
    SetValueEvent() {}
    SetValueEvent(const QString & name, int value)
    {
        set(name, value);
    }
    virtual const QString id() const { return m_keys.first(); }
};
REGISTER_XMLREPLAYEVENT("set", SetValueEvent);

class GetValueEvent : public XmlReplayBase<GetValueEvent>
{
public:
    GetValueEvent() {}
    GetValueEvent(const QString & id)
    {
        set(id, 0);
    }
    virtual const QString id() const { return m_keys.first(); }
    void setValue(qint64 value)
    {
        if (m_keys.isEmpty()) {
            qWarning() << "setValue: get event missing key";
            return;
        }
        set(m_keys.first(), value);
    }
    QString value() const
    {
        if (m_keys.isEmpty()) {
            qWarning() << "getValue: get event missing key";
            return 0;
        }
        return get(m_keys.first());
    }
};
REGISTER_XMLREPLAYEVENT("get", GetValueEvent);

/*
 * Event recorded in the Device Connection Manager XML stream when a
 * open connection is closed. This is the complement to a successful
 * OpenConnectionEvent (see above), and does not appear when the connection
 * failed to open.
 */
class CloseConnectionEvent : public XmlReplayBase<CloseConnectionEvent>
{
public:
    CloseConnectionEvent() {}
    CloseConnectionEvent(const QString & type, const QString & name)
    {
        set("type", type);
        set("name", name);
    }
    virtual const QString id() const { return m_values["name"]; }
};
REGISTER_XMLREPLAYEVENT("closeConnection", CloseConnectionEvent);

class ClearConnectionEvent : public XmlReplayBase<ClearConnectionEvent>
{
};
REGISTER_XMLREPLAYEVENT("clear", ClearConnectionEvent);

class FlushConnectionEvent : public XmlReplayBase<FlushConnectionEvent>
{
};
REGISTER_XMLREPLAYEVENT("flush", FlushConnectionEvent);

/*
 * Event representing data received from a device
 *
 * The data is stored as hexadecimal data in the XML tag's contents.
 */
class ReceiveDataEvent : public XmlReplayBase<ReceiveDataEvent>
{
    virtual bool usesData() const { return true; }
};
REGISTER_XMLREPLAYEVENT("rx", ReceiveDataEvent);

/*
 * Event representing data sent to a device
 *
 * The data is stored as hexadecimal data in the XML tag's contents.
 *
 * These events are random-access events (see discussion above), which cause
 * subsequent event retrieval to begin searching after the transmission
 * event.
 *
 * Since the data sent is used as the ID for these events, we can treat
 * these like distinct "commands" that that elicit a deterministic response,
 * which can be replayed independently of other events if desired.
 *
 * Of course, for any device that has more complex internal state (where
 * responses to multiple transmissions of a particular "command" depend
 * on some intervening event), this reordering will not be accurate.
 *
 * But the intent is that some small changes to client code should still
 * work with existing recordings before requiring creation of new ones.
 */
class TransmitDataEvent : public XmlReplayBase<TransmitDataEvent>
{
    virtual bool usesData() const { return true; }
public:
    virtual const QString id() const { return m_data; }
    virtual bool randomAccess() const { return true; }
};
REGISTER_XMLREPLAYEVENT("tx", TransmitDataEvent);

/*
 * Event representing a "readyRead" signal emitted by a physical device.
 *
 * These events are marked as "signal" events (see discussion of m_signal
 * above) so that connections will automatically send them to clients when
 * the preceding event (API call) is processed.
 */
class ReadyReadEvent : public XmlReplayBase<ReadyReadEvent>
{
public:
    // Use the connection's slot that receives readyRead signals.
    ReadyReadEvent() { m_signal = "onReadyRead"; }
};
REGISTER_XMLREPLAYEVENT("readyRead", ReadyReadEvent);


// MARK: -
// MARK: Serial port connection

/*
 * Serial port connection class
 *
 * This class wraps calls to an underlying QSerialPort with the logic
 * necessary to record and replay arbitrary serial port activity.
 * (or, at least, the serial port functionality currently used by OSCAR).
 *
 * Clients obtain a connection instance via DeviceConnectionManager::openConnection()
 * or openSerialPortConnection (for convenience, if they require a serial port).
 */

REGISTER_DEVICECONNECTION("serial", SerialPortConnection);

SerialPortConnection::SerialPortConnection(const QString & name, XmlRecorder* record, XmlReplay* replay)
    : DeviceConnection(name, record, replay)
{
    connect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

SerialPortConnection::~SerialPortConnection()
{
    // This will only be false if the connection failed to open immediately after construction.
    if (m_opened) {
        close();
        DeviceConnectionManager::getInstance().connectionClosed(this);
    }
    disconnect(&m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
}

/*
 * Try to open the physical port (or replay a previous attempt), returning
 * false if the port was not opened.
 *
 * DeviceConnectionManager::openConnection calls this immediately after
 * creating a connection instance, and will only return open connections
 * to clients.
 */
bool SerialPortConnection::open()
{
    if (m_opened) {
        qWarning() << "serial connection to" << m_name << "already opened";
        return false;
    }
    XmlReplayLock lock(this, m_replay);
    OpenConnectionEvent* replayEvent = nullptr;
    OpenConnectionEvent event("serial", m_name);

    if (!m_replay) {
        // TODO: move this into SerialPortConnection::openDevice() and move
        // the rest of the logic up to DeviceConnection::open().
        m_port.setPortName(m_name);
        checkResult(m_port.open(QSerialPort::ReadWrite), event);
    } else {
        replayEvent = m_replay->getNextEvent<OpenConnectionEvent>(event.id());
        if (replayEvent) {
            event.copyIf(replayEvent);
        } else {
            event.set("error", QSerialPort::DeviceNotFoundError);
        }
    }

    event.record(m_record);
    m_opened = event.ok();

    if (m_opened) {
        // open a connection substream for connection events
        if (m_record) {
            ConnectionEvent connEvent(event);
            m_record = new ConnectionRecorder(m_record, connEvent);
        }
        if (m_replay) {
            Q_ASSERT(replayEvent);
            ConnectionEvent connEvent(*replayEvent);  // we need to use the replay's timestamp to find the referenced substream
            m_replay = new ConnectionReplay(m_replay, connEvent);
        }
    }

    return event.ok();
}

bool SerialPortConnection::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("baudRate", baudRate);
    event.set("directions", directions);

    if (!m_replay) {
        checkResult(m_port.setBaudRate(baudRate, directions), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setDataBits(QSerialPort::DataBits dataBits)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setDataBits", dataBits);

    if (!m_replay) {
        checkResult(m_port.setDataBits(dataBits), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setParity(QSerialPort::Parity parity)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setParity", parity);

    if (!m_replay) {
        checkResult(m_port.setParity(parity), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setStopBits(QSerialPort::StopBits stopBits)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setStopBits", stopBits);

    if (!m_replay) {
        checkResult(m_port.setStopBits(stopBits), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::setFlowControl(QSerialPort::FlowControl flowControl)
{
    XmlReplayLock lock(this, m_replay);
    SetValueEvent event("setFlowControl", flowControl);

    if (!m_replay) {
        checkResult(m_port.setFlowControl(flowControl), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<SetValueEvent>(event.id());
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

bool SerialPortConnection::clear(QSerialPort::Directions directions)
{
    XmlReplayLock lock(this, m_replay);
    ClearConnectionEvent event;
    event.set("directions", directions);

    if (!m_replay) {
        checkResult(m_port.clear(directions), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<ClearConnectionEvent>();
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

qint64 SerialPortConnection::bytesAvailable() const
{
    XmlReplayLock lock(this, m_replay);
    GetValueEvent event("bytesAvailable");
    qint64 result;
    
    if (!m_replay) {
        result = m_port.bytesAvailable();
        event.setValue(result);
        checkResult(result, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<GetValueEvent>(event.id());
        event.copyIf(replayEvent);

        bool ok;
        result = event.value().toLong(&ok);
        if (!ok) {
            qWarning() << event.tag() << event.id() << "has bad value";
        }
    }
    event.record(m_record);

    return result;
}

qint64 SerialPortConnection::read(char *data, qint64 maxSize)
{
    XmlReplayLock lock(this, m_replay);
    qint64 len;
    ReceiveDataEvent event;

    if (!m_replay) {
        len = m_port.read(data, maxSize);
        if (len > 0) {
            event.setData(data, len);
        }
        event.set("len", len);
        if (len != maxSize) {
            event.set("req", maxSize);
        }
        checkResult(len, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<ReceiveDataEvent>();
        event.copyIf(replayEvent);
        if (!replayEvent) {
            qWarning() << "reading data past replay";
            event.set("len", -1);
            event.set("error", QSerialPort::ReadError);
        }

        bool ok;
        len = event.get("len").toLong(&ok);
        if (ok) {
            if (event.ok()) {
                if (len != maxSize) {
                    qWarning() << "replay of" << len << "bytes but" << maxSize << "requested";
                }
                if (len > maxSize) {
                    len = maxSize;
                }
                QByteArray replayData = event.getData();
                memcpy(data, replayData, len);
            }
        } else {
            qWarning() << event << "has bad len";
            len = -1;
        }
    }
    event.record(m_record);

    return len;
}

qint64 SerialPortConnection::write(const char *data, qint64 maxSize)
{
    XmlReplayLock lock(this, m_replay);
    qint64 len;
    TransmitDataEvent event;
    event.setData(data, maxSize);
    
    if (!m_replay) {
        len = m_port.write(data, maxSize);
        event.set("len", len);
        if (len != maxSize) {
            event.set("req", maxSize);
        }
        checkResult(len, event);
    } else {
        auto replayEvent = m_replay->getNextEvent<TransmitDataEvent>(event.id());
        event.copyIf(replayEvent);
        if (!replayEvent) {
            qWarning() << "writing data past replay";
            event.set("len", -1);
            event.set("error", QSerialPort::WriteError);
        }

        bool ok;
        len = event.get("len").toLong(&ok);
        // No need to copy any data, since the event already contains it.
        if (!ok) {
            qWarning() << event << "has bad len";
            len = -1;
        }
    }
    event.record(m_record);

    return len;
}

bool SerialPortConnection::flush()
{
    XmlReplayLock lock(this, m_replay);
    FlushConnectionEvent event;

    if (!m_replay) {
        checkResult(m_port.flush(), event);
    } else {
        auto replayEvent = m_replay->getNextEvent<FlushConnectionEvent>();
        event.copyIf(replayEvent);
    }
    event.record(m_record);

    return event.ok();
}

void SerialPortConnection::close()
{
    if (m_opened) {
        // close event substream first
        if (m_record) {
            m_record = m_record->closeSubstream();
        }
        if (m_replay) {
            m_replay = m_replay->closeSubstream();
        }
    }

    XmlReplayLock lock(this, m_replay);
    CloseConnectionEvent event("serial", m_name);

    // TODO: We'll also need to include a loader ID and stream version number
    // in the "connection" tag, so that if we ever have to change a loader's download code,
    // the older replays will still work as expected.
    // NOTE: This may only be required for downloads rather than all connections.

    if (!m_replay) {
        // TODO: move this into SerialPortConnection::closeDevice() and move
        // the remaining logic up to DeviceConnection::close().
        m_port.close();
        checkError(event);
    } else {
        auto replayEvent = m_replay->getNextEvent<CloseConnectionEvent>(event.id());
        if (replayEvent) {
            event.copyIf(replayEvent);
        } else {
            event.set("error", QSerialPort::ResourceError);
        }
    }

    event.record(m_record);
}

void SerialPortConnection::onReadyRead()
{
    {
        // Wait until the replay signaler (if any) has released its lock.
        XmlReplayLock lock(this, m_replay);
    
        // This needs to be recorded before the signal below, since the slot may trigger more events.
        ReadyReadEvent event;
        event.record(m_record);
        
        // Unlocking will queue any subsequent signals.
    }

    // Because clients typically leave this as Qt::AutoConnection, the below emit may
    // execute immediately in this thread, so we have to release the lock before sending
    // the signal.

    // Unlike client-called events, We don't need to handle replay differently here,
    // because the replay will signal this slot just like the serial port.
    emit readyRead();
}

// Check the boolean returned by a serial port call and the port's error status, and update the event accordingly.
void SerialPortConnection::checkResult(bool ok, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (ok && error == QSerialPort::NoError) return;
    event.set("error", error);
    if (ok) event.set("ok", ok);  // we don't expect to see this, but we should know if it happens
}

// Check the length returned by a serial port call and the port's error status, and update the event accordingly.
void SerialPortConnection::checkResult(qint64 len, XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (len < 0 || error != QSerialPort::NoError) {
        event.set("error", error);
    }
}

// Check the port's error status, and update the event accordingly.
void SerialPortConnection::checkError(XmlReplayEvent & event) const
{
    QSerialPort::SerialPortError error = m_port.error();
    if (error != QSerialPort::NoError) {
        event.set("error", error);
    }
}


// MARK: -
// MARK: SerialPort legacy class

/*
 * SerialPort drop-in replacement for QSerialPort
 *
 * This class mimics the interface of QSerialPort for client code, while
 * using DeviceConnectionManager to open the SerialPortConnection, allowing
 * for transparent recording and replay.
 */

SerialPort::SerialPort()
    : m_conn(nullptr)
{
}

SerialPort::~SerialPort()
{
    if (m_conn) {
        close();
    }
}

void SerialPort::setPortName(const QString &name)
{
    m_portName = name;
}

bool SerialPort::open(QIODevice::OpenMode mode)
{
    Q_ASSERT(!m_conn);
    Q_ASSERT(mode == QSerialPort::ReadWrite);
    m_conn = DeviceConnectionManager::openSerialPortConnection(m_portName);
    if (m_conn) {
        // Listen for readyRead events from the connection so that we can relay them to the client.
        connect(m_conn, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    }
    return m_conn != nullptr;
}

bool SerialPort::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    Q_ASSERT(m_conn);
    return m_conn->setBaudRate(baudRate, directions);
}

bool SerialPort::setDataBits(QSerialPort::DataBits dataBits)
{
    Q_ASSERT(m_conn);
    return m_conn->setDataBits(dataBits);
}

bool SerialPort::setParity(QSerialPort::Parity parity)
{
    Q_ASSERT(m_conn);
    return m_conn->setParity(parity);
}

bool SerialPort::setStopBits(QSerialPort::StopBits stopBits)
{
    Q_ASSERT(m_conn);
    return m_conn->setStopBits(stopBits);
}

bool SerialPort::setFlowControl(QSerialPort::FlowControl flowControl)
{
    Q_ASSERT(m_conn);
    return m_conn->setFlowControl(flowControl);
}

bool SerialPort::clear(QSerialPort::Directions directions)
{
    Q_ASSERT(m_conn);
    return m_conn->clear(directions);
}

qint64 SerialPort::bytesAvailable() const
{
    Q_ASSERT(m_conn);
    return m_conn->bytesAvailable();
}

qint64 SerialPort::read(char *data, qint64 maxSize)
{
    Q_ASSERT(m_conn);
    return m_conn->read(data, maxSize);
}

qint64 SerialPort::write(const char *data, qint64 maxSize)
{
    Q_ASSERT(m_conn);
    return m_conn->write(data, maxSize);
}

bool SerialPort::flush()
{
    Q_ASSERT(m_conn);
    return m_conn->flush();
}

void SerialPort::close()
{
    Q_ASSERT(m_conn);
    disconnect(m_conn, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    delete m_conn;  // this will close the connection
    m_conn = nullptr;
}

void SerialPort::onReadyRead()
{
    // Relay readyRead events from the connection on to the client.
    emit readyRead();
}
