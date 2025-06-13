/* PRS1 Parsing for CPAP and BIPAP (Family 0)
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


//********************************************************************************************
// MARK: 50 Series

bool PRS1DataChunk::ParseComplianceF0V23(void)
{
    if (this->family != 0 || (this->familyVersion != 2 && this->familyVersion != 3)) {
        qWarning() << "ParseComplianceF0V23 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    // All sample devices with FamilyVersion 3 in the properties.txt file have familyVersion 2 in their .001/.002/.005 files!
    // We should flag an actual familyVersion 3 file if we ever encounter one!
    CHECK_VALUE(this->familyVersion, 2);
    
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 0xd, 5, 2, 2 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.

    bool ok = true;
    int pos = 0;
    int code, size, delta;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUES(data[pos], 1, 0);  // usually 1, occasionally 0, no visible difference in report
            // F0V23 doesn't have a separate settings record like F0V6 does, the settings just follow the EquipmentOn data.
                ok = ParseSettingsF0V23(data, 0x0e);
                // Compliance doesn't have pressure set events following settings like summary does.
                break;
            case 2:  // Mask On
                delta = data[pos] | (data[pos+1] << 8);
                if (tt == 0) {
                    CHECK_VALUE(delta, 0);  // we've never seen the initial MaskOn have any delta
                } else {
                    if (delta % 60) UNEXPECTED_VALUE(delta, "even minutes");  // mask-off events seem to be whole minutes?
                }
                tt += delta;
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                // no per-slice humidifer settings as in F0V6
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                // Compliance doesn't record any stats after mask-off like summary does.
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));

                // also seems to be a trailing 01 00 81 after the slices?
                CHECK_VALUES(data[pos+2], 1, 0);  // usually 1, occasionally 0, no visible difference in report
                //CHECK_VALUE(data[pos+3], 0);  // sometimes 1, 2, or 5, no visible difference in report, maybe ramp?
                ParseHumidifierSetting50Series(data[pos+4]);
                break;
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


bool PRS1DataChunk::ParseSummaryF0V23()
{
    if (this->family != 0 || (this->familyVersion != 2 && this->familyVersion != 3)) {
        qWarning() << "ParseSummaryF0V23 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    // All sample devices with FamilyVersion 3 in the properties.txt file have familyVersion 2 in their .001/.002/.005 files!
    // We should flag an actual familyVersion 3 file if we ever encounter one!
    CHECK_VALUE(this->familyVersion, 2);
    
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 0xf, 5, 2, 0x21, 0, 4 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.
    
    bool ok = true;
    int pos = 0;
    int code, size, delta;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUES(data[pos] & 0xF0, 0x60, 0x70);  // TODO: what are these?
                switch (data[pos] & 0x0F) {
                    case 0:  // TODO: What is this? It seems to be related to errors.
                    case 1:  // This is the most frequent value.
                    case 3:  // TODO: What is this?
                    case 4:  // This seems to be related to an automatic transition from CPAP to AutoCPAP.
                        break;
                    default:
                        UNEXPECTED_VALUE(data[pos] & 0x0F, "[0,1,3,4]");
                }
            // F0V23 doesn't have a separate settings record like F0V6 does, the settings just follow the EquipmentOn data.
                ok = ParseSettingsF0V23(data, 0x0e);
                // TODO: register these as pressure set events
                //CHECK_VALUES(data[0x0e], ramp_pressure, min_pressure);  // initial CPAP/EPAP, can be minimum pressure or ramp, or whatever auto decides to use
                //if (cpapmode == PRS1_MODE_BILEVEL) {  // initial IPAP for bilevel modes
                //    CHECK_VALUE(data[0x0f], max_pressure);
                //} else if (cpapmode == PRS1_MODE_AUTOBILEVEL) {
                //    CHECK_VALUE(data[0x0f], min_pressure + 20);
                //}
                break;
            case 2:  // Mask On
                delta = data[pos] | (data[pos+1] << 8);
                if (tt == 0) {
                    if (delta) {
                        CHECK_VALUES(delta, 1, 59);  // we've seen the 550P start its first mask-on at these time deltas
                    }
                } else {
                    if (delta % 60) {
                        if (this->familyVersion == 2 && ((delta + 1) % 60) == 0) {
                            // For some reason F0V2 frequently is frequently 1 second less than whole minute intervals.
                        } else {
                            UNEXPECTED_VALUE(delta, "even minutes");  // mask-off events seem to be whole minutes?
                        }
                    }
                }
                tt += delta;
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                // no per-slice humidifer settings as in F0V6
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
            // F0V23 doesn't have a separate stats record like F0V6 does, the stats just follow the MaskOff data.
                // These are 0x22 bytes in a summary vs. 3 bytes in compliance data
                // TODO: What are these values?
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));

                switch (data[pos+2]) {
                    case 0:  // TODO: What is this? It seems to be related to errors.
                    case 1:  // This is the usual value.
                    case 3:  // TODO: What is this? This has been seen after 90 sec large leak before turning off.
                    case 4:  // TODO: What is this? We've seen it once.
                    case 5:  // This seems to be related to an automatic transition from CPAP to AutoCPAP.
                        break;
                    default:
                        UNEXPECTED_VALUE(data[pos+2], "[0,1,3,4,5]");
                }
                //CHECK_VALUES(data[pos+3], 0, 1);  // TODO: may be related to ramp? 1-5 seems to have a ramp start or two
                ParseHumidifierSetting50Series(data[pos+4]);
                break;
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


bool PRS1DataChunk::ParseSettingsF0V23(const unsigned char* data, int /*size*/)
{
    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;

    switch (data[0x02]) {  // PRS1 mode   // 0 = CPAP, 2 = APAP
    case 0x00:
        cpapmode = PRS1_MODE_CPAP;
        break;
    case 0x01:
        cpapmode = PRS1_MODE_BILEVEL;
        break;
    case 0x02:
        cpapmode = PRS1_MODE_AUTOCPAP;
        break;
    case 0x03:
        cpapmode = PRS1_MODE_AUTOBILEVEL;
        break;
    default:
        UNEXPECTED_VALUE(data[0x02], "known device mode");
        break;
    }

    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));

    int min_pressure = data[0x03];
    int max_pressure = data[0x04];
    int ps  = data[0x05];  // max pressure support (for variable), seems to be zero otherwise

    if (cpapmode == PRS1_MODE_CPAP) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, min_pressure));
        //CHECK_VALUE(max_pressure, 0);  // occasionally nonzero, usually seems to be when the next session is AutoCPAP with this max
        CHECK_VALUE(ps, 0);
    } else if (cpapmode == PRS1_MODE_AUTOCPAP) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
        CHECK_VALUE(ps, 0);
    } else if (cpapmode == PRS1_MODE_BILEVEL) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, max_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, max_pressure - min_pressure));
        CHECK_VALUE(ps, 0);  // this seems to be unused on fixed bilevel
    } else if (cpapmode == PRS1_MODE_AUTOBILEVEL) {
        int min_ps = 20;  // 2.0 cmH2O
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MAX, max_pressure - min_ps));  // TODO: not yet confirmed
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_pressure + min_ps));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ps));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, ps));
    }

    int ramp_time = data[0x06];
    int ramp_pressure = data[0x07];
    if (ramp_time > 0) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, ramp_time));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, ramp_pressure));
    }

    quint8 flex = data[0x08];
    this->ParseFlexSettingF0V2345(flex, cpapmode);

    int humid = data[0x09];
    this->ParseHumidifierSetting50Series(humid, true);
    
    // Tubing lock has no setting byte

    // Menu Options
    bool mask_resist_on = ((data[0x0a] & 0x40) != 0);  // System One Resistance Status bit
    int mask_resist_setting = data[0x0a] & 7;  // System One Resistance setting value
    CHECK_VALUE(mask_resist_on, mask_resist_setting > 0);  // Confirm that we can ignore the status bit.
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, (data[0x0a] & 0x80) != 0)); // System One Resistance Lock Setting, only seen on bricks
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, (data[0x0a] & 0x08) ? 15 : 22));  // TODO: unconfirmed
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, mask_resist_setting));
    CHECK_VALUE(data[0x0a] & (0x20 | 0x10), 0);

    CHECK_VALUE(data[0x0b], 1);
    
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, (data[0x0c] & 0x40) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_OFF, (data[0x0c] & 0x10) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_ALERT, (data[0x0c] & 0x04) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, (data[0x0c] & 0x02) != 0));
    CHECK_VALUE(data[0x0c] & (0xA0 | 0x09), 0);

    CHECK_VALUE(data[0x0d], 0);

    return true;
}


// Flex F0V2 confirmed
// 0x00 = None
// 0x81 = C-Flex 1, lock off (AutoCPAP mode)
// 0x82 = Bi-Flex 2 (Bi-Level mode)
// 0x89 = A-Flex 1 (AutoCPAP mode)
// 0x8A = A-Flex 2, lock off (AutoCPAP mode)
// 0x8B = C-Flex+ 3, lock off (CPAP mode)
// 0x93 = Rise Time 3 (AutoBiLevel mode)

