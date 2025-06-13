/* SleepLib PRS1 Loader Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QDataStream>
#include <QMessageBox>
#include <QDebug>
#include <cmath>

#include "SleepLib/schema.h"
#include "SleepLib/importcontext.h"
#include "prs1_loader.h"
#include "prs1_parser.h"
#include "SleepLib/session.h"
#include "SleepLib/calcs.h"
#include "SleepLib/crypto.h"
#include "rawdata.h"


// Disable this to cut excess debug messages

#define DEBUG_SUMMARY


//const int PRS1_MAGIC_NUMBER = 2;
//const int PRS1_SUMMARY_FILE=1;
//const int PRS1_EVENT_FILE=2;
//const int PRS1_WAVEFORM_FILE=5;


//********************************************************************************************
/// IMPORTANT!!!
//********************************************************************************************
// Please INCREMENT the prs1_data_version in prs1_loader.h when making changes to this loader
// that change loader behaviour or modify channels.
//********************************************************************************************

QString ts(qint64 msecs)
{
    // TODO: make this UTC so that tests don't vary by where they're run
    return QDateTime::fromMSecsSinceEpoch(msecs).toString(Qt::ISODate);
}


ChannelID PRS1_Mode = 0;
ChannelID PRS1_TimedBreath = 0, PRS1_HumidMode = 0, PRS1_TubeTemp = 0;
ChannelID PRS1_FlexLock = 0, PRS1_TubeLock = 0, PRS1_RampType = 0;
ChannelID PRS1_BackupBreathMode = 0, PRS1_BackupBreathRate = 0, PRS1_BackupBreathTi = 0;
ChannelID PRS1_AutoTrial = 0, PRS1_EZStart = 0, PRS1_RiseTime = 0, PRS1_RiseTimeLock = 0;
ChannelID PRS1_PeakFlow = 0;
ChannelID PRS1_VariableBreathing = 0;  // TODO: UNCONFIRMED, but seems to match sample data

QString PRS1Loader::PresReliefLabel() { return QObject::tr(""); }
ChannelID PRS1Loader::PresReliefMode() { return PRS1_FlexMode; }
ChannelID PRS1Loader::PresReliefLevel() { return PRS1_FlexLevel; }
ChannelID PRS1Loader::CPAPModeChannel() { return PRS1_Mode; }
ChannelID PRS1Loader::HumidifierConnected() { return PRS1_HumidStatus; }
ChannelID PRS1Loader::HumidifierLevel() { return PRS1_HumidLevel; }


struct PRS1TestedModel
{
    QString model;
    int family;
    int familyVersion;
    const char* name;
};

static const PRS1TestedModel s_PRS1TestedModels[] = {
    // This first set says "(Philips Respironics)" intead of "(System One)" on official reports.
    { "251P", 0, 2, "REMstar Plus (System One)" },  // (brick)
    { "450P", 0, 2, "REMstar Pro (System One)" },
    { "450P", 0, 3, "REMstar Pro (System One)" },
    { "451P", 0, 2, "REMstar Pro (System One)" },
    { "451P", 0, 3, "REMstar Pro (System One)" },
    { "452P", 0, 3, "REMstar Pro (System One)" },
    { "550P", 0, 2, "REMstar Auto (System One)" },
    { "550P", 0, 3, "REMstar Auto (System One)" },
    { "551P", 0, 2, "REMstar Auto (System One)" },
    { "552P", 0, 3, "REMstar Auto (System One)" },
    { "650P", 0, 2, "BiPAP Pro (System One)" },
    { "750P", 0, 2, "BiPAP Auto (System One)" },

    { "261CA",  0, 4, "REMstar Plus (System One 60 Series)" },  // (brick)
    { "261P",   0, 4, "REMstar Plus (System One 60 Series)" },  // (brick)
    { "460P",   0, 4, "REMstar Pro (System One 60 Series)" },
    { "460PBT", 0, 4, "REMstar Pro (System One 60 Series)" },  // evidently built-in bluetooth
    { "461P",   0, 4, "REMstar Pro (System One 60 Series)" },
    { "462P",   0, 4, "REMstar Pro (System One 60 Series)" },
    { "461CA",  0, 4, "REMstar Pro (System One 60 Series)" },
    { "560P",   0, 4, "REMstar Auto (System One 60 Series)" },
    { "560PBT", 0, 4, "REMstar Auto (System One 60 Series)" },
    { "561P",   0, 4, "REMstar Auto (System One 60 Series)" },
    { "562P",   0, 4, "REMstar Auto (System One 60 Series)" },
    { "660P",   0, 4, "BiPAP Pro (System One 60 Series)" },
    { "760P",   0, 4, "BiPAP Auto (System One 60 Series)" },
    { "761P",   0, 4, "BiPAP Auto (System One 60 Series)" },
    
    { "501V",   0, 5, "Dorma 500 Auto (System One 60 Series)" },  // (brick)

    { "200X110", 0, 6, "DreamStation CPAP" },  // (brick)
    { "400G110", 0, 6, "DreamStation Go" },
    { "400X110", 0, 6, "DreamStation CPAP Pro" },
    { "400X120", 0, 6, "DreamStation CPAP Pro" },
    { "400X130", 0, 6, "DreamStation CPAP Pro" },
    { "400X150", 0, 6, "DreamStation CPAP Pro" },
    { "401X150", 0, 6, "DreamStation CPAP Pro with Auto-Trial" },
    { "500X110", 0, 6, "DreamStation Auto CPAP" },
    { "500X120", 0, 6, "DreamStation Auto CPAP" },
    { "500X130", 0, 6, "DreamStation Auto CPAP" },
    { "500X140", 0, 6, "DreamStation Auto CPAP with A-Flex" },
    { "500X150", 0, 6, "DreamStation Auto CPAP" },
    { "500X180", 0, 6, "DreamStation Auto CPAP" },
    { "501X120", 0, 6, "DreamStation Auto CPAP with P-Flex" },
    { "500G110", 0, 6, "DreamStation Go Auto" },
    { "500G120", 0, 6, "DreamStation Go Auto" },
    { "500G150", 0, 6, "DreamStation Go Auto" },
    { "502G150", 0, 6, "DreamStation Go Auto" },
    { "600X110", 0, 6, "DreamStation BiPAP Pro" },
    { "600X150", 0, 6, "DreamStation BiPAP Pro" },
    { "700X110", 0, 6, "DreamStation Auto BiPAP" },
    { "700X120", 0, 6, "DreamStation Auto BiPAP" },
    { "700X130", 0, 6, "DreamStation Auto BiPAP" },
    { "700X150", 0, 6, "DreamStation Auto BiPAP" },
    
    { "410X150C", 0, 6, "DreamStation 2 CPAP" },
    { "420X150C", 0, 6, "DreamStation 2 Advanced CPAP" },  // from FDA filing
    { "520X110C", 0, 6, "DreamStation 2 Auto CPAP Advanced" },  // based on bottom label, boot screen says "Advanced Auto CPAP"
    { "520X130C", 0, 6, "DreamStation 2 Auto CPAP Advanced" },  // from user report
    { "520X150C", 0, 6, "DreamStation 2 Auto CPAP Advanced" },  // from user report
    { "521X120C", 0, 6, "DreamStation 2 Auto CPAP Advanced with P-Flex" },  // inferred from 501X120 and presence of "P-Flex" on bottom label
    { "521X140C", 0, 6, "DreamStation 2 avec GSM + Humidificateur" },  // from brochure

    { "950P",    5, 0, "BiPAP AutoSV Advanced System One" },
    { "951P",    5, 0, "BiPAP AutoSV Advanced System One" },
    { "960P",    5, 1, "BiPAP autoSV Advanced (System One 60 Series)" },
    { "961P",    5, 1, "BiPAP autoSV Advanced (System One 60 Series)" },
    { "960T",    5, 2, "BiPAP autoSV Advanced 30 (System One 60 Series)" },  // omits "(System One 60 Series)" on official reports
    { "961TCA",  5, 2, "BiPAP autoSV Advanced 30 (System One 60 Series)" },
    { "900X110", 5, 3, "DreamStation BiPAP autoSV" },
    { "900X120", 5, 3, "DreamStation BiPAP autoSV" },
    { "900X150", 5, 3, "DreamStation BiPAP autoSV" },
    
    { "1061401",  3, 0, "BiPAP S/T (C Series)" },
    { "1061T",    3, 3, "BiPAP S/T 30 (System One 60 Series)" },
    { "1160P",    3, 3, "BiPAP AVAPS 30 (System One 60 Series)" },
    { "1030X110", 3, 6, "DreamStation BiPAP S/T 30" },
    { "1030X150", 3, 6, "DreamStation BiPAP S/T 30 with AAM" },
    { "1130X110", 3, 6, "DreamStation BiPAP AVAPS 30" },
    { "1131X150", 3, 6, "DreamStation BiPAP AVAPS 30 AE" },
    { "1130X200", 3, 6, "DreamStation BiPAP AVAPS 30" },
    
    { "", 0, 0, "" },
};
PRS1ModelInfo s_PRS1ModelInfo;

PRS1ModelInfo::PRS1ModelInfo()
{
    for (int i = 0; !s_PRS1TestedModels[i].model.isEmpty(); i++) {
        const PRS1TestedModel & model = s_PRS1TestedModels[i];
        m_testedModels[model.family][model.familyVersion].append(model.model);
        
        m_modelNames[model.model] = model.name;
    }
    
    m_bricks = { "251P", "261CA", "261P", "200X110", "501V" };
}

bool PRS1ModelInfo::IsSupported(int family, int familyVersion) const
{
    if (m_testedModels.value(family).contains(familyVersion)) {
        return true;
    }
    return false;
}

bool PRS1ModelInfo::IsTested(const QString & model, int family, int familyVersion) const
{
    if (m_testedModels.value(family).value(familyVersion).contains(model)) {
        return true;
    }
    // Some 500X150 C0/C1 folders have contained this bogus model number in their PROP.TXT file,
    // with the same serial number seen in the main PROP.TXT file that shows the real model number.
    if (model == "100X100") {
#ifndef UNITTEST_MODE
        qDebug() << "Ignoring 100X100 for untested alert";
#endif
        return true;
    }
    return false;
};

static bool getVersionFromProps(const QHash<QString,QString> & props, int & family, int & familyVersion)
{
    bool ok;
    family = props["Family"].toInt(&ok, 10);
    if (ok) {
        familyVersion = props["FamilyVersion"].toInt(&ok, 10);
    }
    return ok;
}

bool PRS1ModelInfo::IsSupported(const QHash<QString,QString> & props) const
{
    int family, familyVersion;
    bool ok = getVersionFromProps(props, family, familyVersion);
    if (ok) {
        ok = IsSupported(family, familyVersion);
    }
    return ok;
}

bool PRS1ModelInfo::IsTested(const QHash<QString,QString> & props) const
{
    int family, familyVersion;
    bool ok = getVersionFromProps(props, family, familyVersion);
    if (ok) {
        ok = IsTested(props["ModelNumber"], family, familyVersion);
    }
    return ok;
};

bool PRS1ModelInfo::IsBrick(const QString & model) const
{
    bool is_brick = false;
    
    if (m_modelNames.contains(model)) {
        is_brick = m_bricks.contains(model);
    } else if (model.length() > 0) {
        // If we haven't seen it before, assume any 2xx is a brick.
        is_brick = (model.at(0) == QChar('2'));
    }
    
    return is_brick;
};

const char* PRS1ModelInfo::Name(const QString & model) const
{
    const char* name;
    if (m_modelNames.contains(model)) {
        name = m_modelNames[model];
    } else {
        name = "Unknown Model";
    }
    return name;
};


//********************************************************************************************

// Decoder for DreamStation 2 files, which encrypt the actual data after a header with the key.
// The public read/seek/pos/etc. functions are all in terms of the decoded stream.
class PRDS2File : public RawDataFile
{
  public:
    PRDS2File(class QFile & file, QHash<QByteArray,QByteArray> & keycache);
    virtual ~PRDS2File() {};
    bool isValid() const;
    QString guid() const;
  private:
    bool parseDS2Header();
    int read16();
    QByteArray readBytes();
    bool initializeKey();
    bool decryptData();
    QByteArray m_iv;
    QByteArray m_salt;
    QByteArray m_export_key;
    QByteArray m_export_key_tag;
    QByteArray m_payload_key;
    QByteArray m_payload_tag;
    QBuffer m_payload;
    bool m_valid;
  protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    virtual bool seek(qint64 pos);
    virtual qint64 pos() const;
    virtual qint64 size() const;
    
    QByteArray m_guid;
    static const int m_header_size = 0xCA;
};

PRDS2File::PRDS2File(class QFile & file, QHash<QByteArray,QByteArray> & keycache)
    : RawDataFile(file)
{
    bool valid = parseDS2Header();
    if (valid) {
        QByteArray key = m_iv + m_salt + m_export_key + m_export_key_tag;
        m_payload_key = keycache[key];
        if (m_payload_key.isEmpty()) {
            // Derive the key (slow).
            valid = initializeKey();
            if (valid) {
                // Cache the result for the next file.
                keycache[key] = m_payload_key;
            }
        }
        if (valid) {
            valid = decryptData();
        }
    }
    m_valid = valid;
    if (m_valid) {
        seek(0);  // initialize internal position
    }
}

bool PRDS2File::isValid() const {
    return m_valid;
}

QString PRDS2File::guid() const {
    QString guid(m_guid);
    return guid;
}

bool PRDS2File::seek(qint64 pos)
{
    if (!m_valid) {
        qWarning() << "seeking in unsupported DS2 file";
        return false;
    }
    QIODevice::seek(pos);
    return m_payload.seek(pos);
}

qint64 PRDS2File::pos() const
{
    if (!m_valid) {
        qWarning() << "querying pos in unsupported DS2 file";
        return 0;
    }
    return m_payload.pos();
}

qint64 PRDS2File::size() const
{
    return m_payload.size();
}

qint64 PRDS2File::readData(char *data, qint64 maxSize)
{
    if (!m_valid) {
        qWarning() << "reading from unsupported DS2 file";
        return -1;
    }
    return m_payload.read(data, maxSize);
}

bool PRDS2File::decryptData()
{
    bool valid = false;
    QByteArray ciphertext = m_device.read(m_device.size() - m_device.pos());
    QByteArray plaintext;

    CryptoResult error = decrypt_aes256_gcm(m_payload_key, m_iv, ciphertext, m_payload_tag, plaintext);

    if (error) {
        if (error == InvalidTag) {
            static const QByteArray s_zero_tag(16, 0);
            if (m_payload_tag == s_zero_tag) {
                // This has been observed where the tag is zero and the data appears truncated.
                // Decrypt and ignore the tag. Rely on the decrypted payload's CRC for integrity.
                qWarning() << name() << "DS2 payload has zero tag, recovering data";
                error = encrypt_aes256_gcm(m_payload_key, m_iv, ciphertext, plaintext, m_payload_tag);
                if (error) {
                    qWarning() << "*** DS2 unexpected exception decrypting" << name();
                }
            } else {
                qWarning() << name() << "DS2 payload doesn't match tag, skipping";
            }
        } else {
            qWarning() << "*** DS2 unexpected exception decrypting" << name();
        }
    }
    if (!error) {
        m_payload.setData(plaintext);
        m_payload.open(QIODevice::ReadOnly);
        valid = true;
    }
    return valid;
}


static const int KEY_SIZE = 256 / 8;  // AES-256
static const uint8_t OSCAR_KEY[KEY_SIZE+1]  = "Patient access to their own data";
static const uint8_t COMMON_KEY[KEY_SIZE] = { 0x75, 0xB3, 0xA2, 0x12, 0x4A, 0x65, 0xAF, 0x97, 0x54, 0xD8, 0xC1, 0xF3, 0xE5, 0x2E, 0xB6, 0xF0, 0x23, 0x20, 0x57, 0x69, 0x7E, 0x38, 0x0E, 0xC9, 0x4A, 0xDC, 0x46, 0x45, 0xB6, 0x92, 0x5A, 0x98 };

static const QByteArray s_oscar_key((const char*) OSCAR_KEY, KEY_SIZE);
static const QByteArray s_common_key((const char*) COMMON_KEY, KEY_SIZE);

bool PRDS2File::initializeKey()
{
    bool valid = false;
    QByteArray common_key;

    CryptoResult error = decrypt_aes256(s_oscar_key, s_common_key, common_key);
    if (error) {
        qWarning() << "*** DS2 unexpected exception deriving common key";
        return false;
    }

    QByteArray salted_key(KEY_SIZE, 0);
    error = pbkdf2_sha256(common_key, m_salt, 10000, salted_key);
    if (error) {
        qWarning() << "*** DS2 unexpected exception deriving salted key for" << name();
        return false;
    }

    error = decrypt_aes256_gcm(salted_key, m_iv, m_export_key, m_export_key_tag, m_payload_key);
    if (error) {
        if (error == InvalidTag) {
            qWarning() << "DS2 validation of payload key failed for" << name();
        } else {
            qWarning() << "*** DS2 unexpected exception deriving key for" << name();
        }
    } else {
        valid = true;
    }
    return valid;
}

bool PRDS2File::parseDS2Header()
{
    if (m_device.size() == 0) {
        qWarning() << name() << "is empty, skipping";
        return false;
    }
    int a = read16();
    int b = read16();
    int c = read16();
    if (a != 0x0D || b != 1 || c != 1) {
        qWarning() << "DS2 unexpected first bytes =" << a << b << c;
        return false;
    }

    m_guid = readBytes();
    if (m_guid.size() != 36) {
        qWarning() << "DS2 guid unexpected length" << m_guid.size();
    } else {
        //qDebug() << "DS2 guid {" << m_guid << "}";
    }

    m_iv = readBytes();  // 96-bit IV
    m_salt = readBytes();  // 128-bit salt used to decrypt export key
    if (m_iv.size() != 12 || m_salt.size() != 16) {
        qWarning() << "DS2 IV,salt sizes =" << m_iv.size() << m_salt.size();
    } else {
        //qDebug() << "DS2 IV,salt =" << m_iv.toHex() << m_salt.toHex();
    }
    
    int f = read16();
    int g = read16();
    if (f != 0 || g != 1) {
        qWarning() << "DS2 unexpected middle bytes =" << f << g;
    }
    
    QByteArray import_key = readBytes();      // payload key encrypted with device-specific key
    QByteArray import_key_tag = readBytes();  // tag of import key
    if (import_key.size() != 32 || import_key_tag.size() != 16) {
        qWarning() << "DS2 import_key sizes =" << import_key.size() << import_key_tag.size();
    } else {
        //qDebug() << "DS2 import_key,tag =" << import_key.toHex() << import_key_tag.toHex();
    }

    m_export_key = readBytes();      // payload key encrypted with salted common key
    m_export_key_tag = readBytes();  // tag of export key
    if (m_export_key.size() != 32 || m_export_key_tag.size() != 16) {
        qWarning() << "DS2 export_key sizes =" << m_export_key.size() << m_export_key_tag.size();
    } else {
        //qDebug() << "DS2 export_key,tag =" << m_export_key.toHex() << m_export_key_tag.toHex();
    }

    m_payload_tag = readBytes();
    if (m_payload_tag.size() != 16) {
        qWarning() << "DS2 payload tag size =" << m_payload_tag.size();
    } else {
        //qDebug() << "DS2 payload tag =" << m_payload_tag.toHex();
    }

    if (m_device.pos() != m_header_size) {
        qWarning() << "DS2 header size !=" << m_header_size;
    }
    return true;
}

int PRDS2File::read16()
{
    unsigned char data[2];
    int result;
    
    result = m_device.read((char*) data, sizeof(data));  // access the underlying data for the header
    if (result == sizeof(data)) {
        result = data[0] | (data[1] << 8);
    } else {
        result = 0;
    }
    return result;
}

QByteArray PRDS2File::readBytes()
{
    int length = read16();
    QByteArray result = m_device.read(length);  // access the underlying data for the header
    if (result.size() < length) {
        result.clear();
    }
    return result;
}


//********************************************************************************************


QMap<const char*,const char*> s_PRS1Series = {
    { "System One 60 Series", ":/icons/prs1_60s.png" },  // needs to come before following substring
    { "System One",           ":/icons/prs1.png" },
    { "C Series",             ":/icons/prs1vent.png" },
    { "DreamStation 2",       ":/icons/prds2.png" },  // needs to come before following substring
    { "DreamStation",         ":/icons/dreamstation.png" },
};

PRS1Loader::PRS1Loader()
{
#ifndef UNITTEST_MODE  // no QPixmap without a QGuiApplication
    for (auto & series : s_PRS1Series.keys()) {
        QString path = s_PRS1Series[series];
        m_pixmap_paths[series] = path;
        m_pixmaps[series] = QPixmap(path);
    }
#endif

    m_type = MT_CPAP;
}

PRS1Loader::~PRS1Loader()
{
}

bool isdigit(QChar c)
{
    if ((c >= '0') && (c <= '9')) { return true; }

    return false;
}


// Tests path to see if it has (what looks like) a valid PRS1 folder structure
// This is used both to detect newly inserted media and to decide which loader
// to use after the user selects a folder.
//
// TODO: Ideally there should be a way to handle the two scenarios slightly
// differently. In the latter case, it should clean up the selection and
// return the canonical path if it detects one, allowing us to remove the
// notification about selecting the root of the card. That kind of cleanup
// wouldn't be appropriate when scanning devices.
bool PRS1Loader::Detect(const QString & selectedPath)
{
    QString path = selectedPath;
    if (GetPSeriesPath(path).isEmpty()) {
        // Try up one level in case the user selected the P-Series folder within the SD card.
        path = QFileInfo(path).canonicalPath();
    }

    QStringList machines = FindMachinesOnCard(path);
    return !machines.isEmpty();
}

QString PRS1Loader::GetPSeriesPath(const QString & path)
{
    QString outpath = "";
    QDir root(path);
    QStringList dirs = root.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Hidden | QDir::NoSymLinks);
    for (auto & dir : dirs) {
        // We've seen P-Series, P-SERIES, and p-series, so we need to search for the directory
        // in a way that won't break on a case-sensitive filesystem.
        if (dir.toUpper() == "P-SERIES") {
            outpath = path + QDir::separator() + dir;
            break;
        }
    }
    return outpath;
}

QStringList PRS1Loader::FindMachinesOnCard(const QString & cardPath)
{
    QStringList machinePaths;

    QString pseriesPath = this->GetPSeriesPath(cardPath);
    QDir pseries(pseriesPath);

    // If it contains a P-Series folder, it's a PRS1 SD card
    if (!pseriesPath.isEmpty() && pseries.exists()) {
        pseries.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        pseries.setSorting(QDir::Name);
        QFileInfoList plist = pseries.entryInfoList();

        // Look for device directories (containing a PROP.TXT or properties.txt)
        QFileInfoList propertyfiles;
        for (auto & pfi : plist) {
            if (pfi.isDir()) {
                QString machinePath = pfi.canonicalFilePath();
                QDir machineDir(machinePath);
                QFileInfoList mlist = machineDir.entryInfoList();
                for (auto & mfi : mlist) {
                    if (QDir::match("PROP*.TXT", mfi.fileName())) {
                        // Found a properties file, this is a device folder
                        propertyfiles.append(mfi);
                    }
                    if (QDir::match("PROP.BIN", mfi.fileName())) {
                        // Found a DreamStation 2 properties file, this is a device folder
                        propertyfiles.append(mfi);
                    }
                }
            }
        }

        // Sort devices from oldest to newest.
        std::sort(propertyfiles.begin(), propertyfiles.end(),
            [](const QFileInfo & a, const QFileInfo & b)
        {
            return a.lastModified() < b.lastModified();
        });

        for (auto & propertyfile : propertyfiles) {
            machinePaths.append(propertyfile.canonicalPath());
        }
    }

    return machinePaths;
}


void parseModel(MachineInfo & info, const QString & modelnum)
{
    info.modelnumber = modelnum;

    const char* name = s_PRS1ModelInfo.Name(modelnum);
    const char* series = nullptr;
    for (auto & s : s_PRS1Series.keys()) {
        if (QString(name).contains(s)) {
            series = s;
            break;
        }
    }
    if (series == nullptr) {
        if (modelnum != "100X100") {  // Bogus model number seen in empty C0/Clear0 directories.
            qWarning() << "unknown series for" << name << modelnum;
        }
        series = "unknown";
    }
    
    info.model = QObject::tr(name);
    info.series = series;
}

bool PRS1Loader::PeekProperties(const QString & filePath, QHash<QString,QString> & props)
{
    QString filename = filePath;
    const static QMap<QString,QString> s_longFieldNames = {
        // CF?
        { "SN", "SerialNumber" },
        { "MN", "ModelNumber" },
        { "PT", "ProductType" },
        { "DF", "DataFormat" },
        { "DFV", "DataFormatVersion" },
        { "F", "Family" },
        { "FV", "FamilyVersion" },
        { "SV", "SoftwareVersion" },
        { "FD", "FirstDate" },
        { "LD", "LastDate" },
        // SID?
        // SK?
        // TS?
        // DC?
        { "BK", "BasicKey" },
        { "DK", "DetailsKey" },
        { "EK", "ErrorKey" },
        { "FN", "PatientFolderNum" },  // most recent Pn directory
        { "PFN", "PatientFileNum" },   // number of files in the most recent Pn directory
        { "EFN", "EquipFileNum" },     // number of .004 files in the E directory
        { "DFN", "DFileNum" },         // number of .003 files in the D directory
        { "VC", "ValidCheck" },
    };

    #if 1
    // fix enpty PROP.TXT file problem. use PROP.BAK if it exists
    // Otherwise import will fail.
    // Just readis bak file instead of prop.txt. No other changes.
    QFileInfo fi(filename);
    if (fi.isFile() && fi.size()==0 && fi.suffix().toUpper() == "TXT")  {
        QString newFilePath = fi.absolutePath() + "/" + fi.completeBaseName() + ".BAK";
        fi = QFileInfo(newFilePath);
        if (fi.isFile() && fi.size()>0)  {
            filename = newFilePath;
        }
    }
    // END fix enpty PROP.TXT file problem. use PROP.BAK if it exists
    #endif

    QFile f(filename);
    if (!f.open(QFile::ReadOnly)) {
        return false;
    }

    RawDataFile* src;
    if (QFileInfo(f).suffix().toUpper() == "BIN") {
        // If it's a DS2 file, insert the DS2 wrapper to decode the chunk stream.
        PRDS2File* ds2 = new PRDS2File(f, m_keycache);
        if (!ds2->isValid()) {
            //qWarning() << filename << "unable to decrypt";
            delete ds2;
            return false;
        }
        src = ds2;
        props["GUID"] = ds2->guid();
    } else {
        // Otherwise just use the file as input.
        src = new RawDataFile(f);
    }
    
    {
    QTextStream in(src);  // Scope this here so that it's torn down before we delete src below.
    
    do {
        QString line = in.readLine();
        QStringList pair = line.split("=");
        if (pair.size() != 2) {
            qWarning() << src->name() << "malformed line:" << line;
            QHashIterator<QString,QString> i(props);
            while (i.hasNext()) {
                i.next();
                qDebug() << i.key() << ":" << i.value();
            }
            break;
        }

        if (s_longFieldNames.contains(pair[0])) {
            pair[0] = s_longFieldNames[pair[0]];
        }
        if (pair[0] == "Family") {
            if (pair[1] == "xPAP") {
                pair[1] = "0";
            } else if (pair[1] == "Ventilator") {
                pair[1] = "3";
            }
        }
        props[pair[0]] = pair[1];
    } while (!in.atEnd());
    
    }

    delete src;

    return props.size() > 0;
}

bool PRS1Loader::PeekProperties(MachineInfo & info, const QString & filename)
{
    QHash<QString,QString> props;
    if (!PeekProperties(filename, props)) {
        return false;
    }
    QString modelnum;
    for (auto & key : props.keys()) {
        bool skip = false;

        if (key == "ModelNumber") {
            modelnum = props[key];
            skip = true;
        }
        if (key == "SerialNumber") {
            info.serial = props[key];
            skip = true;
        }
        if (key == "ProductType") {
            bool ok;
            props[key].toInt(&ok, 16);
            if (!ok) qWarning() << "ProductType" << props[key];
            skip = true;
        }
        if (skip) continue;

        info.properties[key] = props[key];
    };

    if (!modelnum.isEmpty()) {
        parseModel(info, modelnum);
    } else {
        qWarning() << "missing model number" << filename;
    }

    return true;
}


MachineInfo PRS1Loader::PeekInfo(const QString & path)
{
    QStringList machines = FindMachinesOnCard(path);
    if (machines.isEmpty()) {
        return MachineInfo();
    }

    // Present information about the newest device on the card.
    QString newpath = machines.last();
    
    MachineInfo info = newInfo();
    if (!PeekProperties(info, newpath+"/properties.txt")) {
        if (!PeekProperties(info, newpath+"/PROP.TXT")) {
            // Detect (unsupported) DreamStation 2
            QString filepath(newpath + "/PROP.BIN");
            if (!PeekProperties(info, filepath)) {
                qWarning() << "No properties file found in" << newpath;
            }
        }
    }
    return info;
}


int PRS1Loader::Open(const QString & selectedPath)
{
    QString path = selectedPath;
    if (GetPSeriesPath(path).isEmpty()) {
        // Try up one level in case the user selected the P-Series folder within the SD card.
        path = QFileInfo(path).canonicalPath();
    }

    QStringList machines = FindMachinesOnCard(path);
    // Return an error if no devices were found.
    if (machines.isEmpty()) {
        qDebug() << "No PRS1 devices found at" << path;
        return -1;
    }

    // Import each device, from oldest to newest.
    // TODO: Loaders should return the set of devices during detection, so that Open() will
    // open a unique device, instead of surprising the user.
    int c = 0;
    bool failures = false;
    for (auto & machinePath : machines) {
        if (m_ctx == nullptr) {
            qWarning() << "PRS1Loader::Open() called without a valid m_ctx object present";
            return 0;
        }
        int imported = OpenMachine(machinePath);
        if (imported >= 0) {  // don't let errors < 0 suppress subsequent successes
            c += imported;
        } else {
            failures = true;
        }
        m_ctx->FlushUnexpectedMessages();
    }
    if (c == 0 && failures) {
        // report an error when there were failures and no successess
        c = -1;
    }
    return c;
}


int PRS1Loader::OpenMachine(const QString & path)
{
    Q_ASSERT(m_ctx);

    qDebug() << "Opening PRS1 " << path;
    QDir dir(path);

    if (!dir.exists() || (!dir.isReadable())) {
        return 0;
    }
    m_abort = false;

    emit updateMessage(QObject::tr("Getting Ready..."));
    QCoreApplication::processEvents();

    emit setProgressValue(0);

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = FindSessionDirsAndProperties(path, paths, propertyfile);

    bool supported = CreateMachineFromProperties(propertyfile);
    if (!supported) {
        // Device is unsupported.
        return -1;
    }

    emit updateMessage(QObject::tr("Backing Up Files..."));
    QCoreApplication::processEvents();

    QString backupPath = context()->GetBackupPath() + path.section("/", -2);

    if (QDir::cleanPath(path).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(path, backupPath);
    }

    emit updateMessage(QObject::tr("Scanning Files..."));
    QCoreApplication::processEvents();

    // Walk through the files and create an import task for each logical session.
    ScanFiles(paths, sessionid_base);

    int tasks = countTasks();

    emit updateMessage(QObject::tr("Importing Sessions..."));
    QCoreApplication::processEvents();

    runTasks(AppSetting->multithreading());

    return tasks;
}


int PRS1Loader::FindSessionDirsAndProperties(const QString & path, QStringList & paths, QString & propertyfile)
{
    QDir dir(path);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    QString filename;

    int sessionid_base = 10;

    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        filename = fi.fileName();

        if (fi.isDir()) {
            if ((filename[0].toLower() == 'p') && (isdigit(filename[1]))) {
                // p0, p1, p2.. etc.. folders contain the session data
                paths.push_back(fi.canonicalFilePath());
            } else if (filename.toLower() == "e") {
                // Error files..
                // Reminder: I have been given some info about these. should check it over.
            }
        } else if (filename.compare("properties.txt",Qt::CaseInsensitive) == 0) {
            propertyfile = fi.canonicalFilePath();
        } else if (filename.compare("PROP.TXT",Qt::CaseInsensitive) == 0) {
            sessionid_base = 16;
            propertyfile = fi.canonicalFilePath();
        } else if (filename.compare("PROP.BIN", Qt::CaseInsensitive) == 0) {
            sessionid_base = 16;
            propertyfile = fi.canonicalFilePath();
        }
    }
    return sessionid_base;
}


bool PRS1Loader::CreateMachineFromProperties(QString propertyfile)
{
    m_keycache.clear();
    
    MachineInfo info = newInfo();
    QHash<QString,QString> props;
    if (!PeekProperties(propertyfile, props) || !s_PRS1ModelInfo.IsSupported(props)) {
        if (props.contains("ModelNumber")) {
            int family, familyVersion;
            getVersionFromProps(props, family, familyVersion);
            QString model_number = props["ModelNumber"];
            qWarning().noquote() << "Model" << model_number << QString("(F%1V%2)").arg(family).arg(familyVersion) << "unsupported.";
            info.modelnumber = QObject::tr("model %1").arg(model_number);
        } else {
            qWarning() << "Unable to identify model or series!";
            info.modelnumber = QObject::tr("unknown model");
        }
        emit deviceIsUnsupported(info);
        return false;
    }

    // Have a peek first to get the model number.
    PeekProperties(info, propertyfile);

    if (s_PRS1ModelInfo.IsBrick(info.modelnumber)) {
        emit deviceReportsUsageOnly(info);
    }

    // Which is needed to get the right device record..
    m_ctx->CreateMachineFromInfo(info);

    if (!s_PRS1ModelInfo.IsTested(props)) {
        qDebug() << info.modelnumber << "untested";
        emit deviceIsUntested(info);
    }

    return true;
}


static QString relativePath(const QString & inpath)
{
    #if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
        QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), Qt::SkipEmptyParts);
    #else
        QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QString::SkipEmptyParts);
    #endif
    QString relative = pathlist.mid(pathlist.size()-3).join(QDir::separator());
    return relative;
}

static bool chunksIdentical(const PRS1DataChunk* a, const PRS1DataChunk* b)
{
    return (a->hash() == b->hash());
}

static QString chunkComparison(const PRS1DataChunk* a, const PRS1DataChunk* b)
{
    return QString("Session %1 in %2 @ %3 %4 %5 @ %6, skipping")
            .arg(a->sessionid)
            .arg(relativePath(a->m_path)).arg(a->m_filepos)
            .arg(chunksIdentical(a, b) ? "is identical to" : "differs from")
            .arg(relativePath(b->m_path)).arg(b->m_filepos);

}

void PRS1Loader::ScanFiles(const QStringList & paths, int sessionid_base)
{
    Q_ASSERT(m_ctx);
    SessionID sid;
    long ext;

    QDir dir;
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    int size = paths.size();

    sesstasks.clear();
    new_sessions.clear(); // this hash is used by OpenFile


    PRS1Import * task = nullptr;
    // Note, I have observed p0/p1/etc folders containing duplicates session files (in Robin Sanders data.)

    QDateTime datetime;

    qint64 ignoreBefore = m_ctx->IgnoreSessionsOlderThan().toMSecsSinceEpoch()/1000;
    bool ignoreOldSessions = m_ctx->ShouldIgnoreOldSessions();
    QSet<SessionID> skipped;

    // for each p0/p1/p2/etc... folder
    for (int p=0; p < size; ++p) {
        dir.setPath(paths.at(p));

        if (!dir.exists() || !dir.isReadable()) {
            qWarning() << dir.canonicalPath() << "can't read directory";
            continue;
        }

        QFileInfoList flist = dir.entryInfoList();

        // Scan for individual session files
        for (int i = 0; i < flist.size(); i++) {
#ifndef UNITTEST_MODE
            QCoreApplication::processEvents();
#endif
            if (isAborted()) {
                qDebug() << "received abort signal";
                break;
            }
            QFileInfo fi = flist.at(i);
            QString path = fi.canonicalFilePath();
            bool ok;

            if (fi.fileName() == ".DS_Store") {
                continue;
            }

            QString ext_s = fi.fileName().section(".", -1);
            if (ext_s.toUpper().startsWith("B")) {  // .B01, .B02, etc.
                ext_s = ext_s.mid(1);
            }
            ext = ext_s.toInt(&ok);
            if (!ok) {
                // not a numerical extension
                qInfo() << path << "unexpected filename";
                continue;
            }

            QString session_s = fi.fileName().section(".", 0, -2);
            sid = session_s.toInt(&ok, sessionid_base);
            if (!ok) {
                // not a numerical session ID
                qInfo() << path << "unexpected filename";
                continue;
            }

            // TODO: BUG: This isn't right, since files can have multiple session
            // chunks, which might not correspond to the filename. But before we can
            // fix this we need to come up with a reasonably fast way to filter previously
            // imported files without re-reading all of them.
            if (context()->SessionExists(sid)) {
                // Skip already imported session
                // TODO: Consider reinstating this debug statement if/when we scan only new/changed files.
                //qDebug() << path << "session already exists, skipping" << sid;
                continue;
            }

            if ((ext == 5) || (ext == 6)) {
                if (skipped.contains(sid)) {
                    // We don't know the timestamp until the file is parsed, which we only do for
                    // waveform data at import (after scanning) since it's so large. If we relied
                    // solely on the chunks' timestamps at that point, we'd get half of an otherwise
                    // skipped session (the half after midnight).
                    //
                    // So we skip the entire file here based on the session's other data.
                    continue;
                }

                // Waveform files aren't grouped... so we just want to add the filename for later
                QHash<SessionID, PRS1Import *>::iterator it = sesstasks.find(sid);
                if (it != sesstasks.end()) {
                    task = it.value();
                } else {
                    // Should probably check if session already imported has this data missing..

                    // Create the group if we see it first..
                    task = new PRS1Import(this, sid, sessionid_base);
                    sesstasks[sid] = task;
                    queTask(task);
                }

                if (ext == 5) {
                    // Occasionally waveforms in a session can be split into multiple files.
                    //
                    // This seems to happen when the device begins writing the waveform file
                    // before realizing that it will hit its 500-file-per-directory limit
                    // for the remaining session files, at which point it appears to write
                    // the rest of the waveform data along with the summary and event files
                    // in the next directory.
                    //
                    // All samples exhibiting this behavior are DreamStations.
                    task->m_wavefiles.append(fi.canonicalFilePath());
                } else if (ext == 6) {
                    // Oximetry data can also be split into multiple files, see waveform
                    // comment above.
                    task->m_oxifiles.append(fi.canonicalFilePath());
                }

                continue;
            }

            // Parse the data chunks and read the files..
            QList<PRS1DataChunk *> Chunks = ParseFile(fi.canonicalFilePath());

            for (int i=0; i < Chunks.size(); ++i) {
                if (isAborted()) {
                    qDebug() << "received abort signal 2";
                    break;
                }

                PRS1DataChunk * chunk = Chunks.at(i);

                SessionID chunk_sid = chunk->sessionid;
                if (i == 0 && chunk_sid != sid) {  // log session ID mismatches
                    // This appears to be benign, probably when a card is out of the device one night and
                    // then inserted in the morning. It writes out all of the still-in-memory summaries and
                    // events up through the last night (and no waveform data).
                    //
                    // This differs from the first time a card is inserted, because in that case the filename
                    // *is* equal to the first session contained within it, and then filenames for the
                    // remaining sessions contained in that file are skipped.
                    //
                    // Because the card was present and previous sessions were written with their filenames,
                    // the first available filename isn't the first session contained in the file.
                    //qDebug() << fi.canonicalFilePath() << "first session is" << chunk_sid << "instead of" << sid;
                }
                if (context()->SessionExists(chunk_sid)) {
                    qDebug() << path << "session already imported, skipping" << sid << chunk_sid;
                    delete chunk;
                    continue;
                }
                if (ignoreOldSessions && chunk->timestamp < ignoreBefore) {
                    qDebug().noquote() << relativePath(path) << "skipping session" << chunk_sid << ":"
                        << QDateTime::fromMSecsSinceEpoch(chunk->timestamp*1000).toString() << "older than"
                        << QDateTime::fromMSecsSinceEpoch(ignoreBefore*1000).toString();
                    skipped += chunk_sid;
                    delete chunk;
                    continue;
                }

                task = nullptr;
                QHash<SessionID, PRS1Import *>::iterator it = sesstasks.find(chunk_sid);
                if (it != sesstasks.end()) {
                    task = it.value();
                } else {
                    task = new PRS1Import(this, chunk_sid, sessionid_base);
                    sesstasks[chunk_sid] = task;
                    // save a loop an que this now
                    queTask(task);
                }
                switch (ext) {
                case 0:
                    if (task->compliance) {
                        if (chunksIdentical(chunk, task->compliance)) {
                            // Never seen identical compliance chunks, so keep logging this for now.
                            qDebug() << chunkComparison(chunk, task->compliance);
                        } else {
                            qWarning() << chunkComparison(chunk, task->compliance);
                        }
                        delete chunk;
                        continue; // (skipping to avoid duplicates)
                    }
                    task->compliance = chunk;
                    break;
                case 1:
                    if (task->summary) {
                        if (chunksIdentical(chunk, task->summary)) {
                            // This seems to be benign. It happens most often when a single file contains
                            // a bunch of chunks and subsequent files each contain a single chunk that was
                            // already covered by the first file. It also sometimes happens with entirely
                            // duplicate files between e.g. a P1 and P0 directory.
                            //
                            // It's common enough that we don't emit a message about it by default.
                            //qDebug() << chunkComparison(chunk, task->summary);
                        } else {
                            // Warn about any non-identical duplicate session IDs.
                            //
                            // This seems to happen with F5V1 slice 8, which is the only slice in a session,
                            // and which doesn't update the session ID, so the following slice 7 session
                            // (which can be hours later) has the same session ID. Neither affects import.
                            qWarning() << chunkComparison(chunk, task->summary);
                        }
                        delete chunk;
                        continue;
                    }
                    task->summary = chunk;
                    break;
                case 2:
                    if (task->m_event_chunks.count() > 0) {
                        PRS1DataChunk* previous;
                        if (chunk->family == 3 && chunk->familyVersion <= 3) {
                            // F3V0 and F3V3 events are formatted as waveforms, with one chunk per mask-on slice,
                            // and thus multiple chunks per session.
                            previous = task->m_event_chunks[chunk->timestamp];
                            if (previous != nullptr) {
                                // Skip any chunks with identical timestamps.
                                qWarning() << chunkComparison(chunk, previous);
                                delete chunk;
                                continue;
                            }
                            // fall through to add the new chunk
                        } else {
                            // Nothing else should have multiple event chunks per session.
                            previous = task->m_event_chunks.first();
                            if (chunksIdentical(chunk, previous)) {
                                // See comment above regarding identical summary chunks.
                                //qDebug() << chunkComparison(chunk, previous);
                            } else {
                                qWarning() << chunkComparison(chunk, previous);
                            }
                            delete chunk;
                            continue;
                        }
                    }
                    task->m_event_chunks[chunk->timestamp] = chunk;
                    break;
                default:
                    qWarning() << path << "unexpected file";
                    break;
                }
            }
        }
        if (isAborted()) {
            qDebug() << "received abort signal 3";
            break;
        }
    }
}


// The set of PRS1 "on-demand" channels that only get created on import if the session
// contains events of that type.  Any channels not on this list always get created if
// they're reported/supported by the parser.
static const QVector<PRS1ParsedEventType> PRS1OnDemandChannels =
{
    PRS1TimedBreathEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    
    // Pressure initialized on-demand for F0 due to the possibility of bilevel vs. single pressure.
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    
    // Pressure average initialized on-demand for F0 due to the different semantics of bilevel vs. single pressure.
    PRS1PressureAverageEvent::TYPE,
    PRS1FlexPressureAverageEvent::TYPE,
};

// The set of "non-slice" channels are independent of mask-on slices, i.e. they
// are continuously reported and charted regardless of whether the mask is on.
static const QSet<PRS1ParsedEventType> PRS1NonSliceChannels =
{
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    PRS1SnoresAtPressureEvent::TYPE,
};

// The channel ID (referenced by pointer because their values aren't initialized
// prior to runtime) to which a given PRS1 event should be added.  Events with
// no channel IDs are silently dropped, and events with more than one channel ID
// must be handled specially.
static const QHash<PRS1ParsedEventType,QVector<ChannelID*>> PRS1ImportChannelMap =
{
    { PRS1ClearAirwayEvent::TYPE,       { &CPAP_ClearAirway } },
    { PRS1ObstructiveApneaEvent::TYPE,  { &CPAP_Obstructive } },
    { PRS1HypopneaEvent::TYPE,          { &CPAP_Hypopnea } },
    { PRS1FlowLimitationEvent::TYPE,    { &CPAP_FlowLimit } },
    { PRS1SnoreEvent::TYPE,             { &CPAP_Snore, &CPAP_VSnore2 } },  // VSnore2 is calculated from snore count, used to flag nonzero intervals on overview
    { PRS1VibratorySnoreEvent::TYPE,    { &CPAP_VSnore } },
    { PRS1RERAEvent::TYPE,              { &CPAP_RERA } },

    { PRS1PeriodicBreathingEvent::TYPE, { &CPAP_PB } },
    { PRS1LargeLeakEvent::TYPE,         { &CPAP_LargeLeak } },
    { PRS1TotalLeakEvent::TYPE,         { &CPAP_LeakTotal } },
    { PRS1LeakEvent::TYPE,              { &CPAP_Leak } },

    { PRS1RespiratoryRateEvent::TYPE,   { &CPAP_RespRate } },
    { PRS1TidalVolumeEvent::TYPE,       { &CPAP_TidalVolume } },
    { PRS1MinuteVentilationEvent::TYPE, { &CPAP_MinuteVent } },
    { PRS1PatientTriggeredBreathsEvent::TYPE, { &CPAP_PTB } },
    { PRS1TimedBreathEvent::TYPE,       { &PRS1_TimedBreath } },
    { PRS1FlowRateEvent::TYPE,          { &PRS1_PeakFlow } },  // Only reported by F3V0 and F3V3  // TODO: should this stat be calculated from flow waveforms on other models?

    { PRS1PressureSetEvent::TYPE,       { &CPAP_PressureSet } },
    { PRS1IPAPSetEvent::TYPE,           { &CPAP_IPAPSet, &CPAP_PS } },  // PS is calculated from IPAPset and EPAPset when both are supported (F0) TODO: Should this be a separate channel since it's not a 2-minute average?
    { PRS1EPAPSetEvent::TYPE,           { &CPAP_EPAPSet } },            // EPAPset is supported on F5 without any corresponding IPAPset, so it shouldn't always create a PS channel
    { PRS1PressureAverageEvent::TYPE,   { &CPAP_Pressure } },           // This is the time-weighted average pressure in bilevel modes.
    { PRS1FlexPressureAverageEvent::TYPE, { &CPAP_EPAP } },             // This is effectively EPAP due to Flex reduced pressure in single-pressure modes.
    { PRS1IPAPAverageEvent::TYPE,       { &CPAP_IPAP } },
    { PRS1EPAPAverageEvent::TYPE,       { &CPAP_EPAP, &CPAP_PS } },     // PS is calculated from IPAP and EPAP averages (F3 and F5)
    { PRS1IPAPLowEvent::TYPE,           { &CPAP_IPAPLo } },
    { PRS1IPAPHighEvent::TYPE,          { &CPAP_IPAPHi } },

    { PRS1Test1Event::TYPE,             { &CPAP_Test1 } },  // ??? F3V6
    { PRS1Test2Event::TYPE,             { &CPAP_Test2 } },  // ??? F3V6

    { PRS1PressurePulseEvent::TYPE,     { &CPAP_PressurePulse } },
    { PRS1ApneaAlarmEvent::TYPE,        { /* Not imported */ } },
    { PRS1SnoresAtPressureEvent::TYPE,  { /* Not imported */ } },
    { PRS1AutoPressureSetEvent::TYPE,   { /* Not imported */ } },
    { PRS1VariableBreathingEvent::TYPE, { &PRS1_VariableBreathing } },  // UNCONFIRMED
    
    { PRS1HypopneaCount::TYPE,          { &CPAP_Hypopnea } },     // F3V3 only, generates individual events on import
    { PRS1ObstructiveApneaCount::TYPE,  { &CPAP_Obstructive } },  // F3V3 only, generates individual events on import
    { PRS1ClearAirwayCount::TYPE,       { &CPAP_ClearAirway } },  // F3V3 only, generates individual events on import
};

