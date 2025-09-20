/* SleepLib Viatom Loader Implementation
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 * (Initial importer written by dave madden <dhm@mersenne.com>)
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the viatom_data_version in viatom_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include <QApplication>
#include <QMessageBox>
#include "viatom_loader.h"
#include "SleepLib/machine.h"
#include <memory>
#include <QTimeZone>

// TODO: Merge this with PRS1 macros and generalize for all loaders.
#define SESSIONID m_session->session()
#define UNEXPECTED_VALUE(SRC, VALS) { \
    QString message = QString("%1:%2: %3 = %4 != %5").arg(__func__).arg(__LINE__).arg(#SRC).arg(SRC).arg(VALS); \
    qWarning() << SESSIONID << message; \
    s_unexpectedMessages += message; \
    }
#define CHECK_VALUE(SRC, VAL) if ((SRC) != (VAL)) UNEXPECTED_VALUE(SRC, VAL)
#define CHECK_VALUES(SRC, VAL1, VAL2) if ((SRC) != (VAL1) && (SRC) != (VAL2)) UNEXPECTED_VALUE(SRC, #VAL1 " or " #VAL2)
// for more than 2 values, just write the test manually and use UNEXPECTED_VALUE if it fails
static QSet<QString> s_unexpectedMessages;

bool
ViatomLoader::Detect(const QString & path)
{
    // This is only used for CPAP devices, when detecting CPAP cards.
    qDebug() << "ViatomLoader::Detect(" << path << ")";
    return false;
}

int
ViatomLoader::Open(const QStringList & paths)
{
    qDebug() << "ViatomLoader::Open(" << paths.join("; ") << ")";
    m_mach = nullptr;
    int imported = 0;
    int found = 0;
    s_unexpectedMessages.clear();

    int size = paths.size();
    for (int i=0; i < size; i++) {
        if (isAborted()) {
            break;
        }
        // This filename has already been filtered by QFileDialog.
        int ok = OpenFile(paths[i]);
        if (ok > 0) {
            imported++;
        } else if (ok < 0) {
            // Stop on error...
            break;
        }
        found++;
        emit setProgressValue(i+1);
        QCoreApplication::processEvents();
    }

    if (!found) {
        return -1;
    }

    Machine* mach = m_mach;
    if (imported && mach == nullptr) qWarning() << "No device record created?";
    if (mach) {
        qDebug() << "Imported" << imported << "sessions";
        mach->Save();
        mach->SaveSummaryCache();
        p_profile->StoreMachines();
    }
    if (mach && s_unexpectedMessages.count() > 0 && p_profile->session->warnOnUnexpectedData()) {
        // Compare this to the list of messages previously seen for this device
        // and only alert if there are new ones.
        QSet<QString> newMessages = s_unexpectedMessages - mach->previouslySeenUnexpectedData();
        if (newMessages.count() > 0) {
            // TODO: Rework the importer call structure so that this can become an
            // emit statement to the appropriate import job.
            QMessageBox::information(QApplication::activeWindow(),
                                     QObject::tr("Untested Data"),
                                     QObject::tr("Your Viatom device generated data that OSCAR has never seen before.") +"\n\n"+
                                     QObject::tr("The imported data may not be entirely accurate, so the developers would like a copy of your Viatom files to make sure OSCAR is handling the data correctly.")
                                     ,QMessageBox::Ok);
            mach->previouslySeenUnexpectedData() += newMessages;
        }
    }

    return found;
}

int ViatomLoader::OpenFile(const QString & filename)
{
    Machine* mach = nullptr;
    bool existing = false;

    Session* sess = ParseFile(filename, &existing);
    if (sess) {
        SaveSessionToDatabase(sess);
        mach = sess->machine();
        m_mach = mach;
        return 1;
    }

    return existing ? 0 : -1; // -1 = error
}



bool IsPOD2Structure(const QString& filePath) {
    //POD2 files consist of data only.
    //There is no metadata in the file.
    //To recognize the POD2 data, we have to look for the proper directory structure
    //The directory name 28 is the model (or some other ID for the Wellue POD2)
    //The data are in the host directory.
    //When copying the data from the Android phone to the PC, this structure must be maintained.
    auto path = QDir::cleanPath(filePath);
    if (!QFileInfo(path).path().endsWith("/28/host")){
        return false;
    }
   return true;
}



Session* ViatomLoader::ParseFilePOD2(const QString & filename, bool *existing)
{
    if (existing) {
        *existing = false;
    }
    qDebug() << "Found Viatom POD 2 data file " << filename;
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open Viatom data file" << filename;
        return nullptr;
    }

    MachineInfo info = newInfo();
    //POD2 datafiles don't have a serial number.
    //Import all POD2 data with the same fake "serial number."
    info.serial = "POD2";

    std::unique_ptr<ViatomPOD2File> v = std::unique_ptr<ViatomPOD2File>(new ViatomPOD2File(file));

    if (v->ParseHeader() == false) {
        return nullptr;
    }

    Machine *mach = p_profile->CreateMachine(info);
    if (mach->SessionExists(v->sessionid())) {
        // Skip already imported session
        //qDebug() << filename << "session already exists, skipping" << v.sessionid();
        if (existing) {
            // Inform the caller (if they are interested) that this session was already imported
            *existing = true;
        }
        return nullptr;
    }

    qint64 time_ms = v->timestamp();
    m_session = new Session(mach, v->sessionid());
    m_session->set_first(time_ms);
    bool batterydead = false;
    unsigned char batterymask = 0b11000000;

    QList<ViatomPOD2File::Record> records = v->ReadData();
    m_step = 1000L;
    // Import data
    for (auto & rec : records)
    {
        //qDebug() << "Timestamp: " << time_ms << "  POD2 Pulse: " << rec.hr << "  SPO2: " << rec.spo2;
        //I do not know if the POD 2 has the same limits as the other Viatom devices.
        //Lacking better information, I've kept the limit checks from the other Viatom devices.
        //In testing, there were some streaks of 0 values for SPO2 and HR
        //These seem to be invalid measurements.
        // Viatom advertises a range of 30 - 250 bpm.
        if (rec.hr>0 && (rec.hr < 30 || rec.hr > 250)) {
            UNEXPECTED_VALUE(rec.hr, "30-250");
        }
        else{
            if (rec.hr>0){
                AddEvent(OXI_Pulse, time_ms, rec.hr);
            }
        }


        if (rec.spo2 == 0xFF) {
            //From the original Viatom.  May not apply to the Wellue
            // When the readings fall below 61%, Viatom devices record 0xFF for SpO2.
            // The official software discards these readings.
            // TODO: Consider whether to import these as 60% since they reflect hypoxia.
            EndEventList(OXI_SPO2, time_ms);
            //qDebug() << "<61% at" << QDateTime::fromMSecsSinceEpoch(time_ms);
        } else {
            // Viatom advertises (and graphs) a range of 70% - 99%, but apparently records down to 61%.
            // The official software graphs 61%-70% as 70%.
            // TODO: Consider whether we should import 61%-70% as 70% to match the official reports.
            if (rec.spo2>0 && (rec.spo2 < 61 || rec.spo2 > 99)) {
                UNEXPECTED_VALUE(rec.spo2, "61-99%");
            }
            else
            {
                if (rec.spo2>0)
                {
                    AddEvent(OXI_SPO2, time_ms, rec.spo2);
                }
            }

        }
        float pi = (float)rec.pi / 10;
        if (pi>24)
        {
            //The POD2 manual says the perfusion index ranges from 0 to 20
            //The logged values range from 0 to 200.  2% PI = value of 20
            //The logged values are 10 times the displayed value.
            //2025-08-31: Further experience with the POD2 shows that it will occasionally log
            //perfusion index values above 20%.
            //Changed to throw a warning at 24% instead of 20%

             UNEXPECTED_VALUE(pi, "0-24%");
        }
        else
        {
            AddEvent(OXI_Perf, time_ms, pi);
        }

        time_ms += m_step;
        if ((rec.battery&batterymask) == 0)
        {
            batterydead = true;
        }
    }
    EndEventList(OXI_Pulse, time_ms);
    EndEventList(OXI_SPO2, time_ms);
    EndEventList(OXI_Perf, time_ms);
    m_session->set_last(time_ms);
    if (batterydead)
    {

        QString message = QString(QObject::tr("The imported data for the session starting %1 may be incomplete or incorrect."));
        message=message.arg(QDateTime::fromMSecsSinceEpoch(m_session->first(),QTimeZone::systemTimeZone()).toString());
        QMessageBox::warning(QApplication::activeWindow(),
                                 QObject::tr("Dead battery"),
                                 QObject::tr("Your Viatom device registered a dead battery during operation.") +"\n\n"+
                                 message,
                                 QMessageBox::Ok);

    }


    return m_session;
}

Session* ViatomLoader::ParseFileViatom(const QString & filename, bool *existing)
{
    if (existing) {
        *existing = false;
    }
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Couldn't open Viatom data file" << filename;
        return nullptr;
    }

    // Read in Viatom database version number
    QByteArray data = file.read(2);
    if (data.size() < 2) {
        qDebug() << filename << "too short for a Viatom data file";
        return nullptr;
    }
    const unsigned char* header = (const unsigned char*) data.constData();
    int sig = header[0] | (header[1] << 8);
    std::unique_ptr<ViatomFile> v = nullptr;
    switch (sig) {
    case 0x0003:
    case 0x0005:
        v = std::unique_ptr<ViatomFile>(new ViatomFile(file));
        break;
    case 0x0301:
        v = std::unique_ptr<O2RingS>(new O2RingS(file));
        break;
    default:
        qDebug() << filename << "Unrecognized DB version number in Viatom data file" << sig;
        return nullptr;
    }

    if (!file.seek(0)) {
        qDebug() << filename << "unable to seek to begining of file";
        return nullptr;
    }

    // Parse header specific to database version number
    if (v->ParseHeader() == false) {
        return nullptr;
    }

    MachineInfo info = newInfo();
    // Check whether the enclosing folder looks like a Viatom serial number, and if so, use it.
    QString foldername = QFileInfo(filename).dir().dirName();
    if (foldername.length() >= 9) {
        bool numeric;
        #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            foldername.rightRef(4).toInt(&numeric);
        #else
            foldername.right(4).toInt(&numeric);
        #endif

        if (numeric) {
            info.serial = foldername;
        }
    }
    Machine *mach = p_profile->CreateMachine(info);

    if (mach->SessionExists(v->sessionid())) {
        // Skip already imported session
        //qDebug() << filename << "session already exists, skipping" << v.sessionid();
        if (existing) {
            // Inform the caller (if they are interested) that this session was already imported
            *existing = true;
        }
        return nullptr;
    }

    qint64 time_ms = v->timestamp();
    m_session = new Session(mach, v->sessionid());
    m_session->set_first(time_ms);

    QList<ViatomFile::Record> records = v->ReadData();
    m_step = v->duration() / records.size() * 1000L;

    // Import data
    for (auto & rec : records) {
        if (rec.oximetry_invalid) {
            EndEventList(OXI_Pulse, time_ms);
            EndEventList(OXI_SPO2, time_ms);
        } else {
            // Viatom advertises a range of 30 - 250 bpm.
            if (rec.hr < 30 || rec.hr > 250) {
                UNEXPECTED_VALUE(rec.hr, "30-250");
            }
            AddEvent(OXI_Pulse, time_ms, rec.hr);

            if (rec.spo2 == 0xFF) {
                // When the readings fall below 61%, Viatom devices record 0xFF for SpO2.
                // The official software discards these readings.
                // TODO: Consider whether to import these as 60% since they reflect hypoxia.
                EndEventList(OXI_SPO2, time_ms);
                //qDebug() << "<61% at" << QDateTime::fromMSecsSinceEpoch(time_ms);
            } else {
                // Viatom advertises (and graphs) a range of 70% - 99%, but apparently records down to 61%.
                // The official software graphs 61%-70% as 70%.
                // TODO: Consider whether we should import 61%-70% as 70% to match the official reports.
                if (rec.spo2 < 61 || rec.spo2 > 99) {
                    UNEXPECTED_VALUE(rec.spo2, "61-99%");
                }
                AddEvent(OXI_SPO2, time_ms, rec.spo2);
            }
        }
        AddEvent(POS_Movement, time_ms, rec.motion);
        time_ms += m_step;
    }
    EndEventList(OXI_Pulse, time_ms);
    EndEventList(OXI_SPO2, time_ms);
    EndEventList(POS_Movement, time_ms);
    m_session->set_last(time_ms);

    return m_session;
}

Session* ViatomLoader::ParseFile(const QString & filename, bool *existing)
{
    if (IsPOD2Structure(filename)){
        return ParseFilePOD2(filename, existing);
    }
    else{
        return ParseFileViatom(filename, existing);
    }
}

void ViatomLoader::SaveSessionToDatabase(Session* sess)
{
    Machine* mach = sess->machine();

    sess->SetChanged(true);
    mach->AddSession(sess);
}

void ViatomLoader::AddEvent(ChannelID channel, qint64 t, EventDataType value)
{
    EventList* C = m_importChannels[channel];
    if (C == nullptr) {
        C = m_session->AddEventList(channel, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, m_step);
        Q_ASSERT(C);  // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    // Add the event
    C->AddEvent(t, value);
    m_importLastValue[channel] = value;
}

void ViatomLoader::EndEventList(ChannelID channel, qint64 /*t*/)
{
    EventList* C = m_importChannels[channel];
    if (C != nullptr) {
        // The below would be needed for square charts if the first sample represents
        // the 4 seconds following the starting timestamp:
        //C->AddEvent(t, m_importLastValue[channel]);

        // Mark this channel's event list as ended.
        m_importChannels[channel] = nullptr;
    }
}

