/* PRS1 Parsing for S/T and AVAPS ventilators (Family 3)
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Portions copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1_parser.h"
#include "prs1_loader.h"

// The qt5.15 obsolescence of hex requires this change.
// this solution to QT's obsolescence is only used in debug statements
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    #define QTHEX     Qt::hex
    #define QTDEC     Qt::dec
#else
    #define QTHEX     hex
    #define QTDEC     dec
#endif

static QString hex(int i)
{
    return QString("0x") + QString::number(i, 16).toUpper();
}

//********************************************************************************************
// MARK: -
// MARK: 50 and 60 Series

// borrowed largely from ParseSummaryF5V012
bool PRS1DataChunk::ParseSummaryF3V03(void)
{
    if (this->family != 3 || (this->familyVersion > 3)) {
        qWarning() << "ParseSummaryF3V03 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    QVector<int> minimum_sizes;
    if (this->familyVersion == 0) {
        minimum_sizes = { 0x19, 3, 3, 9 };
    } else {
        minimum_sizes = { 0x1b, 3, 5, 9 };
    }
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.

    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F3V6.
        size = 0;
        if (code < minimum_sizes.length()) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        // NOTE: F3V3 doesn't use 16-bit time deltas in its summary events, it uses absolute timestamps!
        // It's possible that these are 24-bit, but haven't yet seen a timestamp that large.
        
        const unsigned char * ondata = data;
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                if (this->familyVersion == 0) {
                    // F3V0 inserts an extra byte in front
                    CHECK_VALUE(data[pos], 1);
                    ondata = ondata + 1;
                }
                CHECK_VALUE(ondata[pos], 0);
                /*
                CHECK_VALUE(data[pos] & 0xF0, 0);  // TODO: what are these?
                if ((data[pos] & 0x0F) != 1) {  // This is the most frequent value.
                    //CHECK_VALUES(data[pos] & 0x0F, 3, 5);  // TODO: what are these? 0 seems to be related to errors.
                }
                */
            // F3V3 doesn't have a separate settings record like F3V6 does, the settings just follow the EquipmentOn data.
                ok = this->ParseSettingsF3V03(ondata, size);
                break;
            case 2:  // Mask On
                tt = data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                CHECK_VALUE(data[pos+2], 0);  // may be high byte of timestamp
                if (size > 3) {  // F3V3 records the humidifier setting at each mask-on, F3V0 only records the initial setting.
                    this->ParseHumidifierSettingF3V3(data[pos+3], data[pos+4]);
                }
                break;
            case 3:  // Mask Off
                tt = data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
            // F3V3 doesn't have a separate stats record like F3V6 does, the stats just follow the MaskOff data.
                CHECK_VALUE(data[pos+0x2], 0);  // may be high byte of timestamp
                //CHECK_VALUES(data[pos+0x3], 0, 1);  // OA count, probably 16-bit
                CHECK_VALUE(data[pos+0x4], 0);
                //CHECK_VALUE(data[pos+0x5], 0);  // CA count, probably 16-bit
                CHECK_VALUE(data[pos+0x6], 0);
                //CHECK_VALUE(data[pos+0x7], 0);  // H count, probably 16-bit
                CHECK_VALUE(data[pos+0x8], 0);
                break;
            case 1:  // Equipment Off
                tt = data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                CHECK_VALUE(data[pos+2], 0);  // may be high byte of timestamp
                break;
            /*
            case 5:  // Clock adjustment? See ParseSummaryF0V4.
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 5);  // and the only record in the session.
                if (false) {
                    long value = data[pos] | data[pos+1]<<8 | data[pos+2]<<16 | data[pos+3]<<24;
                    qDebug() << this->sessionid << "clock changing from" << ts(value * 1000L)
                                                << "to" << ts(this->timestamp * 1000L)
                                                << "delta:" << (this->timestamp - value);
                }
                break;
            case 6:  // Cleared?
                // Appears in the very first session when that session number is > 1.
                // Presumably previous sessions were cleared out.
                // TODO: add an internal event for this.
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 1);  // and the only record in the session.
                if (this->sessionid == 1) UNEXPECTED_VALUE(this->sessionid, ">1");
                break;
            case 7:  // ???
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                break;
            case 8:  // ???
                tt += data[pos] | (data[pos+1] << 8);  // Since 7 and 8 seem to occur near each other, let's assume 8 also has a timestamp
                CHECK_VALUE(pos, 1);
                CHECK_VALUE(chunk_size, 3);
                CHECK_VALUE(data[pos], 0);  // and alert us if the timestamp is nonzero
                CHECK_VALUE(data[pos+1], 0);
                break;
            case 9:  // Humidifier setting change
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
            */
            default:
                UNEXPECTED_VALUE(code, "known slice code");
                ok = false;  // unlike F0V6, we don't know the size of unknown slices, so we can't recover
                break;
        }
        pos += size;
    }

    if (ok && pos != chunk_size) {
        qWarning() << this->sessionid << (this->size() - pos) << "trailing bytes";
    }

    this->duration = tt;

    return ok;
}


