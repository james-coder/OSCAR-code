/* SleepLib (DeVilbiss) Intellipap Loader Implementation
 *
 * Notes: Intellipap DV54 requires the SmartLink attachment to access this data.
 *
 * Copyright (c) 2011-2018 Mark Watkins 
 * Copyright (c) 2019-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QDir>
#include <QCoreApplication>
#include <QTimeZone>

#include "intellipap_loader.h"

//#define DEBUG6

ChannelID INTP_SmartFlexMode, INTP_SmartFlexLevel;

Intellipap::Intellipap(Profile *profile, MachineID id)
    : CPAP(profile, id)
{
}

Intellipap::~Intellipap()
{
}

IntellipapLoader::IntellipapLoader()
{
    const QString INTELLIPAP_ICON = ":/icons/intellipap.png";
    const QString DV6_ICON = ":/icons/dv64.png";

    QString s = newInfo().series;
    m_pixmap_paths[s] = INTELLIPAP_ICON;
    m_pixmaps[s] = QPixmap(INTELLIPAP_ICON);
    m_pixmap_paths["DV6"] = DV6_ICON;
    m_pixmaps["DV6"] = QPixmap(DV6_ICON);

    m_buffer = nullptr;
    m_type = MT_CPAP;
}

IntellipapLoader::~IntellipapLoader()
{
}

const QString SET_BIN = "SET.BIN";
const QString SET1 = "SET1";
const QString DV6 = "DV6";
const QString SL = "SL";

const QString DV6_DIR = "/" + DV6;
const QString SL_DIR = "/" + SL;

bool IntellipapLoader::Detect(const QString & givenpath)
{
    QString path = givenpath;
    if (path.endsWith(SL_DIR)) {
        path.chop(3);
    }
    if (path.endsWith(DV6_DIR)) {
        path.chop(4);
    }

    QDir dir(path);

    if (!dir.exists()) {
        return false;
    }

    // Intellipap DV54 has a folder called SL in the root directory, DV64 has DV6
    if (dir.cd(SL)) {
        // Test for presence of settings file
        return dir.exists(SET1) ? true : false;
    }

    if (dir.cd(DV6)) { // DV64
        return dir.exists(SET_BIN) ? true : false;
    }
    return false;
}

enum INTPAP_Type { INTPAP_Unknown, INTPAP_DV5, INTPAP_DV6 };


int IntellipapLoader::OpenDV5(const QString & path)
{
    QString newpath = path + SL_DIR;
    QString filename;

    qDebug() << "DV5 Loader started";

    //////////////////////////
    // Parse the Settings File
    //////////////////////////
    filename = newpath + "/" + SET1;
    QFile f(filename);

    if (!f.exists()) {
        return -1;
    }

    f.open(QFile::ReadOnly);
    QTextStream tstream(&f);

    const QString INT_PROP_Serial = "Serial";
    const QString INT_PROP_Model = "Model";
    const QString INT_PROP_Mode = "Mode";
    const QString INT_PROP_MaxPressure = "Max Pressure";
    const QString INT_PROP_MinPressure = "Min Pressure";
    const QString INT_PROP_IPAP = "IPAP";
    const QString INT_PROP_EPAP = "EPAP";
    const QString INT_PROP_PS = "PS";
    const QString INT_PROP_RampPressure = "Ramp Pressure";
    const QString INT_PROP_RampTime = "Ramp Time";

    const QString INT_PROP_HourMeter = "Usage Hours";
    const QString INT_PROP_ComplianceMeter = "Compliance Hours";
    const QString INT_PROP_ErrorCode = "Error";
    const QString INT_PROP_LastErrorCode = "Long Error";
    const QString INT_PROP_LowUseThreshold = "Low Usage";
    const QString INT_PROP_SmartFlex = "SmartFlex";
    const QString INT_PROP_SmartFlexMode = "SmartFlexMode";


    QHash<QString, QString> lookup;
        lookup["Sn"] = INT_PROP_Serial;
        lookup["Mn"] = INT_PROP_Model;
        lookup["Mo"] = INT_PROP_Mode;     // 0 cpap, 1 auto
        //lookup["Pn"]="??";
        lookup["Pu"] = INT_PROP_MaxPressure;
        lookup["Pl"] = INT_PROP_MinPressure;
        lookup["Pi"] = INT_PROP_IPAP;
        lookup["Pe"] = INT_PROP_EPAP;  // == WF on Auto models
        lookup["Ps"] = INT_PROP_PS;             // == WF on Auto models, Pressure support
        //lookup["Ds"]="??";
        //lookup["Pc"]="??";
        lookup["Pd"] = INT_PROP_RampPressure;
        lookup["Dt"] = INT_PROP_RampTime;
        //lookup["Ld"]="??";
        //lookup["Lh"]="??";
        //lookup["FC"]="??";
        //lookup["FE"]="??";
        //lookup["FL"]="??";
        lookup["A%"]="ApneaThreshold";
        lookup["Ad"]="ApneaDuration";
        lookup["H%"]="HypopneaThreshold";
        lookup["Hd"]="HypopneaDuration";
        //lookup["Pi"]="??";
        //lookup["Pe"]="??";
        lookup["Ri"]="SmartFlexIRnd";  // Inhale Rounding (0-5)
        lookup["Re"]="SmartFlexERnd"; // Inhale Rounding (0-5)
        //lookup["Bu"]="??"; // WF
        //lookup["Ie"]="??"; // 20
        //lookup["Se"]="??"; // 05    //Inspiratory trigger?
        //lookup["Si"]="??"; // 05    // Expiratory Trigger?
        //lookup["Mi"]="??"; // 0
        lookup["Uh"]="HoursMeter"; // 0000.0
        lookup["Up"]="ComplianceMeter"; // 0000.00
        //lookup["Er"]="ErrorCode";, // E00
        //lookup["El"]="LongErrorCode"; // E00 00/00/0000
        //lookup["Hp"]="??";, // 1
        //lookup["Hs"]="??";, // 02
        //lookup["Lu"]="LowUseThreshold"; // defaults to 0 (4 hours)
        lookup["Sf"] = INT_PROP_SmartFlex;
        lookup["Sm"] = INT_PROP_SmartFlexMode;
        lookup["Ks=s"]="Ks_s";
        lookup["Ks=i"]="ks_i";

    QHash<QString, QString> set1;
    QHash<QString, QString>::iterator hi;

    Machine *mach = nullptr;

    MachineInfo info = newInfo();


    bool ok;

    //EventDataType min_pressure = 0, max_pressure = 0, set_ipap = 0, set_ps = 0,
    EventDataType ramp_pressure = 0, set_epap = 0, ramp_time = 0;

    int papmode = 0, smartflex = 0, smartflexmode = 0;
    while (1) {
        QString line = tstream.readLine();

        if ((line.length() <= 2) ||
                (line.isNull())) { break; }

        QString key = line.section("\t", 0, 0).trimmed();
        hi = lookup.find(key);

        if (hi != lookup.end()) {
            key = hi.value();
        }

        QString value = line.section("\t", 1).trimmed();

        if (key == INT_PROP_Mode) {
            papmode = value.toInt(&ok);
        } else if (key == INT_PROP_Serial) {
            info.serial = value;
        } else if (key == INT_PROP_Model) {
            info.model = value;
        } else if (key == INT_PROP_MinPressure) {
            //min_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_MaxPressure) {
            //max_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_IPAP) {
            //set_ipap = value.toFloat() / 10.0;
        } else if (key == INT_PROP_EPAP) {
            set_epap = value.toFloat() / 10.0;
        } else if (key == INT_PROP_PS) {
            //set_ps = value.toFloat() / 10.0;
        } else if (key == INT_PROP_RampPressure) {
            ramp_pressure = value.toFloat() / 10.0;
        } else if (key == INT_PROP_RampTime) {
            ramp_time = value.toFloat() / 10.0;
        } else if (key == INT_PROP_SmartFlex) {
            smartflex = value.toInt();
        } else if (key == INT_PROP_SmartFlexMode) {
            smartflexmode = value.toInt();
        } else {
            set1[key] = value;
        }
        qDebug() << key << "=" << value;
    }

    CPAPMode mode = MODE_UNKNOWN;

    switch (papmode) {
    case 0:
        mode = MODE_CPAP;
        break;
    case 1:
        mode = (set_epap > 0) ? MODE_BILEVEL_FIXED : MODE_APAP;
        break;
    default:
        qDebug() << "New device mode";
    }

    if (!info.serial.isEmpty()) {
        mach = p_profile->CreateMachine(info);
    }

    if (!mach) {
        qDebug() << "Couldn't get Intellipap device record";
        return -1;
    }

    QString backupPath = mach->getBackupPath();
    QString copypath = path;

    if (QDir::cleanPath(path).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(path, backupPath);
    }


    // Refresh properties data..
    for (QHash<QString, QString>::iterator i = set1.begin(); i != set1.end(); i++) {
        mach->info.properties[i.key()] = i.value();
    }

    f.close();

    ///////////////////////////////////////////////
    // Parse the Session Index (U File)
    ///////////////////////////////////////////////
    unsigned char buf[27];
    filename = newpath + "/U";
    f.setFileName(filename);

    if (!f.exists()) { return -1; }

    QVector<quint32> SessionStart;
    QVector<quint32> SessionEnd;
    QHash<SessionID, Session *> Sessions;

    quint32 ts1, ts2;//, length;
    //unsigned char cs;
    f.open(QFile::ReadOnly);
    int cnt = 0;
    QDateTime epoch(QDate(2002, 1, 1), QTime(0, 0, 0), QTimeZone("UTC")); // Intellipap Epoch
    int ep = epoch.toSecsSinceEpoch();

    do {
        cnt = f.read((char *)buf, 9);
        // big endian
        ts1 = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
        ts2 = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
        // buf[8] == ??? What is this byte? A Bit Field? A checksum?
        ts1 += ep;
        ts2 += ep;
        SessionStart.append(ts1);
        SessionEnd.append(ts2);
    } while (cnt > 0);

    qDebug() << "U file logs" << SessionStart.size() << "sessions.";
    f.close();

    ///////////////////////////////////////////////
    // Parse the Session Data (L File)
    ///////////////////////////////////////////////
    filename = newpath + "/L";
    f.setFileName(filename);

    if (!f.exists()) { return -1; }

    f.open(QFile::ReadOnly);
    long size = f.size();
    int recs = size / 26;
    m_buffer = new unsigned char [size];

    if (size != f.read((char *)m_buffer, size)) {
        qDebug()  << "Couldn't read 'L' data" << filename;
        return -1;
    }

    Session *sess;
    SessionID sid;
    QHash<SessionID,qint64> rampstart;
    QHash<SessionID,qint64> rampend;

    for (int i = 0; i < SessionStart.size(); i++) {
        sid = SessionStart[i];

        if (mach->SessionExists(sid)) {
            // knock out the already imported sessions..
            SessionStart[i] = 0;
            SessionEnd[i] = 0;
        } else if (!Sessions.contains(sid)) {
            sess = Sessions[sid] = new Session(mach, sid);

            sess->really_set_first(qint64(sid) * 1000L);
         //   sess->really_set_last(qint64(SessionEnd[i]) * 1000L);

            rampstart[sid] = 0;
            rampend[sid] = 0;
            sess->SetChanged(true);
            if (mode >= MODE_BILEVEL_FIXED) {
                sess->AddEventList(CPAP_IPAP, EVL_Event);
                sess->AddEventList(CPAP_EPAP, EVL_Event);
                sess->AddEventList(CPAP_PS, EVL_Event);
            } else {
                sess->AddEventList(CPAP_Pressure, EVL_Event);
            }

            sess->AddEventList(INTELLIPAP_Unknown1, EVL_Event);
            sess->AddEventList(INTELLIPAP_Unknown2, EVL_Event);

            sess->AddEventList(CPAP_LeakTotal, EVL_Event);
            sess->AddEventList(CPAP_MaxLeak, EVL_Event);
            sess->AddEventList(CPAP_TidalVolume, EVL_Event);
            sess->AddEventList(CPAP_MinuteVent, EVL_Event);
            sess->AddEventList(CPAP_RespRate, EVL_Event);
            sess->AddEventList(CPAP_Snore, EVL_Event);

            sess->AddEventList(CPAP_Obstructive, EVL_Event);
            sess->AddEventList(INTP_SnoreFlag, EVL_Event);
            sess->AddEventList(CPAP_Hypopnea, EVL_Event);
            sess->AddEventList(CPAP_NRI, EVL_Event);
            sess->AddEventList(CPAP_LeakFlag, EVL_Event);
            sess->AddEventList(CPAP_ExP, EVL_Event);


        } else {
            // If there is a double up, null out the earlier session
            // otherwise there will be a crash on shutdown.
            for (int z = 0; z < SessionStart.size(); z++) {
                if (SessionStart[z] == (quint32)sid) {
                    SessionStart[z] = 0;
                    SessionEnd[z] = 0;
                    break;
                }
            }

            QDateTime d = QDateTime::fromSecsSinceEpoch(sid);
            qDebug() << sid << "has double ups" << d;
            /*Session *sess=Sessions[sid];
            Sessions.erase(Sessions.find(sid));
            delete sess;
            SessionStart[i]=0;
            SessionEnd[i]=0; */
        }
    }

    long pos = 0;
    int rampval = 0;
    sid = 0;
    //SessionID lastsid = 0;

    //int last_minp=0, last_maxp=0, last_ps=0, last_pres = 0;

    for (int i = 0; i < recs; i++) {
        // convert timestamp to real epoch
        ts1 = ((m_buffer[pos] << 24) | (m_buffer[pos + 1] << 16) | (m_buffer[pos + 2] << 8) | m_buffer[pos + 3]) + ep;

        for (int j = 0; j < SessionStart.size(); j++) {
            sid = SessionStart[j];

            if (!sid) { continue; }

            if ((ts1 >= (quint32)sid) && (ts1 <= SessionEnd[j])) {
                Session *sess = Sessions[sid];

                qint64 time = quint64(ts1) * 1000L;
                sess->really_set_last(time);
                sess->settings[CPAP_Mode] = mode;

                int minp = m_buffer[pos + 0x13];
                int maxp = m_buffer[pos + 0x14];
                int ps = m_buffer[pos + 0x15];
                int pres = m_buffer[pos + 0xd];

                if (mode >= MODE_BILEVEL_FIXED) {
                    rampval = maxp;
                } else {
                    rampval = minp;
                }

                qint64 rs = rampstart[sid];

                if (pres < rampval) {
                    if (!rs) {
                        // ramp started


//                        int rv = pres-rampval;
//                        double ramp =

                        rampstart[sid] = time;
                    }
                    rampend[sid] = time;
                } else {
                    if (rs > 0) {
                        if (!sess->eventlist.contains(CPAP_Ramp)) {
                            sess->AddEventList(CPAP_Ramp, EVL_Event);
                        }
                        int duration = (time - rs) / 1000L;
                        sess->eventlist[CPAP_Ramp][0]->AddEvent(time, duration);

                        rampstart.remove(sid);
                        rampend.remove(sid);
                    }
                }


                // Do this after ramp, because ramp calcs might need to insert interpolated pressure samples
                if (mode >= MODE_BILEVEL_FIXED) {

                    sess->settings[CPAP_EPAP] = float(minp) / 10.0;
                    sess->settings[CPAP_IPAP] = float(maxp) / 10.0;

                    sess->settings[CPAP_PS] = float(ps) / 10.0;


                    sess->eventlist[CPAP_IPAP][0]->AddEvent(time, float(pres) / 10.0);
                    sess->eventlist[CPAP_EPAP][0]->AddEvent(time, float(pres-ps) / 10.0);
//                   rampval = maxp;

                } else {
                    sess->eventlist[CPAP_Pressure][0]->AddEvent(time, float(pres) / 10.0); // current pressure
//                    rampval = minp;

                    if (mode == MODE_APAP) {
                        sess->settings[CPAP_PressureMin] = float(minp) / 10.0;
                        sess->settings[CPAP_PressureMax] = float(maxp) / 10.0;
                    } else if (mode == MODE_CPAP) {
                        sess->settings[CPAP_Pressure] = float(maxp) / 10.0;
                    }
                }


                sess->eventlist[CPAP_LeakTotal][0]->AddEvent(time, m_buffer[pos + 0x7]); // "Average Leak"
                sess->eventlist[CPAP_MaxLeak][0]->AddEvent(time, m_buffer[pos + 0x6]); // "Max Leak"

                int rr = m_buffer[pos + 0xa];
                sess->eventlist[CPAP_RespRate][0]->AddEvent(time, rr); // Respiratory Rate
               // sess->eventlist[INTELLIPAP_Unknown1][0]->AddEvent(time, m_buffer[pos + 0xf]); //
                sess->eventlist[INTELLIPAP_Unknown1][0]->AddEvent(time, m_buffer[pos + 0xc]);

                sess->eventlist[CPAP_Snore][0]->AddEvent(time, m_buffer[pos + 0x4]); //4/5??

                if (m_buffer[pos+0x4] > 0) {
                    sess->eventlist[INTP_SnoreFlag][0]->AddEvent(time, m_buffer[pos + 0x5]);
                }

                // 0x0f == Leak Event
                // 0x04 == Snore?
                if (m_buffer[pos + 0xf] > 0) { // Leak Event
                    sess->eventlist[CPAP_LeakFlag][0]->AddEvent(time, m_buffer[pos + 0xf]);
                }

                if (m_buffer[pos + 0x5] > 4) { // This matches Exhale Puff.. not sure why 4
                    //MW: Are the lower 2 bits something else?

                    sess->eventlist[CPAP_ExP][0]->AddEvent(time, m_buffer[pos + 0x5]);
                }

                if (m_buffer[pos + 0x10] > 0) {
                    sess->eventlist[CPAP_Obstructive][0]->AddEvent(time, m_buffer[pos + 0x10]);
                }

                if (m_buffer[pos + 0x11] > 0) {
                    sess->eventlist[CPAP_Hypopnea][0]->AddEvent(time, m_buffer[pos + 0x11]);
                }

                if (m_buffer[pos + 0x12] > 0) { // NRI // is this == to RERA?? CA??
                    sess->eventlist[CPAP_NRI][0]->AddEvent(time, m_buffer[pos + 0x12]);
                }

                quint16 tv = (m_buffer[pos + 0x8] << 8) | m_buffer[pos + 0x9]; // correct

                sess->eventlist[CPAP_TidalVolume][0]->AddEvent(time, tv);

                EventDataType mv = tv * rr; // MinuteVent=TidalVolume * Respiratory Rate
                sess->eventlist[CPAP_MinuteVent][0]->AddEvent(time, mv / 1000.0);
                break;
            } else {
            }
            //lastsid = sid;
        }

        pos += 26;
    }

    // Close any open ramps and store the event.
    QHash<SessionID,qint64>::iterator rit;
    QHash<SessionID,qint64>::iterator rit_end = rampstart.end();

    for (rit = rampstart.begin(); rit != rit_end; ++rit) {
        qint64 rs = rit.value();
        SessionID sid = rit.key();
        if (rs > 0) {
            qint64 re = rampend[rit.key()];

            Session *sess = Sessions[sid];
            if (!sess->eventlist.contains(CPAP_Ramp)) {
                sess->AddEventList(CPAP_Ramp, EVL_Event);
            }

            int duration = (re - rs) / 1000L;
            sess->eventlist[CPAP_Ramp][0]->AddEvent(re, duration);
            rit.value() = 0;

        }
    }

    for (int i = 0; i < SessionStart.size(); i++) {
        SessionID sid = SessionStart[i];

        if (sid) {
            sess = Sessions[sid];

            if (!sess) continue;

//            quint64 first = qint64(sid) * 1000L;
            //quint64 last = qint64(SessionEnd[i]) * 1000L;

            if (sess->last() > 0) {
             //   sess->really_set_last(last);


                sess->settings[INTP_SmartFlexLevel] = smartflex;

                if (smartflexmode == 0) {
                    sess->settings[INTP_SmartFlexMode] = PM_FullTime;
                } else {
                    sess->settings[INTP_SmartFlexMode] = PM_RampOnly;
                }

                sess->settings[CPAP_RampPressure] = ramp_pressure;
                sess->settings[CPAP_RampTime] = ramp_time;

                sess->UpdateSummaries();

                addSession(sess);
            } else {
                delete sess;
            }

        }
    }

    finishAddingSessions();
    mach->Save();


    delete [] m_buffer;

    f.close();

    int c = Sessions.size();
    return c;
}