// Flex F0V4 confirmed
// 0x00 = None
// 0x81 = Bi-Flex 1 (AutoBiLevel mode)
// 0x81 = C-Flex 1 (AutoCPAP mode)
// 0x82 = C-Flex 2 (CPAP mode)
// 0x82 = C-Flex 2 (CPAP-Check mode)
// 0x82 = C-Flex 2 (Auto-Trial mode)
// 0x83 = Bi-Flex 3 (Bi-Level mode)
// 0x89 = A-Flex 1 (AutoCPAP mode)
// 0x8A = C-Flex+ 2 (CPAP mode)
// 0x8A = C-Flex+ 2, lock off (CPAP-Check mode)
// 0x8A = A-Flex 2, lock off (Auto-Trial mode)
// 0xCB = C-Flex+ 3 (CPAP-Check mode), C-Flex+ Lock on
//
// 0x8A = A-Flex 1 (AutoCPAP mode)
// 0x8B = C-Flex+ 3 (CPAP mode)
// 0x8B = A-Flex 3 (AutoCPAP mode)

// Flex F0V5 confirmed
// 0xE1 = Flex (AutoCPAP mode)
// 0xA1 = Flex (AutoCPAP mode)
// 0xA2 = Flex (AutoCPAP mode)

//   8  = enabled
//   4  = lock
//   2  = Flex (only seen on Dorma series)
//   1  = rise time
//    8 = C-Flex+ / A-Flex (depending on mode)
//    3 = level

void PRS1DataChunk::ParseFlexSettingF0V2345(quint8 flex, int cpapmode)
{
    FlexMode flexmode = FLEX_None;
    bool enabled  = (flex & 0x80) != 0;
    bool lock     = (flex & 0x40) != 0;
    bool plain_flex = (flex & 0x20) != 0;  // "Flex", seen on Dorma series
    bool risetime = (flex & 0x10) != 0;
    bool plusmode = (flex & 0x08) != 0;
    int flexlevel = flex & 0x03;
    if (flex & 0x04) UNEXPECTED_VALUE(flex, "known bits");
    if (this->familyVersion == 2) {
        //CHECK_VALUE(lock, false);  // We've seen this set on F0V2, but it doesn't appear on the reports.
    }

    if (enabled) {
        if (flexlevel < 1) UNEXPECTED_VALUE(flexlevel, "!= 0");
        if (risetime) {
            flexmode = FLEX_RiseTime;
            CHECK_VALUES(cpapmode, PRS1_MODE_BILEVEL, PRS1_MODE_AUTOBILEVEL);
            CHECK_VALUE(plusmode, 0);
        } else if (plusmode) {
            switch (cpapmode) {
                case PRS1_MODE_CPAP:
                case PRS1_MODE_CPAPCHECK:
                    flexmode = FLEX_CFlexPlus;
                    break;
                case PRS1_MODE_AUTOCPAP:
                case PRS1_MODE_AUTOTRIAL:
                    flexmode = FLEX_AFlex;
                    break;
                default:
                    HEX(flex);
                    UNEXPECTED_VALUE(cpapmode, "expected C-Flex+/A-Flex mode");
                    break;
            }
        } else if (plain_flex) {
            CHECK_VALUE(this->familyVersion, 5);  // so far only seen with F0V5
            switch (cpapmode) {
                case PRS1_MODE_AUTOCPAP:
                    flexmode = FLEX_Flex;  // unknown whether this is equivalent to C-Flex, C-Flex+, or A-Flex
                    break;
                default:
                    UNEXPECTED_VALUE(cpapmode, "expected mode");
                    flexmode = FLEX_Flex;  // probably the same for CPAP mode as well, but we haven't tested that yet
                    break;
            }
        } else {
            switch (cpapmode) {
                case PRS1_MODE_CPAP:
                case PRS1_MODE_CPAPCHECK:
                case PRS1_MODE_AUTOCPAP:
                case PRS1_MODE_AUTOTRIAL:
                    flexmode = FLEX_CFlex;
                    break;
                case PRS1_MODE_BILEVEL:
                case PRS1_MODE_AUTOBILEVEL:
                    flexmode = FLEX_BiFlex;
                    break;
                default:
                    HEX(flex);
                    UNEXPECTED_VALUE(cpapmode, "expected mode");
                    break;
            }
        }
    }

    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, (int) flexmode));
    if (flexmode != FLEX_None) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, flexlevel));
    }
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, lock));
}


const QVector<PRS1ParsedEventType> ParsedEventsF0V23 = {
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1VariableBreathingEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1SnoresAtPressureEvent::TYPE,
};

// 750P is F0V2; 550P is F0V2/F0V3 (properties.txt sometimes says F0V3, data files always say F0V2); 450P is F0V3
bool PRS1DataChunk::ParseEventsF0V23()
{
    if (this->family != 0 || this->familyVersion < 2 || this->familyVersion > 3) {
        qWarning() << "ParseEventsF0V23 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    // All sample devices with FamilyVersion 3 in the properties.txt file have familyVersion 2 in their .001/.002/.005 files!
    // We should flag an actual familyVersion 3 file if we ever encounter one!
    CHECK_VALUE(this->familyVersion, 2);
    
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const QMap<int,int> event_sizes = { {1,2}, {3,4}, {0xb,4}, {0xd,2}, {0xe,5}, {0xf,5}, {0x10,5}, {0x11,4}, {0x12,4} };
   
    if (chunk_size < 1) {
        // This does occasionally happen in F0V6.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration, value;
    do {
        code = data[pos++];

        size = 3;  // default size = 2 bytes time delta + 1 byte data
        if (event_sizes.contains(code)) {
            size = event_sizes[code];
        }
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "event" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        startpos = pos;
        if (code != 0x12 && code != 0x01) {  // This one event has no timestamp in F0V6
            elapsed = data[pos] | (data[pos+1] << 8);
            if (elapsed > 0x7FFF) UNEXPECTED_VALUE(elapsed, "<32768s");  // check whether this is generally unsigned, since 0x01 isn't
            t += elapsed;
            pos += 2;
        }

        switch (code) {
            case 0x00:  // Humidifier setting change (logged in summary in 60 series)
                ParseHumidifierSetting50Series(data[pos]);
                if (this->familyVersion == 3) DUMP_EVENT();
                break;
            case 0x01:  // Time elapsed?
                // Only seen twice, on a 550P and 650P.
                // It looks almost like a time-elapsed event 4 found in F0V4 summaries, but
                // 0xFFCC looks like it represents a time adjustment of -52 seconds,
                // since the subsequent 0x11 statistics event has a time offset of 172 seconds,
                // and counting this as -52 seconds results in a total session time that
                // matches the summary and waveform data. Very weird.
                //
                // Similarly 0xFFDC looks like it represents a time adjustment of -36 seconds.
                CHECK_VALUES(data[pos], 0xDC, 0xCC);
                CHECK_VALUE(data[pos+1], 0xFF);
                elapsed = data[pos] | (data[pos+1] << 8);
                if (elapsed & 0x8000) {
                    elapsed = (~0xFFFF | elapsed);  // sign extend 16-bit number to native int
                }
                t += elapsed;
                break;
            case 0x02:  // Pressure adjustment
                // See notes in ParseEventsF0V6.
                this->AddEvent(new PRS1PressureSetEvent(t, data[pos]));
                break;
            case 0x03:  // Pressure adjustment (bi-level)
                // See notes in ParseEventsF0V6.
                this->AddEvent(new PRS1IPAPSetEvent(t, data[pos+1]));
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos]));  // EPAP needs to be added second to calculate PS
                break;
            case 0x04:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x05:  // RERA
                elapsed = data[pos++];
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x06:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            //case 0x08:  // never seen
            //case 0x09:  // never seen
            case 0x0a:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xb, [0x14 and 0x15 on F0V6]?
                elapsed = data[pos++];
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0b:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xa, [0x14 and 0x15 on F0V6]?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0c:  // Flow Limitation
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0d:  // Vibratory Snore
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistics below seem to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0e:  // Variable Breathing?
                // TODO: does duration double like F0V4?
                duration = (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];  // this is always 60 seconds unless it's at the end, so it seems like elapsed
                CHECK_VALUES(elapsed, 60, 0);
                this->AddEvent(new PRS1VariableBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0f:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                // NOTE: F0V2 does NOT double this like F0V6 does
                if (this->familyVersion == 3)  // double-check whether there's doubling on F0V3
                    DUMP_EVENT();
                duration = (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x10:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                // NOTE: F0V2 does NOT double this like F0V4 and F0V6 does
                if (this->familyVersion == 3)  // double-check whether there's doubling on F0V3
                    DUMP_EVENT();
                duration = (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x11:  // Statistics
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos]));
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+1]));
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x12:  // Snore count per pressure
                // Some sessions (with lots of ramps) have multiple of these, each with a
                // different pressure. The total snore count across all of them matches the
                // total found in the stats event.
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 1, 2);  // 0 = CPAP pressure, 1 = bi-level EPAP, 2 = bi-level IPAP
                }
                //CHECK_VALUE(data[pos+1], 0x78);  // pressure
                //CHECK_VALUE(data[pos+2], 1);  // 16-bit snore count
                //CHECK_VALUE(data[pos+3], 0);
                value = (data[pos+2] | (data[pos+3] << 8));
                this->AddEvent(new PRS1SnoresAtPressureEvent(t, data[pos], data[pos+1], value));
                break;
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos));
                ok = false;  // unlike F0V6, we don't know the size of unknown events, so we can't recover
                break;
        }
        pos = startpos + size;
    } while (ok && pos < chunk_size);

    if (ok && pos != chunk_size) {
        qWarning() << this->sessionid << (this->size() - pos) << "trailing event bytes";
    }

    this->duration = t;

    return ok;
}