// Support for 1061, 1061T, 1160P
// logic largely borrowed from ParseSettingsF3V6, values based on sample data
bool PRS1DataChunk::ParseSettingsF3V03(const unsigned char* data, int /*size*/)
{
    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;
    FlexMode flexmode = FLEX_Unknown;

    // data[0] is the event code
    // data[1] is checked in the calling function

    switch (data[2]) {
        case 0: cpapmode = PRS1_MODE_CPAP; break;  // "CPAP" mode
        case 1: cpapmode = PRS1_MODE_S; break;   // "S" mode
        case 2: cpapmode = PRS1_MODE_ST; break;  // "S/T" mode; pressure seems variable?
        case 4: cpapmode = PRS1_MODE_PC; break;  // "PC" mode? Usually "PC - AVAPS", see setting 1 below
        default:
            UNEXPECTED_VALUE(data[2], "known device mode");
            break;
    }

    switch (data[3]) {
        case 0:  // 0 = None
            switch (cpapmode) {
                case PRS1_MODE_CPAP: flexmode = FLEX_None; break;
                case PRS1_MODE_S:    flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                case PRS1_MODE_ST:   flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                default:
                    UNEXPECTED_VALUE(cpapmode, "CPAP, S, or S/T");
                    break;
            }
            break;
        case 1:  // 1 = Bi-Flex, only seen with "S - Bi-Flex"
            flexmode = FLEX_BiFlex;
            CHECK_VALUE(cpapmode, PRS1_MODE_S);
            break;
        case 2:  // 2 = AVAPS: usually "PC - AVAPS", sometimes "S/T - AVAPS"
            switch (cpapmode) {
                case PRS1_MODE_ST: cpapmode = PRS1_MODE_ST_AVAPS; break;
                case PRS1_MODE_PC: cpapmode = PRS1_MODE_PC_AVAPS; break;
                default:
                    UNEXPECTED_VALUE(cpapmode, "S/T or PC");
                    break;
            }
            flexmode = FLEX_RiseTime;  // reports say "AVAPS" but then list a rise time setting
            break;
        default:
            UNEXPECTED_VALUE(data[3], "known flex mode");
            break;
    }
    if (this->familyVersion == 0) {
        // Confirm F3V0 setting encoding
        switch (cpapmode) {
        case PRS1_MODE_CPAP: break;  // CPAP has been confirmed
        case PRS1_MODE_S: break;     // S bi-flex and rise time have been confirmed
        case PRS1_MODE_ST:
            CHECK_VALUE(flexmode, FLEX_RiseTime);  // only rise time has been confirmed
            break;
        default:
            UNEXPECTED_VALUE(cpapmode, "tested modes");
        }
    }
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, (int) flexmode));

    int epap     = data[4] + (data[5] << 8);   // 0x82 = EPAP 13 cmH2O; 0x78 = EPAP 12 cmH2O; 0x50 = EPAP 8 cmH2O
    int min_ipap = data[6] + (data[7] << 8);   // 0xA0 = IPAP 16 cmH2O; 0xBE = 19 cmH2O min IPAP (in AVAPS); 0x78 = IPAP 12 cmH2O
    int max_ipap = data[8] + (data[9] << 8);   // 0xAA = ???;          0x12C = 30 cmH2O max IPAP (in AVAPS); 0x78 = ???
    switch (cpapmode) {
        case PRS1_MODE_CPAP:
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, epap));
            CHECK_VALUE(min_ipap, 0);
            CHECK_VALUE(max_ipap, 0);
            break;
        case PRS1_MODE_S:
        case PRS1_MODE_ST:
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, epap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, min_ipap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, min_ipap - epap));
            //CHECK_VALUES(max_ipap, 170, 300);
            break;
        case PRS1_MODE_ST_AVAPS:
        case PRS1_MODE_PC_AVAPS:
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, epap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_ipap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_ipap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ipap - epap));
            this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, max_ipap - epap));
            break;
        default:
            UNEXPECTED_VALUE(cpapmode, "expected mode");
            break;
    }

    if (cpapmode == PRS1_MODE_CPAP) {
        CHECK_VALUE(flexmode, FLEX_None);
        CHECK_VALUE(data[0xa], 0);
        CHECK_VALUE(data[0xb], 0);
        CHECK_VALUE(data[0xc], 0);
        CHECK_VALUE(data[0xd], 0);
    }
    if (flexmode == FLEX_RiseTime) {
        int rise_time = data[0xa];  // 1 = Rise Time Setting 1, 2 = Rise Time Setting 2, 3 = Rise Time Setting 3
        if (rise_time < 1 || rise_time > 6) UNEXPECTED_VALUE(rise_time, "1-6");  // TODO: what is 0?
        CHECK_VALUES(data[0xb], 0, 1);  // 1 = Rise Time Lock (in "None" and AVAPS flex mode)
        CHECK_VALUE(data[0xc], 0);
        CHECK_VALUES(data[0xd], 0, 1);  // TODO: What is this? It's usually 0.
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME, rise_time));
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME_LOCK, data[0xb] == 1));
    } else if (flexmode == FLEX_BiFlex) {
        CHECK_VALUES(data[0xa], 2, 3);  // TODO: May also be Bi-Flex level? But how is this different from [0xc] below?
        CHECK_VALUES(data[0xb], 0, 1);  // TODO: What is this? It doesn't always match [0xd].
        CHECK_VALUES(data[0xc], 2, 3);
        CHECK_VALUE(data[0x0a], data[0xc]);
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[0xc]));  // 3 = Bi-Flex 3, 2 = Bi-Flex 2 (in bi-flex mode)
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[0xd] == 1));
    }

    if (flexmode == FLEX_None) CHECK_VALUE(data[0xe], 0);
    if (cpapmode == PRS1_MODE_ST_AVAPS || cpapmode == PRS1_MODE_PC_AVAPS) {
        if (data[0xe] < 24 || data[0xe] > 65) UNEXPECTED_VALUE(data[0xe], "24-65");
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TIDAL_VOLUME, data[0xe] * 10.0));
    } else if (flexmode == FLEX_BiFlex || flexmode == FLEX_RiseTime) {
        CHECK_VALUE(data[0xe], 0x14);  // 0x14 = ???
    }


    int breath_rate = data[0xf];
    int timed_inspiration = data[0x10];
    bool backup = false;
    switch (cpapmode) {
        case PRS1_MODE_CPAP:
            CHECK_VALUE(breath_rate, 0);
            CHECK_VALUE(timed_inspiration, 0);
            break;
        case PRS1_MODE_S:
            if (this->familyVersion == 0) {
                CHECK_VALUE(breath_rate, 10);
                CHECK_VALUE(timed_inspiration, 10);
            } else {
                CHECK_VALUE(breath_rate, 0);
                CHECK_VALUE(timed_inspiration, 0);
            }
            break;
        case PRS1_MODE_PC_AVAPS:
            CHECK_VALUE(breath_rate, 0);  // only ever seen 0 on reports so far
            CHECK_VALUE(timed_inspiration, 30);
            backup = true;
            break;
        case PRS1_MODE_ST_AVAPS:
            if (breath_rate) {  // can be 0 on reports
                CHECK_VALUES(breath_rate, 9, 10);
            }
            if (timed_inspiration < 10 || timed_inspiration > 30) UNEXPECTED_VALUE(timed_inspiration, "10-30");
            backup = true;
            break;
        case PRS1_MODE_ST:
            if (breath_rate < 8 || breath_rate > 18) UNEXPECTED_VALUE(breath_rate, "8-18");  // can this be 0?
            if (timed_inspiration < 10 || timed_inspiration > 20) UNEXPECTED_VALUE(timed_inspiration, "10-20");  // 16 = 1.6s
            backup = true;
            break;
        default:
            UNEXPECTED_VALUE(cpapmode, "CPAP, S, S/T, or PC");
            break;
    }
    if (backup) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Fixed));
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_RATE, breath_rate));
        this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_BACKUP_TIMED_INSPIRATION, timed_inspiration, 0.1));
    }

    CHECK_VALUE(data[0x11], 0);

    //CHECK_VALUE(data[0x12], 0x1E, 0x0F);  // 0x1E = ramp time 30 minutes, 0x0F = ramp time 15 minutes
    //CHECK_VALUE(data[0x13], 0x3C, 0x5A, 0x28);  // 0x3C = ramp pressure 6 cmH2O, 0x28 = ramp pressure 4 cmH2O, 0x5A = ramp pressure 9 cmH2O
    CHECK_VALUE(data[0x14], 0);  // the ramp pressure is probably a 16-bit value like the ones above are
    int ramp_time = data[0x12];
    int ramp_pressure = data[0x13];
    if (ramp_time > 0) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, ramp_time));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, ramp_pressure));
    }

    int pos;
    if (this->familyVersion == 0) {
        ParseHumidifierSetting50Series(data[0x15], true);
        pos = 0x16;
    } else {
        this->ParseHumidifierSettingF3V3(data[0x15], data[0x16], true);

        // Menu options?
        CHECK_VALUES(data[0x17], 0x10, 0x90);  // 0x10 = resist 1; 0x90 = resist 1, resist lock
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, (data[0x17] & 0x80) != 0));
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, 1));  // only value seen so far, CHECK_VALUES above will flag any others
        
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, (data[0x18] & 0x80) != 0));
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, (data[0x18] & 0x7f)));
        CHECK_VALUES(data[0x18] & 0x7f, 22, 15);  // 0x16 = tubing 22; 0x0F = tubing 15, 0x96 = tubing 22 with lock
        pos = 0x19;
    }
    
    // Alarms?
    if (this->familyVersion == 0) {
        if (data[pos] != 0) {
            CHECK_VALUES(data[pos], 10, 30);  // Apnea alarm on F3V0
        }
        CHECK_VALUES(data[pos+1], 0, 15);  // Disconnect alarm on F3V0
        CHECK_VALUES(data[pos+2], 0, 17);  // Low MV alarm on F3V0
    } else {
        CHECK_VALUE(data[pos], 0);
        CHECK_VALUE(data[pos+1], 0);
        CHECK_VALUE(data[pos+2], 0);
    }
    return true;
}