QStringList ViatomLoader::getNameFilter()
{
    // Sometimes the files have a SleepU_ or O2Ring_ prefix.
    // Sometimes they have punctuation in the timestamp.
    // Note that ":" is not allowed on macOS, so Mac users will need to rename their files in order to select and import them.
    return QStringList({"*20[0-5][0-9][01][0-9][0-3][0-9][012][0-9][0-5][0-9][0-5][0-9]*",
                        "*20[0-5][0-9]-[01][0-9]-[0-3][0-9] [012][0-9]:[0-5][0-9]:[0-5][0-9]*"
    });
}

static bool viatom_initialized = false;

void
ViatomLoader::Register()
{
    if (!viatom_initialized) {
        qDebug("Registering ViatomLoader");
        RegisterLoader(new ViatomLoader());
        //InitModelMap();
        viatom_initialized = true;
    }
}


// ===============================================================================================

#undef SESSIONID
#define SESSIONID this->m_sessionid

ViatomPOD2File::ViatomPOD2File(QFile & file) : m_file(file)
{
}

QDateTime ViatomPOD2File::getPOD2FilenameTimestamp()
{
    //Filenames are usually just a Unix epoch timestamp.
    QString date_string = QFileInfo(m_file).baseName();
    qint64 msecs = date_string.toLongLong();

    return QDateTime::fromMSecsSinceEpoch(msecs,QTimeZone::systemTimeZone());
}

