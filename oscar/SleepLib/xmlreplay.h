/* XML event recording/replay
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef XMLREPLAY_H
#define XMLREPLAY_H

#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QObject>

/*
 * XML recording base class
 *
 * While this can be used on its own via the public constructors, it is
 * typically used as a base class for a subclasses that handle specific
 * events.
 *
 * A single instance of this class can write a linear sequence of events to
 * XML, either to a string (for testing) or to a file (for production use).
 *
 * Sometimes, however, there is need for certain sequences to be treated as
 * separate, either due to multithreading (such recording as multiple
 * simultaneous connections), or in order to treat a certain excerpt (such
 * as data download that we might wish to archive) separately.
 *
 * These sequences are handled as "substreams" of the parent stream. The
 * parent stream will typically record a substream's open/close or start/
 * stop along with its ID. The substream will be written to a separate XML
 * stream identified by that ID. Substreams are implemented as subclasses of
 * this base class.
 *
 * TODO: At the moment, only file-based substreams are supported. In theory
 * it should be possible to cache string-based substreams and then insert
 * them inline into the parent after the substream-close event is recorded.
 */
class XmlRecorder
{
public:
    static const QString TAG;  // default tag if no subclass

    XmlRecorder(class QFile * file, const QString & tag = XmlRecorder::TAG);  // record XML to the given file
    XmlRecorder(QString & string, const QString & tag = XmlRecorder::TAG);    // record XML to the given string
    virtual ~XmlRecorder();  // write the epilogue and close the recorder
    XmlRecorder* closeSubstream();  // convenience function to close out a substream and return its parent

    inline QXmlStreamWriter & xml() { return *m_xml; }
    inline void lock() { m_mutex.lock(); }
    inline void unlock() { m_mutex.unlock(); }
    void flush();

protected:
    XmlRecorder(XmlRecorder* parent, const QString & id, const QString & tag);  // constructor used by substreams
    QXmlStreamWriter* addSubstream(XmlRecorder* child, const QString & id);     // initialize a child substream, used by above constructor
    const QString m_tag;      // opening/closing tag for this instance
    QFile* m_file;            // nullptr for non-file recordings
    QXmlStreamWriter* m_xml;  // XML output stream
    QMutex m_mutex;           // force one thread at a time to write to m_xml
    XmlRecorder* m_parent;    // parent instance of a substream
    
    void prologue();
    void epilogue();
};


/*
 * XML replay base class
 *
 * A single instance of this class caches events from a previously recorded
 * XML stream, either from a string (for testing) or from a file (for
 * production use).
 *
 * Unlike recording, the replay need not be strictly linear. In fact, the
 * implementation is designed to allow for limited reordering during replay,
 * so that minor changes to code should result in sensible replay until a
 * new recording can be made.
 *
 * There are two aspects to this reordering:
 *
 * First, events can be retrieved (and consumed) in any order, being
 * retrieved by type and ID (and then in order within that type and ID).
 *
 * Second, events that are flagged as random-access (see randomAccess below)
 * will cause the above retrieval to subsequently begin searching on or
 * after the random-access event's timestamp (except for other random-access
 * events, which are always searched from the beginning.)
 *
 * This allow non-stateful events to be replayed arbitrarily, and for
 * stateful events (such as commands sent to a device) to be approximated
 * (where subsequent data received matches the command sent).
 *
 * Furthermore, when events are triggered in the same order as they were
 * during recordering, the above reordering will have no effect, and the
 * original ordering will be replayed identically.
 *
 * See XmlRecorder above for a discussion of substreams.
 */
class XmlReplay
{
public:
    XmlReplay(class QFile * file, const QString & tag = XmlRecorder::TAG);      // replay XML from the given file
    XmlReplay(QXmlStreamReader & xml, const QString & tag = XmlRecorder::TAG);  // replay XML from the given stream
    virtual ~XmlReplay();
    XmlReplay* closeSubstream();  // convenience function to close out a substream and return its parent