////////////////////////////////////////////////////////////////////////////
// Devilbiss DV64 Notes
// 1)  High resolution data (flow and pressure) is kept on SD for only 100 hours
// 1a) Flow graph for days without high resolution data is absent
// 1b) Pressure graph is high resolution when high res data is available and
//     only 1 per minute when using low resolution data.
// 2)  Max and Average leak rates are as reported by DV64 device but we're
//     not sure how those measures relate to other device's data. Leak rate
//     seems to include the intentional mask leak.
// 2a) Not sure how SmartLink calculates the pct of time of poor mask fit.
//     May be same as what we call large leak time for other devices?
////////////////////////////////////////////////////////////////////////////

struct DV6TestedModel
{
    QString model;
    QString name;
};

static const DV6TestedModel testedModels[] = {
    { "DV64D", "Blue StandardPlus" },
    { "DV64E", "Blue AutoPlus" },
    { "DV63E", "Blue (IntelliPAP 2) AutoPlus" },
    { "", "unknown product" } // List stopper -- must be last entry
};

struct DV6_S_Data // Daily summary
{
/***    
    Session * sess;
    unsigned char u1;            //00 (position)
***/    
    unsigned int start_time;     //01 Start time for date
    unsigned int stop_time;      //05 End time
    unsigned int written;        //09 timestamp when this record was written
    EventDataType hours;         //13
//    EventDataType unknown14;   //14
    EventDataType pressureAvg;   //15
    EventDataType pressureMax;   //16
    EventDataType pressure50;    //17 50th percentile
    EventDataType pressure90;    //18 90th percentile
    EventDataType pressure95;    //19 95th percentile
    EventDataType pressureStdDev;//20 std deviation
//    EventDataType unknown_21;    //21
    EventDataType leakAvg;       //22
    EventDataType leakMax;       //23
    EventDataType leak50;        //24 50th percentile
    EventDataType leak90;        //25 90th percentile
    EventDataType leak95;        //26 95th percentile
    EventDataType leakStdDev;    //27 std deviation
    EventDataType tidalVolume;   //28 & 0x29
    EventDataType avgBreathRate; //30
    EventDataType unknown_31;    //31
    EventDataType snores;        //32 snores / hypopnea per minute
    EventDataType timeInExPuf;   //33 Time in Expiratory Puff
    EventDataType timeInFL;      //34 Time in Flow Limitation
    EventDataType timeInPB;      //35 Time in Periodic Breathing
    EventDataType maskFit;       //36 mask fit (or rather, not fit) percentage
    EventDataType indexOA;       //37 Obstructive
    EventDataType indexCA;       //38 Central index
    EventDataType indexHyp;      //39 Hypopnea Index
    EventDataType unknown_40;    //40 Reserved?
    EventDataType unknown_41;    //40 Reserved?
                                 //42-48 unknown
    EventDataType pressureSetMin;   //49
    EventDataType pressureSetMax;   //50
};

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#else
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif


// DV6_S_REC is the day structure in the S.BIN file
PACK (struct DV6_S_REC{
    unsigned char begin[4];         //0 Beginning of day
    unsigned char end[4];           //4 End of day
    unsigned char written[4];       //8 When this record was written??
    unsigned char hours;            //12 Hours in session * 10
    unsigned char unknown_13;       //13
    unsigned char pressureAvg;      //14 All pressure settings are * 10
    unsigned char pressureMax;      //15
    unsigned char pressure50;       //16 50th percentile
    unsigned char pressure90;       //17 90th percentile
    unsigned char pressure95;       //18 95th percentile
    unsigned char pressureStdDev;   //19 std deviation
    unsigned char unknown_20;       //20
    unsigned char leakAvg;          //21
    unsigned char leakMax;          //22
    unsigned char leak50;           //23 50th percentile
    unsigned char leak90;           //24 90th percentile
    unsigned char leak95;           //25 95th percentile
    unsigned char leakStdDev;       //26 std deviation
    unsigned char tv1;              //27 tidal volume = tv2 * 256 + tv1
    unsigned char tv2;              //28
    unsigned char avgBreathRate;    //29
    unsigned char unknown_30;       //30
    unsigned char snores;           //31 snores / hypopnea per minute
    unsigned char timeInExPuf;      //32 % Time in Expiratory Puff * 2
    unsigned char timeInFL;         //33 % Time in Flow Limitation * 2
    unsigned char timeInPB;         //34 % Time in Periodic Breathing * 2
    unsigned char maskFit;          //35 mask fit (or rather, not fit) percentage * 2
    unsigned char indexOA;          //36 Obstructive index * 4
    unsigned char indexCA;          //37 Central index * 4
    unsigned char indexHyp;         //38 Hypopnea Index * 4
    unsigned char unknown_39;       //39 Reserved?
    unsigned char unknown_40;       //40 Reserved?
    unsigned char unknown_41;       //41
    unsigned char unknown_42;       //42
    unsigned char unknown_43;       //43
    unsigned char unknown_44;       //44 % time snoring *4
    unsigned char unknown_45;       //45
    unsigned char unknown_46;       //46
    unsigned char unknown_47;       //47 (related to smartflex and flow rounding?)
    unsigned char pressureSetMin;   //48
    unsigned char pressureSetMax;   //49
    unsigned char unknown_50;       //50
    unsigned char unknown_51;       //51
    unsigned char unknown_52;       //52
    unsigned char unknown_53;       //53
    unsigned char checksum;         //54
});

// DV6 SET.BIN - structure of the entire settings file
PACK (struct SET_BIN_REC {
    char unknown_00;                        // assuming file version
    char serial[11];                        // null terminated
    unsigned char language;
    unsigned char capabilities;             // CPAP or APAP
    unsigned char unknown_11;
    unsigned char cpap_pressure;
    unsigned char unknown_12;
    unsigned char max_pressure;
    unsigned char unknown_13;
    unsigned char min_pressure;
    unsigned char alg_apnea_threshhold;     // always locked at 00
    unsigned char alg_apnea_duration;
    unsigned char alg_hypop_threshold;
    unsigned char alg_hypop_duration;
    unsigned char ramp_pressure;
    unsigned char unknown_01;
    unsigned char ramp_duration;
    unsigned char unknown_02[3];
    unsigned char smartflex_setting;
    unsigned char smartflex_when;
    unsigned char inspFlowRounding;
    unsigned char expFlowRounding;
    unsigned char complianceHours;
    unsigned char unknown_03;
    unsigned char tubing_diameter;
    unsigned char autostart_setting;
    unsigned char unknown_04;
    unsigned char show_hide;
    unsigned char unknown_05;
    unsigned char lock_flags;
    unsigned char unknown_06;
    unsigned char humidifier_setting;  // 0-5
    unsigned char unknown_7;
    unsigned char possible_alg_apnea;
    unsigned char unknown_8[7];
    unsigned char bacteria_filter;
    unsigned char unused[73];
    unsigned char checksum;
}); // http://digitalvampire.org/blog/index.php/2006/07/31/why-you-shouldnt-use-__attribute__packed/

// Unless explicitly noted, all other DV6_x_REC are definitions for the repeating data structure that follows the header
PACK (struct DV6_HEADER {
    unsigned char unknown;          // 0 always zero
    unsigned char filetype;         // 1 e.g. "R" for a R.BIN file
    unsigned char serial[11];       // 2 serial number
    unsigned char numRecords[4];    // 13 Number of records in file (always fixed, 180,000 for R.BIN)
    unsigned char recordLength;     // 17 Length of data record (always 117)
    unsigned char recordStart[4];   // 18 First record in wrap-around buffer
    unsigned char unknown_22[21];   // 22 Unknown values
    unsigned char unknown_43[8];    // 43 Seems always to be zero
    unsigned char lasttime[4];      // 51 OSCAR only: Last timestamp, in history files only
    unsigned char checksum;         // 55 Checksum
});

// DV6 E.BIN - event data
struct DV6_E_REC { // event log record
    unsigned char begin[4];
    unsigned char end[4];
    unsigned char unknown_01;
    unsigned char unknown_02;
    unsigned char unknown_03;
    unsigned char unknown_04;
    unsigned char event_type;
    unsigned char event_severity;
    unsigned char value;
    unsigned char reduction;
    unsigned char duration;
    unsigned char unknown[7];
    unsigned char checksum;
};

// DV6 U.BIN - session start and stop times
struct DV6_U_REC {
    unsigned char begin[4];
    unsigned char end[4];
    unsigned char checksum;  // possible checksum? Not really sure
};

// DV6 R.BIN - High resolution data (breath) and moderate resolution (pressure, flags)
struct DV6_R_REC {
    unsigned char timestamp[4];
    qint16 breath[50];          // 50 breath flow records at 25 Hz
    unsigned char pressure1;    // pressure in first second of frame
    unsigned char pressure2;    // pressure in second second of frame
    unsigned char unknown106;
    unsigned char unknown107;
    unsigned char flags1[4];    // flags for first second of frame
    unsigned char flags2[4];    // flags for second second of frame
    unsigned char checksum;
};

// DV6 L.BIN - Low resolution data
PACK (struct DV6_L_REC {
    unsigned char timestamp[4];     // 0 timestamp
    unsigned char maxLeak;          // 4 lpm
    unsigned char avgLeak;          // 5 lpm
    unsigned char tidalVolume6;     // 6
    unsigned char tidalVolume7;     // 7
    unsigned char breathRate;       // 8 breaths per minute
    unsigned char unknown9;         // 9
    unsigned char avgPressure;      // 10 pressure * 10
    unsigned char unknown11;        // 11 always zero?
    unsigned char unknown12;        // 12
    unsigned char pressureLimitLow; // 13 pressure * 10
    unsigned char pressureLimitHigh;// 14 pressure * 10
    unsigned char timeSnoring;      // 15
    unsigned char snoringSeverity;  // 16
    unsigned char timeEP;           // 17
    unsigned char epSeverity;       // 18
    unsigned char timeX1;           // 19 ??
    unsigned char x1Severity;       // 20 ??
    unsigned char timeX2;           // 21 ??
    unsigned char x2Severity;       // 22 ??
    unsigned char timeX3;           // 23 ??
    unsigned char x3Severity;       // 24 ??
    unsigned char apSeverity;       // 25
    unsigned char TimeApnea;        // 26
    unsigned char noaSeverity;      // 27
    unsigned char timeNOA;          // 28
    unsigned char ukSeverity;       // 29 ??
    unsigned char timeUk;           // 30 ??
    unsigned char unknown31;        // 31
    unsigned char unknown32;        // 32
    unsigned char unknown33;        // 33
    unsigned char unknownFlag34;    // 34
    unsigned char unknownTime35;    // 35
    unsigned char unknownFlag36;    // 36
    unsigned char unknown37;        // 37
    unsigned char unknown38;        // 38
    unsigned char unknown39;        // 39
    unsigned char unknown40;        // 40
    unsigned char unknown41;        // 41
    unsigned char unknown42;        // 42
    unsigned char unknown43;        // 43
    unsigned char checksum;         // 44
});

// Our structure for managing sessions
struct DV6_SessionInfo {
    Session * sess;
    DV6_S_Data *dailyData;
    SET_BIN_REC * dv6Settings;

    unsigned int begin;
    unsigned int end;
    unsigned int written;
//    bool haveHighResData;
    unsigned int firstHighRes;
    unsigned int lastHighRes;
    CPAPMode mode = MODE_UNKNOWN;
};

QString card_path;
QString backup_path;
QString history_path;
QString rebuild_path;

MachineInfo info;
Machine * mach = nullptr;

bool rebuild_from_backups = false;
bool create_backups = false;

QStringList inputFilePaths;

QMap<SessionID, DV6_S_Data> DailySummaries;
QMap<SessionID, DV6_SessionInfo> SessionData;
SET_BIN_REC * settings;

unsigned int ep = 0;

// Convert a 4-character number in DV6 data file to a standard int
unsigned int convertNum (unsigned char num[]) {
    return ((num[3] << 24) + (num[2] << 16) + (num[1] << 8) + num[0]);
}

// Convert a timestamp in DV6 data file to a standard Unix epoch timestamp as used in OSCAR
unsigned int convertTime (unsigned char time[]) {
    if (ep == 0) {
        QDateTime epoch(QDate(2002, 1, 1), QTime(0, 0, 0), QTimeZone("UTC")); // Intellipap Epoch
        ep = epoch.toSecsSinceEpoch();
    }
    return ((time[3] << 24) + (time[2] << 16) + (time[1] << 8) + time[0]) + ep; // Time as Unix epoch time
}

class RollingBackup
{
public:
    RollingBackup () {}
    ~RollingBackup () {
    }

    bool open (const QString filetype, DV6_HEADER * newhdr, QByteArray * startTime); // Open the file
    bool close();           // close the file
    bool save(const QByteArray &dataBA);  // save the next record in the file

private:
    DV6_HEADER hdr;       // file header
    QString filetype;
    QFile histfile;

    const qint64 maxHistFileSize = 10000000;    // Maximum size of file before we create a new file, in MB (40 MB)
                                                // (While 40e6 would be easier to understand, 40e6 is a double, not an int)

    unsigned int lastTimeInFile;    // Timestamp of last data record in history file
    int numWritten;     // Number of records written
};

QStringList getHistoryFileNames (const QString filetype, bool reversed = false) {
    QStringList filters;
    QDir hpath(history_path);

    filters.append(filetype);     // Assume one-letter file name like "S.BIN"
    filters[0].insert(1, "_*");   // Add a wild card like "S_*.BIN"
    hpath.setNameFilters(filters);
    hpath.setFilter(QDir::Files);
    hpath.setSorting(QDir::Name);
    if (reversed) hpath.setSorting(QDir::Name | QDir::Reversed);

    return hpath.entryList(); // Get list of files
}

QString getNewFileName (QString filetype, QByteArray * startTime, int offset=0) {
    unsigned char startTimeChar[5];
    for (int i = 0; i < 4; i++)
        startTimeChar[i] = startTime->at(offset+i);
    unsigned int ts = convertTime(startTimeChar);
    QString newfile = filetype.left(1) + "_" + QDateTime::fromSecsSinceEpoch(ts).toString("yyyyMMdd") + ".BIN";
    qDebug() << "DV6 getNewFileName returns" << newfile;
    return newfile;
}