bool ViatomPOD2File::ParseHeader()
{    
    QDateTime filename_timestamp = getPOD2FilenameTimestamp();
    m_timestamp = filename_timestamp.toMSecsSinceEpoch();
    m_sessionid = m_timestamp / 1000L;

    qint64 datasize = m_file.size();
    m_record_count = datasize / RECORD_SIZE;
    m_resolution = 1100L;
    m_duration   = m_record_count;

    return true;
}

QList<ViatomPOD2File::Record> ViatomPOD2File::ReadData()
{
    QByteArray data = m_file.readAll();
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    QList<ViatomPOD2File::Record> records;

    // Read all Pulse, SPO2 and PI data
    do {
        ViatomPOD2File::Record rec;
        in >> rec.spo2 >> rec.hr >> rec.unk1 >> rec.pi >> rec.unk2 >> rec.battery;

        records.append(rec);
    } while (records.size() < m_record_count);


    return records;
}

ViatomFile::ViatomFile(QFile & file) : m_file(file)
{
}

QDateTime ViatomFile::getFilenameTimestamp()
{
    QString date_string = QFileInfo(m_file).fileName().section("_", -1);  // Strip any SleepU_ etc. prefix.

    int lastPoint = date_string.lastIndexOf("."); // Added to strip off any filename extension
    date_string = date_string.left(lastPoint);

    QString format_string = "yyyyMMddHHmmss";
    if (date_string.contains(":")) {
        format_string = "yyyy-MM-dd HH:mm:ss";
    }
    return QDateTime::fromString(date_string, format_string);
}