//********************************************************************************************
// MARK: -
// MARK: 60 Series

bool PRS1DataChunk::ParseComplianceF0V4(void)
{
    if (this->family != 0 || (this->familyVersion != 4)) {
        qWarning() << "ParseComplianceF0V4 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 0x18, 7, 4, 2, 0, 0, 0, 4, 0 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.
    
    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUES(data[pos], 1, 3);
            // F0V4 doesn't have a separate settings record like F0V6 does, the settings just follow the EquipmentOn data.
                ok = ParseSettingsF0V45(data, 0x11);
                CHECK_VALUE(data[pos+0x11], 0);
                CHECK_VALUE(data[pos+0x12], 0);
                CHECK_VALUE(data[pos+0x13], 0);
                CHECK_VALUE(data[pos+0x14], 0);
                CHECK_VALUE(data[pos+0x15], 0);
                CHECK_VALUE(data[pos+0x16], 0);
                CHECK_VALUE(data[pos+0x17], 0);
                break;
            case 2:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                // Compliance doesn't have any MaskOff stats like summary does
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUES(data[pos+2], 1, 3);  // or 0
                //CHECK_VALUE(data[pos+2] & ~(0x40|8|4|2|1), 0);  // ???, seen various bit combinations
                //CHECK_VALUE(data[pos+3], 0x19);  // 0x17, 0x16
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUE(data[pos+5], 0x35);  // 0x36, 0x36
                if (data[pos+6] != 1) {
                    CHECK_VALUE(data[pos+6] & ~(4|2|1), 0);  // On F0V23 0 seems to be related to errors, 3 seen after 90 sec large leak before turning off?
                }
                // pos+4 == 2, pos+6 == 10 on the session that had a time-elapsed event, maybe it shut itself off
                // when approaching 24h of continuous use?
                break;
            /*
            case 4:  // Time Elapsed
                // For example: mask-on 5:18:49 in a session of 23:41:20 total leaves mask-off time of 18:22:31.
                // That's represented by a mask-off event 19129 seconds after the mask-on, then a time-elapsed
                // event after 65535 seconds, then an equipment off event after another 616 seconds.
                tt += data[pos] | (data[pos+1] << 8);
                // TODO: see if this event exists in earlier versions
                break;
            case 5:  // Clock adjustment?
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 5);  // and the only record in the session.
                // This looks like it's minor adjustments to the clock, but 560PBT-3917 sessions 1-2 are weird:
                // session 1 starts at 2015-12-23T00:01:20 and contains this event with timestamp 2015-12-23T00:05:14.
                // session 2 starts at 2015-12-23T00:01:29, which suggests the event didn't change the clock.
                //
                // It looks like this happens when there are discontinuities in timestamps, for example 560P-4727:
                // session 58 ends at 2015-05-26T09:53:17.
                // session 59 starts at 2015-05-26T09:53:15 with an event 5 timestamp of 2015-05-26T09:53:18.
                //
                // So the session/chunk timestamp has gone backwards. Whenever this happens, it seems to be in
                // a session with an event-5 event having a timestamp that hasn't gone backwards. So maybe
                // this timestamp is the old clock before adjustment? This would explain the 560PBT-3917 sessions above.
                //
                // This doesn't seem particularly associated with discontinuities in the waveform data: there are
                // often clock adjustments without corresponding discontinuities in the waveform, and vice versa.
                // It's possible internal clock inaccuracy causes both independently.
                //
                // TODO: why do some devices have lots of these and others none? Maybe cellular modems make daily tweaks?
                if (false) {
                    long value = data[pos] | data[pos+1]<<8 | data[pos+2]<<16 | data[pos+3]<<24;
                    qDebug() << this->sessionid << "clock changing from" << ts(value * 1000L)
                                                << "to" << ts(this->timestamp * 1000L)
                                                << "delta:" << (this->timestamp - value);
                }
                break;
            */
            case 6:  // Cleared?
                // Appears in the very first session when that session number is > 1.
                // Presumably previous sessions were cleared out.
                // TODO: add an internal event for this.
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 1);  // and the only record in the session.
                if (this->sessionid == 1) UNEXPECTED_VALUE(this->sessionid, ">1");
                break;
            case 7:  // Humidifier setting change (logged in events in 50 series)
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
            /*
            case 8:  // CPAP-Check related, follows Mask On in CPAP-Check mode
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                //CHECK_VALUES(data[pos+2], 0, 79);  // probably 16-bit value, sometimes matches OA + H + FL + VS + RE?
                CHECK_VALUE(data[pos+3], 0);
                //CHECK_VALUES(data[pos+4], 0, 10);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0);
                //CHECK_VALUES(data[pos+6], 0, 79);  // probably 16-bit value, usually the same as +2, but not always?
                CHECK_VALUE(data[pos+7], 0);
                //CHECK_VALUES(data[pos+8], 0, 10);  // probably 16-bit value
                CHECK_VALUE(data[pos+9], 0);
                //CHECK_VALUES(data[pos+0xa], 0, 4);  // or 0? 44 when changed pressure mid-session?
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


bool PRS1DataChunk::ParseSummaryF0V4(void)
{
    if (this->family != 0 || (this->familyVersion != 4)) {
        qWarning() << "ParseSummaryF0V4 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 0x18, 7, 7, 0x24, 2, 4, 0, 4, 0xb };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.
    
    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUES(data[pos] & 0xF0, 0x80, 0xC0);  // TODO: what are these?
                if ((data[pos] & 0x0F) != 1) {  // This is the most frequent value.
                    //CHECK_VALUES(data[pos] & 0x0F, 3, 5);  // TODO: what are these? 0 seems to be related to errors.
                }
            // F0V4 doesn't have a separate settings record like F0V6 does, the settings just follow the EquipmentOn data.
                ok = ParseSettingsF0V45(data, 0x11);
                CHECK_VALUE(data[pos+0x11], 0);
                CHECK_VALUE(data[pos+0x12], 0);
                CHECK_VALUE(data[pos+0x13], 0);
                CHECK_VALUE(data[pos+0x14], 0);
                CHECK_VALUE(data[pos+0x15], 0);
                CHECK_VALUE(data[pos+0x16], 0);
                CHECK_VALUE(data[pos+0x17], 0);
                break;
            case 2:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                //CHECK_VALUES(data[pos+2], 120, 110);  // probably initial pressure
                //CHECK_VALUE(data[pos+3], 0);  // initial IPAP on bilevel?
                //CHECK_VALUES(data[pos+4], 0, 130);  // minimum pressure in auto-cpap
                this->ParseHumidifierSetting60Series(data[pos+5], data[pos+6]);
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
            // F0V4 doesn't have a separate stats record like F0V6 does, the stats just follow the MaskOff data.
                //CHECK_VALUES(data[pos+2], 130);  // probably ending pressure
                //CHECK_VALUE(data[pos+3], 0);  // ending IPAP for bilevel? average?
                //CHECK_VALUES(data[pos+4], 0, 130);  // 130 pressure in auto-cpap: min pressure? 90% IPAP in bilevel?
                //CHECK_VALUES(data[pos+5], 0, 130);  // 130 pressure in auto-cpap, 90% EPAP in bilevel?
                //CHECK_VALUE(data[pos+6], 0);  // 145 maybe max pressure in Auto-CPAP?
                //CHECK_VALUE(data[pos+7], 0);  // Average 90% Pressure (Auto-CPAP)
                //CHECK_VALUE(data[pos+8], 0);  // Average CPAP (Auto-CPAP)
                //CHECK_VALUES(data[pos+9], 0, 4);  // or 1; PB count? LL count? minutes of something?
                CHECK_VALUE(data[pos+0xa], 0);
                //CHECK_VALUE(data[pos+0xb], 0);  // OA count, probably 16-bit
                CHECK_VALUE(data[pos+0xc], 0);
                //CHECK_VALUE(data[pos+0xd], 0);
                CHECK_VALUE(data[pos+0xe], 0);
                //CHECK_VALUE(data[pos+0xf], 0);  // 16-bit CA count
                //CHECK_VALUE(data[pos+0x10], 0);
                //CHECK_VALUE(data[pos+0x11], 40);  // 16-bit something: 0x88, 0x26, etc. ???
                //CHECK_VALUE(data[pos+0x12], 0);
                //CHECK_VALUE(data[pos+0x13], 0);  // 16-bit minutes in LL
                //CHECK_VALUE(data[pos+0x14], 0);
                //CHECK_VALUE(data[pos+0x15], 0);  // minutes in PB, probably 16-bit
                CHECK_VALUE(data[pos+0x16], 0);
                //CHECK_VALUE(data[pos+0x17], 0);  // 16-bit VS count
                //CHECK_VALUE(data[pos+0x18], 0);
                //CHECK_VALUE(data[pos+0x19], 0);  // H count, probably 16-bit
                CHECK_VALUE(data[pos+0x1a], 0);
                //CHECK_VALUE(data[pos+0x1b], 0);  // 0 when no PB or LL?
                CHECK_VALUE(data[pos+0x1c], 0);
                //CHECK_VALUE(data[pos+0x1d], 9);  // RE count, probably 16-bit
                CHECK_VALUE(data[pos+0x1e], 0);
                //CHECK_VALUE(data[pos+0x1f], 0);  // FL count, probably 16-bit
                CHECK_VALUE(data[pos+0x20], 0);
                //CHECK_VALUE(data[pos+0x21], 0x32);  // 0x55, 0x19  // ???
                //CHECK_VALUE(data[pos+0x22], 0x23);  // 0x3f, 0x14  // Average total leak
                //CHECK_VALUE(data[pos+0x23], 0x40);  // 0x7d, 0x3d  // ???
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                CHECK_VALUE(data[pos+2] & ~(0x40|8|4|2|1), 0);  // ???, seen various bit combinations
                //CHECK_VALUE(data[pos+3], 0x19);  // 0x17, 0x16
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUE(data[pos+5], 0x35);  // 0x36, 0x36
                if (data[pos+6] != 1) {  // This is the usual value.
                    CHECK_VALUE(data[pos+6] & ~(8|4|2|1), 0);  // On F0V23 0 seems to be related to errors, 3 seen after 90 sec large leak before turning off?
                }
                // pos+4 == 2, pos+6 == 10 on the session that had a time-elapsed event, maybe it shut itself off
                // when approaching 24h of continuous use?
                break;
            case 4:  // Time Elapsed
                // For example: mask-on 5:18:49 in a session of 23:41:20 total leaves mask-off time of 18:22:31.
                // That's represented by a mask-off event 19129 seconds after the mask-on, then a time-elapsed
                // event after 65535 seconds, then an equipment off event after another 616 seconds.
                tt += data[pos] | (data[pos+1] << 8);
                // TODO: see if this event exists in earlier versions
                break;
            case 5:  // Clock adjustment?
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 5);  // and the only record in the session.
                // This looks like it's minor adjustments to the clock, but 560PBT-3917 sessions 1-2 are weird:
                // session 1 starts at 2015-12-23T00:01:20 and contains this event with timestamp 2015-12-23T00:05:14.
                // session 2 starts at 2015-12-23T00:01:29, which suggests the event didn't change the clock.
                //
                // It looks like this happens when there are discontinuities in timestamps, for example 560P-4727:
                // session 58 ends at 2015-05-26T09:53:17.
                // session 59 starts at 2015-05-26T09:53:15 with an event 5 timestamp of 2015-05-26T09:53:18.
                //
                // So the session/chunk timestamp has gone backwards. Whenever this happens, it seems to be in
                // a session with an event-5 event having a timestamp that hasn't gone backwards. So maybe
                // this timestamp is the old clock before adjustment? This would explain the 560PBT-3917 sessions above.
                //
                // This doesn't seem particularly associated with discontinuities in the waveform data: there are
                // often clock adjustments without corresponding discontinuities in the waveform, and vice versa.
                // It's possible internal clock inaccuracy causes both independently.
                //
                // TODO: why do some devices have lots of these and others none? Maybe cellular modems make daily tweaks?
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
            case 7:  // Humidifier setting change (logged in events in 50 series)
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
            case 8:  // CPAP-Check related, follows Mask On in CPAP-Check mode
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                //CHECK_VALUES(data[pos+2], 0, 79);  // probably 16-bit value, sometimes matches OA + H + FL + VS + RE?
                CHECK_VALUE(data[pos+3], 0);
                //CHECK_VALUES(data[pos+4], 0, 10);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0);
                //CHECK_VALUES(data[pos+6], 0, 79);  // probably 16-bit value, usually the same as +2, but not always?
                CHECK_VALUE(data[pos+7], 0);
                //CHECK_VALUES(data[pos+8], 0, 10);  // probably 16-bit value
                CHECK_VALUE(data[pos+9], 0);
                //CHECK_VALUES(data[pos+0xa], 0, 4);  // or 0? 44 when changed pressure mid-session?
                break;
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


bool PRS1DataChunk::ParseSettingsF0V45(const unsigned char* data, int size)
{
    if (size < 0xd) {
        qWarning() << "invalid size passed to ParseSettingsF0V45";
        return false;
    }
    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;

    switch (data[0x02]) {  // PRS1 mode
    case 0x00:
        cpapmode = PRS1_MODE_CPAP;
        break;
    case 0x20:
        cpapmode = PRS1_MODE_BILEVEL;
        break;
    case 0x40:
        cpapmode = PRS1_MODE_AUTOCPAP;
        break;
    case 0x60:
        cpapmode = PRS1_MODE_AUTOBILEVEL;
        break;
    case 0x80:
        cpapmode = PRS1_MODE_AUTOTRIAL;  // Auto-Trial TODO: where is duration?
        break;
    case 0xA0:
        cpapmode = PRS1_MODE_CPAPCHECK;
        break;
    default:
        UNEXPECTED_VALUE(data[0x02], "known device mode");
        break;
    }

    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));

    int min_pressure = data[0x03];
    int max_pressure = data[0x04];
    int min_ps  = data[0x05]; // pressure support
    int max_ps  = data[0x06]; // pressure support

    if (cpapmode == PRS1_MODE_CPAP) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, min_pressure));
        CHECK_VALUE(max_pressure, 0);
        CHECK_VALUE(min_ps, 0);
        CHECK_VALUE(max_ps, 0);
    } else if (cpapmode == PRS1_MODE_AUTOCPAP || cpapmode == PRS1_MODE_AUTOTRIAL) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
        CHECK_VALUE(min_ps, 0);
        CHECK_VALUE(max_ps, 0);
    } else if (cpapmode == PRS1_MODE_CPAPCHECK) {
        // Sometimes the CPAP pressure is stored in max_ps instead of min_ps, not sure why.
        if (min_ps == 0) {
            if (max_ps == 0) UNEXPECTED_VALUE(max_ps, "nonzero");
            min_ps = max_ps;
        } else {
            CHECK_VALUE(max_ps, 0);
        }
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, min_ps));
        // TODO: Once OSCAR can handle more modes, we can include these settings; right now including
        // these settings makes it think this is AutoCPAP.
        //this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
        //this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
    } else if (cpapmode == PRS1_MODE_BILEVEL) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, max_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, max_pressure - min_pressure));
        CHECK_VALUE(min_ps, 0);  // this seems to be unused on fixed bilevel
        CHECK_VALUE(max_ps, 0);  // this seems to be unused on fixed bilevel
    } else if (cpapmode == PRS1_MODE_AUTOBILEVEL) {
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, min_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MAX, max_pressure - min_ps));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_pressure + min_ps));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_pressure));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ps));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, max_ps));
    }

    CHECK_VALUES(data[0x07], 0, 0x20);  // 0x20 seems to be Opti-Start

    int ramp_time = data[0x08];
    int ramp_pressure = data[0x09];
    if (ramp_time > 0) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, ramp_time));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, ramp_pressure));
    }

    quint8 flex = data[0x0a];
    if (this->familyVersion == 5) { if (flex != 0xE1) CHECK_VALUES(flex, 0xA1, 0xA2); }
    this->ParseFlexSettingF0V2345(flex, cpapmode);

    if (this->familyVersion == 5) {
        CHECK_VALUES(data[0x0c], 0x60, 0x70);
    }
    this->ParseHumidifierSetting60Series(data[0x0b], data[0x0c], true);

    if (size <= 0xd) {
        return true;
    }

    int resist_level = (data[0x0d] >> 3) & 7;  // 0x18 resist=3, 0x11 resist=2, 0x28 resist=5
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, (data[0x0d] & 0x40) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, resist_level));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, (data[0x0d] & 0x01) ? 15 : 22));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, (data[0x0d] & 0x02) != 0));
    CHECK_VALUE(data[0x0d] & (0x80|0x04), 0);

    CHECK_VALUE(data[0x0e], 1);

    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, (data[0x0f] & 0x40) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_OFF, (data[0x0f] & 0x10) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_ALERT, (data[0x0f] & 0x04) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, (data[0x0f] & 0x02) != 0));
    CHECK_VALUE(data[0x0f] & (0xA0 | 0x08), 0);
    //CHECK_VALUE(data[0x0f] & 0x01, 0);  // TODO: What is bit 1? It's sometimes set.
    // TODO: Where is altitude compensation set? We've seen it on 261CA.

    CHECK_VALUE(data[0x10], 0);
    int autotrial_duration = data[0x11];
    if (cpapmode == PRS1_MODE_AUTOTRIAL) {
        CHECK_VALUES(autotrial_duration, 7, 30);
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_TRIAL, autotrial_duration));
    } else {
        CHECK_VALUE(autotrial_duration, 0);
    }

    return true;
}


const QVector<PRS1ParsedEventType> ParsedEventsF0V4 = {
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    PRS1AutoPressureSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1VariableBreathingEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1PressureAverageEvent::TYPE,
    PRS1FlexPressureAverageEvent::TYPE,
    PRS1SnoresAtPressureEvent::TYPE,
};

// 460P, 560P[BT], 660P, 760P are F0V4
bool PRS1DataChunk::ParseEventsF0V4()
{
    if (this->family != 0 || this->familyVersion != 4) {
        qWarning() << "ParseEventsF0V4 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const QMap<int,int> event_sizes = { {0,4}, {2,4}, {3,3}, {0xb,4}, {0xd,2}, {0xe,5}, {0xf,5}, {0x10,5}, {0x11,5}, {0x12,4} };
   
    if (chunk_size < 1) {
        // This does occasionally happen in F0V6.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration, value;
    bool is_bilevel = false;
    do {
        code = data[pos++];

        size = 3;  // default size = 2 bytes time delta + 1 byte data
        if (event_sizes.contains(code)) {
            size = event_sizes[code];
        }
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "event" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        startpos = pos;
        if (code != 0x12) {  // This one event has no timestamp in F0V6
            t += data[pos] | (data[pos+1] << 8);
            pos += 2;
        }

        switch (code) {
            //case 0x00:  // never seen
                // NOTE: the original code thought 0x00 had 2 data bytes, unlike the 1 in F0V23.
                // We don't have any sample data with this event, so it's left out here.
            case 0x01:  // Pressure adjustment: note this was 0x02 in F0V23 and is 0x01 in F0V6
                // See notes in ParseEventsF0V6.
                this->AddEvent(new PRS1PressureSetEvent(t, data[pos]));
                break;
            case 0x02:  // Pressure adjustment (bi-level): note that this was 0x03 in F0V23 and is 0x02 in F0V6
                // See notes above on interpolation.
                this->AddEvent(new PRS1IPAPSetEvent(t, data[pos+1]));
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos]));  // EPAP needs to be added second to calculate PS
                is_bilevel = true;
                break;
            case 0x03:  // Adjust Opti-Start pressure
                // On F0V4 this occasionally shows up in the middle of a session.
                // In that cases, the new pressure corresponds to the next night's Opti-Start
                // pressure. It does not appear to have any effect on the current night's pressure,
                // though presumaby it could if there's a long gap between sessions.
                // See F0V6 event 3 for comparison.
                // TODO: Does this occur in bi-level mode?
                this->AddEvent(new PRS1AutoPressureSetEvent(t, data[pos]));
                break;
            case 0x04:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x05:  // RERA
                elapsed = data[pos];
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x06:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            //case 0x08:  // never seen
            //case 0x09:  // never seen
            case 0x0a:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xb, [0x14 and 0x15 on F0V6]?
                elapsed = data[pos++];
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0b:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xa, 0x14 and 0x15?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0c:  // Flow Limitation
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0d:  // Vibratory Snore
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistics below seem to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0e:  // Variable Breathing?
                // TODO: does duration double like it does for PB/LL?
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];  // this is always 60 seconds unless it's at the end, so it seems like elapsed
                CHECK_VALUES(elapsed, 60, 0);
                this->AddEvent(new PRS1VariableBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0f:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                // NOTE: This (and F0V6) doubles the duration, unlike F0V23.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x10:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                // NOTE: This (and F0V6) doubles the duration, unlike F0V23.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x11:  // Statistics
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos]));
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+1]));

                value = data[pos+2];
                if (is_bilevel) {
                    // For bi-level modes, this appears to be the time-weighted average of EPAP and IPAP actually provided.
                    this->AddEvent(new PRS1PressureAverageEvent(t, value));
                } else {
                    // For single-pressure modes, this appears to be the average effective "EPAP" provided by Flex.
                    //
                    // Sample data shows this value around 10.3 cmH2O for a prescribed pressure of 12.0 (C-Flex+ 3).
                    // That's too low for an average pressure over time, but could easily be an average commanded EPAP.
                    // When flex mode is off, this is exactly the current CPAP set point.
                    this->AddEvent(new PRS1FlexPressureAverageEvent(t, value));
                }
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x12:  // Snore count per pressure
                // Some sessions (with lots of ramps) have multiple of these, each with a
                // different pressure. The total snore count across all of them matches the
                // total found in the stats event.
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 1, 2);  // 0 = CPAP pressure, 1 = bi-level EPAP, 2 = bi-level IPAP
                }
                //CHECK_VALUE(data[pos+1], 0x78);  // pressure
                //CHECK_VALUE(data[pos+2], 1);  // 16-bit snore count
                //CHECK_VALUE(data[pos+3], 0);
                value = (data[pos+2] | (data[pos+3] << 8));
                this->AddEvent(new PRS1SnoresAtPressureEvent(t, data[pos], data[pos+1], value));
                break;
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos));
                ok = false;  // unlike F0V6, we don't know the size of unknown events, so we can't recover
                break;
        }
        pos = startpos + size;
    } while (ok && pos < chunk_size);

    if (ok && pos != chunk_size) {
        qWarning() << this->sessionid << (this->size() - pos) << "trailing event bytes";
    }

    this->duration = t;

    return ok;
}


// Based on ParseComplianceF0V4, but this has shorter settings and stats following equipment off.
bool PRS1DataChunk::ParseComplianceF0V5(void)
{
    if (this->family != 0 || (this->familyVersion != 5)) {
        qWarning() << "ParseComplianceF0V5 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 0xf, 7, 4, 0xf, 0, 4, 0, 4 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.
    
    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < ncodes) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } // else if it's past ncodes, we'll log its information below (rather than handle it)
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                //CHECK_VALUES(data[pos], 0x73, 0x31);  // 0x71
            // F0V5 doesn't have a separate settings record like F0V6 does, the settings just follow the EquipmentOn data.
                ok = ParseSettingsF0V45(data, 0x0d);
                CHECK_VALUE(data[pos+0xd], 0);
                CHECK_VALUE(data[pos+0xe], 0);
                CHECK_VALUES(data[pos+0xf], 0, 2);
                break;
            case 2:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                CHECK_VALUES(data[pos+3], 0x60, 0x70);
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                // F0V5 compliance has MaskOff stats unlike all other compliance.
                // This is presumably because the 501V is an Auto-CPAP, so it needs to record titration data.
                //CHECK_VALUES(data[pos+2], 40, 50);   // min pressure
                //CHECK_VALUES(data[pos+3], 40, 150);  // max pressure
                //CHECK_VALUES(data[pos+4], 40, 150);  // Average Device Pressure <= 90% of Time (report is time-weighted per slice, for all sessions)
                //CHECK_VALUES(data[pos+5], 40, 108);  // Auto CPAP Mean Pressure (report is time-weighted per slice, for all sessions)
                                                       // Peak Average Pressure is the maximum "mean pressure" reported in any session.
                //CHECK_VALUES(data[pos+6], 0, 5);     // Apnea or Hypopnea count (probably 16-bit), contributes to AHI
                CHECK_VALUE(data[pos+7],  0);
                //CHECK_VALUES(data[pos+8], 0, 6);     // Apnea or Hypopnea count (probably 16-bit), contributes to AHI
                CHECK_VALUE(data[pos+9],  0);
                //CHECK_VALUES(data[pos+10], 0, 2);    // Average Large Leak minutes (probably 16-bit, report show sum of all slices)
                CHECK_VALUE(data[pos+11], 0);
                //CHECK_VALUES(data[pos+12], 179, 50);  // Average 90% Leak (report is time-weighted per slice)
                //CHECK_VALUES(data[pos+13], 178, 32);  // Average Total Leak (report is time-weighted per slice)
                //CHECK_VALUES(data[pos+14], 180, 36);  // Max leak (report shows max for all slices)
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                CHECK_VALUE(data[pos+2] & ~(0x40|0x02|0x01), 0);
                //CHECK_VALUES(data[pos+3], 0x16, 0x13);  // 22, 19
                if (data[pos+4] > 3) UNEXPECTED_VALUE(data[pos+4], "0-3");
                //CHECK_VALUES(data[pos+5], 0x2F, 0x26);  // 47, 38
                if (data[pos+6] > 7) UNEXPECTED_VALUE(data[pos+6], "0-7");
                break;
            //case 4:  // Time Elapsed?  See ParseComplianceF0V4 if we encounter this.
            case 5:  // Clock adjustment?
                CHECK_VALUE(pos, 1);  // Always first
                CHECK_VALUE(chunk_size, 5);  // and the only record in the session.
                // This looks like it's minor adjustments to the clock, see ParseComplianceF0V4 for details.
                break;
            //case 6:  // Cleared?  See ParseComplianceF0V4 if we encounter this.
            case 7:  // Humidifier setting change (logged in events in 50 series)
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSetting60Series(data[pos+2], data[pos+3]);
                break;
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


//********************************************************************************************
// MARK: -
// MARK: DreamStation

// The below is based on fixing the fileVersion == 3 parsing in ParseSummary() based
// on our understanding of slices from F0V23. The switch values come from sample files.
bool PRS1DataChunk::ParseComplianceF0V6(void)
{
    if (this->family != 0 || this->familyVersion != 6) {
        qWarning() << "ParseComplianceF0V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    // TODO: hardcoding this is ugly, think of a better approach
    if (this->m_data.size() < 82) {
        qWarning() << this->sessionid << "compliance data too short:" << this->m_data.size();
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int expected_sizes[] = { 1, 0x34, 9, 4, 2, 2, 4, 8 };
    static const int ncodes = sizeof(expected_sizes) / sizeof(int);
    for (int i = 0; i < ncodes; i++) {
        if (this->hblock.contains(i)) {
            CHECK_VALUE(this->hblock[i], expected_sizes[i]);
        } else {
            UNEXPECTED_VALUE(this->hblock.contains(i), true);
        }
    }

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
        if (size < expected_sizes[code]) {
            UNEXPECTED_VALUE(size, expected_sizes[code]);
            qWarning() << this->sessionid << "slice" << code << "too small" << size << "<" << expected_sizes[code];
            if (code != 1) {  // Settings are variable-length, so shorter settings slices aren't fatal.
                ok = false;
                break;
            }
        }
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }

        switch (code) {
            case 0:
                // always first? Maybe equipmenton? Maybe 0 was always equipmenton, even in F0V23?
                CHECK_VALUE(pos, 1);
                //CHECK_VALUES(data[pos], 1, 3);  // sometimes 7?
                break;
            case 1:  // Settings
                // This is where ParseSummaryF0V6 started (after "3 bytes that don't follow the pattern")
                // Both compliance and summary files seem to have the same length for this slice, so maybe the
                // settings are the same?
                ok = this->ParseSettingsF0V6(data + pos, size);
                break;
            case 3:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 4:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                break;
            case 7:
                // Always follows mask off?
                //CHECK_VALUES(data[pos], 0x01, 0x00);  // sometimes 32, 4
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUES(data[pos+2], 0x00, 0x01);  // sometimes 11, 3, 15
                CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUE(data[pos+4], 0x05, 0x0A);  // 00
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUE(data[pos+6], 0x64, 0x69);  // 6E, 6D, 6E, 6E, 80
                //CHECK_VALUE(data[pos+7], 0x3d, 0x5c);  // 6A, 6A, 6B, 6C, 80
                break;
            case 2:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUE(data[pos+2], 0x08);  // 0x01
                //CHECK_VALUE(data[pos+3], 0x14);  // 0x12
                //CHECK_VALUE(data[pos+4], 0x01);  // 0x00
                //CHECK_VALUE(data[pos+5], 0x22);  // 0x28
                //CHECK_VALUE(data[pos+6], 0x02);  // sometimes 1, 0
                CHECK_VALUE(data[pos+7], 0x00);  // 0x00
                CHECK_VALUE(data[pos+8], 0x00);  // 0x00
                break;
            case 6:  // Humidier setting change
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


bool PRS1DataChunk::ParseSummaryF0V6(void)
{
    if (this->family != 0 || this->familyVersion != 6) {
        qWarning() << "ParseSummaryF0V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 1, 0x29, 9, 4, 2, 4, 1, 4, 0x1b, 2, 4, 0x0b, 1, 2, 6 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: The sizes contained in hblock can vary, even within a single device, as can the length of hblock itself!

    // TODO: hardcoding this is ugly, think of a better approach
    if (chunk_size < minimum_sizes[0] + minimum_sizes[1] + minimum_sizes[2]) {
        qWarning() << this->sessionid << "summary data too short:" << chunk_size;
        return false;
    }
    if (chunk_size < 55) UNEXPECTED_VALUE(chunk_size, ">= 55");

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
                //CHECK_VALUES(data[pos], 1, 7);  // or 3?
                if (size == 4) {  // 400G has 3 more bytes?
                    //CHECK_VALUE(data[pos+1], 0);  // or 2, 14, 4, etc.
                    //CHECK_VALUES(data[pos+2], 8, 65);  // or 1
                    //CHECK_VALUES(data[pos+3], 0, 20);  // or 21, 22, etc.
                }
                break;
            case 1:  // Settings
                ok = this->ParseSettingsF0V6(data + pos, size);
                break;
            case 3:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 4:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
                break;
            case 8:  // vs. 7 in compliance, always follows mask off (except when there's a 5, see below), also longer
                // Maybe statistics of some kind, given the pressure stats that seem to appear before it on AutoCPAP devices?
                //CHECK_VALUES(data[pos], 0x02, 0x01);  // probably 16-bit value
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUES(data[pos+2], 0x0d, 0x0a);  // probably 16-bit value, maybe OA count?
                CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUES(data[pos+4], 0x09, 0x0b);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUES(data[pos+6], 0x1e, 0x35);  // probably 16-bit value
                CHECK_VALUE(data[pos+7], 0x00);
                //CHECK_VALUES(data[pos+8], 0x8c, 0x4c);  // 16-bit value, not sure what
                //CHECK_VALUE(data[pos+9], 0x00);
                //CHECK_VALUES(data[pos+0xa], 0xbb, 0x00);  // 16-bit minutes in large leak
                //CHECK_VALUE(data[pos+0xb], 0x00);
                //CHECK_VALUES(data[pos+0xc], 0x15, 0x02);  // 16-bit minutes in PB
                //CHECK_VALUE(data[pos+0xd], 0x00);
                //CHECK_VALUES(data[pos+0xe], 0x01, 0x00);  // 16-bit VS count
                //CHECK_VALUE(data[pos+0xf], 0x00);
                //CHECK_VALUES(data[pos+0x10], 0x21, 5);  // probably 16-bit value, maybe H count?
                CHECK_VALUE(data[pos+0x11], 0x00);
                //CHECK_VALUES(data[pos+0x12], 0x13, 0);  // 16-bit value, not sure what
                //CHECK_VALUE(data[pos+0x13], 0x00);
                //CHECK_VALUES(data[pos+0x14], 0x05, 0);  // probably 16-bit value, maybe RE count?
                CHECK_VALUE(data[pos+0x15], 0x00);
                //CHECK_VALUE(data[pos+0x16], 0x00, 4);  // probably a 16-bit value, PB or FL count?
                CHECK_VALUE(data[pos+0x17], 0x00);
                //CHECK_VALUES(data[pos+0x18], 0x69, 0x23);
                //CHECK_VALUES(data[pos+0x19], 0x44, 0x18);
                //CHECK_VALUES(data[pos+0x1a], 0x80, 0x49);
                if (size >= 0x1f) {  // 500X is only 0x1b long!
                    //CHECK_VALUES(data[pos+0x1b], 0x00, 6);
                    CHECK_VALUE(data[pos+0x1c], 0x00);
                    //CHECK_VALUES(data[pos+0x1d], 0x0c, 0x0d);
                    //CHECK_VALUES(data[pos+0x1e], 0x31, 0x3b);
                    // TODO: 400G and 500G has 8 more bytes?
                    // TODO: 400G sometimes has another 4 on top of that?
                }
                break;
            case 2:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUE(data[pos+2], 0x08);  // 0x01
                //CHECK_VALUE(data[pos+3], 0x14);  // 0x12
                //CHECK_VALUE(data[pos+4], 0x01);  // 0x00
                //CHECK_VALUE(data[pos+5], 0x22);  // 0x28
                //CHECK_VALUE(data[pos+6], 0x02);  // sometimes 1, 0
                CHECK_VALUE(data[pos+7], 0x00);  // 0x00
                CHECK_VALUE(data[pos+8], 0x00);  // 0x00
                if (size == 0x0c) {  // 400G has 3 more bytes, seem to match Equipment On bytes
                    //CHECK_VALUE(data[pos+1], 0);
                    //CHECK_VALUES(data[pos+2], 8, 65);
                    //CHECK_VALUE(data[pos+3], 0);
                }
                break;
            case 0x09:  // Time Elapsed (event 4 in F0V4)
                tt += data[pos] | (data[pos+1] << 8);
                break;
            case 0x0a:  // Humidifier setting change
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                this->ParseHumidifierSettingV3(data[pos+2], data[pos+3]);
                break;
            case 0x0d:  // ???
                // seen on one 500G multiple times
                //CHECK_VALUE(data[pos], 0);  // 16-bit value
                //CHECK_VALUE(data[pos+1], 0);
                break;
            case 0x0e:
                // only seen once on 400G, many times on 500G
                //CHECK_VALUES(data[pos], 0, 6);  // 16-bit value
                //CHECK_VALUE(data[pos+1], 0);
                //CHECK_VALUES(data[pos+2], 7, 9);
                //CHECK_VALUES(data[pos+3], 7, 15);
                //CHECK_VALUES(data[pos+4], 7, 12);
                //CHECK_VALUES(data[pos+5], 0, 3);
                break;
            case 0x05:
                // AutoCPAP-related? First appeared on 500X, follows 4, before 8, look like pressure values
                //CHECK_VALUE(data[pos], 0x4b);    // maybe min pressure? (matches ramp pressure, see ramp on pressure graph)
                //CHECK_VALUE(data[pos+1], 0x5a);  // maybe max pressure? (close to max on pressure graph, time at pressure graph)
                //CHECK_VALUE(data[pos+2], 0x5a);  // seems to match Average 90% Pressure
                //CHECK_VALUE(data[pos+3], 0x58);  // seems to match Average CPAP
                break;
            case 0x07:
                // AutoBiLevel-related? First appeared on 700X, follows 4, before 8, looks like pressure values
                //CHECK_VALUE(data[pos], 0x50);    // maybe min IPAP or max titrated EPAP? (matches time at pressure graph, auto bi-level summary)
                //CHECK_VALUE(data[pos+1], 0x64);  // maybe max IPAP or max titrated IPAP? (matches time at pressure graph, auto bi-level summary)
                //CHECK_VALUE(data[pos+2], 0x4b);  // seems to match 90% EPAP
                //CHECK_VALUE(data[pos+3], 0x64);  // seems to match 90% IPAP
                break;
            case 0x0b:
                // CPAP-Check related, follows Mask On in CPAP-Check mode
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                //CHECK_VALUE(data[pos+2], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+3], 0);
                //CHECK_VALUE(data[pos+4], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0);
                //CHECK_VALUE(data[pos+6], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+7], 0);
                //CHECK_VALUE(data[pos+8], 0);  // probably 16-bit value
                CHECK_VALUE(data[pos+9], 0);
                //CHECK_VALUES(data[pos+0xa], 20, 60);  // or 0? 44 when changed pressure mid-session?
                break;
            case 0x06:
                // Maybe starting pressure? follows 4, before 8, looks like a pressure value, seen with CPAP-Check and EZ-Start
                // Maybe ending pressure: matches ending CPAP-Check pressure if it changes mid-session.
                // TODO: The daily details will show when it changed, so maybe there's an event that indicates a pressure change.
                //CHECK_VALUES(data[pos], 90, 60);  // maybe CPAP-Check pressure, also matches EZ-Start Pressure
                break;
            case 0x0c:
                // EZ-Start pressure for Auto-CPAP, seen on 500X110 following 4, before 8
                // Appears to reflect the current session's EZ-Start pressure, though reported afterwards
                //CHECK_VALUE(data[pos], 70, 80);
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


// The below is based on a combination of the old mainblock parsing for fileVersion == 3
// in ParseSummary() and the switch statements of ParseSummaryF0V6.
//
// Both compliance and summary files (at least for 200X and 400X devices) seem to have
// the same length for this slice, so maybe the settings are the same? At least 0x0a
// looks like a pressure in compliance files.
bool PRS1DataChunk::ParseSettingsF0V6(const unsigned char* data, int size)
{
    static const QMap<int,int> expected_lengths = { {0x0c,3}, {0x0d,2}, {0x0e,2}, {0x0f,4}, {0x10,3}, {0x35,2} };
    bool ok = true;

    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;
    FlexMode flexmode = FLEX_Unknown;

    int pressure = 0;
    int imin_ps   = 0;
    int imax_ps   = 0;
    int min_pressure = 0;
    int max_pressure = 0;
    bool ramp_type_set = false;

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
                case 0: cpapmode = PRS1_MODE_CPAP; break;
                case 1: cpapmode = PRS1_MODE_BILEVEL; break;
                case 2: cpapmode = PRS1_MODE_AUTOCPAP; break;
                case 3: cpapmode = PRS1_MODE_AUTOBILEVEL; break;
                case 4: cpapmode = PRS1_MODE_CPAPCHECK; break;
                default:
                    UNEXPECTED_VALUE(data[pos], "known device mode");
                    break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
                break;
            case 1: // ???
                CHECK_VALUES(len, 1, 2);
                if (data[pos] != 0 && data[pos] != 3) {
                    CHECK_VALUES(data[pos], 1, 2);  // 1 when EZ-Start is enabled? 2 when Auto-Trial? 3 when Auto-Trial is off or Opti-Start isn't off?
                }
                if (len == 2) {  // 400G, 500G has extra byte
                    switch (data[pos+1]) {
                        case 0x00:  // 0x00 seen with EZ-Start disabled, no auto-trial, with CPAP-Check on 400X110
                        case 0x10:  // 0x10 seen with EZ-Start enabled, Opti-Start off on 500X110
                        case 0x20:  // 0x20 seen with Opti-Start enabled
                        case 0x30:  // 0x30 seen with both Opti-Start and EZ-Start enabled on 500X110
                        case 0x40:  // 0x40 seen with Auto-Trial
                        case 0x80:  // 0x80 seen with EZ-Start and CPAP-Check+ on 500X150
                            break;
                        default:
                            UNEXPECTED_VALUE(data[pos+1], "[0,0x10,0x20,0x30,0x40,0x80]")
                    }
                }
                break;
            case 0x0a:  // CPAP pressure setting
                CHECK_VALUE(len, 1);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAP);
                pressure = data[pos];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, pressure));
                break;
            case 0x0c:  // CPAP-Check pressure setting
                CHECK_VALUE(len, 3);
                CHECK_VALUE(cpapmode, PRS1_MODE_CPAPCHECK);
                min_pressure = data[pos];  // Min Setting on pressure graph
                max_pressure = data[pos+1];  // Max Setting on pressure graph
                pressure = data[pos+2];  // CPAP on pressure graph and CPAP-Check Pressure on settings detail
                // This seems to be the initial pressure. If the pressure changes mid-session, the pressure
                // graph will show either the changed pressure or the majority pressure, not sure which.
                // The time of change is most likely in the events file. See slice 6 for ending pressure.
                //CHECK_VALUE(pressure, 0x5a);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE, pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x0d:  // AutoCPAP pressure setting
                CHECK_VALUE(len, 2);
                CHECK_VALUE(cpapmode, PRS1_MODE_AUTOCPAP);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x0e:  // Bi-Level pressure setting
                CHECK_VALUE(len, 2);
                CHECK_VALUE(cpapmode, PRS1_MODE_BILEVEL);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                imin_ps = max_pressure - min_pressure;
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP, max_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS, imin_ps));
                break;
            case 0x0f:  // Auto Bi-Level pressure setting
                CHECK_VALUE(len, 4);
                CHECK_VALUE(cpapmode, PRS1_MODE_AUTOBILEVEL);
                min_pressure = data[pos];
                max_pressure = data[pos+1];
                imin_ps = data[pos+2];
                imax_ps = data[pos+3];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, max_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, imin_ps));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, imax_ps));
                break;
            case 0x10: // Auto-Trial mode
                // This is not encoded as a separate mode as in F0V4, but instead as an auto-trial
                // duration on top of the CPAP or CPAP-Check mode. Reports show Auto-CPAP results,
                // but curiously report the use of C-Flex+, even though Auto-CPAP uses A-Flex.
                CHECK_VALUE(len, 3);
                CHECK_VALUES(cpapmode, PRS1_MODE_CPAP, PRS1_MODE_CPAPCHECK);
                if (data[pos] < 5 || data[pos] > 30) {  // We've seen 5, 9, 14, 25, and 30
                    UNEXPECTED_VALUE(data[pos], "5-30");  // Auto-Trial Duration
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_TRIAL, data[pos]));
                // If we want C-Flex+ to be reported as A-Flex, we can set cpapmode = PRS1_MODE_AUTOTRIAL here.
                // (Note that the setting event has already been added above, which is why ImportSummary needs
                // to adjust it when it sees this setting.)
                min_pressure = data[pos+1];
                max_pressure = data[pos+2];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MIN, min_pressure));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PRESSURE_MAX, max_pressure));
                break;
            case 0x2a:  // EZ-Start
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0x00, 0x80);  // both seem to mean enabled
                // 0x80 is CPAP Mode - EZ-Start in pressure detail chart, 0x00 is just CPAP mode with no EZ-Start pressure
                // TODO: How to represent which one is active in practice? Should this always be "true" since
                // either value means that the setting is enabled?
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_EZ_START, data[pos] != 0));
                break;
            case 0x42:  // EZ-Start enabled for Auto-CPAP?
                // Seen on 500X110 before 0x2b when EZ-Start is enabled on Auto-CPAP
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0x00, 0x80);  // both seem to mean enabled, 0x00 appears when Opti-Start is used instead
                // TODO: How to represent which one is active in practice? Should this always be "true" since
                // either value means that the setting is enabled?
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_EZ_START, data[pos] != 0));
                break;
            case 0x2b:  // Ramp Type
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);  // 0 == "Linear", 0x80 = "SmartRamp"
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TYPE, data[pos] != 0));
                ramp_type_set = true;
                break;
            case 0x2c:  // Ramp Time
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == ramp off, and ramp pressure setting doesn't appear
                    if (data[pos] < 5 || data[pos] > 45) UNEXPECTED_VALUE(data[pos], "5-45");
                }
                if (!ramp_type_set) {
                    // If there's a ramp time that's neither linear nor SmartRamp, then it's Ramp+.
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TYPE, 2));
                    ramp_type_set = true;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, data[pos]));
                break;
            case 0x2d:  // Ramp Pressure
                CHECK_VALUE(len, 1);
                // 0 = Off for Ramp+ (since time is always set)
                // Turning it on during therapy creates a new session.
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, data[pos]));
                break;
            case 0x2e:  // Flex mode
                CHECK_VALUE(len, 1);
                switch (data[pos]) {
                case 0:
                    flexmode = FLEX_None;
                    break;
                case 0x80:
                    switch (cpapmode) {
                        case PRS1_MODE_CPAP:
                        case PRS1_MODE_CPAPCHECK:
                        case PRS1_MODE_AUTOCPAP:
                        //case PRS1_MODE_AUTOTRIAL:
                            flexmode = FLEX_CFlex;
                            break;
                        case PRS1_MODE_BILEVEL:
                        case PRS1_MODE_AUTOBILEVEL:
                            flexmode = FLEX_BiFlex;
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "untested mode");
                            break;
                    }
                    break;
                case 0x90:  // C-Flex+ or A-Flex, depending on device mode
                    switch (cpapmode) {
                    case PRS1_MODE_CPAP:
                    case PRS1_MODE_CPAPCHECK:
                        flexmode = FLEX_CFlexPlus;
                        break;
                    case PRS1_MODE_AUTOCPAP:
                        flexmode = FLEX_AFlex;
                        break;
                    default:
                        UNEXPECTED_VALUE(cpapmode, "cpap or apap");
                        break;
                    }
                    break;
                case 0xA0:  // Rise Time
                    flexmode = FLEX_RiseTime;
                    switch (cpapmode) {
                        case PRS1_MODE_BILEVEL:
                        case PRS1_MODE_AUTOBILEVEL:
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "autobilevel");
                            break;
                    }
                    break;
                case 0xB0:  // P-Flex
                    flexmode = FLEX_PFlex;
                    switch (cpapmode) {
                        case PRS1_MODE_AUTOCPAP:
                            break;
                        default:
                            HEX(flexmode);
                            UNEXPECTED_VALUE(cpapmode, "apap");
                            break;
                    }
                    break;
                default:
                    UNEXPECTED_VALUE(data[pos], "known flex mode");
                    break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, flexmode));
                break;
            case 0x2f:  // Flex lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                // DS2 doesn't have a specific flex lock. See patient controls access below.
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[pos] != 0));
                break;
            case 0x30:  // Flex level
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[pos]));
                if (flexmode == FLEX_PFlex) {
                    CHECK_VALUE(data[pos], 4);  // No number appears on reports.
                }
                if (flexmode == FLEX_RiseTime) {
                    if (data[pos] < 1 || data[pos] > 3) UNEXPECTED_VALUE(data[pos], "1-3");
                }
                break;
            case 0x35:  // Humidifier setting
                CHECK_VALUE(len, 2);
                this->ParseHumidifierSettingV3(data[pos], data[pos+1], true);
                break;
            case 0x36:  // Mask Resistance Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                // DS2 doesn't have a mask resistance lock, as the resistance setting is only in the provider menu.
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
                // DS2 doesn't have a tubing type lock, it is always available (unless a heated tube is auto-detected).
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, data[pos] != 0));
                break;
            case 0x3b:  // Tubing Type
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 2, 1);  // 15HT = 2, 15 = 1, 22 = 0
                }
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x40:  // new to 400G, also seen on 500X110, alternate tubing type? appears after 0x39 and before 0x3c
                CHECK_VALUE(len, 1);
                if (data[pos] > 3) UNEXPECTED_VALUE(data[pos], "0-3");  // 0 = 22mm, 1 = 15mm, 2 = 15HT, 3 = 12mm
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x3c:  // View Optional Screens
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, data[pos] != 0));
                break;
            case 0x3e:  // Auto On
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, data[pos] != 0));
                break;
            case 0x3f:  // Auto Off
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_OFF, data[pos] != 0));
                break;
            case 0x43:  // new to 502G, sessions 3-8, Auto-Trial is off, Opti-Start is missing
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0x3C);
                break;
            case 0x44:  // new to 502G, sessions 3-8, Auto-Trial is off, Opti-Start is missing
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0xFF);
                break;
            case 0x45:  // Target Time, specific to DreamStation Go
                CHECK_VALUE(len, 1);
                // Included in the data, but not shown on reports when humidifier is in Fixed mode.
                // According to the FAQ, this setting is only available in Adaptive mode.
                if (data[pos] < 40 || data[pos] > 100) {  // 4.0 through 10.0 hours in 0.5-hour increments
                    CHECK_VALUES(data[pos], 0, 1);  // Off and Auto
                }
                this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_HUMID_TARGET_TIME, data[pos], 0.1));
                break;
            case 0x46:  // Tubing Type (alternate, seen instead of 0x3b on 700X110 v1.2 firmware and on DS2)
                CHECK_VALUE(len, 1);
                if (data[pos] > 4) UNEXPECTED_VALUE(data[pos], "0-4");  // 0 = 22mm, 1 = 15mm, 2 = 15HT, 3 = 12mm, 4 = HT12
                // TODO: Confirm that 4 is 12HT and update ParseTubingTypeV3.
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x48:  // ??? Seen on DreamStation 2 non-Advanced (410) but not either Advanced (420 or 520)
                // Appears between 0x2C (ramp time) and 0x2E (flex mode), with a value of 0-4.
                CHECK_VALUE(len, 1);
                if (data[pos] > 4) {
                    UNEXPECTED_VALUE(data[pos], "0-4");
                }
                //this->AddEvent(new PRS1UnknownDataEvent(QByteArray((const char*) data, size), pos, len));
                break;
            case 0x4a:  // Patient controls access, specific to DreamStation 2.
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                // Turning off patient controls access essentially locks only flex and ramp time.
                // Humidification, heated tube temperature, and ramp level are still adjustable during therapy.
                // (DS2 doesn't have a separate flex lock setting.)
                if (data[pos] == 0) {
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, true));
                }
                break;
            case 0x4b:  // Patient data access, specific to DreamStation 2.
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                // Turning off patient data access hides both AHI and on-device reports.
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


const QVector<PRS1ParsedEventType> ParsedEventsF0V6 = {
    PRS1PressureSetEvent::TYPE,
    PRS1IPAPSetEvent::TYPE,
    PRS1EPAPSetEvent::TYPE,
    PRS1AutoPressureSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1RERAEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1VariableBreathingEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1PressureAverageEvent::TYPE,
    PRS1FlexPressureAverageEvent::TYPE,
    PRS1SnoresAtPressureEvent::TYPE,
};

// DreamStation family 0 CPAP/APAP devices (400X-700X, 400G-502G)
// Originally derived from F5V3 parsing + (incomplete) F0V234 parsing + sample data
bool PRS1DataChunk::ParseEventsF0V6()
{
    if (this->family != 0 || this->familyVersion != 6) {
        qWarning() << "ParseEventsF0V6 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 2, 3, 4, 3, 3, 3, 3, 3, 3, 2, 3, 4, 3, 2, 5, 5, 5, 5, 4, 3, 3, 3 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);

    if (chunk_size < 1) {
        // This does occasionally happen.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration, value;
    bool is_bilevel = false;
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
        if (code != 0x12) {  // This one event has no timestamp
            t += data[pos] | (data[pos+1] << 8);
            pos += 2;
        }

        switch (code) {
            //case 0x00:  // never seen
            case 0x01:  // Pressure adjustment
                // Matches pressure setting, both initial and when ramp button pressed.
                // Based on waveform reports, it looks like the pressure graph is drawn by
                // interpolating between these pressure adjustments, by 0.5 cmH2O spaced evenly between
                // adjustments. E.g. 6 at 28:11 and 7.3 at 29:05 results in the following dots:
                // 6 at 28:11, 6.5 around 28:30, 7.0 around 28:50, 7(.3) at 29:05. That holds until
                // subsequent "adjustment" of 7.3 at 30:09 followed by 8.0 at 30:19.
                this->AddEvent(new PRS1PressureSetEvent(t, data[pos]));
                break;
            case 0x02:  // Pressure adjustment (bi-level)
                // See notes above on interpolation.
                this->AddEvent(new PRS1IPAPSetEvent(t, data[pos+1]));
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos]));  // EPAP needs to be added second to calculate PS
                is_bilevel = true;
                break;
            case 0x03:  // Auto-CPAP starting pressure
                // Most of the time this occurs, it's at the start and end of a session with
                // the same pressure at both. Occasionally an additional event shows up in the
                // middle of a session, and then the pressure at the end matches that.
                // In these cases, the new pressure corresponds to the next night's starting
                // pressure for auto-CPAP. It does not appear to have any effect on the current
                // night's pressure, unless there's a substantial gap between sessions, in
                // which case the next session may use the new starting pressure.
                //CHECK_VALUE(data[pos], 40);
                // TODO: What does this mean in bi-level mode?
                // See F0V4 event 3 for comparison. TODO: See if there's an Opti-Start label on F0V6 reports.
                this->AddEvent(new PRS1AutoPressureSetEvent(t, data[pos]));
                break;
            case 0x04:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x05:  // RERA
                elapsed = data[pos];  // based on sample waveform, the RERA is over after this
                this->AddEvent(new PRS1RERAEvent(t - elapsed, 0));
                break;
            case 0x06:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x07:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            //case 0x08:  // never seen
            //case 0x09:  // never seen
            //case 0x0a:  // Hypopnea, see 0x15
            case 0x0b:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xa, 0x14 and 0x15?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x0c:  // Flow Limitation
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0d:  // Vibratory Snore
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistics below seem to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0e:  // Variable Breathing?
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];  // this is always 60 seconds unless it's at the end, so it seems like elapsed
                CHECK_VALUES(elapsed, 60, 0);
                this->AddEvent(new PRS1VariableBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0f:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x10:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x11:  // Statistics
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos]));
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+1]));

                value = data[pos+2];
                if (is_bilevel) {
                    // For bi-level modes, this appears to be the time-weighted average of EPAP and IPAP actually provided.
                    this->AddEvent(new PRS1PressureAverageEvent(t, value));
                } else {
                    // For single-pressure modes, this appears to be the average effective "EPAP" provided by Flex.
                    //
                    // Sample data shows this value around 10.3 cmH2O for a prescribed pressure of 12.0 (C-Flex+ 3).
                    // That's too low for an average pressure over time, but could easily be an average commanded EPAP.
                    // When flex mode is off, this is exactly the current CPAP set point.
                    this->AddEvent(new PRS1FlexPressureAverageEvent(t, value));
                }
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x12:  // Snore count per pressure
                // Some sessions (with lots of ramps) have multiple of these, each with a
                // different pressure. The total snore count across all of them matches the
                // total found in the stats event.
                if (data[pos] != 0) {
                    CHECK_VALUES(data[pos], 1, 2);  // 0 = CPAP pressure, 1 = bi-level EPAP, 2 = bi-level IPAP
                }
                //CHECK_VALUE(data[pos+1], 0x78);  // pressure
                //CHECK_VALUE(data[pos+2], 1);  // 16-bit snore count
                //CHECK_VALUE(data[pos+3], 0);
                value = (data[pos+2] | (data[pos+3] << 8));
                this->AddEvent(new PRS1SnoresAtPressureEvent(t, data[pos], data[pos+1], value));
                break;
            //case 0x13:  // never seen
            case 0x0a:  // Hypopnea
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x14:  // Hypopnea, new to F0V6
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x15:  // Hypopnea, new to F0V6
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating hypopneas ourselves. Their official definition
                // is 40% reduction in flow lasting at least 10s.
                duration = data[pos];
                this->AddEvent(new PRS1HypopneaEvent(t - duration, 0));
                break;
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