// XX XX = F3V3 Humidifier bytes
// 43 15 = heated tube temp 5, humidity 2
// 43 14 = heated tube temp 4, humidity 2
// 63 13 = heated tube temp 3, humidity 3
// 63 11 = heated tube temp 1, humidity 3
// 45 08 = system one 5
// 44 08 = system one 4
// 43 08 = system one 3
// 42 08 = system one 2
// 41 08 = system one 1
// 40 08 = system one 0 (off)
// 40 60 = system one 3, no data
// 40 20 = system one 3, no data
// 40 90 = heated tube, tube off, data=tube t=0,h=0
// 45 80 = classic 5
// 44 80 = classic 4
// 43 80 = classic 3
// 42 80 = classic 2

// 40 80 = classic 0 (off)
//
//  7    = humidity level without tube
//  8    = ? (never seen)
// 1     = ? (never seen)
// 6     = heated tube humidity level (when tube present, 0x40 all other times? including when tube is off?)
// 8     = ? (never seen)
//     7 = tube temp
//     8 = "System One" mode
//    1  = tube present
//    6  = no data, seems to show system one 3 in settings
//    8  = (classic mode; also seen when heated tube present but off, possibly ignored in that case)
//
// Note that, while containing similar fields as ParseHumidifierSetting60Series, the bit arrangement is different for F3V3!