bool ViatomFile::ParseHeader()
{
    static const int HEADER_SIZE = 40;
    QByteArray data = m_file.read(HEADER_SIZE);
    if (data.size() < HEADER_SIZE) {
        qDebug() << m_file.fileName() << "too short for a Viatom data file";
        return false;
    }

    const unsigned char* header = (const unsigned char*) data.constData();
    int sig   = header[0] | (header[1] << 8);
    int year  = header[2] | (header[3] << 8);
    int month = header[4];
    int day   = header[5];
    int hour  = header[6];
    int min   = header[7];
    int sec   = header[8];

    switch (sig) { //Viatom database version number  - Crimson Nape
    case 0x0003:
    case 0x0005:
        break;
    default:
        qDebug() << m_file.fileName() << "Unrecognized DB version number in Viatom data file" << sig;
        return false;
        break;
    }
    m_sig = sig;
    CHECK_VALUES(m_sig, 3, 5);

    if ((year < 2015 || year > 2059) || (month < 1 || month > 12) || (day < 1 || day > 31) ||
        (hour > 23) || (min > 59) || (sec > 59)) {
        qDebug() << m_file.fileName() << "invalid timestamp in Viatom data file";
        return false;
    }

    // It's unclear what the starting timestamp represents: is it the time at which
    // the device starts measuring data, and the first sample is 4s after that? Or
    // is the starting timestamp the time at which the first 4s average is reported
    // (and the first 4 seconds being average precede the starting timestamp)?
    //
    // If the former, then the chart draws the first sample too early (right at the
    // starting timestamp). Technically these should probably be square charts, but
    // the code currently forces them to be non-square.
    QDateTime data_timestamp = QDateTime(QDate(year, month, day), QTime(hour, min, sec));
    QDateTime filename_timestamp = getFilenameTimestamp();
    if (filename_timestamp.isValid()) {
        if (filename_timestamp != data_timestamp) {
            // TODO: Once there's a better/easier way to adjust session times within OSCAR, we can remove the below.
            qDebug() << m_file.fileName() << "Using filename timestamp" << filename_timestamp.toString("yyyy-MM-dd HH:mm:ss")
                     << "instead of header timestamp" << data_timestamp.toString("yyyy-MM-dd HH:mm:ss");
            data_timestamp = filename_timestamp;
        }
    } else {
        qWarning() << m_file.fileName() << "invalid timestamp in Viatom filename";
    }

    m_timestamp = data_timestamp.toMSecsSinceEpoch();
    m_sessionid = m_timestamp / 1000L;

    int filesize = header[9] | (header[10] << 8) | (header[11] << 16);  // possibly 32-bit
    CHECK_VALUE(header[12], 0);

    m_duration   = header[13] | (header[14] << 8);  // possibly 32-bit
    CHECK_VALUE(header[15], 0);
    CHECK_VALUE(header[16], 0);

    //int spo2_avg = header[17];
    //int spo2_min = header[18];
    //int spo2_3pct = header[19];  // number of events
    //int spo2_4pct = header[20];  // number of events
    //CHECK_VALUE(header[21], 0);  // ??? sometimes nonzero; maybe pulse spike, not a threshold of SpO2 or pulse, not always smaller than spo2_4pct
    //int time_under_90pct = header[22] | (header[23] << 8);  // in seconds
    //int events_under_90pct = header[24];  // number of distinct events
    //float o2_score = header[25] * 0.1;
    //CHECK_VALUES(header[26], 0, 4);  // number of steps taken (when nonzero, only reported by some models)
    CHECK_VALUE(header[27], 0);
    CHECK_VALUE(header[28], 0);
    CHECK_VALUE(header[29], 0);
    //CHECK_VALUE(header[30], 0);  // average pulse rate (when nonzero)
    CHECK_VALUE(header[31], 0);
    CHECK_VALUE(header[32], 0);
    CHECK_VALUE(header[33], 0);
    CHECK_VALUE(header[34], 0);
    CHECK_VALUE(header[35], 0);
    CHECK_VALUE(header[36], 0);
    CHECK_VALUE(header[37], 0);
    CHECK_VALUE(header[38], 0);
    CHECK_VALUE(header[39], 0);

    // Calculate timing resolution (in ms) of the data
    qint64 datasize = m_file.size() - HEADER_SIZE;
    m_record_count = datasize / RECORD_SIZE;
    m_resolution = m_duration / m_record_count * 1000L;
    if (m_resolution == 2000 && m_sig == 3) {
        // Interestingly the file size in the header corresponds the number of
        // distinct samples. These files actually double-report each sample!
        // So this resolution isn't really the real one. The importer should
        // calculate resolution from duration / record count after reading the
        // records, which will be deduplicated.
        CHECK_VALUE(filesize, ((m_file.size() - HEADER_SIZE) / 2) + HEADER_SIZE);
    } else {
        CHECK_VALUE(filesize, m_file.size());
    }
    CHECK_VALUES(m_resolution, 2000, 4000);
    if (true) {  // TODO: We need CheckMe sample data where this doesn't hold true.
        CHECK_VALUE(datasize % RECORD_SIZE, 0);
        CHECK_VALUE(m_duration % m_record_count, 0);
    }

    //qDebug().noquote() << m_file.fileName() << ts(m_timestamp) << dur(m_duration * 1000L) << ":" << m_record_count << "records @" << m_resolution << "ms";

    return true;
}