//********************************************************************************************


PRS1Import::~PRS1Import()
{
    delete compliance;
    delete summary;
    for (auto & e : m_event_chunks.values()) {
        delete e;
    }
    for (int i=0;i < waveforms.size(); ++i) {
        delete waveforms.at(i);
    }
    for (auto & c : oximetry) {
        delete c;
    }
}


void PRS1Import::CreateEventChannels(const PRS1DataChunk* chunk)
{
    const QVector<PRS1ParsedEventType> & supported = GetSupportedEvents(chunk);

    // Generate the list of channels created by non-slice events for this device.
    // We can't just use the full list of non-slice events, since on some devices
    // PS is generated by slice events (EPAP/IPAP average).
    // Duplicates need to be removed. QSet does the removal.
    #if QT_VERSION < QT_VERSION_CHECK(5,14,0)
        // convert QVvector to QList then to QSet
        QSet<PRS1ParsedEventType> supportedNonSliceEvents = QSet<PRS1ParsedEventType>::fromList( QList<PRS1ParsedEventType>::fromVector( supported ) );
    #else
        // release 5.14 supports the direct conversion.
        QSet<PRS1ParsedEventType> supportedNonSliceEvents(supported.begin(),supported.end() ) ;
    #endif

    supportedNonSliceEvents.intersect(PRS1NonSliceChannels);
    QSet<ChannelID> supportedNonSliceChannels;
    for (auto & e : supportedNonSliceEvents) {
        for (auto & pChannelID : PRS1ImportChannelMap[e]) {
            supportedNonSliceChannels += *pChannelID;
        }
    }

    // Clear channels to prepare for a new slice, except for channels created by
    // non-slice events.
    for (auto & c : m_importChannels.keys()) {
        if (supportedNonSliceChannels.contains(c) == false) {
            m_importChannels.remove(c);
        }
    }

    // Create all supported channels (except for on-demand ones that only get created if an event appears)
    for (auto & e : supported) {
        if (!PRS1OnDemandChannels.contains(e)) {
            for (auto & pChannelID : PRS1ImportChannelMap[e]) {
                GetImportChannel(*pChannelID);
            }
        }
    }
}


