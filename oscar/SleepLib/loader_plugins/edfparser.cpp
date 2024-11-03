/* EDF Parser Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QDateTime>
#include <QTimeZone>
#include <QDebug>
#include <QFile>
#include <QMutexLocker>
#ifdef _MSC_VER
#include <QtZlib/zlib.h>
#else
#include <zlib.h>
#endif

#include "edfparser.h"

//EDFSignal::~EDFSignal()
//{
//    delete [] dataArray;
//}

#ifdef TEST_MACROS_ENABLED
#include <QRegExp>
QHash<QString,int> edfSignalsFound;
QStringList edfSignalList;
void edfDebugInit() {
    edfSignalsFound.clear();
    edfSignalList.clear();
    edfSignalList
        // Signals to display containing these names
        << "Date"
        << "Mode"
        ;
}
#else
void edfDebugInit() {};
#endif

EDFInfo::EDFInfo()
{
    filename = QString();
    edfsignals.clear();
    filesize = 0;
    datasize = 0;
    signalPtr = nullptr;
    hdrPtr = nullptr;
//    fileData = nullptr;
}

    int EDFInfo::TZ_offset = QTimeZone::systemTimeZone().offsetFromUtc(QDateTime::currentDateTime());
    QTimeZone EDFInfo::localNoDST = QTimeZone(TZ_offset);

EDFInfo::~EDFInfo()
{
//    if ( fileData ) {
	if (fileData.size() > 0) {
#ifdef EDF_DEBUG
	    qDebug() << "EDFInfo destructor clearing fileData";
#endif
	    fileData.clear();
	}
//    }

    for (auto & s : edfsignals) {
        if (s.dataArray)  
            delete [] s.dataArray;
    }
//    for (auto & a : annotations)
//    	delete  a;
}

// Set timezone to UTC
void EDFInfo::setTimeZoneUTC () {
    TZ_offset = 0;
    EDFInfo::localNoDST = QTimeZone(TZ_offset);
}

bool EDFInfo::Open(const QString & name)
{
    if (hdrPtr != nullptr) {
        qWarning() << "EDFInfo::Open() called with file already open " << name;
        return false;
    }
    QFile fi(name);
    if (!fi.open(QFile::ReadOnly)) {
        qDebug() << "EDFInfo::Open() Couldn't open file" << name  << "error" << fi.error() << fi.errorString();
        return false;
    }
//    fileData = new QByteArray();
#ifndef DUMPSTR
    if (name.endsWith(STR_ext_gz)) {
        fileData = gUncompress(fi.readAll()); // Open and decompress file
    } else {
        fileData = fi.readAll(); // Open and read uncompressed file
    }
#else
    fileData = fi.readAll(); // Open and read uncompressed file
#endif    
    fi.close();
    if (fileData.size() <= EDFHeaderSize) {
    	fileData.clear();;
        qDebug() << "EDFInfo::Open() File too short " << name;
        return false;
    }
    filename = name;
//    return fileData;
    return true;
}

bool EDFInfo::Open(const QByteArray &data)
{
    fileData = data;
    if (fileData.size() <= EDFHeaderSize) {
        fileData.clear();;
        qDebug() << "EDFInfo::Open() buffer too short";
        return false;
    }
    // TODO AXT
    filename = "bytearray";
    return true;
}


QDateTime EDFInfo::getStartDT( QString dateTimeStr )
{
//  edfHdr.startdate_orig = QDateTime::fromString(QString::fromLatin1(hdrPtr->datetime, 16), "dd.MM.yyHH.mm.ss");
//  QString dateTimeStr;    // , dateStr, timeStr;
    QDate qDate;
    QTime qTime;
//  dateTimeStr = QString::fromLatin1(hdrPtr->datetime, 16);
//    dateStr = dateTimeStr.left(8);
//    timeStr = dateTimeStr.right(8);
    qDate = QDate::fromString(dateTimeStr.left(8), "dd.MM.yy");
    qTime = QTime::fromString(dateTimeStr.right(8), "HH.mm.ss");
    return QDateTime(qDate, qTime, localNoDST);
}

bool EDFInfo::parseHeader( EDFHeaderRaw *hdrPtr )
{
    bool ok;

    edfHdr.version = QString::fromLatin1(hdrPtr->version, 8).toLong(&ok);
    if (!ok) {
#ifdef EDF_DEBUG
        qWarning() << "EDFInfo::Parse() Bad Version " << filename;
//      sleep(1);
#endif
        fileData.clear();
        return false;
    }

    edfHdr.patientident=QString::fromLatin1(hdrPtr->patientident,80).trimmed();
    edfHdr.recordingident = QString::fromLatin1(hdrPtr->recordingident, 80).trimmed(); // Serial number is in here..
    edfHdr.startdate_orig = getStartDT(QString::fromLatin1(hdrPtr->datetime, 16));
    // This conversion will fail in 2084 after when the spec calls for the year to be 'yy' instead of digits
    // The solution is left for the afflicted - it won't be me!
    QDate d2 = edfHdr.startdate_orig.date();
    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        edfHdr.startdate_orig.setDate(d2);
    }

    edfHdr.num_header_bytes = QString::fromLatin1(hdrPtr->num_header_bytes, 8).toLong(&ok);
    if (!ok) {
#ifdef EDF_DEBUG
        qWarning() << "EDFInfo::Parse() Bad header byte count " << filename;
//      sleep(1);
#endif
        fileData.clear();
        return false;
    }
    edfHdr.reserved44=QString::fromLatin1(hdrPtr->reserved, 44).trimmed();
    edfHdr.num_data_records = QString::fromLatin1(hdrPtr->num_data_records, 8).toLong(&ok);
// TODO AXT
//    if ( (! ok) || (edfHdr.num_data_records < 1) ) {
//#ifdef EDF_DEBUG
//        qWarning() << "EDFInfo::Parse() Bad data record count " << filename;
// //      sleep(1);
//#endif
//        fileData.clear();
//        return false;
//    }
    edfHdr.duration_Seconds = QString::fromLatin1(hdrPtr->dur_data_records, 8).toDouble(&ok);
    if (!ok) {
#ifdef EDF_DEBUG
        qWarning() << "EDFInfo::Parse() Bad duration " << filename;
//      sleep(1);
#endif
        fileData.clear();
        return false;
    }
    edfHdr.num_signals = QString::fromLatin1(hdrPtr->num_signals, 4).toLong(&ok);
    if ( (! ok) || (edfHdr.num_signals < 1) || (edfHdr.num_signals > 256) ) {
#ifdef EDF_DEBUG
        qWarning() << "EDFInfo::Parse() Bad number of signals " << filename;
//      sleep(1);
#endif
        fileData.clear();
        return false;
    }
    return true;
}

bool EDFInfo::Parse() {
    bool ok;

    if (fileData.size() == 0) {
        qWarning() << "EDFInfo::Parse() called without valid EDF data " << filename;
//        sleep(1);
        return false;
    }
    
    hdrPtr = (EDFHeaderRaw *)fileData.constData();
    signalPtr = (char *)fileData.constData() + EDFHeaderSize;
    filesize = fileData.size();
    datasize = filesize - EDFHeaderSize;
    pos = 0;

    eof = false;

    if ( ! parseHeader( hdrPtr ) )
        return false;

    // Initialize fixed-size signal list.
    edfsignals.resize(edfHdr.num_signals);

    // Now copy all the Signal descriptives into edfsignals
    for (auto & sig : edfsignals) {
        sig.dataArray = nullptr;
        sig.label = ReadBytes(16);

        signal_labels.push_back(sig.label);
        signalList[sig.label].push_back(&sig);
        if (eof) {
            qWarning() << "EDFInfo::Parse() Early end of file " << filename;
//          sleep(1);
            fileData.clear();
            return false;
        }
    }
    for (auto & sig : edfsignals) { 
        sig.transducer_type = ReadBytes(80); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_dimension = ReadBytes(8); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_minimum = ReadBytes(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.physical_maximum = ReadBytes(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.digital_minimum = ReadBytes(8).toDouble(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.digital_maximum = ReadBytes(8).toDouble(&ok);
        sig.gain = (sig.physical_maximum - sig.physical_minimum) / (sig.digital_maximum - sig.digital_minimum);
        sig.offset = 0;
    }
    for (auto & sig : edfsignals) { 
        sig.prefiltering = ReadBytes(80); 
    }
    for (auto & sig : edfsignals) { 
        sig.sampleCnt = ReadBytes(8).toLong(&ok); 
    }
    for (auto & sig : edfsignals) { 
        sig.reserved = ReadBytes(32); 
    }

    // could do it earlier, but it won't crash from > EOF Reads
    if (eof) {
        qWarning() << "EDFInfo::Parse() Early end of file " << filename;
//        sleep(1);
        fileData.clear();
        return false;
    }
    bool ret = ParseSignalData();
    fileData.clear();
    return ret;
}

bool EDFInfo::ParseSignalData() {
    // Now check the file isn't truncated before allocating space for the values
    long allocsize = 0;
    for (auto & sig : edfsignals) {
        if (edfHdr.num_data_records > 0) {
            allocsize += sig.sampleCnt * edfHdr.num_data_records * 2;
        }
    }
    if (allocsize > (datasize - pos)) {
        // Space required more than the remainder left to read,
        // so abort and let the user clean up the corrupted file themselves
        qWarning() << "EDFInfo::Parse(): " << filename << " is too short!";
//        sleep(1);
        fileData.clear();
        return false;
    }
    // allocate the arrays for the signal values
    for (auto & sig : edfsignals) {
        long samples = sig.sampleCnt * edfHdr.num_data_records;
        if (edfHdr.num_data_records <= 0) {
            sig.dataArray = nullptr;
            continue;
        }
        sig.dataArray = new qint16 [samples];
//      sig.pos = 0;
    }

    for (int recNo = 0; recNo < edfHdr.num_data_records; recNo++) {
        for (auto & sig : edfsignals) {
            if ( sig.label.contains("Annotations") ) {
                annotations.push_back(ReadAnnotations( (char *)&signalPtr[pos], sig.sampleCnt*2));
                pos += sig.sampleCnt * 2;
            } else {    // it's got genuine 16-bit values
                for (int j=0;j<sig.sampleCnt;j++) { // Big endian safe
                    qint16 t=Read16();
                    sig.dataArray[recNo*sig.sampleCnt+j]=t;
                    #ifdef TEST_MACROS_ENABLED
                      if (
                        (t !=-1)   // only display non-null data
                        && (j==0)     // Only display first data element
                        )
                      {
                        bool displayItem = false;
                        static QRegExp rx("^\\w[.]");
                        static QRegExp rx2("\\d$)");
                        bool havematch = edfSignalList.contains(sig.label) ;
                        bool havesig  = (rx.indexIn(sig.label) != -1) || havematch ;
                        bool haveNum  = (rx2.indexIn(sig.label) != -1);
                        displayItem = havesig && !haveNum;
                        // displayItem = true;
                        if (displayItem) {
                            edfSignalsFound.insert(sig.label,t);
                            if (sig.label.contains("Mode") ) {
                                DEBUGFC Q(filename);
                            }
                            DEBUGCF O(QString("                     L:%1 %2 Gain:%3 off:%4")
                                .arg(sig.label,-16)
                                .arg(t,5)
                                .arg(sig.gain,5)
                                .arg(sig.offset) )
                                ;
                        };
                      }
                    #endif
                }
            }
        }
    }

    return true;
}

EDFHeaderQT * EDFInfo::GetHeader( const QString & name)
{
    QFile fi(name);
    if (!fi.open(QFile::ReadOnly)) {
        qDebug() << "EDFInfo::Open() Couldn't open file" << name << "error" << fi.error() << fi.errorString();
//      sleep(1);
        return nullptr;
    }
//    fileData = new QByteArray();
#ifndef DUMPSTR
    if (name.endsWith(STR_ext_gz)) {
        fileData = gUncompress(fi.read(sizeof(EDFHeaderRaw))); // Open and decompress file
    } else {
        fileData = fi.read(sizeof(EDFHeaderRaw)); // Open and read uncompressed file
    }
#else
    fileData = fi.read(sizeof(EDFHeaderRaw)); // Open and read uncompressed file
#endif    
    fi.close();
    filename = name;
    hdrPtr = (EDFHeaderRaw *)fileData.constData();

    if ( ! parseHeader( hdrPtr ) )
        return nullptr;

    fileData.clear();
    return & edfHdr;    
}

// Parse the EDF file to get the annotations out of it.
QVector<Annotation> EDFInfo::ReadAnnotations(const char * data, int charLen)
{
	QVector<Annotation> annoVec = QVector<Annotation>();
	
    // Process event annotation record

    long pos = 0;
    double offset;
    double duration;

    while (pos < charLen) {
	    QString text;
        bool sign, ok;
        char c = data[pos];

        if ((c != '+') && (c != '-'))       // Annotaion must start with a +/- sign
            break;
        sign = (data[pos++] == '+');

        text = "";
        c = data[pos];

        do {            // collect the offset 
            text += c;
            pos++;
            c = data[pos];
        } while ((c != AnnoSep) && (c != AnnoDurMark)); // a duration is optional

        offset = text.toDouble(&ok);
        if (!ok) {
#ifdef EDF_DEBUG
            qDebug() << "Faulty offset in  annotation record ";
        //  sleep(1);
#endif
            break;
        }

        if (!sign)
            offset = -offset;

        duration = -1.0;	// This indicates no duration supplied
        // First entry
        if (data[pos] == AnnoDurMark) { // get duration.(preceded by decimal 21 byte)
            pos++;
            text = "";

            do {        // collect the duration
                text += data[pos];
                pos++;
            } while ((data[pos] != AnnoSep) && (pos < charLen)); // separator code

            duration = text.toDouble(&ok);
            if (!ok) {
#ifdef EDF_DEBUG
                qDebug() << "Faulty duration in  annotation record ";
        //      sleep(1);
#endif
                break;
            }
        }

        while ((data[pos] == AnnoSep) && (pos < charLen)) {
            int textLen = 0;
            pos++;
            const char * textStart = &data[pos];
            if (data[pos] == AnnoEnd)
                break;
            if (data[pos] == AnnoSep) {
                pos++;
                break;
            }
            do {            // collect the annotation text
                pos++;      // officially UTF-8 is allowed here, so don't mangle it
                textLen++;
            } while ((data[pos] != AnnoSep) && (pos < charLen)); // separator code
            text = QString::fromUtf8(textStart, textLen);
            annoVec.push_back( Annotation( offset, duration, text) );
            if (pos >= charLen) {
#ifdef EDF_DEBUG
                qDebug() << "Short EDF Annotations record";
        //      sleep(1);
#endif
                break;
            }
        }

        while ((pos < charLen) && (data[pos] == AnnoEnd))
            pos++;

        if (pos >= charLen)
            break;
    }
    return annoVec;
}

// Read a 16 bits integer
qint16 EDFInfo::Read16()
{
    if ((pos + 2) > datasize) {
        eof = true;
        return 0;
    }
#ifdef Q_LITTLE_ENDIAN // Intel, etc...
    qint16 res = *(qint16 *)&signalPtr[pos];
#else // ARM, PPC, etc..
    qint16 res = quint8(signalPtr[pos]) | (qint8(signalPtr[pos+1]) << 8);
#endif
    pos += 2;
    return res;
}

QString EDFInfo::ReadBytes(unsigned n)
{
    if ((pos + long(n)) > datasize) {
        eof = true;
        return QString();
    }
    QByteArray buf(&signalPtr[pos], n);
    pos+=n;
    return buf.trimmed();
}

EDFSignal *EDFInfo::lookupLabel(const QString & name, int index)
{
    auto it = signalList.find(name);
    if (it == signalList.end()) 
        return nullptr;

    if (index >= it.value().size()) 
        return nullptr;

    return it.value()[index];
}