void PRS1DataChunk::ParseHumidifierSettingF3V3(unsigned char humid1, unsigned char humid2, bool add_setting)
{
    if (false) qWarning() << this->sessionid << "humid" << hex(humid1) << hex(humid2) << add_setting;

    int humidlevel = humid1 & 7;  // Ignored when heated tube is present: humidifier setting on tube disconnect is always reported as 3
    if (humidlevel > 5) UNEXPECTED_VALUE(humidlevel, "<= 5");
    CHECK_VALUE(humid1 & 0x40, 0x40);  // seems always set, even without heated tube
    CHECK_VALUE(humid1 & 0x98, 0);  // never seen
    int tubehumidlevel = (humid1 >> 5) & 7;  // This mask is a best guess based on other masks.
    if (tubehumidlevel > 5) UNEXPECTED_VALUE(tubehumidlevel, "<= 5");
    CHECK_VALUE(tubehumidlevel & 4, 0);  // never seen, but would clarify whether above mask is correct

    int tubetemp = humid2 & 7;
    if (tubetemp > 5) UNEXPECTED_VALUE(tubetemp, "<= 5");

    if (humid2 & 0x60) {
        CHECK_VALUES(humid2 & 0x60, 0x20, 0x60);  // no humidifier data on chart
    }
    bool humidclassic = (humid2 & 0x80) != 0;  // Set on classic mode reports; evidently ignored (sometimes set!) when tube is present
    //bool no_tube? = (humid2 & 0x20) != 0;  // Something tube related: whenever it is set, tube is never present (inverse is not true)
    bool no_data = (humid2 & 0x60) != 0;  // As described in chart, settings still show up
    int tubepresent = (humid2 & 0x10) != 0;
    bool humidsystemone = (humid2 & 0x08) != 0;  // Set on "System One" humidification mode reports when tubepresent is false

    if (humidsystemone + tubepresent + no_data == 0) CHECK_VALUE(humidclassic, true);  // Always set when everything else is off in F0V4
    if (humidsystemone + tubepresent /*+ no_data*/ > 1) UNEXPECTED_VALUE(humid2, "one bit set");  // Only one of these ever seems to be set at a time
    //if (tubepresent && tubetemp == 0) CHECK_VALUE(tubehumidlevel, 0);  // When the heated tube is off, tube humidity seems to be 0 in F0V4, but not F3V3

    if (tubepresent) humidclassic = false;  // Classic mode bit is evidently ignored when tube is present

    //qWarning() << this->sessionid << (humidclassic ? "C" : ".") << (humid2 & 0x20 ? "?" : ".") << (tubepresent ? "T" : ".") << (no_data ? "X" : ".") << (humidsystemone ? "1" : ".");
    /*
    if (tubepresent) {
        if (tubetemp) {
            qWarning() << this->sessionid << "tube temp" << tubetemp << "tube humidity" << tubehumidlevel << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        } else {
            qWarning() << this->sessionid << "heated tube off" << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
        }
    } else {
        qWarning() << this->sessionid << (humidclassic ? "classic" : "systemone") << "humidity" << humidlevel;
    }
    */
    HumidMode humidmode = HUMID_Fixed;
    if (tubepresent) {
        humidmode = HUMID_HeatedTube;
    } else {
        if (humidsystemone + humidclassic > 1) UNEXPECTED_VALUE(humid2, "fixed or adaptive");
        if (humidsystemone) humidmode = HUMID_Adaptive;
    }

    if (add_setting) {
        bool humidifier_present = (no_data == 0);
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_STATUS, humidifier_present));
        if (humidifier_present) {
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_MODE, humidmode));
            if (humidmode == HUMID_HeatedTube) {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HEATED_TUBE_TEMP, tubetemp));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, tubehumidlevel));
            } else {
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HUMID_LEVEL, humidlevel));
            }
        }
    }
    
    // Check for previously unseen data that we expect to be normal:
    if (humidclassic && humidlevel == 1) UNEXPECTED_VALUE(humidlevel, "!= 1");
    if (tubepresent) {
        if (tubetemp) CHECK_VALUES(tubehumidlevel, 2, 3);
        if (tubetemp == 2) UNEXPECTED_VALUE(tubetemp, "!= 2");
    }
}


const QVector<PRS1ParsedEventType> ParsedEventsF3V0 = {
    PRS1IPAPAverageEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1FlowRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    // No LEAK, unlike F3V3
    PRS1HypopneaCount::TYPE,
    PRS1ClearAirwayCount::TYPE,  // TODO
    PRS1ObstructiveApneaCount::TYPE,  // TODO
    // No PP, FL, VS, RERA, PB, LL
    // No TB
};

const QVector<PRS1ParsedEventType> ParsedEventsF3V3 = {
    PRS1IPAPAverageEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1FlowRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1LeakEvent::TYPE,
    PRS1HypopneaCount::TYPE,
    PRS1ClearAirwayCount::TYPE,
    PRS1ObstructiveApneaCount::TYPE,
    // No PP, FL, VS, RERA, PB, LL
    // No TB
};

