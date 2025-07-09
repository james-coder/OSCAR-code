/* EDF Parser Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef EDFPARSER_H
#define EDFPARSER_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QList>
#include <QDateTime>
#include <QTimeZone>

#include "SleepLib/common.h"

const QString STR_ext_EDF = "edf";
const QString STR_ext_gz = ".gz";

const char AnnoSep = 20;
const char AnnoDurMark = 21;
const char AnnoEnd = 0;

// EDFType is used by all the edf loaders - resmed and sleepstyle, so far
enum EDFType { EDF_UNKNOWN, EDF_BRP, EDF_PLD, EDF_SAD, EDF_SA2, EDF_EVE, EDF_CSL, EDF_AEV, EDF_RT };

/*! \struct EDFHeader
    \brief  Represents the EDF+ header structure, used as a place holder while processing the text data.
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
*/
struct EDFHeaderRaw {
    char version[8];
    char patientident[80];
    char recordingident[80];
    char datetime[16];
    char num_header_bytes[8];
    char reserved[44];
    char num_data_records[8];
    char dur_data_records[8];
    char num_signals[4];
}
#ifndef _MSC_VER
__attribute__((packed))
#endif
;
const int EDFHeaderSize = sizeof(EDFHeaderRaw);

/*! \struct EDFHeaderQT
    \brief Contains the QT version of the EDF header information
    */
struct EDFHeaderQT {
  public:
    long version;
    QString patientident;
    QString recordingident;
    QDateTime startdate_orig;
    long num_header_bytes;
    QString reserved44;
    long num_data_records;
    double duration_Seconds;
    int num_signals;
};

/*! \struct EDFSignal
    \brief Contains information about a single EDF+ Signal
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
    */
struct EDFSignal {
  public:
//  virtual ~EDFSignal();

    QString label;                      //! \brief Name of this Signal
    QString transducer_type;            //! \brief Tranducer Type (source of the data, usually blank)
    QString physical_dimension;         //! \brief The units of measurements represented by this signal
    EventDataType physical_minimum;     //! \brief The minimum limits of the ungained data
    EventDataType physical_maximum;     //! \brief The maximum limits of the ungained data
    EventDataType digital_minimum;      //! \brief The minimum limits of the data with gain and offset applied
    EventDataType digital_maximum;      //! \brief The maximum limits of the data with gain and offset applied
    EventDataType gain;                 //! \brief Raw integer data is multiplied by this value
    EventDataType offset;               //! \brief This value is added to the raw data
    QString prefiltering;               //! \brief Any prefiltering methods used (usually blank)
    long sampleCnt;                     //! \brief Number of samples per record
    QString reserved;                   //! \brief Reserved (usually blank)
    qint16 * dataArray;                 //! \brief Pointer to the signals sample data

//    int pos;                            //! \brief a non-EDF extra used internally to count the signal data
};

/*! \class Annotation
    \author Phil Olynyk
    \brief Hold the annotation text from an EDF file
    */
class Annotation
{
  public:
    Annotation() { duration = -1.0; };
    Annotation( double off, double dur, QString tx ) {
        offset = off;
        duration = dur;
        text = tx;
    };
    virtual ~Annotation() {};

    double  offset;
    double  duration;
    QString text;
};

/*! \class EDFInfo
    \author Phil Olynyk
    \author Mark Watkins 
    \brief Parse an EDF+ data file into a list of EDFSignal's
    \note More information on the EDF+ file format can be obtained from http://edfplus.info
    */
class EDFInfo
{
  public:
    //! \brief Constructs an EDFParser object, opening the filename if one supplied
    EDFInfo();

    virtual ~EDFInfo();

    virtual bool Open(const QString & name);                    //! \brief Open the EDF+ file, and read it's header
    virtual bool Open(const QByteArray &data);


    virtual bool Parse();                          //! \brief Parse the EDF+ file into the EDFheaderQT. Must call Open(..) first.

    virtual bool ParseSignalData();                          //! \brief Parse the signal data

    virtual bool parseHeader( EDFHeaderRaw * hdrPtr );                  //! \brief parse just the edf header for duration, etc

    virtual EDFSignal * lookupLabel(const QString & name, int index=0);  //! \brief Return a ptr to the i'th signal with that name

    virtual EDFHeaderQT * GetHeader( const QString & name);             //! \brief returna pointer to the header block

    virtual long GetNumSignals() { return edfHdr.num_signals; }         //! \brief Returns the number of signals contained in this EDF file

    virtual long GetNumDataRecords() { return edfHdr.num_data_records; } //! \brief Returns the number of data records contained per signal.

    virtual double GetDuration() { return edfHdr.duration_Seconds; }    //! \brief Returns the duration represented by this EDF file

    virtual QString GetPatient() { return edfHdr.patientident; }        //! \brief Returns the patientid field from the EDF header

    static QDateTime getStartDT(const QString str);                    //! \brief Returns the start time using noLocalDST

    static void setTimeZoneUTC();                                      //! \brief Sets noLocalDST to UTC (for EDF files using UTC time)


//  The data members follow

    static int  TZ_offset;
    static QTimeZone localNoDST;

    QString filename;								//!	\brief For debug and error messages

    EDFHeaderQT edfHdr;                             //! \brief The header in a QT friendly form

    QVector<EDFSignal> edfsignals;                  //! \brief Holds the EDFSignals contained in this edf file

    QVector< QVector<Annotation> > annotations;     //! \brief Holds the Annotaions for this EDF file

    QStringList signal_labels;                      //! \brief An by-name indexed into the EDFSignal data

    QHash<QString, QList<EDFSignal *> > signalList; //! \brief ResMed sometimes re-uses the SAME signal name

//  the following could be private
  protected:
    //! \brief This is the array of signal descriptors and values
    char *signalPtr;
    long filesize;
    long datasize;
    long pos;
    bool eof;

    //! \brief This is the array holding the EDF file data
    QByteArray  fileData;
    //! \brief Read 16 bit word of data from the EDF+ data stream
    qint16 Read16();

  private:
    QVector<Annotation> ReadAnnotations( const char * data, int charLen );	//! \brief Create an Annotaion vector from the signal values

    QString ReadBytes(unsigned n);                                   //! \brief Read n bytes of 8 bit data from the EDF+ data stream

    //! \brief  The EDF+ files header structure, used as a place holder while processing the text data.
    EDFHeaderRaw *hdrPtr;

};


#endif // EDFPARSER_H