EventList* PRS1Import::GetImportChannel(ChannelID channel)
{
    if (!channel) {
        qCritical() << this->sessionid << "channel in import table has not been added to schema!";
    }
    EventList* C = m_importChannels[channel];
    if (C == nullptr) {
        C = session->AddEventList(channel, EVL_Event);
        Q_ASSERT(C);  // Once upon a time AddEventList could return nullptr, but not any more.
        m_importChannels[channel] = C;
    }
    return C;
}


void PRS1Import::AddEvent(ChannelID channel, qint64 t, float value, float gain)
{
    EventList* C = GetImportChannel(channel);
    Q_ASSERT(C);
    if (C->count() == 0) {
        // Initialize the gain (here, since required channels are created with default gain).
        C->setGain(gain);
    } else {
        // Any change in gain is a programming error.
        if (gain != C->gain()) {
            qWarning() << "gain mismatch for channel" << channel << "at" << ts(t);
        }
    }

    // Add the event
    C->AddEvent(t, value, gain);
}


bool PRS1Import::UpdateCurrentSlice(PRS1DataChunk* chunk, qint64 t)
{
    bool updated = false;

    if (!m_currentSliceInitialized) {
        m_currentSliceInitialized = true;
        m_currentSlice = m_slices.constBegin();
        m_lastIntervalEvents.clear();  // there was no previous slice, so there are no pending end-of-slice events
        m_lastIntervalEnd = 0;
        updated = true;
    }

    // Update the slice iterator to point to the mask-on slice encompassing time t.
    while ((*m_currentSlice).status != MaskOn || t > (*m_currentSlice).end) {
        m_currentSlice++;
        updated = true;
        if (m_currentSlice == m_slices.constEnd()) {
            qWarning() << sessionid << "Events after last mask-on slice?";
            m_currentSlice--;
            break;
        }
    }

    if (updated) {
        // Write out any pending end-of-slice events.
        FinishSlice();
    }

    if (updated && (*m_currentSlice).status == MaskOn) {
        // Set the interval start times based on the new slice's start time.
        m_statIntervalStart = 0;
        StartNewInterval((*m_currentSlice).start);

        // Create a new eventlist for this new slice, to allow for a gap in the data between slices.
        CreateEventChannels(chunk);
    }

    return updated;
}