// 1061, 1061T, 1160P series
bool PRS1DataChunk::ParseEventsF3V03(void)
{
    // NOTE: Older ventilators (BiPAP S/T and AVAPS) devices don't use timestamped events like everything else.
    // Instead, they use a fixed interval format like waveforms do (see PRS1_HTYPE_INTERVAL).

    if (this->family != 3 || (this->familyVersion != 0 && this->familyVersion != 3)) {
        qWarning() << "ParseEventsF3V03 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    if (this->fileVersion == 3) {
        // NOTE: The original comment in the header for ParseF3EventsV3 said there was a 1060P with fileVersion 3.
        // We've never seen that, so warn if it ever shows up.
        qWarning() << "F3V3 event file with fileVersion 3?";
    }
    
    int t = 0;
    static const int record_size = 0x10;
    int size = this->m_data.size()/record_size;
    CHECK_VALUE(this->m_data.size() % record_size, 0);
    unsigned char * h = (unsigned char *)this->m_data.data();

    static const qint64 block_duration = 120;

    // Make sure the assumptions here agree with the header
    CHECK_VALUE(this->htype, PRS1_HTYPE_INTERVAL);
    CHECK_VALUE(this->interval_count, size);
    CHECK_VALUE(this->interval_seconds, block_duration);
    for (auto & channel : this->waveformInfo) {
        CHECK_VALUE(channel.interleave, 1);
    }
    
    for (int x=0; x < size; x++) {
        // Use the timestamp of the end of this interval, to be consistent with other parsers,
        // but see note below regarding the duration of the final interval.
        t += block_duration;

        // TODO: The duration of the final interval isn't clearly defined in this format:
        // there appears to be no way (apart from looking at the summary or waveform data)
        // to determine the end time, which may truncate the last interval.
        //
        // TODO: What if there are multiple "final" intervals in a session due to multiple
        // mask-on slices?
        this->AddEvent(new PRS1IPAPAverageEvent(t, h[0] | (h[1] << 8)));
        this->AddEvent(new PRS1EPAPAverageEvent(t, h[2] | (h[3] << 8)));
        this->AddEvent(new PRS1TotalLeakEvent(t, h[4]));
        this->AddEvent(new PRS1TidalVolumeEvent(t, h[5]));
        this->AddEvent(new PRS1FlowRateEvent(t, h[6]));
        this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, h[7]));
        this->AddEvent(new PRS1RespiratoryRateEvent(t, h[8]));
        if (this->familyVersion == 0) {
            if (h[9] < 4 || h[9] > 65) UNEXPECTED_VALUE(h[9], "4-65");
        } else {
            if (h[9] < 4 || h[9] > 84) UNEXPECTED_VALUE(h[9], "5-84");  // not sure what this is.. encore doesn't graph it.
        }
        if (this->familyVersion == 0) {
            // 1 shows as Apnea (AP) alarm
            // 2 shows as a Patient Disconnect (PD) alarm
            // 4 shows as a Low Minute Vent (LMV) alarm
            // 8 shows as a Low Pressure (LP) alarm
            // 10 shows as PD + LP in the same interval
            if (h[10] & ~(0x01 | 0x02 | 0x04 | 0x08)) UNEXPECTED_VALUE(h[10], "known bits");
        } else {
            // This is probably the same as F3V0, but we don't yet have the sample data to confirm.
            CHECK_VALUES(h[10], 0, 8);  // 8 shows as a Low Pressure (LP) alarm
        }
        this->AddEvent(new PRS1MinuteVentilationEvent(t, h[11]));
        if (this->familyVersion == 0) {
            CHECK_VALUE(h[12], 0);
            this->AddEvent(new PRS1HypopneaCount(t, h[13]));          // count of hypopnea events
            this->AddEvent(new PRS1ClearAirwayCount(t, h[14]));       // count of clear airway events
            this->AddEvent(new PRS1ObstructiveApneaCount(t, h[15]));  // count of obstructive events
        } else {
            this->AddEvent(new PRS1HypopneaCount(t, h[12]));          // count of hypopnea events
            this->AddEvent(new PRS1ClearAirwayCount(t, h[13]));       // count of clear airway events
            this->AddEvent(new PRS1ObstructiveApneaCount(t, h[14]));  // count of obstructive events
            this->AddEvent(new PRS1LeakEvent(t, h[15]));
        }
        this->AddEvent(new PRS1IntervalBoundaryEvent(t));

        h += record_size;
    }
    
    this->duration = t;

    return true;
}


//********************************************************************************************
// MARK: -
// MARK: DreamStation