QList<ViatomFile::Record> ViatomFile::ReadData()
{
    QByteArray data = m_file.readAll();
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);

    QList<ViatomFile::Record> records;

    // Read all Pulse, SPO2 and Motion data
    do {
        ViatomFile::Record rec;
        in >> rec.spo2 >> rec.hr >> rec.oximetry_invalid >> rec.motion >> rec.vibration;
        CHECK_VALUES(rec.oximetry_invalid, 0, 0xFF);
        if (rec.vibration) {
            CHECK_VALUES(rec.vibration, 0x40, 0x80);  // 0x40 or 0x80 when vibration is triggered
        }
        // Invalid readings indicate any interruption in the measurements, whether
        // transitory (e.g. due to movement) or when the device is removed at the end of a session.
        if (rec.oximetry_invalid == 0xFF) {
            CHECK_VALUE(rec.spo2, 0xFF);
            CHECK_VALUE(rec.hr, 0xFF);
        }
        records.append(rec);
    } while (records.size() < m_record_count);

    // It turns out 2s files are actually just double-reported samples on older models!
    if (m_resolution == 2000) {
        QList<ViatomFile::Record> dedup;
        bool all_are_duplicated = true;

        if ((records.size() % 2) != 0) {
            // An odd number of samples inherently can't be all duplicates.
            all_are_duplicated = false;
        } else {
            for (int i = 0; i < records.size(); i += 2) {
                auto & a = records.at(i);
                auto & b = records.at(i+1);
                if (a.spo2 != b.spo2
                    || a.hr != b.hr
                    || a.oximetry_invalid != b.oximetry_invalid
                    || a.motion != b.motion
                    || a.vibration != b.vibration) {
                    all_are_duplicated = false;
                    break;
                }
                dedup.append(a);
            }
        }
        if (m_sig == 5) {
            // Confirm that CheckMe O2 Max is a true 2s sample rate.
            CHECK_VALUE(all_are_duplicated, false);
        } else {
            // Confirm that older models are actually a 4s sample rate.
            CHECK_VALUE(m_sig, 3);
            CHECK_VALUE(all_are_duplicated, true);
        }
        if (all_are_duplicated) {
            // Return the deduplicated list.
            records = dedup;
        }
    }
    if (m_sig == 5) {
        CHECK_VALUES(duration() / records.size(), 2, 4);  // We've seen 2s and 4s resolution.
    } else {
        CHECK_VALUE(duration() / records.size(), 4);  // We've only seen 4s true resolution so far.
    }

    return records;
}