bool RollingBackup::open (const QString filetype, DV6_HEADER * inputhdr, QByteArray * startTimeOfBackup) {
    if (!create_backups)
        return true;

    QDir hpath(history_path);
    QString historypath = hpath.absolutePath() + "/";
    int histfilesize = 0;

    this->filetype = filetype;

    bool needNewFile = false;
    memcpy (&hdr, inputhdr, sizeof(DV6_HEADER));
    numWritten = 0;

    QStringList fileNames = getHistoryFileNames(filetype, true);

    // Handle first time a history file is being created
    if (fileNames.isEmpty()) {
        for (int i = 0; i < 4; i++) {
            hdr.recordStart[i] = 0;
            hdr.lasttime[i] = 0;
        }
        lastTimeInFile = 0;
        histfile.setFileName(historypath + getNewFileName (filetype, startTimeOfBackup));
        needNewFile= true;
    }
    
    // We have an existing history record
    if (! fileNames.isEmpty()) {
        histfile.setFileName(historypath + fileNames.first());      // File names are in reverse order, so latest is first

        // Open and read history file header and save the header
        if (!histfile.open(QIODevice::ReadWrite)) {
            qWarning() << "DV6 rb(open) could not open" << fileNames.first() << "for readwrite, error code" << histfile.error() << histfile.errorString();
            return false;
        }
        histfilesize = histfile.size();
        QByteArray dataBA = histfile.read(sizeof(DV6_HEADER));
        memcpy (&hdr, dataBA.data(), sizeof(DV6_HEADER));
        lastTimeInFile = convertTime(hdr.lasttime);

        // See if this file is large enough that we want to create a new file
        // If it is large, we'll start a new file.
        if (histfile.size() > maxHistFileSize) {
            QString nextFile = historypath + getNewFileName (filetype, &dataBA, 51);
            QString hh = histfile.fileName();

            if (hh != nextFile) {
                lastTimeInFile = convertTime(hdr.lasttime);
                histfile.close();
                // Update header saying we are starting at record 0
                for (int i = 0; i < 4; i++)
                    hdr.recordStart[i] = 0;
                histfile.setFileName(nextFile);
                needNewFile = true;
            }
        }
    }

    if (needNewFile) {
        if (!histfile.open(QIODevice::ReadWrite)) {
            qWarning() << "DV6 rb(open) could not create new file" << histfile.fileName() << "for readwrite, error code" << histfile.error() << histfile.errorString();
            return false;
        }
        if (histfile.write((char *)&hdr.unknown, sizeof(DV6_HEADER)) != sizeof(DV6_HEADER)) {
            qWarning() << "DV6 rb(open) could not write header to new file" << histfile.fileName() << "for readwrite, error code" << histfile.error() << histfile.errorString();
            histfile.close();
            return false;
        }
    } else {
//        qDebug() << "DV6 rb(open) history file size" << histfilesize;
        histfile.seek(histfilesize);
    }

    return true;
}

bool RollingBackup::close() {
    if (!create_backups)
        return true;

    qint32 size = histfile.size();

    if (!histfile.seek(0)) {
        qWarning() << "DV6 rb(close) unable to seek to file beginning" << histfile.error() << histfile.errorString();
        histfile.close();
        return false;
    }

    quint32 sizehdr = sizeof(DV6_HEADER);
    quint32 reclen = hdr.recordLength;
    quint32 wrap_point = (size - sizehdr) / reclen;

    hdr.recordStart[0] = wrap_point & 0xff;
    hdr.recordStart[1] = (wrap_point >> 8)  & 0xff;
    hdr.recordStart[2] = (wrap_point >> 16) & 0xff;
    hdr.recordStart[3] = (wrap_point >> 24) & 0xff;

    if (histfile.write((char *)&hdr, sizeof(DV6_HEADER)) != sizeof(DV6_HEADER)) {
        qWarning() << "DV6 rb(close) could not write header to file" << histfile.fileName() << "error code" << histfile.error() << histfile.errorString();
        histfile.close();
        return false;
    }

    histfile.close();
    qDebug() << "DV6 rb(close) wrote" << numWritten << "records.";
    return true;
}

bool RollingBackup::save(const QByteArray &dataBA) {

    if (!create_backups)
        return true;

    unsigned char * data = (unsigned char *)dataBA.data();
    unsigned int thisTimeStamp = convertTime(data);

    if (thisTimeStamp > lastTimeInFile) {   // Is this data new to us?
        // If so, save it to the history file.
        if (histfile.write(dataBA) == -1) {
            qWarning() << "DV6 rb(save) could not save record" << histfile.fileName() << "for readwrite, error code" << histfile.error() << histfile.errorString();
            histfile.close();
            return false;
        }
        memcpy(&hdr.lasttime, data, 4);
//        if (!histfile.seek(histfile.pos() + dataBA.length()))
//            qWarning() << "DV6 rb(save) failed respositioning" << histfile.fileName() << "for readwrite, error code" << histfile.error() << histfile.errorString();
        numWritten++;
    }
/***
    else {
        qDebug() << "DV6 rb(save) skipping record" << numWritten << QDateTime::fromSecsSinceEpoch(thisTimeStamp).toString("MM/dd/yyyy hh:mm:ss")
                 << "last in file" << QDateTime::fromSecsSinceEpoch(lastTimeInFile).toString("MM/dd/yyyy hh:mm:ss");
    }
***/

    return true;
}

class RollingFile
{
public:
    RollingFile () { }

    ~RollingFile () {
        if (data)
            delete [] data;
        data = nullptr;
        if (hdr)
            delete hdr;
        hdr = nullptr;
    }

    bool open (QString fn, bool getNext = false); // Open the file
    bool close();           // close the file
    unsigned char * get();  // read the next record in the file

    int numread () {return number_read;};   // Return number of records read
    int recnum () {return record_number;};  // Return last-read record number

    RollingBackup rb;

private:
    QString filename;
    QFile file;
    int record_length;
    int wrap_record;
    bool wrapping = false;

    int number_read = 0;    // Number of records read

    int record_number = 0;  // Number of record.  First record in the file  is #1. First record read is wrap_record;

    DV6_HEADER * hdr;       // file header

    unsigned char * data = nullptr; // record pointer
};

bool RollingFile::open(QString filetype, bool getNext) {

    filename = filetype;

    if (rebuild_from_backups) {
        // Building from backup
        if (!getNext) { // Initialize on first call
            inputFilePaths.clear();
            QStringList histFileNames = getHistoryFileNames(filetype);
            qDebug() << "DV6 rf(open) History file names" << histFileNames;
            for (int i=0; i < histFileNames.size(); i++) {
                file.setFileName(history_path + "/" + histFileNames.at(i));
                inputFilePaths.append(file.fileName());
            }
        }
        if (inputFilePaths.empty())
            return false;
        file.setFileName(inputFilePaths.at(0));
        inputFilePaths.removeAt(0);

    } else {
        file.setFileName(card_path + "/" +filetype);
        inputFilePaths.clear();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DV6 rf(open) could not open" << filename << "for reading, error code" << file.error() << file.errorString();
        return false;
    }

    // Save header for use in making backups of data
    hdr = new DV6_HEADER;
    QByteArray dataBA = file.read(sizeof(DV6_HEADER));
    memcpy (hdr, dataBA.data(), sizeof(DV6_HEADER));

    // Extract control information from header
    record_length = hdr->recordLength;
    wrap_record = convertNum(hdr->recordStart);
    record_number = wrap_record;
    number_read = 0;
    wrapping = false;

    // Create buffer to hold each record as it is read
    data = new unsigned char[record_length];

    // Seek to oldest data record in file, which is always at the wrap point
    // wrap_record is the C offset where the next data record is to be written.
    // Since C offsets begin with zero, it is also the number of records in the file.
    int seekpos = sizeof(DV6_HEADER) + wrap_record * record_length;
    if (!file.seek(seekpos)) {
        qWarning() << "DV6 rf(open) unable to make initial seek to record" << wrap_record << "in" + filename << file.error() << file.errorString();
        file.close();
        return false;
    }
    qDebug() << "DV6 rf(open)" << filetype << "positioning to oldest record at pos" << seekpos << "after seek" << file.pos();

    if (file.atEnd()) {
        file.seek(sizeof(DV6_HEADER));
    }
    dataBA = file.read(4);      // Read timestamp of newest data record
    file.seek(seekpos);  // Reset read position before what we just read so we start reading data records here
    if (!rb.open(filetype, hdr, &dataBA)) {
        qWarning() << "DV6 rf(open) failed";
        file.close();
        return false;
    }

    qDebug() << "DV6 rf(open)" << filename << "at wrap record" << wrap_record << "now at pos" << file.pos();
    return true;
}

bool RollingFile::close() {

/*** Works for backing up but prevents chart appearing for the last day
    // Flush any additional input that has not been backed up
    if (create_backups) {
        do {
            DV6_U_REC * rec = (DV6_U_REC *) get();
            if (rec == nullptr)
                break;
        } while (true);
    }
***/

    file.close();

    rb.close();

    if (data)
        delete [] data;
    data = nullptr;
    if (hdr)
        delete hdr;
    hdr = nullptr;

    return true;
}

unsigned char * RollingFile::get() {

//    int readpos;
    record_number++;

    // If we have found the wrap record again, we are done
    if (wrapping && record_number == wrap_record) {
        // Unless we are rebuilding from backup and may have more files
        if (rebuild_from_backups && !inputFilePaths.empty()) {
            qDebug() << "DV6 rf(get) closing" << file.fileName();
            file.close();
            open(inputFilePaths.at(0), true);
        } else {
            return nullptr;
        }
    }

    // Have we reached end of file and need to wrap around to beginning?
    if (file.atEnd()) {
        if (wrapping) {
            qDebug() << "DV6 rf(get) wrap - second time through";
            return nullptr;
        }
        qDebug() << "DV6 rf(get) wrapping to beginning of data in" << filename << "record number is" << record_number-1 << "records read" << number_read;
        record_number = 1;
        wrapping = true;
        if (!file.seek(sizeof(DV6_HEADER))) {
            file.close();
            qWarning() << "DV6 rf(get) unable to seek to first data record in file";
            return nullptr;
        }
        qDebug() << "DV6 rf(get) #" << record_number << "now at pos" << file.pos();
    }

    QByteArray dataBA;
//    readpos = file.pos();
    dataBA=file.read(record_length); // read next record
    if (dataBA.size() != record_length) {
        qWarning() << "DV6 rf(get) #" << record_number << "wrong length";
        file.close();
        return nullptr;
    }

    if (!rb.save(dataBA)) {
        qWarning() << "DV6 rf(get) failed";
    }

    number_read++;

//    qDebug() << "DV6 rf(get)" << filename << "at start pos" << readpos << "end pos" << file.pos() << "record number" << record_number << "of length" << record_length << "number read so far" << number_read;
    memcpy (data, (unsigned char *) dataBA.data(), record_length);
    return data;
}

// Returns empty QByteArray() on failure.
QByteArray fileChecksum(const QString &fileName,
                        QCryptographicHash::Algorithm hashAlgorithm)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly)) {
        QCryptographicHash hash(hashAlgorithm);
        bool res = hash.addData(&f);
        f.close();
        if (res) {
            return hash.result();
        }
    }
    return QByteArray();
}

// Return date used within OSCAR, assuming day ends at split time in preferences (usually noon)
QDate getNominalDate (QDateTime dt) {
    QDate d = dt.date();
    QTime tm = dt.time();
    QTime daySplitTime = p_profile->session->getPref(STR_IS_DaySplitTime).toTime();
    if (tm < daySplitTime)
        d = d.addDays(-1);
    return d;
}
QDate getNominalDate (unsigned int dt) {
    QDateTime xdt = QDateTime::fromSecsSinceEpoch(dt);
    return getNominalDate(xdt);
}

///////////////////////////////////////////////
// U.BIN - Open and parse session list and create session data structures
//         with session start and stop times.
///////////////////////////////////////////////

bool load6Sessions () {

    RollingFile rf;
    unsigned int ts1,ts2;

    SessionData.clear();

    qDebug() << "Parsing U.BIN";

    if (!rf.open("U.BIN")) {
        qWarning() << "Unable to open U.BIN";
        return false;
    }

    do {
        DV6_U_REC * rec = (DV6_U_REC *) rf.get();
        if (rec == nullptr)
            break;
        DV6_SessionInfo sinfo;
        // big endian
        ts1 = convertTime(rec->begin); // session start time (this is also the session id)
        ts2 = convertTime(rec->end); // session end time
//#ifdef DEBUG6
        qDebug() << "U.BIN Session" << QDateTime::fromSecsSinceEpoch(ts1).toString("MM/dd/yyyy hh:mm:ss") << ts1 << "to" << QDateTime::fromSecsSinceEpoch(ts2).toString("MM/dd/yyyy hh:mm:ss") << ts2;
//#endif
        sinfo.sess = nullptr;
        sinfo.dailyData = nullptr;
        sinfo.begin = ts1;
        sinfo.end = ts2;
        sinfo.written = 0;
//        sinfo.haveHighResData = false;
        sinfo.firstHighRes = 0;
        sinfo.lastHighRes = 0;

        SessionData[ts1] = sinfo;
    } while (true);

    rf.close();
    qDebug() << "DV6 U.BIN processed" << rf.numread() << "records";

    return true;
}