// Originally based on ParseSummaryF5V3, with changes observed in ventilator sample data
//
// TODO: surely there will be a way to merge ParseSummary (FV3) loops and abstract the device-specific
// encodings into another function or class, but that's probably worth pursuing only after
// the details have been figured out.
bool PRS1DataChunk::ParseSummaryF3V6(void)
{
    if (this->family != 3 || this->familyVersion != 6) {
        qWarning() << "ParseSummaryF3V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 1, 0x25, 9, 7, 4, 2, 1, 2, 2, 1, 0x18, 2, 4 };  // F5V3 = { 1, 0x38, 4, 2, 4, 0x1e, 2, 4, 9 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: The sizes contained in hblock can vary, even within a single device, as can the length of hblock itself!

    // TODO: hardcoding this is ugly, think of a better approach
    if (chunk_size < minimum_sizes[0] + minimum_sizes[1] + minimum_sizes[2]) {
        qWarning() << this->sessionid << "summary data too short:" << chunk_size;
        return false;
    }
    // We've once seen a short summary with no mask-on/off: just equipment-on, settings, 2, equipment-off
    // (And we've seen something similar in F5V3.)
    if (chunk_size < 58) UNEXPECTED_VALUE(chunk_size, ">= 58");

    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        if (!this->hblock.contains(code)) {
            qWarning() << this->sessionid << "missing hblock entry for" << code;
            ok = false;
            break;
        }
        size = this->hblock[code];
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            if (size < minimum_sizes[code]) {
                UNEXPECTED_VALUE(size, minimum_sizes[code]);
                qWarning() << this->sessionid << "slice" << code << "too small" << size << "<" << minimum_sizes[code];
                if (code != 1) {  // Settings are variable-length, so shorter settings slices aren't fatal.
                    ok = false;
                    break;
                }
            }
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }

        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first?
                //CHECK_VALUE(data[pos], 0x10);  // usually 0x10 for 1030X, sometimes 0x40 or 0x80 are set in addition or instead
                CHECK_VALUE(size, 1);
                break;
            case 1:  // Settings
                ok = this->ParseSettingsF3V6(data + pos, size);
                break;
            case 2:  // seems equivalent to F5V3 #9, comes right after settings, usually 9 bytes, identical values
                // TODO: This may be structurally similar to settings: a list of (code, length, value).
                CHECK_VALUE(data[pos], 0);
                CHECK_VALUE(data[pos+1], 1);
                //CHECK_VALUE(data[pos+2], 0);  // Apnea Alarm (0=off, 1=10, 2=20)
                if (data[pos+2] != 0) {
                    CHECK_VALUES(data[pos+2], 1, 2);
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_APNEA_ALARM, data[pos+2] * 10));
                }
                CHECK_VALUE(data[pos+3], 1);
                CHECK_VALUE(data[pos+4], 1);
                CHECK_VALUES(data[pos+5], 0, 1);  // 1 = Low Minute Ventilation Alarm set to 1
                CHECK_VALUE(data[pos+6], 2);
                CHECK_VALUE(data[pos+7], 1);
                CHECK_VALUE(data[pos+8], 0);  // 1 = patient disconnect alarm of 15 sec on F5V3, not sure where time is encoded
                if (size > 9) {
                    CHECK_VALUE(data[pos+9],  3);
                    CHECK_VALUE(data[pos+10], 1);
                    CHECK_VALUE(data[pos+11], 0);
                    CHECK_VALUE(size, 12);
                }
                break;
            case 4:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 5:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                break;
            case 6:  // Ventilator CPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x3C);  // Average CPAP
                break;
            case 7:  // Ventilator EPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x69);  // Average EPAP
                //CHECK_VALUE(data[pos+1], 0x80);  // Average 90% EPAP
                break;
            case 8:  // Ventilator IPAP stats, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x86);  // Average IPAP
                //CHECK_VALUE(data[pos+1], 0xA8);  // Average 90% IPAP
                break;
            case 0xa:  // Patient statistics, presumably per mask-on slice
                //CHECK_VALUE(data[pos], 0x00);  // 16-bit OA count
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUE(data[pos+2], 0x00);  // 16-bit CA count
                CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUE(data[pos+4], 0x00);  // 16-bit minutes in LL
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUE(data[pos+6], 0x0A);  // 16-bit VS count
                //CHECK_VALUE(data[pos+7], 0x00);  // We've actually seen someone with more than 255 VS in a night!
                //CHECK_VALUE(data[pos+8], 0x01);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+9], 0x00);
                //CHECK_VALUE(data[pos+0xa], 0x00);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+0xb], 0x00);
                //CHECK_VALUE(data[pos+0xc], 0x00);  // 16-bit RE count
                CHECK_VALUE(data[pos+0xd], 0x00);
                //CHECK_VALUE(data[pos+0xe], 0x3e);  // average total leak
                //CHECK_VALUE(data[pos+0xf], 0x03);  // 16-bit H count (partial)
                CHECK_VALUE(data[pos+0x10], 0x00);
                //CHECK_VALUE(data[pos+0x11], 0x11);  // average breath rate
                //CHECK_VALUE(data[pos+0x12], 0x41);  // average TV / 10
                //CHECK_VALUE(data[pos+0x13], 0x60);  // average % PTB
                //CHECK_VALUE(data[pos+0x14], 0x0b);  // average minute vent
                //CHECK_VALUE(data[pos+0x15], 0x1d);  // average leak? (similar position to F5V3, similar delta to total leak)
                //CHECK_VALUE(data[pos+0x16], 0x00);  // 16-bit minutes in PB
                CHECK_VALUE(data[pos+0x17], 0x00);
                break;
            case 3:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUES(data[pos+2], 1, 4);  // bitmask, have seen 1, 4, 6, 0x41
                //CHECK_VALUE(data[pos+3], 0x17);  // 0x16, etc.
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUE(data[pos+5], 0x15);  // 0x16, etc.
                //CHECK_VALUES(data[pos+6], 0, 1);  // or 2
                break;
            case 0xc:  // Humidier setting change
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            default:
                UNEXPECTED_VALUE(code, "known slice code");
                break;
        }
        pos += size;
    }

    this->duration = tt;

    return ok;
}