O2RingS::O2RingS(QFile & file) : ViatomFile(file)
{
}

bool O2RingS::ParseHeader()
{
    // For the O2Ring S, the header only contains the signature
    // Additional metadata is stored at the end of the file
    // The record count is stored 36 bytes prior to EOF
    int record_count_loc = m_file.size() - 36;
    if (record_count_loc < 0 || !m_file.seek(record_count_loc)) {
        qDebug() << m_file.fileName() << "error locating Viatom record count";
        return false;
    }

    // read record count as a 2-byte little endian value
    // max number of records in a O2Ring S file is 36000
    QDataStream in(m_file.read(2));
    in.setByteOrder(QDataStream::LittleEndian);
    quint16 record_count;
    in >> record_count;

    m_sig = 0x0301;
    m_record_count = m_duration = record_count;
    m_timestamp = getFilenameTimestamp().toMSecsSinceEpoch();
    m_sessionid = m_timestamp / 1000L;
    m_resolution = 1000;

    // advance past the header
    return m_file.seek(10);
}

QList<ViatomFile::Record> O2RingS::ReadData()
{
    QList<ViatomFile::Record> records;

    // Read all Pulse, SPO2 and Motion data
    // 0xFF for spo2 or hr indicates an interruption in measurement
    // Vibration data is likely stored in a variable length block following the
    // fixed-width pulse/SPO2/motion data. Zero out for now since OSCAR doesn't
    // use this data.
    QDataStream in(m_file.readAll());
    do {
        ViatomFile::Record rec;
        in >> rec.spo2 >> rec.hr >> rec.motion;
        rec.oximetry_invalid = (rec.spo2 == 0xFF || rec.hr == 0xFF) ? 0xFF : 0;
        rec.vibration = 0;
        records.append(rec);
    } while (records.size() < m_record_count);

    // Confirm that we have a 1s sample rate
    CHECK_VALUE(duration() / records.size(), 1);
    return records;
}
