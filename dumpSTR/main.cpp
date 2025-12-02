/* Dump an STR.edf file */

#include <QApplication>
// #include <iostream>
#include <QDebug>
#include <QThread>

typedef float EventDataType;

#include "edfparser.h"

// using namespace std;

void dumpHeader( const EDFHeaderQT  hdr ) {
    qDebug() << "EDF Header:";
    qDebug() << "Version " << hdr.version << " Patient >" << hdr.patientident << "<";
    qDebug() << "Recording >" << hdr.recordingident << "<";
    qDebug() << "Date: " << hdr.startdate_orig.toString();
    qDebug() << "Header size (bytes): " << hdr.num_header_bytes;
    qDebug() << "EDF type: >" << hdr.reserved44 << "<";
    qDebug() << "Duration(secs): " << hdr.duration_Seconds;
    qDebug() << "Number of Signals: " << hdr.num_signals << "\n";
}

void ifprint( QString label, QString text) {
    if ( text.isEmpty() )
        return;
    qDebug() << label << ": " << text;
    return;
}

void dumpSignals( const QVector<EDFSignal>  sigs ) {
    int i = 1;
    for (auto sig: sigs) {
        qDebug() << "Signal #" << i++;
        qDebug() << "Label: " << sig.label;
        ifprint( "Transducer", sig.transducer_type );
        ifprint( "Dimension", sig.physical_dimension );
        qDebug() << "Physical min: " << sig.physical_minimum << " max: " << sig.physical_maximum;
        qDebug() << "Digital  min: " << sig.digital_minimum << " max: " << sig.digital_maximum;
        qDebug() << "Gain: " << sig.gain << " Offset: " << sig.offset;
        ifprint( "Pre-filter", sig.prefiltering );
        qDebug() << "Sample Count: " << sig.sampleCnt;
        qDebug() << "dataArray is at " << sig.dataArray << "\n";
    }
}

void usage() {
    qDebug() << "dumpSTR [ options ] filename";
    qDebug() << "Options";
    qDebug() << "\t-a Show all";
    qDebug() << "\t-f ### First day";
    qDebug() << "\t-l ### Last day";
    qDebug() << "\t-g ### First signal";
    qDebug() << "\t-m ### Last signal";
    qDebug() << "\t-s Signal list only";
    qDebug() << "\t-H Don't print the header";
    qDebug() << "\t-S Don't print signals list";
    qDebug() << "\t-h or -? This help message";
}

int main(int argc, char *argv[]) {

//    QString homeDocs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/";
//    QCoreApplication::setApplicationName(getAppName());
//    QCoreApplication::setOrganizationName(getDeveloperName());
//    QCoreApplication::setOrganizationDomain(getDeveloperDomain());

    int first = 0, last = 0;
    int firstSig = 1, lastSig = 0;

    QApplication a(argc, argv);
    QStringList args = a.arguments();
    
	if ( args.size() < 2 ) {
		usage();
		exit(1);
	}
	
    QString filename = args[args.size()-1];
    bool showall = false, brief = false;
    bool skipHeader = false, skipSignals = false;

    for (int i = 1; i < args.size()-1; i++) {
        if (args[i] == "-f")
            first = args[++i].toInt();
        else if (args[i] == "-g")
            firstSig = args[++i].toInt();
        else if (args[i] == "-l")
            last = args[++i].toInt();
        else if (args[i] == "-m")
            lastSig = args[++i].toInt();
        else if (args[i] == "-a")
            showall = true;
        else if (args[i] == "-s")
            brief = true;
        else if (args[i] == "-H")
            skipHeader = true;
        else if (args[i] == "-S")
            skipSignals = true;
        else if ((args[i] == "-?") || (args[i] == "-h")) {
            usage();
            exit(0);
        }
    }

    EDFInfo str;
    if ( ! str.Open(filename) ) {
        qDebug() << "Failed to open" << filename;
        exit(-1);
    }
    if ( ! str.Parse() ) {
        qDebug() << "Parsing failed on" << filename;
        exit(-1);
    }

    QDate d2 = str.edfHdr.startdate_orig.date();
    if (d2.year() < 2000) {
        d2.setDate(d2.year() + 100, d2.month(), d2.day());
        str.edfHdr.startdate_orig.setDate(d2);
    }



    QDate date = str.edfHdr.startdate_orig.date(); // each STR.edf record starts at 12 noon
    int numDays = str.GetNumDataRecords();

    qDebug() << str.filename << " starts at " << date << " for " << numDays
                << " days, ending at " << date.addDays(numDays) << " with " << str.GetNumSignals() << " signals";

    if (args.size() == 2) {
        exit(0);
    }

    if ( ! skipHeader)
        dumpHeader( (str.edfHdr) );
    if ( ! skipSignals )
        dumpSignals( (str.edfsignals) );

    if ( brief )
        exit(0);
	
    int size = str.GetNumDataRecords();

    if (showall) {
        first = 0;
        last = size;
        firstSig = 1;
        lastSig = str.GetNumSignals();
    }
    if (lastSig == 0 )
        lastSig = str.GetNumSignals();

    if ((last == 0) || (last > size))
        last = size;

    date = date.addDays(first);
    // For each data record, representing 1 day each
    for (int rec = first; rec < last+1; ++rec, date = date.addDays(1)) {
		qDebug() << "Record no. " << rec << " Date: " << date.toString() ;
	    for (int j = firstSig-1; j < lastSig; j++ ) {
        //  qDebug() << "Signal #" << j;
	        EDFSignal sig = str.edfsignals[j];
//          if ( sig == nullptr ) {
//              qDebug() << "Bad sig pointer at signal " << j;
//              exit(2);
//          }
            if ( ! sig.label.contains("Annotations")) {
		        qint16 * sample = sig.dataArray;
//              qDebug() << "Sample pointer is " << sample;
                if (sample == nullptr) {
                    qDebug() << "Bad sample pointer at signal " << j;
                    exit(3);
                }
	            QString dataStr = "";
		        if (sig.sampleCnt == 1) {
//                  qDebug() << "Single sample is " << sample[rec];
		            dataStr.setNum(sample[rec]);
//                  qDebug() << "Datastr is " << dataStr;
                }
		        else {
		            for (int i = 0; i < sig.sampleCnt; i++ ) {
                        QString num;
                        num.setNum(sample[rec*sig.sampleCnt + i]);
		                dataStr.append(" ").append(num);
                    }
	            }
    	        qDebug() << "#" << j+1 << sig.label << dataStr;
            }
        }
	}
//  qDebug() << "Deleting the edf object";
//  delete &str;
	QThread::sleep(1);
    qDebug() << "Done";

    exit(0);
}