// Based initially on ParseSettingsF5V3. Many of the codes look the same, like always starting with 0, 0x35 looking like
// a humidifier setting, etc., but the contents are sometimes a bit different, such as mode values and pressure settings.
//
// new settings to find: ...
bool PRS1DataChunk::ParseSettingsF3V6(const unsigned char* data, int size)
{
    static const QMap<int,int> expected_lengths = { {0x1e,3}, {0x35,2} };
    bool ok = true;

    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;
    FlexMode flexmode = FLEX_Unknown;

    // F5V3 and F3V6 use a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: parameterize this somewhere better

    int fixed_pressure = 0;
    int fixed_epap = 0;
    int fixed_ipap = 0;
    int min_ipap = 0;
    int max_ipap = 0;
    int breath_rate;
    int timed_inspiration;

    // Parse the nested data structure which contains settings
    int pos = 0;
    do {
        int code = data[pos++];
        int len = data[pos++];

        int expected_len = 1;
        if (expected_lengths.contains(code)) {
            expected_len = expected_lengths[code];
        }
        //CHECK_VALUE(len, expected_len);
        if (len < expected_len) {
            qWarning() << this->sessionid << "setting" << code << "too small" << len << "<" << expected_len;
            ok = false;
            break;
        }
        if (pos + len > size) {
            qWarning() << this->sessionid << "setting" << code << "@" << pos << "longer than remaining slice";
            ok = false;
            break;
        }

        switch (code) {
            case 0: // Device Mode
                CHECK_VALUE(pos, 2);  // always first?
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                case 0: cpapmode = PRS1_MODE_CPAP; break; // "CPAP" mode
                case 1: cpapmode = PRS1_MODE_S; break;   // "S" mode
                case 2: cpapmode = PRS1_MODE_ST; break;  // "S/T" mode; pressure seems variable?
                case 4: cpapmode = PRS1_MODE_PC; break;  // "PC" mode? Usually "PC - AVAPS", see setting 1 below
                default:
                    UNEXPECTED_VALUE(data[pos], "known device mode");
                    break;
                }
                break;
            case 1: // Flex Mode
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                    case 0:  // 0 = None
                        switch (cpapmode) {
                            case PRS1_MODE_CPAP: flexmode = FLEX_None; break;
                            case PRS1_MODE_S:    flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                            case PRS1_MODE_ST:   flexmode = FLEX_RiseTime; break;  // reports say "None" but then list a rise time setting
                            default:
                                UNEXPECTED_VALUE(cpapmode, "CPAP, S, or S/T");
                                break;
                        }
                        break;
                    case 1:  // 1 = Bi-Flex, only seen with "S - Bi-Flex"
                        flexmode = FLEX_BiFlex;
                        CHECK_VALUE(cpapmode, PRS1_MODE_S);
                        break;
                    case 2:  // 2 = AVAPS: usually "PC - AVAPS", sometimes "S/T - AVAPS"
                        switch (cpapmode) {
                            case PRS1_MODE_ST: cpapmode = PRS1_MODE_ST_AVAPS; break;
                            case PRS1_MODE_PC: cpapmode = PRS1_MODE_PC_AVAPS; break;
                            default:
                                UNEXPECTED_VALUE(cpapmode, "S/T or PC");
                                break;
                        }
                        flexmode = FLEX_RiseTime;  // reports say "AVAPS" but then list a rise time setting
                        break;
                    default:
                        UNEXPECTED_VALUE(data[pos], "known flex mode");
                        break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, (int) flexmode));
                break;
            case 2: // ??? Maybe AAM?
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0);
                break;
            case 3: // CPAP Pressure
                CHECK_VALUE(len, 1);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAP);
                fixed_pressure = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, fixed_pressure, GAIN));
                break;
            case 4: // EPAP Pressure
                CHECK_VALUE(len, 1);
                if (cpapmode == PRS1_MODE_CPAP) UNEXPECTED_VALUE(cpapmode, "!cpap");
                // pressures seem variable on practice, maybe due to ramp or leaks?
                fixed_epap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, fixed_epap, GAIN));
                break;
            case 7: // IPAP Pressure
                CHECK_VALUE(len, 1);
                CHECK_VALUES(cpapmode, PRS1_MODE_S, PRS1_MODE_ST);
                // pressures seem variable on practice, maybe due to ramp or leaks?
                fixed_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, fixed_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, fixed_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 8:  // Min IPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUE(fixed_ipap, 0);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                min_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 9:  // Max IPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUE(fixed_ipap, 0);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                if (min_ipap == 0) UNEXPECTED_VALUE(min_ipap, ">0");
                max_ipap = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_ipap, GAIN));
                // TODO: We need to revisit whether PS should be shown as a setting.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, max_ipap - fixed_epap, GAIN));
                if (fixed_epap == 0) UNEXPECTED_VALUE(fixed_epap, ">0");
                break;
            case 0x19:  // Tidal Volume (AVAPS)
                CHECK_VALUE(len, 1);
                CHECK_VALUES(cpapmode, PRS1_MODE_ST_AVAPS, PRS1_MODE_PC_AVAPS);
                //CHECK_VALUE(data[pos], 47);  // gain 10.0
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TIDAL_VOLUME, data[pos] * 10.0));
                break;
            case 0x1e:  // (Backup) Breath Rate (S/T and PC)
                CHECK_VALUE(len, 3);
                if (cpapmode == PRS1_MODE_CPAP || cpapmode == PRS1_MODE_S) UNEXPECTED_VALUE(cpapmode, "S/T or PC");
                switch (data[pos]) {
                case 0:  // Breath Rate Off
                    // TODO: Is this mode essentially bilevel? The pressure graphs are confusing.
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Off));
                    CHECK_VALUE(data[pos+1], 0);
                    CHECK_VALUE(data[pos+2], 0);
                    break;
                //case 1:  // Breath Rate Auto in F5V3 setting 0x14
                case 2:  // Breath Rate (fixed BPM)
                    breath_rate = data[pos+1];
                    timed_inspiration = data[pos+2];
                    if (breath_rate < 9 || breath_rate > 15) UNEXPECTED_VALUE(breath_rate, "9-15");
                    if (timed_inspiration < 8 || timed_inspiration > 20) UNEXPECTED_VALUE(timed_inspiration, "8-20");
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Fixed));
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_RATE, breath_rate));
                    this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_BACKUP_TIMED_INSPIRATION, timed_inspiration, 0.1));
                    break;
                default:
                    CHECK_VALUES(data[pos], 0, 2);  // 0 = Breath Rate off (S), 2 = fixed BPM (1 = auto on F5V3 setting 0x14, haven't seen it on F3V6 yet)
                    break;
                }
                break;
            //0x2b: Ramp type sounds like it's linear for F3V6 unless AAM is enabled, so no setting may be needed.
            case 0x2c:  // Ramp Time
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == ramp off, and ramp pressure setting doesn't appear
                    if (data[pos] < 5 || data[pos] > 45) UNEXPECTED_VALUE(data[pos], "5-45");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, data[pos]));
                break;
            case 0x2d:  // Ramp Pressure (with ASV/ventilator pressure encoding), only present when ramp is on
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, data[pos], GAIN));
                break;
            case 0x2e:  // Bi-Flex level or Rise Time
                // On F5V3 the first byte could specify Bi-Flex or Rise Time, and second byte contained the value.
                // On F3V6 there's only one byte, which seems to correspond to Rise Time on the reports with flex
                // mode None or AVAPS and to Bi-Flex Setting (level) in Bi-Flex mode.
                CHECK_VALUE(len, 1);
                if (flexmode == FLEX_BiFlex) {
                    // Bi-Flex level
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[pos]));
                } else if (flexmode == FLEX_RiseTime) {
                    // Rise time
                    if (data[pos] < 1 || data[pos] > 6) UNEXPECTED_VALUE(data[pos], "1-6");  // 1-6 have been seen
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME, data[pos]));
                } else {
                    UNEXPECTED_VALUE(flexmode, "BiFlex or RiseTime");
                }
                // Timed inspiration specified in the backup breath rate.
                break;
            case 0x2f:  // Flex / Rise Time lock
                CHECK_VALUE(len, 1);
                if (flexmode == FLEX_BiFlex) {
                    CHECK_VALUE(cpapmode, PRS1_MODE_S);
                    CHECK_VALUES(data[pos], 0, 0x80);  // Bi-Flex Lock
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[pos] != 0));
                } else if (flexmode == FLEX_RiseTime) {
                    CHECK_VALUES(data[pos], 0, 0x80);  // Rise Time Lock
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME_LOCK, data[pos] != 0));
                } else {
                    UNEXPECTED_VALUE(flexmode, "BiFlex or RiseTime");
                }
                break;
            case 0x35:  // Humidifier setting
                CHECK_VALUE(len, 2);
                this->ParseHumidifierSettingV3(data[pos], data[pos+1], true);
                break;
            case 0x36:  // Mask Resistance Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, data[pos] != 0));
                break;
            case 0x38:  // Mask Resistance
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == mask resistance off
                    if (data[pos] < 1 || data[pos] > 5) UNEXPECTED_VALUE(data[pos], "1-5");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, data[pos]));
                break;
            case 0x39:  // Tubing Type Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, data[pos] != 0));
                break;
            case 0x3b:  // Tubing Type
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 2, 1);  // 15HT = 2, 15 = 1, 22 = 0, though report only says "15" for 15HT
                }
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x3c:  // View Optional Screens
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, data[pos] != 0));
                break;
            default:
                UNEXPECTED_VALUE(code, "known setting");
                qDebug() << "Unknown setting:" << QTHEX << code << "in" << this->sessionid << "at" << pos;
                this->AddEvent(new PRS1UnknownDataEvent(QByteArray((const char*) data, size), pos, len));
                break;
        }

        pos += len;
    } while (ok && pos + 2 <= size);
    
    return ok;
}