void PRS1Import::FinishSlice()
{
    qint64 t = m_lastIntervalEnd;  // end of the slice (at least of its interval data)

    // If the most recently recorded interval stats aren't at the end of the slice,
    // import additional events marking the end of the data.
    if (t != m_prevIntervalStart) {
        // Make sure to use the same pressure used to import the original events,
        // otherwise calculated channels (such as PS or LEAK) will be wrong.
        EventDataType orig_pressure = m_currentPressure;
        m_currentPressure = m_intervalPressure;

        // Import duplicates of each event with the end-of-slice timestamp.
        for (auto & e : m_lastIntervalEvents) {
            ImportEvent(t, e);
        }

        // Restore the current pressure.
        m_currentPressure = orig_pressure;
    }
    m_lastIntervalEvents.clear();
}


void PRS1Import::StartNewInterval(qint64 t)
{
    if (t == m_prevIntervalStart) {
        qWarning() << sessionid << "Multiple zero-length intervals at end of slice?";
    }
    m_prevIntervalStart = m_statIntervalStart;
    m_statIntervalStart = t;
}


bool PRS1Import::IsIntervalEvent(PRS1ParsedEvent* e)
{
    bool intervalEvent = false;

    // Statistical timestamps are reported at the end of a (generally) 2-minute
    // interval, rather than the start time that OSCAR expects for its imported
    // events.  (When a session or slice ends, there will be a shorter interval,
    // the previous statistics to the end of the session/slice.)
    switch (e->m_type) {
        case PRS1PressureAverageEvent::TYPE:
        case PRS1FlexPressureAverageEvent::TYPE:
        case PRS1IPAPAverageEvent::TYPE:
        case PRS1IPAPLowEvent::TYPE:
        case PRS1IPAPHighEvent::TYPE:
        case PRS1EPAPAverageEvent::TYPE:
        case PRS1TotalLeakEvent::TYPE:
        case PRS1LeakEvent::TYPE:
        case PRS1RespiratoryRateEvent::TYPE:
        case PRS1PatientTriggeredBreathsEvent::TYPE:
        case PRS1MinuteVentilationEvent::TYPE:
        case PRS1TidalVolumeEvent::TYPE:
        case PRS1FlowRateEvent::TYPE:
        case PRS1Test1Event::TYPE:
        case PRS1Test2Event::TYPE:
        case PRS1SnoreEvent::TYPE:
        case PRS1HypopneaCount::TYPE:
        case PRS1ClearAirwayCount::TYPE:
        case PRS1ObstructiveApneaCount::TYPE:
            intervalEvent = true;
            break;
        default:
            break;
    }

    return intervalEvent;
}