    //! \brief Retrieve next matching event of the given XmlReplayEvent subclass.
    template<class T> inline T* getNextEvent(const QString & id = "")
    {
        T* event = dynamic_cast<T*>(getNextEvent(T::TAG, id));
        return event;
    }

protected:
    XmlReplay(XmlReplay* parent, const QString & id, const QString & tag = XmlRecorder::TAG);  // constructor used by substreams
    QXmlStreamReader* findSubstream(XmlReplay* child, const QString & id);                     // initialize a child substream, used by above constructor
    void deserialize(QXmlStreamReader & xml);
    void deserializeEvents(QXmlStreamReader & xml);

    class XmlReplayEvent* getNextEvent(const QString & type, const QString & id = "");
    void seekToTime(const QDateTime & time);

    const QString m_tag;  // opening/closing tag for this instance
    QFile* m_file;        // nullptr for non-file replay
    QHash<QString,QHash<QString,QList<XmlReplayEvent*>>> m_eventIndex;  // type and ID-based index into the events, see discussion of reordering above
    QHash<QString,QHash<QString,int>> m_indexPosition;                  // positions at which to begin searching the index, updated by random-access events
    QList<XmlReplayEvent*> m_events;                                    // linear list of all events in their original order
    XmlReplayEvent* m_pendingSignal;  // the signal (if any) that should be replayed as soon as the current event has been processed
    QMutex m_lock;                    // prevent signals from being dispatched while an event is being processed, see XmlReplayLock below
    XmlReplay* m_parent;  // parent instance of a substream

    inline void lock() { m_lock.lock(); }
    inline void unlock() { m_lock.unlock(); }
    void processPendingSignals(const QObject* target);

    friend class XmlReplayLock;
};


/*
 * XML replay event base class
 *
 * This class is used to represent a replayable event. An event is created
 * when performing any replayable action, and then recorded (via record())
 * when appropriate. During replay, an event is retrieved from the XmlReplay
 * instance and its previously recorded result should be returned instead of
 * performing the original action.
 *
 * Subclasses are created as subclasses of the XmlReplayBase template (see
 * below), which handles their factory method and tag registration.
 *
 * Subclasses that should be retrieved by ID as well as type will need to
 * override id() to return the ID to use for indexing.
 *
 * Subclasses that represent signal events (rather than API calls) will need
 * to set their m_signal string to the name of the signal to be emitted, and
 * additionally override signal() if they need to pass parameters with the
 * signal.
 *
 * Subclasses that represent random-access events (see discussion above)
 * will need to override randomAccess() to return true.
 *
 * Subclasses whose XML contains raw hexadecimal data will need to override
 * usesData() to return true. Subclasses whose XML contains other data
 * (such as complex data types) will instead need to override read() and
 * write().
 */
class XmlReplayEvent
{
public:
    XmlReplayEvent();
    virtual ~XmlReplayEvent() = default;
    
    //! \brief Add the given key/value to the event. This will be written as an XML attribute in the order it added.
    void set(const QString & name, const QString & value);
    //! \brief Add the given key/integer to the event. This will be written as an XML attribute in the order it added.
    void set(const QString & name, qint64 value);
    //! \brief Add the raw data to the event. This will be written in hexadecimal as content of the event's XML tag.
    void setData(const char* data, qint64 length);
    //! \brief Get the value for the given key.
    QString get(const QString & name) const;
    //! \brief Get the raw data for this event.
    QByteArray getData() const;
    //! \brief True if there are no errors in this event, or false if the "error" attribute is set.
    inline bool ok() const { return m_values.contains("error") == false; }

    //! \brief Copy the result from the retrieved replay event (if any) into the current event.
    void copyIf(const XmlReplayEvent* other);
    //! \brief Record this event to the given XML recorder, doing nothing if the recorder is null.
    void record(XmlRecorder* xml) const;

    // Serialize this event to an XML stream.
    friend QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const XmlReplayEvent & event);
    // Deserialize this event's contents from an XML stream. The instance is first created via createInstance() based on the tag.
    friend QXmlStreamReader & operator>>(QXmlStreamReader & xml, XmlReplayEvent & event);
    // Write the tag's attributes and contents.
    void writeTag(QXmlStreamWriter & xml) const;
    //! \brief Return a string of this event as an XML tag.
    operator QString() const;

    // Subclassing support

    //! \brief Return the XML tag used for this event. Automatically generated for subclasses by template.
    virtual const QString & tag() const = 0;
    //! \brief Return the ID for this event, if applicable. Subclasses should override this if their events should be retrieved by ID.
    virtual const QString id() const { static const QString none(""); return none; }
    //! \brief True if this event represents a "random-access" event that should cause subsequent event searches to start after this event's timestamp. Subclasses that represent such a state change should override this method.
    virtual bool randomAccess() const { return false; }

    // Event subclass registration and instance creation
    typedef XmlReplayEvent* (*FactoryMethod)();
    static bool registerClass(const QString & tag, FactoryMethod factory);
    static XmlReplayEvent* createInstance(const QString & tag);