const QVector<PRS1ParsedEventType> ParsedEventsF3V6 = {
    PRS1TimedBreathEvent::TYPE,
    PRS1IPAPAverageEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1Test2Event::TYPE,
    PRS1Test1Event::TYPE,
    PRS1SnoreEvent::TYPE,  // No individual VS, only snore count
    PRS1LeakEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1ApneaAlarmEvent::TYPE,
    // No FL?
};

// 1030X, 11030X series
// based on ParseEventsF5V3, updated for F3V6
bool PRS1DataChunk::ParseEventsF3V6(void)
{
    if (this->family != 3 || this->familyVersion != 6) {
        qWarning() << "ParseEventsF3V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 2, 3, 0xe, 3, 3, 3, 4, 5, 3, 5, 3, 3, 2, 2, 2, 2 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);

    if (chunk_size < 1) {
        // This does occasionally happen.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    // F3V6 uses a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: this should be parameterized somewhere more logical
    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration;
    do {
        code = data[pos++];
        if (!this->hblock.contains(code)) {
            qWarning() << this->sessionid << "missing hblock entry for event" << code;
            ok = false;
            break;
        }
        size = this->hblock[code];
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            if (size < minimum_sizes[code]) {
                qWarning() << this->sessionid << "event" << code << "too small" << size << "<" << minimum_sizes[code];
                ok = false;
                break;
            }
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "event" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        startpos = pos;
        t += data[pos] | (data[pos+1] << 8);
        pos += 2;

        switch (code) {
            // case 0x00?
            case 1:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                // TODO: make sure F3 import logic matches F5 in adjusting TB start time
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 2:  // Statistics
                // These appear every 2 minutes, so presumably summarize the preceding period.
                //data[pos+0];  // TODO: 0 = ???
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+2], GAIN));        // 02=IPAP
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+1], GAIN));        // 01=EPAP, needs to be added second to calculate PS
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1Test2Event(t, data[pos+8]));                    // 08=Flow???
                this->AddEvent(new PRS1Test1Event(t, data[pos+9]));                    // 09=TMV???
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+0xa]));                  // 0A=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1LeakEvent(t, data[pos+0xb]));                   // 0B=Leak (average?)
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x03:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x04:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x05:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x06:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xd and 0xe?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x08:  // RERA
                elapsed = data[pos];  // based on sample waveform, the RERA is over after this
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x09:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x0a:  // Hypopnea
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x0b:  // Hypopnea
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating hypopneas ourselves. Their official definition
                // is 40% reduction in flow lasting at least 10s.
                duration = data[pos];
                this->AddEvent(new PRS1HypopneaEvent(t - duration, 0));
                break;
            case 0x0c:  // Apnea Alarm
                // no additional data
                this->AddEvent(new PRS1ApneaAlarmEvent(t, 0));
                break;
            case 0x0d:  // Low MV Alarm
                // no additional data
                this->AddEvent(new PRS1LowMinuteVentilationAlarmEvent(t, 0));
                break;
            // case 0x0e?
            // case 0x0f?
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos-1, size+1));
                break;
        }
        pos = startpos + size;
    } while (ok && pos < chunk_size);

    this->duration = t;

    return ok;
}