/////////////////////////////////////////////////////////////////////////////////
// Parse SET.BIN settings file
/////////////////////////////////////////////////////////////////////////////////

bool load6Settings (const QString & path) {

    QByteArray dataBA;

    QFile f(path+"/"+SET_BIN);
    if (rebuild_from_backups)
        f.setFileName(rebuild_path+"/"+SET_BIN);

    if (f.open(QIODevice::ReadOnly)) {
        // Read and parse entire SET.BIN file
        dataBA = f.readAll();
        f.close();

        settings = (SET_BIN_REC *)dataBA.data();

    } else { // if f.open settings file
        // Settings file open failed, return
        qWarning() << "Unable to open SET.BIN file";
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// S.BIN - Open and load day summary list
////////////////////////////////////////////////////////////////////////////////////////

bool load6DailySummaries () {
    RollingFile rf;

    DailySummaries.clear();

    if (!rf.open("S.BIN")) {
        qWarning() << "Unable to open S.BIN";
        return false;
    }

    qDebug() << "Reading S.BIN summaries";

    do {
        DV6_S_REC * rec = (DV6_S_REC *) rf.get();
        if (rec == nullptr)
            break;

        DV6_S_Data dailyData;

        dailyData.start_time = convertTime(rec->begin);
        dailyData.stop_time = convertTime(rec->end);
        dailyData.written = convertTime(rec->written);

#ifdef DEBUG6
        qDebug() << "DV6 S.BIN start" << dailyData.start_time
                 << "stop" << dailyData.stop_time
                 << "written" << dailyData.written;
#endif

        dailyData.hours = float(rec->hours) / 10.0F;
        dailyData.pressureSetMin = float(rec->pressureSetMin) / 10.0F;
        dailyData.pressureSetMax = float(rec->pressureSetMax) / 10.0F;

        // The following stuff is not necessary to decode, but can be used to verify we are on the right track
        dailyData.pressureAvg = float(rec->pressureAvg) / 10.0F;
        dailyData.pressureMax = float(rec->pressureMax) / 10.0F;
        dailyData.pressure50 = float(rec->pressure50) / 10.0F;
        dailyData.pressure90 = float(rec->pressure90) / 10.0F;
        dailyData.pressure95 = float(rec->pressure95) / 10.0F;
        dailyData.pressureStdDev = float(rec->pressureStdDev) / 10.0F;

        dailyData.leakAvg = float(rec->leakAvg) / 10.0F;
        dailyData.leakMax = float(rec->leakMax) / 10.0F;
        dailyData.leak50= float(rec->leak50) / 10.0F;
        dailyData.leak90 = float(rec->leak90) / 10.0F;
        dailyData.leak95 = float(rec->leak95) / 10.0F;
        dailyData.leakStdDev = float(rec->leakStdDev) / 10.0F;

        dailyData.tidalVolume = float(rec->tv1 | rec->tv2 << 8);
        dailyData.avgBreathRate = float(rec->avgBreathRate);

        dailyData.snores = float(rec->snores);
        dailyData.timeInExPuf = float(rec->timeInExPuf) / 2.0F;
        dailyData.timeInFL = float(rec->timeInFL) / 2.0F;
        dailyData.timeInPB = float(rec->timeInPB) / 2.0F;
        dailyData.maskFit = float(rec->maskFit) / 2.0F;
        dailyData.indexOA = float(rec->indexOA) / 4.0F;
        dailyData.indexCA = float(rec->indexCA) / 4.0F;
        dailyData.indexHyp = float(rec->indexHyp) / 4.0F;

        DailySummaries[dailyData.start_time] = dailyData;

/**** Previous loader did this:
        if (!mach->sessionlist.contains(ts1)) { // Check if already imported
            qDebug() << "Detected new Session" << ts1;
            R.sess = new Session(mach, ts1);
            R.sess->SetChanged(true);

            R.sess->really_set_first(qint64(ts1) * 1000L);
            R.sess->really_set_last(qint64(ts2) * 1000L);

            if (data[49] != data[50]) {
                R.sess->settings[CPAP_PressureMin] = R.pressureSetMin;
                R.sess->settings[CPAP_PressureMax] = R.pressureSetMax;
                R.sess->settings[CPAP_Mode] = MODE_APAP;
            } else {
                R.sess->settings[CPAP_Mode] = MODE_CPAP;
                R.sess->settings[CPAP_Pressure] = R.pressureSetMin;
            }
            R.hasMaskPressure = false;
***/

    } while (true);

    rf.close();
    qDebug() << "DV6 S.BIN processed" << rf.numread() << "records";

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Parse VER.BIN for model number, serial, etc.
////////////////////////////////////////////////////////////////////////////////////////

bool load6VersionInfo(const QString & path) {

    QByteArray dataBA;
    QByteArray str;

    QFile f(path+"/VER.BIN");
    if (rebuild_from_backups)
        f.setFileName(rebuild_path+"/VER.BIN");
    info.series = "DV6";
    info.brand = "DeVilbiss";

    if (f.open(QIODevice::ReadOnly)) {
        dataBA = f.readAll();
        f.close();

        int cnt = 0;
        for (int i=0; i< dataBA.size(); ++i) { // deliberately going one further to catch end condition
            if ((dataBA.at(i) == 0) || (i >= dataBA.size()-1)) { // if null terminated or last byte

                switch(cnt) {
                case 1: // serial
                    info.serial = str;
                    break;
                case 2: // modelnumber
//                    info.model = str;
                    info.modelnumber = str;
                    info.modelnumber = info.modelnumber.trimmed();
                    for (int i = 0; i < (int)sizeof(testedModels); i++) {
                        if (   testedModels[i].model == info.modelnumber
                            || testedModels[i].model.isEmpty()) {
                            info.model = testedModels[i].name;
                            break;
                        }
                    }
                    break;
                case 7: // ??? V025RN20170
                    break;
                case 9: // ??? V014BL20150630
                    break;
                case 11: // ??? 01 09
                    break;
                case 12: // ??? 0C 0C
                    break;
                case 14: // ??? BA 0C
                    break;
                default:
                    break;

                }

                // Clear and start a new data record
                str.clear();
                cnt++;
            } else {
                // Add the character to the current string
                str.append(dataBA[i]);
            }
        }
        return true;

    } else { // if (f.open(...)
        // VER.BIN open failed
        qWarning() << "Unable to open VER.BIN";
        return false;
    }

}

////////////////////////////////////////////////////////////////////////////////////////
// Create DV6_SessionInfo structures for each session and store in SessionData qmap
////////////////////////////////////////////////////////////////////////////////////////

int create6Sessions() {
    SessionID sid = 0;
    Session * sess;
    
    for (auto sinfo=SessionData.begin(), end=SessionData.end(); sinfo != end; ++sinfo) {
        sid = sinfo->begin;

        if (mach->SessionExists(sid)) {
            // skip already imported sessions..
            qDebug() << "Session already exists" << QDateTime::fromSecsSinceEpoch(sid).toString("MM/dd/yyyy hh:mm:ss");

        } else if (sinfo->sess == nullptr) {
            // process new sessions
            sess = new Session(mach, sid);
#ifdef DEBUG6
            qDebug() << "Creating session" << QDateTime::fromSecsSinceEpoch(sinfo->begin).toString("MM/dd/yyyy hh:mm:ss") << "to" << QDateTime::fromSecsSinceEpoch(sinfo->end).toString("MM/dd/yyyy hh:mm:ss");
#endif

            sinfo->sess = sess;
            sinfo->dailyData = nullptr;
            sinfo->written = 0;
//            sinfo->haveHighResData = false;
            sinfo->firstHighRes = 0;
            sinfo->lastHighRes = 0;

            sess->really_set_first(quint64(sinfo->begin) * 1000L);
            sess->really_set_last(quint64(sinfo->end) * 1000L);

//            rampstart[sid] = 0;
//            rampend[sid] = 0;
            sess->SetChanged(true);
            
            sess->AddEventList(INTELLIPAP_Unknown1, EVL_Event);
            sess->AddEventList(INTELLIPAP_Unknown2, EVL_Event);

            sess->AddEventList(CPAP_LeakTotal, EVL_Event);
            sess->AddEventList(CPAP_MaxLeak, EVL_Event);
            sess->AddEventList(CPAP_TidalVolume, EVL_Event);
            sess->AddEventList(CPAP_MinuteVent, EVL_Event);
            sess->AddEventList(CPAP_RespRate, EVL_Event);
            sess->AddEventList(CPAP_Snore, EVL_Event);
            sess->AddEventList(INTP_SnoreFlag, EVL_Event);

            sess->AddEventList(CPAP_Obstructive, EVL_Event);
            sess->AddEventList(CPAP_Hypopnea, EVL_Event);
            sess->AddEventList(CPAP_NRI, EVL_Event);
//            sess->AddEventList(CPAP_LeakFlag, EVL_Event);
            sess->AddEventList(CPAP_ExP, EVL_Event);
            sess->AddEventList(CPAP_FlowLimit, EVL_Event);


        } else {
            // If there is a duplicate session, null out the earlier session
            // otherwise there will be a crash on shutdown.
//??            for (int z = 0; z < SessionStart.size(); z++) {
//??                if (SessionStart[z] == sid) {
//??                    SessionStart[z] = 0;
//??                    SessionEnd[z] = 0;
//??                    break;
//??                }
            qDebug() << sid << "has double ups" << QDateTime::fromSecsSinceEpoch(sid).toString("MM/dd/yyyy hh:mm:ss");
            
            /*Session *sess=Sessions[sid];
            Sessions.erase(Sessions.find(sid));
            delete sess;
            SessionStart[i]=0;
            SessionEnd[i]=0; */
        }
    }

    qDebug() << "Created" << SessionData.size() << "sessions";
    return SessionData.size();
}

////////////////////////////////////////////////////////////////////////////////////////
// Parse R.BIN for high resolution flow data
////////////////////////////////////////////////////////////////////////////////////////

bool load6HighResData () {

    RollingFile rf;
    Session *sess = nullptr;
    unsigned int rec_ts1, previousRecBegin = 0;
    bool inSession = false; // true if we are adding data to this session

    if (!rf.open("R.BIN")) {
        qWarning() << "DV6 Unable to open R.BIN";
        return false;
    }

    qDebug() << "R.BIN starting at record" << rf.recnum();

    sess = NULL;
    EventList * flow = NULL;
    EventList * pressure = NULL;
    EventList * FLG = NULL;
    EventList * snore = NULL;
//        EventList * leak = NULL;
/***
    EventList * OA  = NULL;
    EventList * HY  = NULL;
    EventList * NOA = NULL;
    EventList * EXP = NULL;
    EventList * FL  = NULL;
    EventList * PB  = NULL;
    EventList * VS  = NULL;
    EventList * LL  = NULL;
    EventList * RE  = NULL;
    bool inOA = false, inH = false, inCA = false, inExP = false, inVS = false, inFL = false, inPB = false, inRE = false, inLL = false;
    qint64 OAstart = 0, OAend = 0;
    qint64 Hstart = 0, Hend = 0;
    qint64 CAstart = 0, CAend = 0;
    qint64 ExPstart = 0, ExPend = 0;
    qint64 VSstart = 0, VSend = 0;
    qint64 FLstart = 0, FLend = 0;
    qint64 PBstart = 0, PBend = 0;
    qint64 REstart =0, REend = 0;
    qint64 LLstart =0, LLend = 0;
//    lastts1 = 0;
***/

    // sinfo is for walking through sessions when matching up with flow data records
    QMap<SessionID, DV6_SessionInfo>::iterator sinfo;
    sinfo = SessionData.begin();

    do {
        DV6_R_REC * R = (DV6_R_REC *) rf.get();
        if (R == nullptr)
            break;

        sess = sinfo->sess;

        // Get the timestamp from the record
        rec_ts1 = convertTime(R->timestamp);

        if (rec_ts1 < previousRecBegin) {
            qWarning() << "R.BIN - Corruption/Out of sequence data found, skipping record" << rf.recnum() << ", prev"
                       << QDateTime::fromSecsSinceEpoch(previousRecBegin).toString("MM/dd/yyyy hh:mm:ss") << previousRecBegin
                       << "this"
                       << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << rec_ts1;
            continue;
        }

        // Look for a gap in DV6_R records.  They should be at two second intervals.
        // If there is a gap, we are probably in a new session
        if (inSession && ((rec_ts1 - previousRecBegin) > 2)) {
            if (sess) {
                sess->set_last(flow->last());
                if (sess->first() == 0)
                    qWarning() << "R.BIN first = 0 - 1320";
                EventDataType min = flow->Min();
                EventDataType max = flow->Max();
                sess->setMin(CPAP_FlowRate, min);
                sess->setMax(CPAP_FlowRate, max);
                sess->setPhysMax(CPAP_FlowRate, min); // not sure :/
                sess->setPhysMin(CPAP_FlowRate, max);
//                sess->really_set_last(flow->last());
            }
            sess = nullptr;
            flow = nullptr;
            pressure = nullptr;
            FLG = nullptr;
            snore = nullptr;
            inSession = false;
        }

        // Skip over sessions until we find one that this record is in
        while (rec_ts1 > sinfo->end) {
#ifdef DEBUG6
            qDebug() << "R.BIN - skipping session" << QDateTime::fromSecsSinceEpoch(sinfo->begin).toString("MM/dd/yyyy hh:mm:ss")
                     << "looking for" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss")
                     << "record" << rf.recnum();
#endif
            if (inSession && sess) {
                // update min and max
                // then add to device
                if (sess->first() == 0)
                    qWarning() << "R.BIN first = 0 - 1284";
                EventDataType min = flow->Min();
                EventDataType max = flow->Max();
                sess->setMin(CPAP_FlowRate, min);
                sess->setMax(CPAP_FlowRate, max);
                sess->setPhysMax(CPAP_FlowRate, min); // not sure :/
                sess->setPhysMin(CPAP_FlowRate, max);

                sess = nullptr;
                flow = nullptr;
                pressure = nullptr;
                FLG = nullptr;
                snore = nullptr;
                inSession = false;
            }
            sinfo++;
            if (sinfo == SessionData.end())
                break;
        }

        previousRecBegin = rec_ts1;

        // If we have data beyond last session, we are in trouble (for unknown reasons)
        if (sinfo == SessionData.end()) {
            qWarning() << "DV6 R.BIN import ran out of sessions to match flow data, record" << rf.recnum();
            break;
        }

        // Check if record belongs in this session or a future session
        if (!inSession && rec_ts1 <= sinfo->end) {
            sess = sinfo->sess;         // this is the Session we want
            if (!inSession && sess) {
                inSession = true;
                flow = sess->AddEventList(CPAP_FlowRate, EVL_Waveform, 0.01f, 0.0f, 0.0f, 0.0f, double(2000) / double(50));
                pressure = sess->AddEventList(CPAP_Pressure, EVL_Waveform, 0.1f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));
                FLG = sess->AddEventList(CPAP_FLG, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));
                snore = sess->AddEventList(CPAP_Snore, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));
                //                    sinfo->hasMaskPressure = true;
                //                    leak = R->sess->AddEventList(CPAP_Leak, EVL_Waveform, 1.0, 0.0, 0.0, 0.0, double(2000) / double(1));
                /***
                    OA = R->sess->AddEventList(CPAP_Obstructive, EVL_Event);
                    NOA = R->sess->AddEventList(CPAP_NRI, EVL_Event);
                    RE = R->sess->AddEventList(CPAP_RERA, EVL_Event);
                    VS = R->sess->AddEventList(CPAP_VSnore, EVL_Event);
                    HY = R->sess->AddEventList(CPAP_Hypopnea, EVL_Event);
                    EXP = R->sess->AddEventList(CPAP_ExP, EVL_Event);
                    FL = R->sess->AddEventList(CPAP_FlowLimit, EVL_Event);
                    PB = R->sess->AddEventList(CPAP_PB, EVL_Event);
                    LL = R->sess->AddEventList(CPAP_LargeLeak, EVL_Event);
***/
            }
        }
        if (inSession) {
            // Record breath and pressure waveforms
            qint64 ti = qint64(rec_ts1) * 1000;
            flow->AddWaveform(ti,R->breath,50,2000);
            pressure->AddWaveform(ti, &R->pressure1, 2, 2000);
            if (sinfo->firstHighRes == 0 || sinfo->firstHighRes > rec_ts1) sinfo->firstHighRes = rec_ts1;
            if (sinfo->lastHighRes == 0 || sinfo->lastHighRes < rec_ts1+2) sinfo->lastHighRes = rec_ts1+2;
//            sinfo->haveHighResData = true;
            if (sess->first() == 0)
                qWarning() << "first = 0 - 1442";


            //////////////////////////////////////////////////////////////////
            // Show Flow Limitation Events as a graph
            //////////////////////////////////////////////////////////////////
            qint16 severity = (R->flags1[0] >> 4) & 0x03;
            FLG->AddWaveform(ti, &severity, 1, 1000);
            severity = (R->flags2[0] >> 4) & 0x03;
            FLG->AddWaveform(ti+1000, &severity, 1, 1000);

            //////////////////////////////////////////////////////////////////
            // Show Snore Events as a graph
            //////////////////////////////////////////////////////////////////
            severity = R->flags1[0] & 0x03;
            snore->AddWaveform(ti, &severity, 1, 1000);
            severity = R->flags2[0] & 0x03;
            snore->AddWaveform(ti+1000, &severity, 1, 1000);

            /****
                // Fields data[107] && data[108] are bitfields default is 0x90, occasionally 0x98

                d[0] = data[107];
                d[1] = data[108];

                //leak->AddWaveform(ti+40000, d, 2, 2000);

                // Needs to track state to pull events out cleanly..

                //////////////////////////////////////////////////////////////////
                // High Leak
                //////////////////////////////////////////////////////////////////

                if (data[110] & 3) {  // LL state 1st second
                    if (!inLL) {
                        LLstart = ti;
                        inLL = true;
                    }
                    LLend = ti+1000L;
                } else {
                    if (inLL) {
                        inLL = false;
                        LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
                        LLstart = 0;
                    }
                }
                if (data[114] & 3) {
                    if (!inLL) {
                        LLstart = ti+1000L;
                        inLL = true;
                    }
                    LLend = ti+2000L;

                } else {
                    if (inLL) {
                        inLL = false;
                        LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
                        LLstart = 0;
                    }
                }


                //////////////////////////////////////////////////////////////////
                // Obstructive Apnea
                //////////////////////////////////////////////////////////////////

                if (data[110] & 12) {  // OA state 1st second
                    if (!inOA) {
                        OAstart = ti;
                        inOA = true;
                    }
                    OAend = ti+1000L;
                } else {
                    if (inOA) {
                        inOA = false;
                        OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
                        OAstart = 0;
                    }
                }
                if (data[114] & 12) {
                    if (!inOA) {
                        OAstart = ti+1000L;
                        inOA = true;
                    }
                    OAend = ti+2000L;

                } else {
                    if (inOA) {
                        inOA = false;
                        OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
                        OAstart = 0;
                    }
                }


                //////////////////////////////////////////////////////////////////
                // Hypopnea
                //////////////////////////////////////////////////////////////////

                if (data[110] & 192) {
                    if (!inH) {
                        Hstart = ti;
                        inH = true;
                    }
                    Hend = ti + 1000L;
                } else {
                    if (inH) {
                        inH = false;
                        HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
                        Hstart = 0;
                    }
                }

                if (data[114] & 192) {
                    if (!inH) {
                        Hstart = ti+1000L;
                        inH = true;
                    }
                    Hend = ti + 2000L;
                } else {
                    if (inH) {
                        inH = false;
                        HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
                        Hstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // Non Responding Apnea Event (Are these CA's???)
                //////////////////////////////////////////////////////////////////
                if (data[110] & 48) {  // OA state 1st second
                    if (!inCA) {
                        CAstart = ti;
                        inCA = true;
                    }
                    CAend = ti+1000L;
                } else {
                    if (inCA) {
                        inCA = false;
                        NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
                        CAstart = 0;
                    }
                }
                if (data[114] & 48) {
                    if (!inCA) {
                        CAstart = ti+1000L;
                        inCA = true;
                    }
                    CAend = ti+2000L;

                } else {
                    if (inCA) {
                        inCA = false;
                        NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
                        CAstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // VSnore Event
                //////////////////////////////////////////////////////////////////
                if (data[109] & 3) {  // OA state 1st second
                    if (!inVS) {
                        VSstart = ti;
                        inVS = true;
                    }
                    VSend = ti+1000L;
                } else {
                    if (inVS) {
                        inVS = false;
                        VS->AddEvent(VSstart,(VSend-VSstart) / 1000L);
                        VSstart = 0;
                    }
                }
                if (data[113] & 3) {
                    if (!inVS) {
                        VSstart = ti+1000L;
                        inVS = true;
                    }
                    VSend = ti+2000L;

                } else {
                    if (inVS) {
                        inVS = false;
                        VS->AddEvent(VSstart,(VSend-VSstart) / 1000L);
                        VSstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // Expiratory puff Event
                //////////////////////////////////////////////////////////////////
                if (data[109] & 12) {  // OA state 1st second
                    if (!inExP) {
                        ExPstart = ti;
                        inExP = true;
                    }
                    ExPend = ti+1000L;
                } else {
                    if (inExP) {
                        inExP = false;
                        EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
                        ExPstart = 0;
                    }
                }
                if (data[113] & 12) {
                    if (!inExP) {
                        ExPstart = ti+1000L;
                        inExP = true;
                    }
                    ExPend = ti+2000L;

                } else {
                    if (inExP) {
                        inExP = false;
                        EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
                        ExPstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // Flow Limitation Event
                //////////////////////////////////////////////////////////////////
                if (data[109] & 48) {  // OA state 1st second
                    if (!inFL) {
                        FLstart = ti;
                        inFL = true;
                    }
                    FLend = ti+1000L;
                } else {
                    if (inFL) {
                        inFL = false;
                        FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
                        FLstart = 0;
                    }
                }
                if (data[113] & 48) {
                    if (!inFL) {
                        FLstart = ti+1000L;
                        inFL = true;
                    }
                    FLend = ti+2000L;

                } else {
                    if (inFL) {
                        inFL = false;
                        FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
                        FLstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // Periodic Breathing Event
                //////////////////////////////////////////////////////////////////
                if (data[109] & 192) {  // OA state 1st second
                    if (!inPB) {
                        PBstart = ti;
                        inPB = true;
                    }
                    PBend = ti+1000L;
                } else {
                    if (inPB) {
                        inPB = false;
                        PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
                        PBstart = 0;
                    }
                }
                if (data[113] & 192) {
                    if (!inPB) {
                        PBstart = ti+1000L;
                        inPB = true;
                    }
                    PBend = ti+2000L;

                } else {
                    if (inPB) {
                        inPB = false;
                        PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
                        PBstart = 0;
                    }
                }

                //////////////////////////////////////////////////////////////////
                // Respiratory Effort Related Arousal Event
                //////////////////////////////////////////////////////////////////
                if (data[111] & 48) {  // OA state 1st second
                    if (!inRE) {
                        REstart = ti;
                        inRE = true;
                    }
                    REend = ti+1000L;
                } else {
                    if (inRE) {
                        inRE = false;
                        RE->AddEvent(REstart,(REend-REstart) / 1000L);
                        REstart = 0;
                    }
                }
                if (data[115] & 48) {
                    if (!inRE) {
                        REstart = ti+1000L;
                        inRE = true;
                    }
                    REend = ti+2000L;

                } else {
                    if (inRE) {
                        inRE = false;
                        RE->AddEvent(REstart,(REend-REstart) / 1000L);
                        REstart = 0;
                    }
                }
***/
        }

    } while (true);

    if (inSession && sess) {
        /***
            // Close event states if they are still open, and write event.
            if (inH) HY->AddEvent(Hstart,(Hend-Hstart) / 1000L);
            if (inOA) OA->AddEvent(OAstart,(OAend-OAstart) / 1000L);
            if (inCA) NOA->AddEvent(CAstart,(CAend-CAstart) / 1000L);
            if (inLL) LL->AddEvent(LLstart,(LLend-LLstart) / 1000L);
            if (inVS) HY->AddEvent(VSstart,(VSend-VSstart) / 1000L);
            if (inExP) EXP->AddEvent(ExPstart,(ExPend-ExPstart) / 1000L);
            if (inFL) FL->AddEvent(FLstart,(FLend-FLstart) / 1000L);
            if (inPB) PB->AddEvent(PBstart,(PBend-PBstart) / 1000L);
            if (inPB) RE->AddEvent(REstart,(REend-REstart) / 1000L);
***/
        // update min and max
        // then add to device
        if (sess->first() == 0)
            qWarning() << "R.BIN first = 0 - 1665";
        EventDataType min = flow->Min();
        EventDataType max = flow->Max();
        sess->setMin(CPAP_FlowRate, min);
        sess->setMax(CPAP_FlowRate, max);

        sess->setPhysMax(CPAP_FlowRate, min); // TODO: not sure :/
        sess->setPhysMin(CPAP_FlowRate, max);
        sess->really_set_last(flow->last());

        sess = nullptr;
        inSession = false;
    }

    rf.close();
    qDebug() << "DV6 R.BIN processed" << rf.numread() << "records";
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Parse L.BIN for per minute data
////////////////////////////////////////////////////////////////////////////////////////

bool load6PerMinute () {

    RollingFile rf;
    Session *sess = nullptr;
    unsigned int rec_ts1, previousRecBegin = 0;
    bool inSession = false; // true if we are adding data to this session

    if (!rf.open("L.BIN")) {
        qWarning() << "DV6 Unable to open L.BIN";
        return false;
    }

    qDebug() << "L.BIN Minute Data starting at record" << rf.recnum();

    EventList * leak = NULL;
    EventList * maxleak = NULL;
    EventList * RR  = NULL;
    EventList * Pressure  = NULL;
    EventList * TV  = NULL;
    EventList * MV = NULL;

    QMap<SessionID, DV6_SessionInfo>::iterator sinfo;
    sinfo = SessionData.begin();

    // Walk through all the records
    do {
        DV6_L_REC * rec = (DV6_L_REC *) rf.get();
        if (rec == nullptr)
            break;

        sess = sinfo->sess;

        // Get the timestamp from the record
        rec_ts1 = convertTime(rec->timestamp);

        if (rec_ts1 < previousRecBegin) {
#ifdef DEBUG6
            qWarning() << "L.BIN - Corruption/Out of sequence data found, skipping record" << rf.recnum() << ", prev"
                       << QDateTime::fromSecsSinceEpoch(previousRecBegin).toString("MM/dd/yyyy hh:mm:ss") << previousRecBegin
                       << "this"
                       << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << rec_ts1;
#endif
            continue;
        }
/****
        // Look for a gap in DV6_L records.  They should be at one minute intervals.
        // If there is a gap, we are probably in a new session
        if (inSession && ((rec_ts1 - previousRecBegin) > 60)) {
            qDebug() << "L.BIN record gap, current" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss")
                     << "previous" << QDateTime::fromSecsSinceEpoch(previousRecBegin).toString("MM/dd/yyyy hh:mm:ss");
            sess->set_last(maxleak->last());
            sess = nullptr;
            leak = maxleak = MV = TV = RR = Pressure = nullptr;
            inSession = false;
        }
****/
        // Skip over sessions until we find one that this record is in
        while (rec_ts1 > sinfo->end) {
#ifdef DEBUG6
            qDebug() << "L.BIN - skipping session" << QDateTime::fromSecsSinceEpoch(sinfo->begin).toString("MM/dd/yyyy hh:mm:ss") << "looking for" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss");
#endif
            if (inSession && sess) {
                // Close the open session and update the min and max
                sess->set_last(maxleak->last());
                sess = nullptr;
                leak = maxleak = MV = TV = RR = Pressure = nullptr;
                inSession = false;
            }
            sinfo++;
            if (sinfo == SessionData.end())
                break;
        }

        previousRecBegin = rec_ts1;

        // If we have data beyond last session, we are in trouble (for unknown reasons)
        if (sinfo == SessionData.end()) {
            qWarning() << "DV6 L.BIN import ran out of sessions to match flow data";
            break;
        }

        if (rec_ts1 < previousRecBegin) {
            qWarning() << "L.BIN - Corruption/Out of sequence data found, stopping import, prev"
                       << QDateTime::fromSecsSinceEpoch(previousRecBegin).toString("MM/dd/yyyy hh:mm:ss")
                       << "this"
                       << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss");
            break;
        }

        // Check if record belongs in this session or a future session
        if (!inSession && rec_ts1 <= sinfo->end) {
            sess = sinfo->sess;         // this is the Session we want
            if (!inSession && sess) {
                leak = sess->AddEventList(CPAP_Leak, EVL_Event); // , 1.0, 0.0, 0.0, 0.0, double(60000) / double(1));
                maxleak = sess->AddEventList(CPAP_LeakTotal, EVL_Event);// , 1.0, 0.0, 0.0, 0.0, double(60000) / double(1));
                RR = sess->AddEventList(CPAP_RespRate, EVL_Event);
                MV = sess->AddEventList(CPAP_MinuteVent, EVL_Event);
                TV = sess->AddEventList(CPAP_TidalVolume, EVL_Event);

                if (sess->last()/1000 > sinfo->end)
                    sinfo->end = sess->last()/1000;

//                if (!sinfo->haveHighResData) {
                    // Don't use this pressure if we already have higher resolution data

                if (sinfo->mode == MODE_UNKNOWN) {
                    if (rec->pressureLimitLow != rec->pressureLimitHigh) {
                        sess->settings[CPAP_PressureMin] = rec->pressureLimitLow / 10.0f;
                        sess->settings[CPAP_PressureMax] = rec->pressureLimitHigh / 10.0f;
// if available         sess->settings[CPAP_PS) = ....
                        sess->settings[CPAP_Mode] = MODE_APAP;
                        sinfo->mode = MODE_APAP;
                    } else {
                        sess->settings[CPAP_Mode] = MODE_CPAP;
                        sess->settings[CPAP_Pressure] = rec->pressureLimitHigh / 10.0f;
                        sinfo->mode = MODE_CPAP;
                    }
                    inSession = true;
                }
            }
        }
        if (inSession) {
            // Record breath and pressure waveforms
            qint64 ti = qint64(rec_ts1) * 1000;
            maxleak->AddEvent(ti, rec->maxLeak);  //???
            leak->AddEvent(ti, rec->avgLeak);     //???
            RR->AddEvent(ti, rec->breathRate);

            if (   sinfo->firstHighRes ==  0        // No high res data
                || rec_ts1 < sinfo->firstHighRes    // Before high res data begins
                || ((rec_ts1 > (sinfo->lastHighRes+2)) && (sinfo->lastHighRes > 0))) // or after high res data ends
            {
                if (!Pressure)
                    Pressure = sess->AddEventList(CPAP_Pressure, EVL_Event, 0.1f);

//                if (sinfo->firstHighRes ==  0) {
                Pressure->AddEvent(ti, rec->avgPressure);  // average pressure for next minute
                Pressure->AddEvent(ti + 59998, rec->avgPressure);  // end of pressure block
//                } else {
//                    for (int i = 0; i < 60; i++) {
//                        Pressure->AddEvent(ti+i, rec->avgPressure);  // average pressure for next minute
//                    }
//                }
            }

/***
            if (Pressure)
                qDebug() << "Lowres pressure" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss")
                         << "rec_ts1" << rec_ts1 << "firstHighRes" << sinfo->firstHighRes << "last" << sinfo->lastHighRes
                         << "Pressure" << rec->avgPressure / 10.0f;
***/

            unsigned tv = rec->tidalVolume6 + (rec->tidalVolume7 << 8);
            MV->AddEvent(ti, rec->breathRate * tv / 1000.0 );
            TV->AddEvent(ti, tv);

            if (!sess->channelExists(CPAP_FlowRate)) {
                // No flow rate, so lets grab this data...
            }
        }

    } while (true);

    if (sess && inSession) {
        sess->set_last(maxleak->last());
    }

    rf.close();
    qDebug() << "DV6 L.BIN processed" << rf.numread() << "records";

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Parse E.BIN for event data
////////////////////////////////////////////////////////////////////////////////////////

bool load6EventData () {
    RollingFile rf;

    Session *sess = nullptr;
    unsigned int rec_ts1, rec_ts2, previousRecBegin;
    bool inSession = false; // true if we are adding data to this session

    EventList * OA = nullptr;
    EventList * CA = nullptr;
    EventList * H  = nullptr;
    EventList * RE = nullptr;
    EventList * PB = nullptr;
    EventList * LL = nullptr;
    EventList * EP = nullptr;
    EventList * SN = nullptr;
    EventList * FL = nullptr;
//    EventList * FLG = nullptr;

    if (!rf.open("E.BIN")) {
        qWarning() << "DV6 Unable to open E.BIN";
        return false;
    }

    qDebug() << "Processing E.BIN starting at record" << rf.recnum();

    QMap<SessionID, DV6_SessionInfo>::iterator sinfo;
    sinfo = SessionData.begin();

    // Walk through all the records
    do {
        DV6_E_REC * rec = (DV6_E_REC *) rf.get();
        if (rec == nullptr)
            break;
        sess = sinfo->sess;

        // Get the timestamp from the record
        rec_ts1 = convertTime(rec->begin);
        rec_ts2 = convertTime(rec->end);

        // Skip over sessions until we find one that this record is in
        while (rec_ts1 > sinfo->end) {
#ifdef DEBUG6
            qDebug() << "E.BIN - skipping session" << QDateTime::fromSecsSinceEpoch(sinfo->begin).toString("MM/dd/yyyy hh:mm:ss") << "looking for" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss");
#endif
            if (inSession) {
                // Close the open session and update the min and max
                if (OA->last() > 0)
                    sess->set_last(OA->last());
                if (CA->last() > 0)
                    sess->set_last(CA->last());
                if (H->last() > 0)
                    sess->set_last(H->last());
                if (RE->last() > 0)
                    sess->set_last(RE->last());
                if (PB->last() > 0)
                    sess->set_last(PB->last());
                if (LL->last() > 0)
                    sess->set_last(LL->last());
                if (EP->last() > 0)
                    sess->set_last(EP->last());
                if (FL->last() > 0)
                    sess->set_last(FL->last());
                if (SN->last() > 0)
                    sess->set_last(SN->last());
/***
                if (FLG->last() > 0)
                    sess->set_last(FLG->last());
***/
                sess = nullptr;
                H = CA = RE = OA = PB = LL = EP = SN = FL = nullptr;
                inSession = false;
            }
            sinfo++;
            if (sinfo == SessionData.end())
                break;
        }

        previousRecBegin = rec_ts1;

        // If we have data beyond last session, we are in trouble (for unknown reasons)
        if (sinfo == SessionData.end()) {
            qWarning() << "DV6 E.BIN import ran out of sessions,"
                       << "event data begins"
                       << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss");
            break;
        }

        if (rec_ts1 < previousRecBegin) {
            qWarning() << "DV6 E.BIN - Out of sequence data found, skipping, prev"
                       << QDateTime::fromSecsSinceEpoch(previousRecBegin).toString("MM/dd/yyyy hh:mm:ss")
                       << "this event"
                       << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss");
            continue; // break;
        }

        // Check if record belongs in this session or a future session
        if (!inSession && rec_ts1 <= sinfo->end) {
            sess = sinfo->sess;         // this is the Session we want
            if (!inSession && sess) {
                OA = sess->AddEventList(CPAP_Obstructive, EVL_Event);
                H  = sess->AddEventList(CPAP_Hypopnea, EVL_Event);
                RE = sess->AddEventList(CPAP_RERA, EVL_Event);
                CA = sess->AddEventList(CPAP_ClearAirway, EVL_Event);
                PB = sess->AddEventList(CPAP_PB, EVL_Event);
                LL = sess->AddEventList(CPAP_LargeLeak, EVL_Event);
                EP = sess->AddEventList(CPAP_ExP, EVL_Event);
                SN = sess->AddEventList(INTP_SnoreFlag, EVL_Event);
                FL = sess->AddEventList(CPAP_FlowLimit, EVL_Event);

//                FLG = sess->AddEventList(CPAP_FLG, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));
//                SN = sess->AddEventList(CPAP_Snore, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, double(2000) / double(2));

                inSession = true;
            }
        }
        if (inSession) {
            qint64 duration = rec_ts2 - rec_ts1;
            // We make an ad hoc adjustment to the start time so that the event lines up better with the flow graph
            // TODO: We don't know what is really going on here. Is it sloppiness on the part of the DV6 in recording time stamps?
//            qint64 ti = qint64(rec_ts1 - (duration/2)) * 1000L;
            qint64 ti = qint64(rec_ts1 - duration) * 1000L;
            if (duration < 0) {
                qDebug() << "E.BIN at" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss")
                         << "reports duration of" << duration
                         << "ending" <<  QDateTime::fromSecsSinceEpoch(rec_ts2).toString("MM/dd/yyyy hh:mm:ss");
            }
            int code = rec->event_type;
/***
            //////////////////////////////////////////////////////////////////
            // Show Snore Events as a graph
            //////////////////////////////////////////////////////////////////
            if (code == 9) {
                qint16 severity = rec->event_severity;
                SN->AddWaveform(ti, &severity, 1, duration*1000);
            }

            //////////////////////////////////////////////////////////////////
            // Show Flow Limit Events as a graph
            //////////////////////////////////////////////////////////////////
            if (code == 10) {
                qint16 severity = rec->event_severity;
                FLG->AddWaveform(ti, &severity, 1, duration*1000);
            }
***/
            if (rec->event_severity >= 3)
                switch (code) {
                case 1:
                    CA->AddEvent(ti, duration);
                    break;
                case 2:
                    OA->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - OA" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 4:
                    H->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - H" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 5:
                    RE->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - RERA" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 8:     // snore
                    SN->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - Snore" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 9:     // expiratory puff
                    EP->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - exhale puff" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 10:    // flow limitation
                    FL->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - flow limit" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 11:    // periodic breathing
                    PB->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - periodic breathing" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 12:    // large leaks
                    LL->AddEvent(ti, duration);
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - large leak" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "ti" << QDateTime::fromSecsSinceEpoch(ti/1000).toString("hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                case 13:    // pressure change
                    break;
                case 14:    // start of session
#ifdef DEBUGDV6
                    qDebug() << "E.BIN - session start" << QDateTime::fromSecsSinceEpoch(rec_ts1).toString("MM/dd/yyyy hh:mm:ss") << "duration" << duration << "r" << rf.recnum();
#endif
                    break;
                default:
                    break;
                }
        }
    } while (true);

    rf.close();
    qDebug() << "DV6 E.BIN processed" << rf.numread() << "records";

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Finalize data and add to database
////////////////////////////////////////////////////////////////////////////////////////

int addSessions() {

    for (auto si=SessionData.begin(), end=SessionData.end(); si != end; ++si) {
        Session * sess = si.value().sess;

        if (sess) {
            if ( ! mach->AddSession(sess) ) {
                qWarning() << "Session" << sess->session() << "was not addded";
            }
#ifdef DEBUG6
            else
                qDebug() << "Added session" << sess->session()  << QDateTime::fromSecsSinceEpoch(sess->session()).toString("MM/dd/yyyy hh:mm:ss");;
#endif

            // Update indexes, process waveform and perform flagging
            sess->UpdateSummaries();

            // Save is not threadsafe
            sess->Store(mach->getDataPath());

            // Unload them from memory
            sess->TrashEvents();
        } // else
          //  qWarning() << "addSessions: session pointer is null";
    }

    return SessionData.size();

}

////////////////////////////////////////////////////////////////////////////////////////
// Create backup of input files
// Create dated backup of settings file if changed
////////////////////////////////////////////////////////////////////////////////////////

bool backup6 (const QString & path)  {

    if (rebuild_from_backups || !create_backups)
        return true;

    QDir ipath(path);
    QDir cpath(card_path);
    QDir bpath(backup_path);
    QDir hpath(history_path);

    // Copy input data to backup location
    copyPath(ipath.absolutePath(), bpath.absolutePath(), true);

    // Create archive of settings file if needed (SET.BIN)
    bool backup_settings = true;

    QStringList filters;

    QFile settingsFile;
    QString inputFile = cpath.absolutePath() + "/SET.BIN";
    settingsFile.setFileName(inputFile);

    filters << "SET_*.BIN";
    hpath.setNameFilters(filters);
    hpath.setFilter(QDir::Files);
    hpath.setSorting(QDir::Name | QDir::Reversed);
    QStringList fileNames = hpath.entryList(); // Get list of files
    if (! fileNames.isEmpty()) {
        QString lastFile = fileNames.first();
        qDebug() << "last settings file is" << lastFile << "new file is" << settingsFile.fileName();
        QByteArray newMD5 = fileChecksum(settingsFile.fileName(), QCryptographicHash::Md5);
        QByteArray oldMD5 = fileChecksum(hpath.absolutePath()+"/"+lastFile, QCryptographicHash::Md5);
        if (newMD5 == oldMD5)
            backup_settings = false;
    }

    if (backup_settings && !DailySummaries.isEmpty()) {
        DV6_S_Data ds = DailySummaries.last();
        QString newFile = hpath.absolutePath() + "/SET_" + getNominalDate(ds.start_time).toString("yyyyMMdd") + ".BIN";
        if (!settingsFile.copy(inputFile, newFile)) {
            qWarning() << "DV6 backup could not copy" << inputFile << "to" << newFile << ", error code" << settingsFile.error() << settingsFile.errorString();
        }
    }

    // We're done!
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Initialize DV6 environment
////////////////////////////////////////////////////////////////////////////////////////

bool init6Environment (const QString & path) {

    // Create device database record if it doesn't exist already
    mach = p_profile->CreateMachine(info);
    if (mach == nullptr) {
        qWarning() << "Could not create DV6 device data structure";
        return false;
    }

    backup_path = mach->getBackupPath();
    history_path = backup_path + "/HISTORY";
    rebuild_path = backup_path + "/DV6";

    // Compare QDirs rather than QStrings because separators may be different, especially on Windows.
    QDir ipath(path);
    QDir bpath(backup_path);
    QDir hpath(history_path);

    if (ipath == bpath) {
        // Don't create backups if importing from backup folder
        rebuild_from_backups = true;
        create_backups = false;
    } else {
        rebuild_from_backups = false;
        create_backups = p_profile->session->backupCardData();

        if ( ! bpath.exists()) {
            if ( ! bpath.mkpath(backup_path) ) {
                qWarning() << "Could not create DV6 backup directory" << backup_path;
                return false;
            }
        }
        if ( ! hpath.exists()) {
            if ( ! hpath.mkpath(history_path) ) {
                qWarning() << "Could not create DV6 backup HISTORY directory" << history_path;
                return false;
            }
        }

    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Open a DV6 SD card, parse everything, add to OSCAR database
////////////////////////////////////////////////////////////////////////////////////////

int IntellipapLoader::OpenDV6(const QString & path)
{
    qDebug() << "DV6 loader started";
    card_path = path + DV6_DIR;

    emit updateMessage(QObject::tr("Getting Ready..."));
    emit setProgressValue(0);
    QCoreApplication::processEvents();

    // 1. Prime the device database's info field with this device
    info = newInfo();

    // 2. VER.BIN - Parse model number, serial, etc. into info structure
    if (!load6VersionInfo(card_path))
        return -1;

    // 3. Initialize rest of the DV6 loader environment
    if (!init6Environment (path))
        return -1;

    // 4. SET.BIN - Parse settings file (which is only the latest settings)
    if (!load6Settings(card_path))
        return -1;

    // 5. S.BIN - Open and parse day summary list and create a list of days
    if (!load6DailySummaries())
        return -1;

    emit updateMessage(QObject::tr("Backing up files..."));
    QCoreApplication::processEvents();

    // 6. Back up data files (must do after parsing VER.BIN, S.BIN, and creating device)
    if (!backup6(path))
        return -1;

    emit updateMessage(QObject::tr("Reading data files..."));
    QCoreApplication::processEvents();

    // 7. U.BIN - Open and parse session list and create a list of session times
    // (S.BIN must already be loaded)
    if (!load6Sessions())
        return -1;

    // Create OSCAR session list from session times and summary data
    if (create6Sessions() <= 0)
        return -1;

    // R.BIN - Open and parse flow data
    if (!load6HighResData())
        return -1;

    // L.BIN - Open and parse per minute data
    if (!load6PerMinute())
        return -1;

    // E.BIN - Open and parse event data
    if (!load6EventData())
        return -1;

    emit updateMessage(QObject::tr("Finishing up..."));
    QCoreApplication::processEvents();

    // Finalize input
    return addSessions();
}

int IntellipapLoader::Open(const QString & dirpath)
{
    // Check for SL directory
    // Check for DV5MFirm.bin?
    QString path(dirpath);
    path = path.replace("\\", "/");

    if (path.endsWith(SL_DIR)) {
        path.chop(3);
    } else if (path.endsWith(DV6_DIR)) {
        path.chop(4);
    }

    QDir dir;

    int r = -1;
    // Sometimes there can be an SL folder because SmartLink dumps an old DV5 firmware in it, so check it first
    if (dir.exists(path + SL_DIR))
        r = OpenDV5(path);

    if ((r<0) && dir.exists(path + DV6_DIR))
        r = OpenDV6(path);

    return r;
}

void IntellipapLoader::initChannels()
{
    using namespace schema;
    Channel * chan = nullptr;
    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlexMode = 0x1165, SETTING,  MT_CPAP,  SESSION,
        "INTPSmartFlexMode", QObject::tr("SmartFlex Mode"),
        QObject::tr("Intellipap pressure relief mode."),
        QObject::tr("SmartFlex Mode"),
        "", DEFAULT, Qt::green));


    chan->addOption(0, STR_TR_Off);
    chan->addOption(1, QObject::tr("Ramp Only"));
    chan->addOption(2, QObject::tr("Full Time"));

    channel.add(GRP_CPAP, chan = new Channel(INTP_SmartFlexLevel = 0x1169, SETTING,  MT_CPAP,  SESSION,
        "INTPSmartFlexLevel", QObject::tr("SmartFlex Level"),
        QObject::tr("Intellipap pressure relief level."),
        QObject::tr("SmartFlex Level"),
        "", DEFAULT, Qt::green));

    channel.add(GRP_CPAP, new Channel(INTP_SnoreFlag = 0xe301, FLAG,  MT_CPAP,   SESSION,
        "INTP_SnoreFlag",
        QObject::tr("Snore"),
        QObject::tr("Snoring event."),
        QObject::tr("SN"),
        STR_UNIT_EventsPerHour,    DEFAULT,    QColor("#e20004")));

}

bool intellipap_initialized = false;
void IntellipapLoader::Register()
{
    if (!intellipap_initialized) {
        qDebug() << "Registering IntellipapLoader";
        RegisterLoader(new IntellipapLoader());
        //InitModelMap();
        intellipap_initialized = true;
    }
    return;
}