bool PRS1Import::ImportEventChunk(PRS1DataChunk* event)
{
    m_currentPressure=0;

    const QVector<PRS1ParsedEventType> & supported = GetSupportedEvents(event);
    
    // Calculate PS from IPAP/EPAP set events only when both are supported. This includes F0, but excludes
    // F5, which only reports EPAP set events, but both IPAP/EPAP average, from which PS will be calculated.
    m_calcPSfromSet = supported.contains(PRS1IPAPSetEvent::TYPE) && supported.contains(PRS1EPAPSetEvent::TYPE);
    
    qint64 t = qint64(event->timestamp) * 1000L;
    if (session->first() == 0) {
        qWarning() << sessionid << "Start time not set by summary?";
    } else if (t < session->first()) {
        qWarning() << sessionid << "Events start before summary?";
    }

    bool ok;
    ok = event->ParseEvents();
    
    // Set up the (possibly initial) slice based on the chunk's starting timestamp.
    UpdateCurrentSlice(event, t);

    for (int i=0; i < event->m_parsedData.count(); i++) {
        PRS1ParsedEvent* e = event->m_parsedData.at(i);
        t = qint64(event->timestamp + e->m_start) * 1000L;

        // Skip unknown events with no timestamp
        if (e->m_type == PRS1UnknownDataEvent::TYPE) {
            continue;
        }
        
        // Skip zero-length PB or LL or VB events
        if ((e->m_type == PRS1PeriodicBreathingEvent::TYPE || e->m_type == PRS1LargeLeakEvent::TYPE || e->m_type == PRS1VariableBreathingEvent::TYPE) &&
            (e->m_duration == 0)) {
            // LL occasionally appear about a minute before a new mask-on slice
            // begins, when the previous mask-on slice ended with a large leak.
            // This probably indicates the end of LL and beginning
            // of breath detection, but we don't get any real data until mask-on.
            //
            // It has also happened once in a similar scenario for PB and VB, even when
            // the two mask-on slices are in different sessions!
            continue;
        }
        
        if (e->m_type == PRS1IntervalBoundaryEvent::TYPE) {
            StartNewInterval(t);
            continue;  // these internal pseudo-events don't get imported
        }
        
        bool intervalEvent = IsIntervalEvent(e);
        qint64 interval_end_t = 0;
        if (intervalEvent) {
            // Deal with statistics that are reported at the end of an interval, but which need to be imported
            // at the start of the interval.

            if (event->family == 3 && event->familyVersion <= 3) {
                // In F3V0 and F3V3, each slice has its own chunk, so the initial call to UpdateCurrentSlice()
                // for this chunk is all that's needed.
                //
                // We can't just call it again here for simplicity, since the timestamps of F3V3 stat events
                // can go past the end of the slice.
            } else {
                // For all other devices, the event's time stamp will be within bounds of its slice, so
                // we can use it to find the current slice.
                UpdateCurrentSlice(event, t);
            }
            // Clamp this interval's end time to the end of the slice.
            interval_end_t = min(t, (*m_currentSlice).end);
            // Set this event's timestamp as the start of the interval, since that what OSCAR assumes.
            t = m_statIntervalStart;
            // TODO: ideally we would also set the duration of the event, but OSCAR doesn't have any notion of that yet.
        } else {
            // Advance the slice if needed for the regular event's timestamp.
            if (!PRS1NonSliceChannels.contains(e->m_type)) {
                UpdateCurrentSlice(event, t);
            }
        }

        // Sanity check: warn if a (non-slice) event is earlier than the current mask-on slice
        if (t < (*m_currentSlice).start && (*m_currentSlice).status == MaskOn) {
            if (!PRS1NonSliceChannels.contains(e->m_type)) {
                // LL and VB at the beginning of a mask-on session sometimes start 1 second early,
                // so suppress that warning.
                if ((*m_currentSlice).start - t > 1000 || (e->m_type != PRS1LargeLeakEvent::TYPE && e->m_type != PRS1VariableBreathingEvent::TYPE)) {
                    qWarning() << sessionid << "Event" << e->m_type << "before mask-on slice:" << ts(t);
                }
            }
        }

        // Import the event.
        switch (e->m_type) {
            // F3V3 doesn't have individual HY/CA/OA events, only counts every 2 minutes, where
            // nonzero counts show up as overview flags. Currently OSCAR doesn't have a way to
            // chart those numeric statistics, so we generate events based on the count.
            //
            // TODO: This (and VS2) would probably be better handled as numeric charts only,
            // along with enhancing overview flags to be drawn when channels have nonzero values,
            // instead of the fictitious "events" that are currently generated.
            case PRS1HypopneaCount::TYPE:
            case PRS1ClearAirwayCount::TYPE:
            case PRS1ObstructiveApneaCount::TYPE:
                // Make sure PRS1ClearAirwayEvent/etc. isn't supported before generating events from counts.
                CHECK_VALUE(supported.contains(PRS1HypopneaEvent::TYPE), false);
                CHECK_VALUE(supported.contains(PRS1ClearAirwayEvent::TYPE), false);
                CHECK_VALUE(supported.contains(PRS1ObstructiveApneaEvent::TYPE), false);

                // Divide each count into events evenly spaced over the interval.
                // NOTE: This is slightly fictional, but there's no waveform data for F3V3, so it won't
                // incorrectly associate specific events with specific flow or pressure events.
                if (e->m_value > 0) {
                    qint64 blockduration = interval_end_t - m_statIntervalStart;
                    qint64 div = blockduration / e->m_value;
                    qint64 tt = t;
                    PRS1ParsedDurationEvent ee(e->m_type, t, 0);
                    for (int i=0; i < e->m_value; ++i) {
                        ImportEvent(tt, &ee);
                        tt += div;
                    }
                }
                
                // TODO: Consider whether to have a numeric channel for HY/CA/OA that gets charted like VS does,
                // in which case we can fall through.
                break;
                
            default:
                ImportEvent(t, e);

                // Cache the most recently imported interval events so that we can import duplicate end-of-slice events if needed.
                // We can't write them here because we don't yet know if they're the last in the slice.
                if (intervalEvent) {
                    // Reset the list of pending events when we encounter a stat event in a new interval.
                    //
                    // This logic has grown sufficiently complex that it may eventually be worth encapsulating
                    // each batch of parsed interval events into a composite interval event when parsing,
                    // rather than requiring timestamp-based gymnastics to infer that structure on import.
                    if (m_lastIntervalEnd != interval_end_t) {
                        m_lastIntervalEvents.clear();
                        m_lastIntervalEnd = interval_end_t;
                        m_intervalPressure = m_currentPressure;
                    }
                    // The events need to be in order so that any dynamically calculated channels (such as PS) are properly computed.
                    m_lastIntervalEvents.append(e);
                }
                break;
        }
    }
    
    // Write out any pending end-of-slice events.
    FinishSlice();

    if (!ok) {
        return false;
    }

    // TODO: This needs to be special-cased for F3V0 and F3V3 due to their weird interval-based event format
    // until there's a way for its parser to correctly set the timestamps for truncated
    // intervals in sessions that don't end on a 2-minute boundary.
    if (!(event->family == 3 && event->familyVersion <= 3)) {
        // If the last event has a non-zero duration, t will not reflect the full duration of the chunk, so update it.
        t = qint64(event->timestamp + event->duration) * 1000L;
        if (session->last() == 0) {
            qWarning() << sessionid << "End time not set by summary?";
        } else if (t > session->last()) {
            // This has only been seen in two instances:
            // 1. Once with corrupted data, in which the summary and event files each contained
            //    multiple conflicting sessions (all brief) with the same ID.
            // 2. On one 500G110, multiple PRS1PressureSetEvents appear after the end of the session,
            //    across roughtly two dozen sessions. These seem to be discarded on official reports,
            //    see ImportEvent() below.
            qWarning() << sessionid << "Events continue after summary?";
        }
        // Events can end before the session if the mask was off before the equipment turned off.
    }

    return true;
}


void PRS1Import::ImportEvent(qint64 t, PRS1ParsedEvent* e)
{
    qint64 duration;

    // TODO: Filter out duplicate/overlapping PB and RE events.
    //
    // These actually get reported by the devices, but they cause "unordered time" warnings
    // and they throw off the session statistics. Even official reports show the wrong stats,
    // for example counting each of 3 duplicate PBs towards the total time in PB.
    //
    // It's not clear whether filtering can reasonably be done here or whether it needs
    // to be done in ImportEventChunk.
    
    const QVector<ChannelID*> & channels = PRS1ImportChannelMap[e->m_type];
    ChannelID channel = NoChannel, PS, VS2;
    if (channels.count() > 0) {
        channel = *channels.at(0);
    }
    
    switch (e->m_type) {
        case PRS1PressureSetEvent::TYPE:  // currentPressure is used to calculate unintentional leak, not just PS
            // TODO: These have sometimes been observed with t > session->last() on a 500G110.
            // Official reports seem to discard such events, OSCAR currently doesn't.
            // Test this more thoroughly before changing behavior here.
            // fall through
        case PRS1IPAPSetEvent::TYPE:
        case PRS1IPAPAverageEvent::TYPE:
            AddEvent(channel, t, e->m_value, e->m_gain);
            m_currentPressure = e->m_value;
            break;
        case PRS1EPAPSetEvent::TYPE:
            AddEvent(channel, t, e->m_value, e->m_gain);
            if (m_calcPSfromSet) {
                PS = *(PRS1ImportChannelMap[PRS1IPAPSetEvent::TYPE].at(1));
                AddEvent(PS, t, m_currentPressure - e->m_value, e->m_gain);  // Pressure Support
            }
            break;
        case PRS1EPAPAverageEvent::TYPE:
            PS = *channels.at(1);
            AddEvent(channel, t, e->m_value, e->m_gain);
            AddEvent(PS, t, m_currentPressure - e->m_value, e->m_gain);  // Pressure Support
            break;

        case PRS1TimedBreathEvent::TYPE:
            // The duration appears to correspond to the length of the timed breath in seconds when multiplied by 0.1 (100ms)!
            // TODO: consider changing parsers to use milliseconds for time, since it turns out there's at least one way
            // they can express durations less than 1 second.
            // TODO: consider allowing OSCAR to record millisecond durations so that the display will say "2.1" instead of "21" or "2".
            duration = e->m_duration * 100L;  // for now do this here rather than in parser, since parser events don't use milliseconds
            AddEvent(*channels.at(0), t - duration, e->m_duration * 0.1F, 0.1F);  // TODO: a gain of 0.1 should render this unnecessary, but gain doesn't seem to work currently
            break;

        case PRS1ObstructiveApneaEvent::TYPE:
        case PRS1ClearAirwayEvent::TYPE:
        case PRS1HypopneaEvent::TYPE:
        case PRS1FlowLimitationEvent::TYPE:
            AddEvent(channel, t, e->m_duration, e->m_gain);
            break;

        case PRS1PeriodicBreathingEvent::TYPE:
        case PRS1LargeLeakEvent::TYPE:
        case PRS1VariableBreathingEvent::TYPE:
            // TODO: The graphs silently treat the timestamp of a span as an end time rather than start (see gFlagsLine::paint).
            // Decide whether to preserve that behavior or change it universally and update either this code or comment.
            duration = e->m_duration * 1000L;
            AddEvent(channel, t + duration, e->m_duration, e->m_gain);
            break;

        case PRS1SnoreEvent::TYPE:  // snore count that shows up in flags but not waveform
            // TODO: The numeric snore graph is the right way to present this information,
            // but it needs to be shifted left 2 minutes, since it's not a starting value
            // but a past statistic.
            AddEvent(channel, t, e->m_value, e->m_gain);  // Snore count, continuous data
            if (e->m_value > 0) {
                // TODO: currently these get drawn on our waveforms, but they probably shouldn't,
                // since they don't have a precise timestamp. They should continue to be drawn
                // on the flags overview. See the comment in ImportEventChunk regarding flags
                // for numeric channels.
                //
                // We need to pass the count along so that the VS2 index will tabulate correctly.
                VS2 = *channels.at(1);
                AddEvent(VS2, t, e->m_value, 1);
            }
            break;
        case PRS1VibratorySnoreEvent::TYPE:  // real VS marker on waveform
            // TODO: These don't need to be drawn separately on the flag overview, since
            // they're presumably included in the overall snore count statistic. They should
            // continue to be drawn on the waveform, due to their precise timestamp.
            AddEvent(channel, t, e->m_value, e->m_gain);
            break;

        default:
            if (channels.count() == 1) {
                // For most events, simply pass the value through to the mapped channel.
                AddEvent(channel, t, e->m_value, e->m_gain);
            } else if (channels.count() > 1) {
                // Anything mapped to more than one channel must have a case statement above.
                qWarning() << "Missing import handler for PRS1 event type" << (int) e->m_type;
                break;
            } else {
                // Not imported, no channels mapped to this event
                // These will show up in chunk YAML and any user alerts will be driven by the parser.
            }
            break;
    }
}


CPAPMode PRS1Import::importMode(int prs1mode)
{
    CPAPMode mode = MODE_UNKNOWN;
    
    switch (prs1mode) {
        case PRS1_MODE_CPAPCHECK:   mode = MODE_CPAP; break;
        case PRS1_MODE_CPAP:        mode = MODE_CPAP; break;
        case PRS1_MODE_AUTOCPAP:    mode = MODE_APAP; break;
        case PRS1_MODE_AUTOTRIAL:   mode = MODE_APAP; break;
        case PRS1_MODE_BILEVEL:     mode = MODE_BILEVEL_FIXED; break;
        case PRS1_MODE_AUTOBILEVEL: mode = MODE_BILEVEL_AUTO_VARIABLE_PS; break;
        case PRS1_MODE_ASV:         mode = MODE_ASV_VARIABLE_EPAP; break;
        case PRS1_MODE_S:           mode = MODE_BILEVEL_FIXED; break;
        case PRS1_MODE_ST:          mode = MODE_BILEVEL_FIXED; break;
        case PRS1_MODE_PC:          mode = MODE_BILEVEL_FIXED; break;
        case PRS1_MODE_ST_AVAPS:    mode = MODE_AVAPS; break;
        case PRS1_MODE_PC_AVAPS:    mode = MODE_AVAPS; break;
        default:
            UNEXPECTED_VALUE(prs1mode, "known PRS1 mode");
            break;
    }

    return mode;
}


