/* XML event recording/replay
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "xmlreplay.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QBuffer>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>


// Derive the filepath for the given substream ID relative to the parent stream.
static QString substreamFilepath(QFile* parent, const QString & id)
{
    Q_ASSERT(parent);
    QFileInfo info(*parent);
    QString path = info.canonicalPath() + QDir::separator() + info.completeBaseName() + "-" + id + ".xml";
    return path;
}


// MARK: -
// MARK: XML record/playback base classes

const QString XmlRecorder::TAG = "xmlreplay";

XmlRecorder::XmlRecorder(QFile* stream, const QString & tag)
    : m_tag(tag), m_file(stream), m_xml(new QXmlStreamWriter(stream)), m_parent(nullptr)
{
    prologue();
}

XmlRecorder::XmlRecorder(QString & string, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_xml(new QXmlStreamWriter(&string)), m_parent(nullptr)
{
    prologue();
}

// Protected constructor for substreams
XmlRecorder::XmlRecorder(XmlRecorder* parent, const QString & id, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_xml(nullptr), m_parent(parent)
{
    Q_ASSERT(m_parent);
    m_xml = m_parent->addSubstream(this, id);
    if (m_xml == nullptr) {
        qWarning() << "Not recording" << id;
        static QString null;
        m_xml = new QXmlStreamWriter(&null);
    }

    prologue();
}

// Initialize a child recording substream.
QXmlStreamWriter* XmlRecorder::addSubstream(XmlRecorder* child, const QString & id)
{
    Q_ASSERT(child);
    QXmlStreamWriter* xml = nullptr;

    if (m_file) {
        QString childPath = substreamFilepath(m_file, id);
        child->m_file = new QFile(childPath);
        if (child->m_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
            xml = new QXmlStreamWriter(child->m_file);
            qDebug() << "Recording to" << childPath;
        } else {
            qWarning() << "Unable to open" << childPath << "for writing";
        }
    } else {
        qWarning() << "String-based substreams are not supported";
        // Maybe some day support string-based substreams:
        // - parent passes string to child
        // - on connectionClosed, parent asks recorder to flush string to stream
    }
    
    return xml;
}

XmlRecorder::~XmlRecorder()
{
    epilogue();
    delete m_xml;
    // File substreams manage their own file.
    if (m_parent && m_file) {
        delete m_file;
    }
}

// Close out a substream and return its parent.
XmlRecorder* XmlRecorder::closeSubstream()
{
    auto parent = m_parent;
    delete this;
    return parent;
}

void XmlRecorder::prologue()
{
    Q_ASSERT(m_xml);
    m_xml->setAutoFormatting(true);
    m_xml->setAutoFormattingIndent(2);
    m_xml->writeStartElement(m_tag);  // open enclosing tag
}

void XmlRecorder::epilogue()
{
    Q_ASSERT(m_xml);
    m_xml->writeEndElement();  // close enclosing tag
}

void XmlRecorder::flush()
{
    if (m_file) {
        if (!m_file->flush()) {
            qWarning().noquote() << "Unable to flush XML to" << m_file->fileName();
        }
    }
}

XmlReplay::XmlReplay(QFile* file, const QString & tag)
    : m_tag(tag), m_file(file), m_pendingSignal(nullptr), m_parent(nullptr)
{
    Q_ASSERT(file);
    QFileInfo info(*file);
    qDebug() << "Replaying from" << info.canonicalFilePath();

    QXmlStreamReader xml(file);
    deserialize(xml);
}

XmlReplay::XmlReplay(QXmlStreamReader & xml, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_pendingSignal(nullptr), m_parent(nullptr)
{
    deserialize(xml);
}

// Protected constructor for substreams
XmlReplay::XmlReplay(XmlReplay* parent, const QString & id, const QString & tag)
    : m_tag(tag), m_file(nullptr), m_pendingSignal(nullptr), m_parent(parent)
{
    Q_ASSERT(m_parent);

    auto xml = m_parent->findSubstream(this, id);
    if (xml) {
        deserialize(*xml);
        delete xml;
    } else {
        qWarning() << "Not replaying" << id;
    }
}

// Initialize a child replay substream.
QXmlStreamReader* XmlReplay::findSubstream(XmlReplay* child, const QString & id)
{
    Q_ASSERT(child);
    QXmlStreamReader* xml = nullptr;

    if (m_file) {
        QString childPath = substreamFilepath(m_file, id);
        child->m_file = new QFile(childPath);
        if (child->m_file->open(QIODevice::ReadOnly)) {
            xml = new QXmlStreamReader(child->m_file);
            qDebug() << "Replaying from" << childPath;
        } else {
            qWarning() << "Unable to open" << childPath << "for reading";
        }
    } else {
        qWarning() << "String-based substreams are not supported";
        // Maybe some day support string-based substreams:
        // - when deserializing, use e.g. ConnectionEvents to cache the substream strings
        // - then return a QXmlStreamReader here using that string
    }
 
    return xml;
}

XmlReplay::~XmlReplay()
{
    for (auto event : m_events) {
        delete event;
    }
    // File substreams manage their own file.
    if (m_parent && m_file) {
        delete m_file;
    }
}

// Close out a substream and return its parent.
XmlReplay* XmlReplay::closeSubstream()
{
    auto parent = m_parent;
    delete this;
    return parent;
}

void XmlReplay::deserialize(QXmlStreamReader & xml)
{
    if (xml.readNextStartElement()) {
        if (xml.name() == m_tag) {
            deserializeEvents(xml);
        } else {
            qWarning() << "unexpected payload in replay XML:" << xml.name();
            xml.skipCurrentElement();
        }
    }
}

void XmlReplay::deserializeEvents(QXmlStreamReader & xml)
{
    while (xml.readNextStartElement()) {
        QString type = xml.name().toString();
        XmlReplayEvent* event = XmlReplayEvent::createInstance(type);
        if (event) {
            xml >> *event;

            // Add to list
            if (m_events.isEmpty() == false) {
                m_events.last()->m_next = event;
            }
            m_events.append(event);

            // Add to index
            const QString & id = event->id();
            auto & events = m_eventIndex[type][id];
            events.append(event);
        } else {
            xml.skipCurrentElement();
        }
    }
}

// Queue any pending signals when a replay lock is released.
void XmlReplay::processPendingSignals(const QObject* target)
{
    if (m_pendingSignal) {
        XmlReplayEvent* pending = m_pendingSignal;
        m_pendingSignal = nullptr;

        // Dequeue the signal from the index; this may update m_pendingSignal.
        XmlReplayEvent* dequeued = getNextEvent(pending->tag(), pending->id());
        if (dequeued != pending) {
            qWarning() << "triggered signal doesn't match dequeued signal!" << pending << dequeued;
        }
        
        // It is safe to re-cast this as non-const because signals are deferred
        // and cannot alter the underlying target until the const method holding
        // the lock releases it at function exit.
        pending->signal(const_cast<QObject*>(target));
    }
}

// Update the positions at which to begin searching the index, so that only events on or after the given time are returned by getNextEvent.
void XmlReplay::seekToTime(const QDateTime & time)
{
    for (auto & type : m_eventIndex.keys()) {
        for (auto & key : m_eventIndex[type].keys()) {
            // Find the index of the first event on or after the given time.
            auto & events = m_eventIndex[type][key];
            int pos;
            for (pos = 0; pos < events.size(); pos++) {
                auto & event = events.at(pos);
                // Random-access events should always start searching from position 0.
                if (event->randomAccess() || event->m_time >= time) {
                    break;
                }
            }
            // If pos == events.size(), that means there are no more events of this type
            // after the given time.
            m_indexPosition[type][key] = pos;
        }
    }
}

// Find and return the next event of the given type with the given ID, or nullptr if no more events match.
XmlReplayEvent* XmlReplay::getNextEvent(const QString & type, const QString & id)
{
    XmlReplayEvent* event = nullptr;
    
    // Event handlers should always be wrapped in an XmlReplayLock, so warn if that's not the case.
    if (m_lock.tryLock()) {
        qWarning() << "XML replay" << type << "object not locked by event handler!";
        m_lock.unlock();
    }

    // Search the index for the next matching event (if any).
    if (m_eventIndex.contains(type)) {
        auto & ids = m_eventIndex[type];
        if (ids.contains(id)) {
            auto & events = ids[id];

            // Start at the position identified by the previous random-access event.
            int pos = m_indexPosition[type][id];
            if (pos < events.size()) {
                event = events.at(pos);
                // TODO: if we're simulating the original timing, return nullptr if we haven't reached this event's time yet;
                // otherwise:
                events.removeAt(pos);
            }
        }
    }
    
    // If this is a random-access event, we need to update the index positions for all non-random-access events.
    if (event && event->randomAccess()) {
        seekToTime(event->m_time);
    }

    // If the event following this one is a signal (that replay needs to trigger), save it as pending
    // so that it can be emitted when the replay lock for this event is released.
    if (event && event->m_next && event->m_next->isSignal()) {
        Q_ASSERT(m_pendingSignal == nullptr);  // if this ever fails, we may need m_pendingSignal to be a list
        m_pendingSignal = event->m_next;
    }

    return event;
}


// MARK: -
// MARK: XML record/playback event base class

void XmlReplayEvent::set(const QString & name, const QString & value)
{
    if (!m_values.contains(name)) {
        m_keys.append(name);
    }
    m_values[name] = value;
}

void XmlReplayEvent::set(const QString & name, qint64 value)
{
    set(name, QString::number(value));
}

void XmlReplayEvent::setData(const char* data, qint64 length)
{
    Q_ASSERT(usesData() == true);
    QByteArray bytes = QByteArray::fromRawData(data, length);
    m_data = bytes.toHex(' ').toUpper();
}

QString XmlReplayEvent::get(const QString & name) const
{
    if (!m_values.contains(name)) {
        qWarning().noquote() << *this << "missing attribute:" << name;
    }
    return m_values[name];
}

QByteArray XmlReplayEvent::getData() const
{
    Q_ASSERT(usesData() == true);
    if (m_data.isEmpty()) {
        qWarning().noquote() << "replaying event with missing data" << *this;
        QByteArray empty;
        return empty;  // toUtf8() below crashes with an empty string.
    }
    return QByteArray::fromHex(m_data.toUtf8());
}

void XmlReplayEvent::copyIf(const XmlReplayEvent* other)
{
    // Leave the proposed event alone if there was no replay event.
    if (other == nullptr) {
        return;
    }
    // Do not copy timestamp.
    m_values = other->m_values;
    m_keys = other->m_keys;
    m_data = other->m_data;
}

void XmlReplayEvent::copy(const XmlReplayEvent & other)
{
    copyIf(&other);
    // Copy the timestamp, as it is necessary for replaying substreams that use the timestamp as part of their ID.
    m_time = other.m_time;
}

void XmlReplayEvent::signal(QObject* target)
{
    // Queue the signal so that it won't be processed before the current event returns to its caller.
    // (See XmlReplayLock below.)
    QMetaObject::invokeMethod(target, m_signal, Qt::QueuedConnection);
}

void XmlReplayEvent::write(QXmlStreamWriter & xml) const
{
    // Write key/value pairs as attributes in the order they were set.
    for (auto key : m_keys) {
        xml.writeAttribute(key, m_values[key]);
    }
    if (!m_data.isEmpty()) {
        Q_ASSERT(usesData() == true);
        xml.writeCharacters(m_data);
    }
}

void XmlReplayEvent::read(QXmlStreamReader & xml)
{
    QXmlStreamAttributes attribs = xml.attributes();
    for (auto & attrib : attribs) {
        //error: result of comparison against a string literal is unspecified
        if (attrib.name() != QStringLiteral("time")) {    // skip outer timestamp, which is decoded by operator>>
            set(attrib.name().toString(), attrib.value().toString());
        }
    }
    if (usesData()) {
        m_data = xml.readElementText();
    } else {
        xml.skipCurrentElement();
    }
}

void XmlReplayEvent::record(XmlRecorder* writer) const
{
    // Do nothing if we're not recording.
    if (writer != nullptr) {
        writer->lock();
        writer->xml() << *this;
        writer->flush();
        writer->unlock();
    }
}

XmlReplayEvent::XmlReplayEvent()
    : m_time(QDateTime::currentDateTime()), m_next(nullptr), m_signal(nullptr)
{
}

QHash<QString,XmlReplayEvent::FactoryMethod> & XmlReplayEvent::factories()
{
    // Use a local static variable so that it is guaranteed to be initialized when registerClass and createInstance are called.
    static QHash<QString,FactoryMethod> s_factories;
    return s_factories;
}

bool XmlReplayEvent::registerClass(const QString & tag, XmlReplayEvent::FactoryMethod factory)
{
    if (factories().contains(tag)) {
        qWarning() << "Event class already registered for tag" << tag;
        return false;
    }
    factories()[tag] = factory;
    return true;
}

XmlReplayEvent* XmlReplayEvent::createInstance(const QString & tag)
{
    XmlReplayEvent* event = nullptr;
    XmlReplayEvent::FactoryMethod factory = factories().value(tag);
    if (factory == nullptr) {
        qWarning() << "No event class registered for XML tag" << tag;
    } else {
        event = factory();
    }
    return event;
}

void XmlReplayEvent::writeTag(QXmlStreamWriter & xml) const
{
    QDateTime time = m_time.toOffsetFromUtc(m_time.offsetFromUtc());  // force display of UTC offset
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    // TODO: Can we please deprecate support for Qt older than 5.9?
    QString timestamp = time.toString(Qt::ISODate);
#else
    QString timestamp = time.toString(Qt::ISODateWithMs);
#endif
    xml.writeAttribute("time", timestamp);

    // Call this event's overridable write method.
    write(xml);
}

QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event)
{
    xml.writeStartElement(event.tag());
    event.writeTag(xml);
    xml.writeEndElement();
    return xml;
}

QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == event.tag());

    QDateTime time;
    if (xml.attributes().hasAttribute("time")) {
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
        // TODO: Can we please deprecate support for Qt older than 5.9?
        time = QDateTime::fromString(xml.attributes().value("time").toString(), Qt::ISODate);
#else
        time = QDateTime::fromString(xml.attributes().value("time").toString(), Qt::ISODateWithMs);
#endif
    } else {
        qWarning() << "Missing timestamp in" << xml.name() << "tag, using current time";
        time = QDateTime::currentDateTime();
    }
    event.m_time = time;
    
    // Call this event's overridable read method.
    event.read(xml);
    return xml;
}

XmlReplayEvent::operator QString() const
{
    QString out;
    QXmlStreamWriter xml(&out);
    xml << *this;
    return out;
}


XmlReplayLock::XmlReplayLock(const QObject* obj, XmlReplay* replay)
    : m_target(obj), m_replay(replay)
{
    if (m_replay) {
        // Prevent any triggered signal events from processing until the triggering lock is released.
        m_replay->lock();
    }
}

XmlReplayLock::~XmlReplayLock()
{
    if (m_replay) {
        m_replay->processPendingSignals(m_target);
        m_replay->unlock();
    }
}