protected:
    static QHash<QString,FactoryMethod> & factories();  // registered subclass factory methods, arranged by XML tag

    //! \brief True this event contains raw data. Defaults to false, so subclasses that use raw data must override this.
    virtual bool usesData() const { return false; }
    //! \brief True if this event represents a signal event. Subclasses representing such events must set m_signal.
    inline bool isSignal() const { return m_signal != nullptr; }
    //! \brief Send a signal to the target object. Subclasses may override this to send signal arguments.
    virtual void signal(QObject* target);
    //! \brief Write any attributes or content needed specific to event. Subclasses may override this to support complex data types.
    virtual void write(QXmlStreamWriter & xml) const;
    //! \brief Read any attributes or content specific to this event. Subclasses may override this to support complex data types.
    virtual void read(QXmlStreamReader & xml);

    QDateTime m_time;        // timestamp of event
    XmlReplayEvent* m_next;  // next recorded event, used during replay to trigger signals that automatically fire after an event is processed
    const char* m_signal;    // name of the signal to be emitted for this event, if any
    QHash<QString,QString> m_values;  // hash of key/value pairs for this event, written as attributes of the XML tag
    QList<QString> m_keys;            // list of keys so that attributes will be written in the order they were set
    QString m_data;                   // hexademical string representing this event's raw data, written as contents of the XML tag

    // Copy the timestamp as well as the attributes. Used when creating substreams.
    void copy(const XmlReplayEvent & other);

    friend class XmlReplay;
};

// Convenience template for serializing QLists to XML
template<typename T> QXmlStreamWriter & operator<<(QXmlStreamWriter & xml, const QList<T> & list)
{
    for (auto & item : list) {
        xml << item;
    }
    return xml;
}

// Convenience template for deserializing QLists from XML
template<typename T> QXmlStreamReader & operator>>(QXmlStreamReader & xml, QList<T> & list)
{
    list.clear();
    while (xml.readNextStartElement()) {
        T item;
        xml >> item;
        list.append(item);
    }
    return xml;
}


/*
 * Intermediate parent class of concrete event subclasses.
 *
 * We use this extra CRTP templating so that concrete event subclasses
 * require as little code as possible:
 *
 * The subclass's tag and factory method are automatically generated by this
 * template.
 */
template <typename Derived>
class XmlReplayBase : public XmlReplayEvent
{
public:
    static const QString TAG;
    static const bool registered;
    virtual const QString & tag() const { return TAG; };

    static XmlReplayEvent* createInstance()
    {
        Derived* instance = new Derived();
        return static_cast<XmlReplayEvent*>(instance);
    }
};

/*
 * Macro to define an XmlReplayEvent subclass's tag and automatically
 * register the subclass at global-initialization time, before main()
 */
#define REGISTER_XMLREPLAYEVENT(tag, type) \
template<> const QString XmlReplayBase<type>::TAG = tag; \
template<> const bool XmlReplayBase<type>::registered = XmlReplayEvent::registerClass(XmlReplayBase<type>::TAG, XmlReplayBase<type>::createInstance);


/*
 * XML replay lock class
 *
 * An instance of this class should be created on the stack during any replayable
 * event. Exiting scope will release the lock, at which point any signals that
 * need to be replayed will be queued.
 *
 * Has no effect if events are not being replayed.
 */
class XmlReplayLock
{
public:
    //! \brief Temporarily lock the XML replay (if any) until exiting scope, at which point any pending signals will be sent to the specified object.
    XmlReplayLock(const QObject* obj, XmlReplay* replay);
    ~XmlReplayLock();

protected:
    const QObject* m_target;  // target object to receive any pending signals
    XmlReplay* m_replay;      // replay instance, or nullptr if not replaying
};

#endif // XMLREPLAY_H