bool PRS1Import::ImportCompliance()
{
    bool ok;
    ok = compliance->ParseCompliance();
    qint64 start = qint64(compliance->timestamp) * 1000L;
    
    for (int i=0; i < compliance->m_parsedData.count(); i++) {
        PRS1ParsedEvent* e = compliance->m_parsedData.at(i);
        if (e->m_type == PRS1ParsedSliceEvent::TYPE) {
            AddSlice(start, e);
            continue;
        } else if (e->m_type != PRS1ParsedSettingEvent::TYPE) {
            qWarning() << "Compliance had non-setting event:" << (int) e->m_type;
            continue;
        }
        PRS1ParsedSettingEvent* s = (PRS1ParsedSettingEvent*) e;
        switch (s->m_setting) {
            case PRS1_SETTING_CPAP_MODE:
                session->settings[PRS1_Mode] = (PRS1Mode) e->m_value;
                session->settings[CPAP_Mode] = importMode(e->m_value);
                break;
            case PRS1_SETTING_PRESSURE:
                session->settings[CPAP_Pressure] = e->value();
                break;
            case PRS1_SETTING_PRESSURE_MIN:
                session->settings[CPAP_PressureMin] = e->value();
                break;
            case PRS1_SETTING_PRESSURE_MAX:
                session->settings[CPAP_PressureMax] = e->value();
                break;
            case PRS1_SETTING_FLEX_MODE:
                session->settings[PRS1_FlexMode] = e->m_value;
                break;
            case PRS1_SETTING_FLEX_LEVEL:
                session->settings[PRS1_FlexLevel] = e->m_value;
                break;
            case PRS1_SETTING_FLEX_LOCK:
                session->settings[PRS1_FlexLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_RAMP_TIME:
                session->settings[CPAP_RampTime] = e->m_value;
                break;
            case PRS1_SETTING_RAMP_PRESSURE:
                session->settings[CPAP_RampPressure] = e->value();
                break;
            case PRS1_SETTING_RAMP_TYPE:
                session->settings[PRS1_RampType] = e->m_value;
                break;
            case PRS1_SETTING_HUMID_STATUS:
                session->settings[PRS1_HumidStatus] = (bool) e->m_value;
                break;
            case PRS1_SETTING_HUMID_MODE:
                session->settings[PRS1_HumidMode] = e->m_value;
                break;
            case PRS1_SETTING_HEATED_TUBE_TEMP:
                session->settings[PRS1_TubeTemp] = e->m_value;
                break;
            case PRS1_SETTING_HUMID_LEVEL:
                session->settings[PRS1_HumidLevel] = e->m_value;
                break;
            case PRS1_SETTING_MASK_RESIST_LOCK:
                session->settings[PRS1_MaskResistLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_MASK_RESIST_SETTING:
                session->settings[PRS1_MaskResistSet] = e->m_value;
                break;
            case PRS1_SETTING_HOSE_DIAMETER:
                session->settings[PRS1_HoseDiam] = e->m_value;
                break;
            case PRS1_SETTING_TUBING_LOCK:
                session->settings[PRS1_TubeLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_AUTO_ON:
                session->settings[PRS1_AutoOn] = (bool) e->m_value;
                break;
            case PRS1_SETTING_AUTO_OFF:
                session->settings[PRS1_AutoOff] = (bool) e->m_value;
                break;
            case PRS1_SETTING_MASK_ALERT:
                session->settings[PRS1_MaskAlert] = (bool) e->m_value;
                break;
            case PRS1_SETTING_SHOW_AHI:
                session->settings[PRS1_ShowAHI] = (bool) e->m_value;
                break;
            default:
                qWarning() << "Unknown PRS1 setting type" << (int) s->m_setting;
                break;
        }
    }

    if (!ok) {
        return false;
    }
    if (compliance->duration == 0) {
        // This does occasionally happen and merely indicates a brief session with no useful data.
        // This requires the use of really_set_last below, which otherwise rejects 0 length.
        qDebug() << compliance->sessionid << "compliance duration == 0";
    }
    session->setSummaryOnly(true);
    session->set_first(start);
    session->really_set_last(qint64(compliance->timestamp + compliance->duration) * 1000L);

    return true;
}


void PRS1Import::AddSlice(qint64 start, PRS1ParsedEvent* e)
{
    // Cache all slices and incrementally calculate their durations.
    PRS1ParsedSliceEvent* s = (PRS1ParsedSliceEvent*) e;
    qint64 tt = start + qint64(s->m_start) * 1000L;
    if (!m_slices.isEmpty()) {
        SessionSlice & prevSlice = m_slices.last();
        prevSlice.end = tt;
    }
    m_slices.append(SessionSlice(tt, tt, (SliceStatus) s->m_value));
}


bool PRS1Import::ImportSummary()
{
    if (!summary) {
        qWarning() << "ImportSummary() called with no summary?";
        return false;
    }

    qint64 start = qint64(summary->timestamp) * 1000L;
    session->set_first(start);

    // TODO: The below max pressures aren't right for the 30 cmH2O models.
    session->setPhysMax(CPAP_LeakTotal, 120);
    session->setPhysMin(CPAP_LeakTotal, 0);
    session->setPhysMax(CPAP_Pressure, 25);
    session->setPhysMin(CPAP_Pressure, 4);
    session->setPhysMax(CPAP_IPAP, 25);
    session->setPhysMin(CPAP_IPAP, 4);
    session->setPhysMax(CPAP_EPAP, 25);
    session->setPhysMin(CPAP_EPAP, 4);
    session->setPhysMax(CPAP_PS, 25);
    session->setPhysMin(CPAP_PS, 0);
    
    bool ok;
    ok = summary->ParseSummary();
    
    PRS1Mode nativemode = PRS1_MODE_UNKNOWN;
    CPAPMode cpapmode = MODE_UNKNOWN;
    bool humidifierConnected = false;
    for (int i=0; i < summary->m_parsedData.count(); i++) {
        PRS1ParsedEvent* e = summary->m_parsedData.at(i);
        if (e->m_type == PRS1ParsedSliceEvent::TYPE) {
            AddSlice(start, e);
            continue;
        } else if (e->m_type != PRS1ParsedSettingEvent::TYPE) {
            qWarning() << "Summary had non-setting event:" << (int) e->m_type;
            continue;
        }
        PRS1ParsedSettingEvent* s = (PRS1ParsedSettingEvent*) e;
        switch (s->m_setting) {
            case PRS1_SETTING_CPAP_MODE:
                nativemode = (PRS1Mode) e->m_value;
                cpapmode = importMode(e->m_value);
                break;
            case PRS1_SETTING_PRESSURE:
                session->settings[CPAP_Pressure] = e->value();
                break;
            case PRS1_SETTING_PRESSURE_MIN:
                session->settings[CPAP_PressureMin] = e->value();
                break;
            case PRS1_SETTING_PRESSURE_MAX:
                session->settings[CPAP_PressureMax] = e->value();
                break;
            case PRS1_SETTING_EPAP:
                session->settings[CPAP_EPAP] = e->value();
                break;
            case PRS1_SETTING_IPAP:
                session->settings[CPAP_IPAP] = e->value();
                break;
            case PRS1_SETTING_PS:
                session->settings[CPAP_PS] = e->value();
                break;
            case PRS1_SETTING_EPAP_MIN:
                session->settings[CPAP_EPAPLo] = e->value();
                break;
            case PRS1_SETTING_EPAP_MAX:
                session->settings[CPAP_EPAPHi] = e->value();
                break;
            case PRS1_SETTING_IPAP_MIN:
                session->settings[CPAP_IPAPLo] = e->value();
                break;
            case PRS1_SETTING_IPAP_MAX:
                session->settings[CPAP_IPAPHi] = e->value();
                break;
            case PRS1_SETTING_PS_MIN:
                session->settings[CPAP_PSMin] = e->value();
                break;
            case PRS1_SETTING_PS_MAX:
                session->settings[CPAP_PSMax] = e->value();
                break;
            case PRS1_SETTING_FLEX_MODE:
                session->settings[PRS1_FlexMode] = e->m_value;
                break;
            case PRS1_SETTING_FLEX_LEVEL:
                session->settings[PRS1_FlexLevel] = e->m_value;
                break;
            case PRS1_SETTING_FLEX_LOCK:
                session->settings[PRS1_FlexLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_RAMP_TIME:
                session->settings[CPAP_RampTime] = e->m_value;
                break;
            case PRS1_SETTING_RAMP_PRESSURE:
                session->settings[CPAP_RampPressure] = e->value();
                break;
            case PRS1_SETTING_RAMP_TYPE:
                session->settings[PRS1_RampType] = e->m_value;
                break;
            case PRS1_SETTING_HUMID_STATUS:
                humidifierConnected = (bool) e->m_value;
                session->settings[PRS1_HumidStatus] = humidifierConnected;
                break;
            case PRS1_SETTING_HUMID_MODE:
                session->settings[PRS1_HumidMode] = e->m_value;
                break;
            case PRS1_SETTING_HEATED_TUBE_TEMP:
                session->settings[PRS1_TubeTemp] = e->m_value;
                break;
            case PRS1_SETTING_HUMID_LEVEL:
                session->settings[PRS1_HumidLevel] = e->m_value;
                break;
            case PRS1_SETTING_HUMID_TARGET_TIME:
                // Only import this setting if there's a humidifier connected.
                // (This setting appears in the data even when it's disconnected.)
                // TODO: Consider moving this logic into the parser for target time.
                if (humidifierConnected) {
                    if (e->m_value > 1) {
                        // use scaled numeric value
                        session->settings[PRS1_HumidTargetTime] = e->value();
                    } else {
                        // use unscaled 0 or 1 for Off or Auto respectively
                        session->settings[PRS1_HumidTargetTime] = e->m_value;
                    }
                }
                break;
            case PRS1_SETTING_MASK_RESIST_LOCK:
                session->settings[PRS1_MaskResistLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_MASK_RESIST_SETTING:
                session->settings[PRS1_MaskResistSet] = e->m_value;
                break;
            case PRS1_SETTING_HOSE_DIAMETER:
                session->settings[PRS1_HoseDiam] = e->m_value;
                break;
            case PRS1_SETTING_TUBING_LOCK:
                session->settings[PRS1_TubeLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_AUTO_ON:
                session->settings[PRS1_AutoOn] = (bool) e->m_value;
                break;
            case PRS1_SETTING_AUTO_OFF:
                session->settings[PRS1_AutoOff] = (bool) e->m_value;
                break;
            case PRS1_SETTING_MASK_ALERT:
                session->settings[PRS1_MaskAlert] = (bool) e->m_value;
                break;
            case PRS1_SETTING_SHOW_AHI:
                session->settings[PRS1_ShowAHI] = (bool) e->m_value;
                break;
            case PRS1_SETTING_BACKUP_BREATH_MODE:
                session->settings[PRS1_BackupBreathMode] = e->m_value;
                break;
            case PRS1_SETTING_BACKUP_BREATH_RATE:
                session->settings[PRS1_BackupBreathRate] = e->m_value;
                break;
            case PRS1_SETTING_BACKUP_TIMED_INSPIRATION:
                session->settings[PRS1_BackupBreathTi] = e->value();
                break;
            case PRS1_SETTING_TIDAL_VOLUME:
                session->settings[CPAP_TidalVolume] = e->m_value;
                break;
            case PRS1_SETTING_AUTO_TRIAL:  // new to F0V6
                session->settings[PRS1_AutoTrial] = e->m_value;
                nativemode = PRS1_MODE_AUTOTRIAL;  // Note: F0V6 reports show the underlying CPAP mode rather than Auto-Trial.
                cpapmode = importMode(nativemode);
                break;
            case PRS1_SETTING_EZ_START:
                session->settings[PRS1_EZStart] = (bool) e->m_value;
                break;
            case PRS1_SETTING_RISE_TIME:
                session->settings[PRS1_RiseTime] = e->m_value;
                break;
            case PRS1_SETTING_RISE_TIME_LOCK:
                session->settings[PRS1_RiseTimeLock] = (bool) e->m_value;
                break;
            case PRS1_SETTING_APNEA_ALARM:
            case PRS1_SETTING_DISCONNECT_ALARM:
            case PRS1_SETTING_LOW_MV_ALARM:
            case PRS1_SETTING_LOW_TV_ALARM:
                // TODO: define and add new channels for alarms once we have more samples and can reliably parse them.
                break;
            default:
                qWarning() << "Unknown PRS1 setting type" << (int) s->m_setting;
                break;
        }
    }

    if (!ok) {
        return false;
    }
    
    if (summary->m_parsedData.count() > 0) {
        if (nativemode == PRS1_MODE_UNKNOWN) UNEXPECTED_VALUE(nativemode, "known mode");
        if (cpapmode == MODE_UNKNOWN) UNEXPECTED_VALUE(cpapmode, "known mode");
        session->settings[PRS1_Mode] = nativemode;
        session->settings[CPAP_Mode] = cpapmode;
    }

    if (summary->duration == 0) {
        // This does occasionally happen and merely indicates a brief session with no useful data.
        // This requires the use of really_set_last below, which otherwise rejects 0 length.
        //qDebug() << summary->sessionid << "session duration == 0";
    }
    session->really_set_last(qint64(summary->timestamp + summary->duration) * 1000L);
    
    return true;
}


bool PRS1Import::ImportEvents()
{
    bool ok = true;
    
    for (auto & event : m_event_chunks.values()) {
        bool chunk_ok = this->ImportEventChunk(event);
        if (!chunk_ok && m_event_chunks.count() > 1) {
            // Specify which chunk had problems if there's more than one. ParseSession will warn about the overall result.
            qWarning() << event->sessionid << QString("Error parsing events in %1 @ %2, continuing")
                .arg(relativePath(event->m_path))
                .arg(event->m_filepos);
        }
        ok &= chunk_ok;
    }

    if (ok) {
        // Sanity check: warn if channels' eventlists don't line up with the final mask-on slices.
        // First make a list of the mask-on slices that will be imported (nonzero duration)
        QVector<SessionSlice> maskOn;
        for (auto & slice : m_slices) {
            if (slice.status == MaskOn) {
                if (slice.end > slice.start) {
                    maskOn.append(slice);
                } else {
                    qWarning() << this->sessionid << "Dropping empty mask-on slice:" << ts(slice.start);
                }
            }
        }
        // Then go through each required channel and make sure each eventlist is within
        // the bounds of the corresponding slice, warn if not.
        if (maskOn.count() > 0 && m_event_chunks.count() > 0) {
            QVector<SessionSlice> maskOnWithEvents = maskOn;
            if (m_event_chunks.first()->family == 3 && m_event_chunks.first()->familyVersion <= 3) {
                // F3V0 and F3V3 sometimes omit (empty) event chunks if the mask-on slice is shorter than 2 minutes.
                // Specifically, 1061401 and 1061T always do, but 1160P usually doesn't. Sometimes 1160P will omit
                // just the first event chunk if the first mask-on slice is shorter than 2 minutes.
                int empty = maskOn.count() - m_event_chunks.count();
                if (empty > 0) {
                    // If there are fewer event chunks than mask-on slices, filter the list to have just the
                    // mask-on slices that we expect to have events.
                    int skipped = 0;
                    maskOnWithEvents.clear();
                    for (auto & slice : maskOn) {
                        if (skipped < empty && slice.end - slice.start < 120 * 1000L) {
                            skipped++;
                            continue;
                        }
                        maskOnWithEvents.append(slice);
                    }
                }
            }
            if (maskOnWithEvents.count() < m_event_chunks.count()) {
                qWarning() << sessionid << "has more event chunks than mask-on slices!";
            }
            const QVector<PRS1ParsedEventType> & supported = GetSupportedEvents(m_event_chunks.first());
            for (auto & e : supported) {
                if (!PRS1OnDemandChannels.contains(e) && !PRS1NonSliceChannels.contains(e)) {
                    for (auto & pChannelID : PRS1ImportChannelMap[e]) {
                        auto & eventlists = session->eventlist[*pChannelID];
                        if (eventlists.count() != maskOnWithEvents.count()) {
                            qWarning() << sessionid << "has" << maskOnWithEvents.count() << "mask-on slices, channel"
                                << *pChannelID << "has" << eventlists.count() << "eventlists";
                            continue;
                        }
                        for (int i = 0; i < eventlists.count(); i++) {
                            if (eventlists[i]->count() == 0) continue;  // no first/last timestamp
                            auto & list = eventlists[i];
                            auto & slice = maskOnWithEvents[i];
                            if (list->first() < slice.start || list->first() > slice.end ||
                                list->last() < slice.start || list->last() > slice.end) {
                                qWarning() << sessionid << "channel" << *pChannelID << "has events outside of mask-on slice" << i;
                            }
                        }
                    }
                }
            }
        }
        // The above is just sanity-checking the results of our import process, that discontinuous
        // data is fully contained within mask-on slices.
    
        session->m_cnt.clear();
        session->m_cph.clear();

        session->m_valuesummary[CPAP_Pressure].clear();
        session->m_valuesummary.erase(session->m_valuesummary.find(CPAP_Pressure));
    }

    return ok;
}


QList<PRS1DataChunk *> PRS1Import::CoalesceWaveformChunks(QList<PRS1DataChunk *> & allchunks)
{
    QList<PRS1DataChunk *> coalesced;
    PRS1DataChunk *chunk = nullptr, *lastchunk = nullptr;
    int num;
    
    for (int i=0; i < allchunks.size(); ++i) {
        chunk = allchunks.at(i);
        
        // Log mismatched waveform session IDs
        QFileInfo fi(chunk->m_path);
        bool numeric;
        QString session_s = fi.fileName().section(".", 0, -2);
        quint32 sid = session_s.toInt(&numeric, m_sessionid_base);
        if (!numeric || sid != chunk->sessionid) {
            qWarning() << chunk->m_path << "@" << chunk->m_filepos << "session ID mismatch:" << chunk->sessionid;
        }

        if (lastchunk != nullptr) {
            // A handful of 960P waveform files have been observed to have multiple sessions.
            //
            // This breaks the current approach of deferring waveform parsing until the (multithreaded)
            // import, since each session is in a separate import task and could be in a separate
            // thread, or already imported by the time it is discovered that this file contains
            // more than one session.
            //
            // For now, we just dump the chunks that don't belong to the session currently
            // being imported in this thread, since this happens so rarely.
            //
            // TODO: Rework the import process to handle waveform data after compliance/summary/
            // events (since we're no longer inferring session information from it) and add it to the
            // newly imported sessions.
            if (lastchunk->sessionid != chunk->sessionid) {
                qWarning() << chunk->m_path << "@" << chunk->m_filepos
                    << "session ID" << lastchunk->sessionid << "->" << chunk->sessionid
                    << ", skipping" << allchunks.size() - i << "remaining chunks";
                // Free any remaining chunks
                for (int j=i; j < allchunks.size(); ++j) {
                    chunk = allchunks.at(j);
                    delete chunk;
                }
                break;
            }
            
            // Check whether the data format is the same between the two chunks
            bool same_format = (lastchunk->waveformInfo.size() == chunk->waveformInfo.size());
            if (same_format) {
                num = chunk->waveformInfo.size();
                for (int n=0; n < num; n++) {
                    const PRS1Waveform &a = lastchunk->waveformInfo.at(n);
                    const PRS1Waveform &b = chunk->waveformInfo.at(n);
                    if (a.interleave != b.interleave) {
                        // We've never seen this before
                        qWarning() << chunk->m_path << "format change?" << a.interleave << b.interleave;
                        same_format = false;
                        break;
                    }
                }
            } else {
                // We've never seen this before
                qWarning() << chunk->m_path << "channels change?" << lastchunk->waveformInfo.size() << chunk->waveformInfo.size();
            }
            
            qint64 diff = (chunk->timestamp - lastchunk->timestamp) - lastchunk->duration;
            if (same_format && diff == 0) {
                // Same format and in sync, so append waveform data to previous chunk
                lastchunk->m_data.append(chunk->m_data);
                lastchunk->duration += chunk->duration;
                delete chunk;
                continue;
            }
            // else start a new chunk to resync
        }
        
        // Report any formats we haven't seen before
        num = chunk->waveformInfo.size();
        if (num > 2) {
            qDebug() << chunk->m_path << num << "channels";
        }
        for (int n=0; n < num; n++) {
            int interleave = chunk->waveformInfo.at(n).interleave;
            switch (chunk->ext) {
                case 5:  // flow data, 5 samples per second
                    if (interleave != 5) {
                        qDebug() << chunk->m_path << "interleave?" << interleave;
                    }
                    break;
                case 6:  // oximetry, 1 sample per second
                    if (interleave != 1) {
                        qDebug() << chunk->m_path << "interleave?" << interleave;
                    }
                    break;
                default:
                    qWarning() << chunk->m_path << "unknown waveform?" << chunk->ext;
                    break;
            }
        }
        
        coalesced.append(chunk);
        lastchunk = chunk;
    }
    
    // In theory there could be broken sessions that have waveform data but no summary or events.
    // Those waveforms won't be skipped by the scanner, so we have to check for them here.
    //
    // This won't be perfect, since any coalesced chunks starting after midnight of the threshhold
    // date will also be imported, but those should be relatively few, and tolerable imprecision.
    QList<PRS1DataChunk *> coalescedAndFiltered;
    qint64 ignoreBefore = loader->context()->IgnoreSessionsOlderThan().toMSecsSinceEpoch()/1000;
    bool ignoreOldSessions = loader->context()->ShouldIgnoreOldSessions();

    for (auto & chunk : coalesced) {
        if (ignoreOldSessions && chunk->timestamp < ignoreBefore) {
            qWarning().noquote() << relativePath(chunk->m_path) << "skipping session" << chunk->sessionid << ":"
                << QDateTime::fromMSecsSinceEpoch(chunk->timestamp*1000).toString() << "older than"
                << QDateTime::fromMSecsSinceEpoch(ignoreBefore*1000).toString();
            delete chunk;
            continue;
        }
        coalescedAndFiltered.append(chunk);
    }

    return coalescedAndFiltered;
}


void PRS1Import::ImportOximetry()
{
    int size = oximetry.size();

    for (int i=0; i < size; ++i) {
        PRS1DataChunk * oxi = oximetry.at(i);
        int num = oxi->waveformInfo.size();
        CHECK_VALUE(num, 2);

        int size = oxi->m_data.size();
        if (size == 0) {
            qDebug() << oxi->sessionid << oxi->timestamp << "empty?";
            continue;
        }
        quint64 ti = quint64(oxi->timestamp) * 1000L;
        qint64 dur = qint64(oxi->duration) * 1000L;

        if (num > 1) {
            CHECK_VALUE(oxi->waveformInfo.at(0).interleave, 1);
            CHECK_VALUE(oxi->waveformInfo.at(1).interleave, 1);
            
            // Process interleaved samples
            QVector<QByteArray> data;
            data.resize(num);

            int pos = 0;
            do {
                for (int n=0; n < num; n++) {
                    int interleave = oxi->waveformInfo.at(n).interleave;
                    data[n].append(oxi->m_data.mid(pos, interleave));
                    pos += interleave;
                }
            } while (pos < size);
            CHECK_VALUE(data[0].size(), data[1].size());

            ImportOximetryChannel(OXI_Pulse, data[0], ti, dur);

            ImportOximetryChannel(OXI_SPO2, data[1], ti, dur);
        }
    }
}


void PRS1Import::ImportOximetryChannel(ChannelID channel, QByteArray & data, quint64 ti, qint64 dur)
{
    if (data.size() == 0)
        return;

    unsigned char* raw = (unsigned char*) data.data();
    qint64 step = dur / data.size();
    CHECK_VALUE(dur % data.size(), 0);
    
    bool pending_samples = false;
    quint64 start_ti;
    int start_i;
    
    // Split eventlist on invalid values (254-255)
    for (int i=0; i < data.size(); i++) {
        unsigned char value = raw[i];
        bool valid = (value < 254);

        if (valid) {
            if (pending_samples == false) {
                pending_samples = true;
                start_i  = i;
                start_ti = ti;
            }
            
            if (channel == OXI_Pulse) {
                // Values up through 253 are confirmed to be reported as valid on official reports.
            } else {
                if (value > 100) UNEXPECTED_VALUE(value, "<= 100%");
            }
        } else {
            if (pending_samples) {
                // Create the pending event list
                EventList* el = session->AddEventList(channel, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, step);
                el->AddWaveform(start_ti, &raw[start_i], i - start_i, ti - start_ti);
                pending_samples = false;
            }
        }
        ti += step;
    }

    if (pending_samples) {
        // Create the pending event list
        EventList* el = session->AddEventList(channel, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, step);
        el->AddWaveform(start_ti, &raw[start_i], data.size() - start_i, ti - start_ti);
        pending_samples = false;
    }
}


void PRS1Import::ImportWaveforms()
{
    int size = waveforms.size();
    quint64 s1, s2;

    int discontinuities = 0;
    qint64 lastti=0;

    for (int i=0; i < size; ++i) {
        PRS1DataChunk * waveform = waveforms.at(i);
        int num = waveform->waveformInfo.size();

        int size = waveform->m_data.size();
        if (size == 0) {
            qDebug() << waveform->sessionid << waveform->timestamp << "empty?";
            continue;
        }
        quint64 ti = quint64(waveform->timestamp) * 1000L;
        quint64 dur = qint64(waveform->duration) * 1000L;

        qint64 diff = ti - lastti;
        if ((lastti != 0) && (diff == 1000 || diff == -1000)) {
            // TODO: Handle discontinuities properly.
            // Option 1: preserve the discontinuity and make it apparent:
            // - In the case of a 1-sec overlap, truncate the previous waveform by 1s (+1 sample).
            // - Then start a new eventlist for the new section.
            // > The down side of this approach is gaps in the data.
            // Option 2: slide the waveform data a fraction of a second to avoid the discontinuity
            // - In the case of a single discontinuity, simply adjust the timestamps of each section by 0.5s so they meet.
            // - In the case of multiple discontinuities, fitting them is more complicated
            // > The down side of this approach is that events won't line up exactly the same as official reports.
            //
            // Evidently the devices' internal clock drifts slightly, and in some sessions that
            // means two adjacent (5-minute) waveform chunks have have a +/- 1 second difference in
            // their notion of the correct time, since the devices only record time at 1-second
            // resolution. Presumably the real drift is fractional, but there's no way to tell from
            // the data.
            //
            // Encore apparently drops the second chunk entirely if it overlaps with the first
            // (even by 1 second), and inserts a 1-second gap in the data if it's 1 second later than
            // the first ended.
            //
            // At worst in the former case it seems preferable to drop the overlap and then one
            // additional second to mark the discontinuity. But depending how often these drifts
            // occur, it may be possible to adjust all the data so that it's continuous. "Overlapping"
            // data is not identical, so it seems like these discontinuities are simply an artifact
            // of timestamping at 1-second intervals right around the 1-second boundary.

            //qDebug() << waveform->sessionid << "waveform discontinuity:" << (diff / 1000L) << "s @" << ts(waveform->timestamp * 1000L);
            discontinuities++;
        }

        if (num > 1) {
            float pressure_gain = 0.1F;  // standard pressure gain
            if ((waveform->family == 5 && (waveform->familyVersion == 2 || waveform->familyVersion == 3)) ||
                (waveform->family == 3 && waveform->familyVersion == 6)){
                // F5V2, F5V3, and F3V6 use a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
                pressure_gain = 0.125F;  // TODO: this should be parameterized somewhere better, once we have a clear idea of which devices use this
            }
            
            // Process interleaved samples
            QVector<QByteArray> data;
            data.resize(num);

            int pos = 0;
            do {
                for (int n=0; n < num; n++) {
                    int interleave = waveform->waveformInfo.at(n).interleave;
                    data[n].append(waveform->m_data.mid(pos, interleave));
                    pos += interleave;
                }
            } while (pos < size);

            s1 = data[0].size();
            s2 = data[1].size();

            if (s1 > 0) {
                EventList * flow = session->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(dur) / double(s1));
                flow->AddWaveform(ti, (char *)data[0].data(), data[0].size(), dur);
            }

            if (s2 > 0) {
                // NOTE: The 900X (F5V3) firmware V1.0.1 clamps the values at 127 (15.875 cmH2O)
                // due to incorrectly treating this value as a signed integer. This bug is fixed
                // in firmware V1.0.6.
                EventList * pres = session->AddEventList(CPAP_MaskPressureHi, EVL_Waveform, pressure_gain, 0.0f, 0.0f, 0.0f, double(dur) / double(s2));
                pres->AddWaveform(ti, (unsigned char *)data[1].data(), data[1].size(), dur);
            }

        } else {
            // Non interleaved, so can process it much faster
            EventList * flow = session->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(dur) / double(waveform->m_data.size()));
            flow->AddWaveform(ti, (char *)waveform->m_data.data(), waveform->m_data.size(), dur);
        }
        lastti = dur+ti;
    }
    
    if (discontinuities > 1) {
        qWarning() << session->session() << "multiple discontinuities!" << discontinuities;
    }
}

void PRS1Import::run()
{
    if (ParseSession()) {
        loader->context()->AddSession(session);
    }
}


bool PRS1Import::ParseSession(void)
{
    bool ok = false;
    bool save = false;
    session = loader->context()->CreateSession(sessionid);

    do {
        if (compliance != nullptr) {
            ok = ImportCompliance();
            if (!ok) {
                // We don't see any parse errors with our test data, so warn if there's ever an error encountered.
                qWarning() << sessionid << "Error parsing compliance, skipping session";
                break;
            }
        }
        if (summary != nullptr) {
            if (compliance != nullptr) {
                qWarning() << sessionid << "Has both compliance and summary?!";
                // Never seen this, but try the summary anyway.
            }
            ok = ImportSummary();
            if (!ok) {
                // We don't see any parse errors with our test data, so warn if there's ever an error encountered.
                qWarning() << sessionid << "Error parsing summary, skipping session";
                break;
            }
        }
        if (compliance == nullptr && summary == nullptr) {
            // With one exception, the only time we've seen missing .000 or .001 data has been with a corrupted card,
            // or occasionally with partial cards where the .002 is the first file in the Pn directory
            // and we're missing the preceding directory. Since the lack of compliance or summary means we
            // don't know the therapy settings or if the mask was ever off, we just skip this very rare case.
            qWarning() << sessionid << "No compliance or summary, skipping session";
            break;
        }
        
        // Import the slices into the session
        for (auto & slice : m_slices) {
            // Filter out 0-length slices, since they cause problems for Day::total_time().
            if (slice.end > slice.start) {
                // Filter out everything except mask on/off, since gSessionTimesChart::paint assumes those are the only options.
                if (slice.status == MaskOn) {
                    session->m_slices.append(slice);
                } else if (slice.status == MaskOff) {
                    // Mark this slice as BND
                    AddEvent(PRS1_BND, slice.end, (slice.end - slice.start) / 1000L, 1.0);
                    session->m_slices.append(slice);
                }
            }
        }
        
        // If are no mask-on slices, then there's not any meaningful event or waveform data for the session.
        // If there's no no event or waveform data, mark this session as a summary.
        if (session->m_slices.count() == 0 || (m_event_chunks.count() == 0 && m_wavefiles.isEmpty() && m_oxifiles.isEmpty())) {
            session->setSummaryOnly(true);
            save = true;
            break;  // and skip the occasional fragmentary event or waveform data
        }

        // TODO: There should be a way to distinguish between no-data-to-import vs. parsing errors
        // (once we figure out what's benign and what isn't).
        if (m_event_chunks.count() > 0) {
            ok = ImportEvents();
            if (!ok) {
                qWarning() << sessionid << "Error parsing events, proceeding anyway?";
            }
        }

        if (!m_wavefiles.isEmpty()) {
            // Parse .005 Waveform files
            waveforms = ReadWaveformData(m_wavefiles, "Waveform");

            // Extract and import raw data into channels.
            ImportWaveforms();
        }

        if (!m_oxifiles.isEmpty()) {
            // Parse .006 Waveform files
            oximetry = ReadWaveformData(m_oxifiles, "Oximetry");

            // Extract and import raw data into channels.
            ImportOximetry();
        }

        save = true;
    } while (false);
    
    return save;
}


QList<PRS1DataChunk *> PRS1Import::ReadWaveformData(QList<QString> & files, const char* label)
{
    QMap<qint64,PRS1DataChunk *> waveform_chunks;
    QList<PRS1DataChunk *> result;

    if (files.count() > 1) {
        qDebug() << session->session() << label << "data split across multiple files";
    }
    
    for (auto & f : files) {
        // Parse a single .005 or .006 waveform file
        QList<PRS1DataChunk *> file_chunks = loader->ParseFile(f);
        for (auto & chunk : file_chunks) {
            PRS1DataChunk* previous = waveform_chunks[chunk->timestamp];
            if (previous != nullptr) {
                // Skip any chunks with identical timestamps. Never yet seen, so warn.
                qWarning() << chunkComparison(chunk, previous);
                delete chunk;
                continue;
            }
            waveform_chunks[chunk->timestamp] = chunk;
        }
    }
    
    // Get the list of pointers sorted by timestamp.
    result = waveform_chunks.values();

    // Coalesce contiguous waveform chunks into larger chunks.
    result = CoalesceWaveformChunks(result);

    return result;
}


QList<PRS1DataChunk *> PRS1Loader::ParseFile(const QString & path)
{
    QList<PRS1DataChunk *> CHUNKS;

    if (path.isEmpty()) {
        // ParseSession passes empty filepaths for waveforms if none exist.
        //qWarning() << path << "ParseFile given empty path";
        return CHUNKS;
    }

    QFile f(path);

    if (!f.exists()) {
        qWarning() << path << "missing";
        return CHUNKS;
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << path << "can't open";
        return CHUNKS;
    }
    
    RawDataFile* src;
    if (QFileInfo(f).suffix().toUpper().startsWith("B")) {  // .B01, .B02, etc.
        // If it's a DS2 file, insert the DS2 wrapper to decode the chunk stream.
        PRDS2File* ds2 = new PRDS2File(f, m_keycache);
        if (!ds2->isValid()) {
            //qWarning() << path << "unable to decrypt";
            delete ds2;
            return CHUNKS;
        }
        src = ds2;
    } else {
        // Otherwise just use the file as input.
        src = new RawDataFile(f);
    }

    PRS1DataChunk *chunk = nullptr, *lastchunk = nullptr;

    int cnt = 0;

    do {
        chunk = PRS1DataChunk::ParseNext(*src, this);
        if (chunk == nullptr) {
            break;
        }
        chunk->SetIndex(cnt);  // for logging/debugging purposes

        if (lastchunk != nullptr) {
            if ((lastchunk->fileVersion != chunk->fileVersion)
                    || (lastchunk->ext != chunk->ext)
                    || (lastchunk->family != chunk->family)
                    || (lastchunk->familyVersion != chunk->familyVersion)
                    || (lastchunk->htype != chunk->htype)) {
                QString message = "*** unexpected change in header data";
                qWarning() << path << message;
                m_ctx->LogUnexpectedMessage(message);
                // There used to be error-recovery code here, written before we checked CRCs.
                // If we ever encounter data with a valid CRC that triggers the above warnings,
                // we can then revisit how to handle it.
            }
        }

        CHUNKS.append(chunk);
        lastchunk = chunk;
        cnt++;
    } while (!src->atEnd());

    delete src;
    return CHUNKS;
}


bool initialized = false;

using namespace schema;

Channel PRS1Channels;

void PRS1Loader::initChannels()
{
    Channel * chan = nullptr;

    channel.add(GRP_CPAP, new Channel(CPAP_PressurePulse = 0x1009, MINOR_FLAG,  MT_CPAP,   SESSION,
        "PressurePulse",
        QObject::tr("Pressure Pulse"),
        QObject::tr("A pulse of pressure 'pinged' to detect a closed airway."),
        QObject::tr("PP"),
        STR_UNIT_EventsPerHour,    DEFAULT,    QColor("dark red")));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_Mode = 0xe120, SETTING,  MT_CPAP,  SESSION,
        "PRS1Mode", QObject::tr("Mode"),
        QObject::tr("PAP Mode"),
        QObject::tr("Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(PRS1_MODE_CPAPCHECK, QObject::tr("CPAP-Check"));
    chan->addOption(PRS1_MODE_CPAP,      QObject::tr("CPAP"));
    chan->addOption(PRS1_MODE_AUTOCPAP,  QObject::tr("AutoCPAP"));
    chan->addOption(PRS1_MODE_AUTOTRIAL, QObject::tr("Auto-Trial"));
    chan->addOption(PRS1_MODE_BILEVEL,   QObject::tr("Bi-Level"));
    chan->addOption(PRS1_MODE_AUTOBILEVEL, QObject::tr("AutoBiLevel"));
    chan->addOption(PRS1_MODE_ASV,       QObject::tr("ASV"));
    chan->addOption(PRS1_MODE_S,         QObject::tr("S"));
    chan->addOption(PRS1_MODE_ST,        QObject::tr("S/T"));
    chan->addOption(PRS1_MODE_PC,        QObject::tr("PC"));
    chan->addOption(PRS1_MODE_ST_AVAPS,  QObject::tr("S/T - AVAPS"));
    chan->addOption(PRS1_MODE_PC_AVAPS,  QObject::tr("PC - AVAPS"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_FlexMode = 0xe105, SETTING,  MT_CPAP,  SESSION,
        "PRS1FlexMode", QObject::tr("Flex Mode"),
        QObject::tr("PRS1 pressure relief mode."),
        QObject::tr("Flex Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(FLEX_None, STR_TR_None);
    chan->addOption(FLEX_CFlex, QObject::tr("C-Flex"));
    chan->addOption(FLEX_CFlexPlus, QObject::tr("C-Flex+"));
    chan->addOption(FLEX_AFlex, QObject::tr("A-Flex"));
    chan->addOption(FLEX_PFlex, QObject::tr("P-Flex"));
    chan->addOption(FLEX_RiseTime, QObject::tr("Rise Time"));
    chan->addOption(FLEX_BiFlex, QObject::tr("Bi-Flex"));
    //chan->addOption(FLEX_AVAPS, QObject::tr("AVAPS"));  // Converted into AVAPS PRS1_Mode with FLEX_RiseTime
    chan->addOption(FLEX_Flex, QObject::tr("Flex"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_FlexLevel = 0xe106, SETTING, MT_CPAP,   SESSION,
        "PRS1FlexSet",
        QObject::tr("Flex Level"),
        QObject::tr("PRS1 pressure relief setting."),
        QObject::tr("Flex Level"),
        "", LOOKUP, Qt::blue));
    chan->addOption(0, STR_TR_Off);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_FlexLock = 0xe111, SETTING, MT_CPAP,   SESSION,
        "PRS1FlexLock",
        QObject::tr("Flex Lock"),
        QObject::tr("Whether Flex settings are available to you."),
        QObject::tr("Flex Lock"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_RiseTime = 0xe119, SETTING, MT_CPAP,   SESSION,
        "PRS1RiseTime",
        QObject::tr("Rise Time"),
        QObject::tr("Amount of time it takes to transition from EPAP to IPAP, the higher the number the slower the transition"),
        QObject::tr("Rise Time"),
        "", LOOKUP, Qt::blue));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_RiseTimeLock = 0xe11a, SETTING, MT_CPAP,   SESSION,
        "PRS1RiseTimeLock",
        QObject::tr("Rise Time Lock"),
        QObject::tr("Whether Rise Time settings are available to you."),
        QObject::tr("Rise Lock"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_HumidStatus = 0xe101, SETTING, MT_CPAP, SESSION,
        "PRS1HumidStat",
        QObject::tr("Humidifier Status"),
        QObject::tr("PRS1 humidifier connected?"),
        QObject::tr("Humidifier"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, QObject::tr("Disconnected"));
    chan->addOption(1, QObject::tr("Connected"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_HumidMode = 0xe110, SETTING, MT_CPAP, SESSION,
        "PRS1HumidMode",
        QObject::tr("Humidification Mode"),
        QObject::tr("PRS1 Humidification Mode"),
        QObject::tr("Humid. Mode"),
        "", LOOKUP, Qt::green));
    chan->addOption(HUMID_Fixed, QObject::tr("Fixed (Classic)"));
    chan->addOption(HUMID_Adaptive, QObject::tr("Adaptive (System One)"));
    chan->addOption(HUMID_HeatedTube, QObject::tr("Heated Tube"));
    chan->addOption(HUMID_Passover, QObject::tr("Passover"));
    chan->addOption(HUMID_Error, QObject::tr("Error"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_TubeTemp = 0xe10f, SETTING, MT_CPAP,  SESSION,
        "PRS1TubeTemp",
        QObject::tr("Tube Temperature"),
        QObject::tr("PRS1 Heated Tube Temperature"),
        QObject::tr("Tube Temp."),
        "", LOOKUP, Qt::red));
    chan->addOption(0, STR_TR_Off);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_HumidLevel = 0xe102, SETTING,  MT_CPAP,  SESSION,
        "PRS1HumidLevel",
        QObject::tr("Humidifier"),  // label varies in reports, "Humidifier Setting" in 50-series, "Humidity Level" in 60-series, "Humidifier" in DreamStation
        QObject::tr("PRS1 Humidifier Setting"),
        QObject::tr("Humid. Level"),
        "", LOOKUP, Qt::blue));
    chan->addOption(0, STR_TR_Off);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_HumidTargetTime = 0xe11b, SETTING, MT_CPAP, SESSION,
        "PRS1HumidTargetTime",
        QObject::tr("Target Time"),
        QObject::tr("PRS1 Humidifier Target Time"),
        QObject::tr("Hum. Tgt Time"),
        STR_UNIT_Hours, DEFAULT, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("Auto"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_MaskResistSet = 0xe104, SETTING, MT_CPAP,   SESSION,
        "MaskResistSet",
        QObject::tr("Mask Resistance Setting"),
        QObject::tr("Mask Resistance Setting"),
        QObject::tr("Mask Resist."),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_HoseDiam = 0xe107, SETTING,  MT_CPAP,  SESSION,
        "PRS1HoseDiam",
        QObject::tr("Hose Diameter"),
        QObject::tr("Diameter of primary CPAP hose"),
        QObject::tr("Hose Diam."),
        "", LOOKUP, Qt::green));
    chan->addOption(22, QObject::tr("22mm"));
    chan->addOption(15, QObject::tr("15mm"));
    chan->addOption(12, QObject::tr("12mm"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_TubeLock = 0xe112, SETTING, MT_CPAP,   SESSION,
        "PRS1TubeLock",
        QObject::tr("Tubing Type Lock"),
        QObject::tr("Whether tubing type settings are available to you."),
        QObject::tr("Tube Lock"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_MaskResistLock = 0xe108, SETTING,  MT_CPAP,  SESSION,
        "MaskResistLock",
        QObject::tr("Mask Resistance Lock"),
        QObject::tr("Whether mask resistance settings are available to you."),
        QObject::tr("Mask Res. Lock"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_AutoOn = 0xe109, SETTING, MT_CPAP,   SESSION,
        "PRS1AutoOn",
        QObject::tr("Auto On"),
        QObject::tr("A few breaths automatically starts device"),
        QObject::tr("Auto On"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_AutoOff = 0xe10a, SETTING, MT_CPAP,   SESSION,
        "PRS1AutoOff",
        QObject::tr("Auto Off"),
        QObject::tr("Device automatically switches off"),
        QObject::tr("Auto Off"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_MaskAlert = 0xe10b, SETTING,  MT_CPAP,  SESSION,
        "PRS1MaskAlert",
        QObject::tr("Mask Alert"),
        QObject::tr("Whether or not device allows Mask checking."),
        QObject::tr("Mask Alert"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_ShowAHI = 0xe10c, SETTING, MT_CPAP,   SESSION,
        "PRS1ShowAHI",
        QObject::tr("Show AHI"),
        QObject::tr("Whether or not device shows AHI via built-in display."),
        QObject::tr("Show AHI"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_RampType = 0xe113, SETTING, MT_CPAP,   SESSION,
        "PRS1RampType",
        QObject::tr("Ramp Type"),
        QObject::tr("Type of ramp curve to use."),
        QObject::tr("Ramp Type"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, QObject::tr("Linear"));
    chan->addOption(1, QObject::tr("SmartRamp"));
    chan->addOption(2, QObject::tr("Ramp+"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_BackupBreathMode = 0xe114, SETTING, MT_CPAP,   SESSION,
        "PRS1BackupBreathMode",
        QObject::tr("Backup Breath Mode"),
        QObject::tr("The kind of backup breath rate in use: none (off), automatic, or fixed"),
        QObject::tr("Breath Rate"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("Auto"));
    chan->addOption(2, QObject::tr("Fixed"));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_BackupBreathRate = 0xe115, SETTING, MT_CPAP,   SESSION,
        "PRS1BackupBreathRate",
        QObject::tr("Fixed Backup Breath BPM"),
        QObject::tr("Minimum breaths per minute (BPM) below which a timed breath will be initiated"),
        QObject::tr("Breath BPM"),
        STR_UNIT_BreathsPerMinute, LOOKUP, Qt::black));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_BackupBreathTi = 0xe116, SETTING, MT_CPAP,   SESSION,
        "PRS1BackupBreathTi",
        QObject::tr("Timed Inspiration"),
        QObject::tr("The time that a timed breath will provide IPAP before transitioning to EPAP"),
        QObject::tr("Timed Insp."),
        STR_UNIT_Seconds, DEFAULT, Qt::blue));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_AutoTrial = 0xe117, SETTING, MT_CPAP,   SESSION,
        "PRS1AutoTrial",
        QObject::tr("Auto-Trial Duration"),
        QObject::tr("The number of days in the Auto-CPAP trial period, after which the device will revert to CPAP"),
        QObject::tr("Auto-Trial Dur."),
        "", LOOKUP, Qt::black));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_EZStart = 0xe118, SETTING, MT_CPAP,   SESSION,
        "PRS1EZStart",
        QObject::tr("EZ-Start"),
        QObject::tr("Whether or not EZ-Start is enabled"),
        QObject::tr("EZ-Start"),
        "", LOOKUP, Qt::black));
    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, STR_TR_On);

    channel.add(GRP_CPAP, chan = new Channel(PRS1_VariableBreathing = 0x1156, SPAN, MT_CPAP,    SESSION,
        "PRS1_VariableBreathing",
        QObject::tr("Variable Breathing"),
        QObject::tr("UNCONFIRMED: Possibly variable breathing, which are periods of high deviation from the peak inspiratory flow trend"),
        "VB",
        STR_UNIT_Seconds,
        DEFAULT,    QColor("#ffe8f0")));
    chan->setEnabled(false);  // disable by default

    channel.add(GRP_CPAP, new Channel(PRS1_BND = 0x1159, SPAN,  MT_CPAP,   SESSION,
        "PRS1_BND",
        QObject::tr("Breathing Not Detected"),
        QObject::tr("A period during a session where the device could not detect flow."),
        QObject::tr("BND"),
        STR_UNIT_Unknown,
        DEFAULT,    QColor("light purple")));
    channel.add(GRP_CPAP, new Channel(PRS1_TimedBreath = 0x1180, MINOR_FLAG, MT_CPAP,    SESSION,
        "PRS1TimedBreath",
        QObject::tr("Timed Breath"),
        QObject::tr("Machine Initiated Breath"),
        QObject::tr("TB"),
        STR_UNIT_Seconds,
        DEFAULT,    QColor("black")));

    channel.add(GRP_CPAP, chan = new Channel(PRS1_PeakFlow = 0x115a, WAVEFORM, MT_CPAP, SESSION,
        "PRS1PeakFlow",
        QObject::tr("Peak Flow"),
        QObject::tr("Peak flow during a 2-minute interval"),
        QObject::tr("Peak Flow"),
        STR_UNIT_LPM,
        DEFAULT,    QColor("red")));
    chan->setShowInOverview(true);
}

void PRS1Loader::Register()
{
    if (initialized) { return; }

    qDebug() << "Registering PRS1Loader";
    RegisterLoader(new PRS1Loader());
    initialized = true;
}
