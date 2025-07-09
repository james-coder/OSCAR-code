/* PRS1 Parsing for BiPAP autoSV (ASV) (Family 5)
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
// MARK: -
// MARK: 50 and 60 Series

// borrowed largely from ParseSummaryF0V4
bool PRS1DataChunk::ParseSummaryF5V012(void)
{
    if (this->family != 5 || (this->familyVersion > 2)) {
        qWarning() << "ParseSummaryF5V012 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    QVector<int> minimum_sizes;
    switch (this->familyVersion) {
        case 0: minimum_sizes = { 0x12, 4, 3, 0x1f, 0, 4, 0, 2, 2 }; break;
        case 1: minimum_sizes = { 0x13, 7, 5, 0x20, 0, 4, 0, 2, 2, 4 }; break;
        case 2: minimum_sizes = { 0x13, 7, 5, 0x22, 0, 4, 0, 2, 2, 4 }; break;
    }
    // NOTE: These are fixed sizes, but are called minimum to more closely match the F0V6 parser.

    bool ok = true;
    int pos = 0;
    int code, size;
    int tt = 0;
    while (ok && pos < chunk_size) {
        code = data[pos++];
        // There is no hblock prior to F0V6.
        size = 0;
        if (code < minimum_sizes.length()) {
            // make sure the handlers below don't go past the end of the buffer
            size = minimum_sizes[code];
        } else {
            // We can't defer warning until later, because F5V0 doesn't have slice 9.
            UNEXPECTED_VALUE(code, "known slice code");
            ok = false;  // unlike F0V6, we don't know the size of unknown slices, so we can't recover
            break;
        }
        if (pos + size > chunk_size) {
            qWarning() << this->sessionid << "slice" << code << "@" << pos << "longer than remaining chunk";
            ok = false;
            break;
        }
        
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first
                /*
                CHECK_VALUE(data[pos] & 0xF0, 0);  // TODO: what are these?
                if ((data[pos] & 0x0F) != 1) {  // This is the most frequent value.
                    //CHECK_VALUES(data[pos] & 0x0F, 3, 5);  // TODO: what are these? 0 seems to be related to errors.
                }
                */
            // F5V012 doesn't have a separate settings record like F5V3 does, the settings just follow the EquipmentOn data.
                ok = this->ParseSettingsF5V012(data, size);
                /*
                CHECK_VALUE(data[pos+0x11], 0);
                CHECK_VALUE(data[pos+0x12], 0);
                CHECK_VALUE(data[pos+0x13], 0);
                CHECK_VALUE(data[pos+0x14], 0);
                CHECK_VALUE(data[pos+0x15], 0);
                CHECK_VALUE(data[pos+0x16], 0);
                CHECK_VALUE(data[pos+0x17], 0);
                */
                break;
            case 2:  // Mask On
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOn));
                /*
                //CHECK_VALUES(data[pos+2], 120, 110);  // probably initial pressure
                //CHECK_VALUE(data[pos+3], 0);  // initial IPAP on bilevel?
                //CHECK_VALUES(data[pos+4], 0, 130);  // minimum pressure in auto-cpap
                this->ParseHumidifierSetting60Series(data[pos+5], data[pos+6]);
                */
                break;
            case 3:  // Mask Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, MaskOff));
            // F5V012 doesn't have a separate stats record like F5V3 does, the stats just follow the MaskOff data.
                /*
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
                //CHECK_VALUE(data[pos+0xf], 0);  // CA count, probably 16-bit
                CHECK_VALUE(data[pos+0x10], 0);
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
                */
                break;
            case 1:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                if (this->familyVersion == 0) {
                    //CHECK_VALUE(data[pos+2], 1);  // Usually 1, also seen 0, 6, and 7.
                    ParseHumidifierSetting50Series(data[pos+3]);
                }
                /* Possibly F5V12?
                CHECK_VALUE(data[pos+2] & ~(0x40|8|4|2|1), 0);  // ???, seen various bit combinations
                //CHECK_VALUE(data[pos+3], 0x19);  // 0x17, 0x16
                //CHECK_VALUES(data[pos+4], 0, 1);  // or 2
                //CHECK_VALUE(data[pos+5], 0x35);  // 0x36, 0x36
                if (data[pos+6] != 1) {  // This is the usual value.
                    CHECK_VALUE(data[pos+6] & ~(8|4|2|1), 0);  // On F0V23 0 seems to be related to errors, 3 seen after 90 sec large leak before turning off?
                }
                // pos+4 == 2, pos+6 == 10 on the session that had a time-elapsed event, maybe it shut itself off
                // when approaching 24h of continuous use?
                */
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
            case 7:  // Time Elapsed?
                tt += data[pos] | (data[pos+1] << 8);  // This adds to the total duration (otherwise it won't match report)
                break;
            case 8:  // Time Elapsed? How is this different from 7?
                tt += data[pos] | (data[pos+1] << 8);  // This also adds to the total duration (otherwise it won't match report)
                break;
            case 9:  // Humidifier setting change, F5V1 and F5V2 only
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


bool PRS1DataChunk::ParseSettingsF5V012(const unsigned char* data, int /*size*/)
{
    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;

    float GAIN = PRS1PressureSettingEvent::GAIN;
    if (this->familyVersion == 2) GAIN = 0.125f;  // TODO: parameterize this somewhere better

    int imax_pressure = data[0x2];
    int imin_epap = data[0x3];
    int imax_epap = data[0x4];
    int imin_ps = data[0x5];
    int imax_ps = data[0x6];

    // Only one mode available, so apparently there's no byte in the settings that encodes it?
    cpapmode = PRS1_MODE_ASV;
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));

    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, imin_epap, GAIN));
    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MAX, imax_epap, GAIN));
    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, imin_epap + imin_ps, GAIN));
    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, imax_pressure, GAIN));
    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, imin_ps, GAIN));
    this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, imax_ps, GAIN));

    //CHECK_VALUE(data[0x07], 1, 2);  // 1 = backup breath rate "Auto"; 2 = fixed BPM, see below
    //CHECK_VALUE(data[0x08], 0);     // backup "Breath Rate" in mode 2
    //CHECK_VALUE(data[0x09], 0);     // backup "Timed Inspiration" (gain 0.1) in mode 2
    int pos = 0x7;
    int backup_mode = data[pos];
    int breath_rate;
    int timed_inspiration;
    switch (backup_mode) {
        case 0:
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Off));
            break;
        case 1:
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Auto));
            CHECK_VALUE(data[pos+1], 0);
            CHECK_VALUE(data[pos+2], 0);
            break;
        case 2:
            breath_rate = data[pos+1];
            timed_inspiration = data[pos+2];
            if (breath_rate < 4 || breath_rate > 29) UNEXPECTED_VALUE(breath_rate, "4-29");
            if (timed_inspiration < 5 || timed_inspiration > 30) UNEXPECTED_VALUE(timed_inspiration, "5-30");
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Fixed));
            this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_RATE, breath_rate));
            this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_BACKUP_TIMED_INSPIRATION, timed_inspiration, 0.1));
            break;
        default:
            UNEXPECTED_VALUE(backup_mode, "0-2");
            break;
    }

    int ramp_time = data[0x0a];
    int ramp_pressure = data[0x0b];
    if (ramp_time > 0) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, ramp_time));
        this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, ramp_pressure, GAIN));
    }

    quint8 flex = data[0x0c];
    this->ParseFlexSettingF5V012(flex, cpapmode);

    if (this->familyVersion == 0) {  // TODO: either split this into two functions or use size to differentiate like FV3 parsers do
        this->ParseHumidifierSetting50Series(data[0x0d], true);
        pos = 0xe;
    } else {
        // 60-Series devices have a 2-byte humidfier setting.
        this->ParseHumidifierSetting60Series(data[0x0d], data[0x0e], true);
        pos = 0xf;
    }

    // TODO: may differ between F5V0 and F5V12
    // 0x01, 0x41 = auto-on, view AHI, tubing type = 15
    // 0x41, 0x41 = auto-on, view AHI, tubing type = 15, resist lock
    // 0x42, 0x01 = (no auto-on), view AHI, tubing type = 22, resist lock, tubing lock
    // 0x00, 0x41 = auto-on, view AHI, tubing type = 22, no tubing lock
    // 0x0B, 0x41 = mask resist 1, tube lock, tubing type = 15, auto-on, view AHI
    // 0x09, 0x01 = mask resist 1, tubing 15, view AHI
    // 0x19, 0x41 = mask resist 3, tubing 15, auto-on, view AHI
    // 0x29, 0x41 = mask resist 5, tubing 15, auto-on, view AHI
    //          1 = view AHI
    //         4  = auto-on
    //    1       = tubing type: 0=22, 1=15
    //    2       = tubing lock
    //   38       = mask resist level
    //   4        = resist lock
    int resist_level = (data[pos] >> 3) & 7;  // 0x09 resist=1, 0x11 resist=2, 0x19=resist 3, 0x29=resist 5
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, (data[pos] & 0x40) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, resist_level));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_HOSE_DIAMETER, (data[pos] & 0x01) ? 15 : 22));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_TUBING_LOCK, (data[pos] & 0x02) != 0));
    CHECK_VALUE(data[pos] & (0x80|0x04), 0);
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, (data[pos+1] & 0x40) != 0));
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, (data[pos+1] & 1) != 0));
    CHECK_VALUE(data[pos+1] & ~(0x40|1), 0);
    
    int apnea_alarm = data[pos+2];
    int low_mv_alarm = data[pos+3];
    int disconnect_alarm = data[pos+4];
    if (apnea_alarm) {
        CHECK_VALUES(apnea_alarm, 1, 3);  // 1 = apnea alarm 10, 3 = apnea alarm 30
    }
    if (low_mv_alarm) {
        if (low_mv_alarm < 20 || low_mv_alarm > 99) {
            UNEXPECTED_VALUE(low_mv_alarm, "20-99");  // we've seen 20, 80 and 99, all of which correspond to the number on the report
        }
    }
    if (disconnect_alarm) {
        CHECK_VALUES(disconnect_alarm, 1, 2);  // 1 = disconnect alarm 15, 2 = disconnect alarm 60
    }

    return true;
}


// Flex F5V0 confirmed
// 0x81 = Bi-Flex 1 (ASV mode)
// 0x82 = Bi-Flex 2 (ASV mode)
// 0x83 = Bi-Flex 3 (ASV mode)

// Flex F5V1 confirmed
// 0x81 = Bi-Flex 1 (ASV mode)
// 0x82 = Bi-Flex 2 (ASV mode)
// 0x83 = Bi-Flex 3 (ASV mode)
// 0xC9 = Rise Time 1, Rise Time Lock (ASV mode)
// 0x8A = Rise Time 2 (ASV mode) (Shows "ASV - None" in mode summary, but then rise time in details)
// 0x8B = Rise Time 3 (ASV mode) (breath rate auto)
// 0x08 = Rise Time 2 (ASV mode) (falls back to level=2? bits encode level=0)

// Flex F5V2 confirmed
// 0x02 = Bi-Flex 2 (ASV mode) (breath rate auto, but min/max PS=0)
// this could be different from F5V01, or PS=0 could disable flex?

//   8  = ? (once was 0 when rise time was on and backup breathing was off, rise time level was also 0 in that case)
//          (was also 0 on F5V2)
//   4  = Rise Time Lock
//    8 = Rise Time (vs. Bi-Flex)
//    3 = level

void PRS1DataChunk::ParseFlexSettingF5V012(quint8 flex, int cpapmode)
{
    FlexMode flexmode = FLEX_Unknown;
    bool valid    = (flex & 0x80) != 0;
    bool lock     = (flex & 0x40) != 0;
    bool risetime = (flex & 0x08) != 0;
    int flexlevel = flex & 0x03;

    if (flex & (0x20 | 0x10 | 0x04)) UNEXPECTED_VALUE(flex, "known bits");
    CHECK_VALUE(cpapmode, PRS1_MODE_ASV);
    if (this->familyVersion == 0) {
        CHECK_VALUE(valid, true);
        CHECK_VALUE(lock, false);
        CHECK_VALUE(risetime, false);
    } else if (this->familyVersion == 1) {
        if (valid == false) {
            CHECK_VALUE(flex, 0x08);
            flexlevel = 2;  // These get reported as Rise Time 2
            valid = true;
        }
    } else {
        CHECK_VALUE(flex, 0x02);  // only seen one example, unsure if it matches F5V01; seems to encode Bi-Flex 2
        valid = true;  // add the flex mode and setting to the parsed settings
    }
    if (flexlevel == 0 || flexlevel >3) UNEXPECTED_VALUE(flexlevel, "1-3");

    CHECK_VALUE(valid, true);
    if (risetime) {
        flexmode = FLEX_RiseTime;
    } else {
        flexmode = FLEX_BiFlex;
    }
    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, (int) flexmode));

    if (flexmode == FLEX_BiFlex) {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, flexlevel));
        CHECK_VALUE(lock, 0);  // Flag any sample data that will let us confirm flex lock
        //this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, lock != 0));
    } else {
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME, flexlevel));
        this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME_LOCK, lock != 0));
    }
}


const QVector<PRS1ParsedEventType> ParsedEventsF5V0 = {
    PRS1EPAPSetEvent::TYPE,
    // No PP, unlike F5V1
    PRS1TimedBreathEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1IPAPAverageEvent::TYPE,
    PRS1IPAPLowEvent::TYPE,
    PRS1IPAPHighEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    // No LEAK, unlike F5V1
};

// 950P is F5V0
bool PRS1DataChunk::ParseEventsF5V0(void)
{
    if (this->family != 5 || this->familyVersion != 0) {
        qWarning() << "ParseEventsF5V0 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const QMap<int,int> event_sizes = { {1,2}, {3,4}, {8,4}, {0xa,2}, {0xb,5}, {0xc,5}, {0xd,0xc} };

    if (chunk_size < 1) {
        // This does occasionally happen in F0V6.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration;
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
        t += data[pos] | (data[pos+1] << 8);
        pos += 2;

        switch (code) {
            case 0x00:  // Humidifier setting change (logged in summary in 60 series)
                this->ParseHumidifierSetting50Series(data[pos]);
                break;
            //case 0x01:  // never seen on F5V0
            case 0x02:  // Pressure adjustment
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos++]));
                break;
            //case 0x03:  // never seen on F5V0; probably pressure pulse, see F5V1
            case 0x04:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 0x05:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x06:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x07:  // Hypopnea
                // NOTE: No additional (unknown) first byte as in F5V3 0x07, but see below.
                // This seems closer to F5V3 0x0d or 0x0e.
                elapsed = data[pos];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x08:  // Hypopnea, note this is 0x7 in F5V3
                // TODO: How is this hypopnea different from event 0x7?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x09:  // Flow Limitation, note this is 0x8 in F5V3
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0a:  // Vibratory Snore, note this is 0x9 in F5V3
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistic above seems to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0b:  // Periodic Breathing, note this is 0xa in F5V3
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));  // confirmed to double in F5V0
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0c:  // Large Leak, note this is 0xb in F5V3
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));  // confirmed to double in F5V0
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x0d:  // Statistics
                // These appear every 2 minutes, so presumably summarize the preceding period.
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+0]));              // 00=IPAP
                this->AddEvent(new PRS1IPAPLowEvent(t, data[pos+1]));                  // 01=IAP Low
                this->AddEvent(new PRS1IPAPHighEvent(t, data[pos+2]));                 // 02=IAP High
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+8]));                    // 08=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+9]));              // 09=EPAP average
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos));
                ok = false;  // unlike F0V6, we don't know the size of unknown slices, so we can't recover
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


const QVector<PRS1ParsedEventType> ParsedEventsF5V1 = {
    PRS1EPAPSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1TimedBreathEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
    PRS1IPAPAverageEvent::TYPE,
    PRS1IPAPLowEvent::TYPE,
    PRS1IPAPHighEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1LeakEvent::TYPE,
};

// 960P and 961P are F5V1
bool PRS1DataChunk::ParseEventsF5V1(void)
{
    if (this->family != 5 || this->familyVersion != 1) {
        qWarning() << "ParseEventsF5V1 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const QMap<int,int> event_sizes = { {1,2}, {8,4}, {9,3}, {0xa,2}, {0xb,5}, {0xc,5}, {0xd,0xd} };

    if (chunk_size < 1) {
        // This does occasionally happen in F0V6.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed, duration;
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
        if (code != 0) {  // Does this code really not have a timestamp? Never seen on F5V1, checked in F5V0.
            t += data[pos] | (data[pos+1] << 8);
            pos += 2;
        }

        switch (code) {
            //case 0x00:  // never seen on F5V1
            //case 0x01:  // never seen on F5V1
            case 0x02:  // Pressure adjustment
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos++]));
                break;
            case 0x03:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x04:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 0x05:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x06:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x07:  // Hypopnea
                // TODO: How is this hypopnea different from event 0x8?
                // NOTE: No additional (unknown) first byte as in F5V3 0x7, but see below.
                // This seems closer to F5V3 0x0d or 0x0e.
                elapsed = data[pos];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x08:  // Hypopnea, note this is 0x7 in F5V3
                // TODO: How is this hypopnea different from event 0x7?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x09:  // Flow Limitation, note this is 0x8 in F5V3
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0a:  // Vibratory Snore, note this is 0xb in F5V2 and 0x9 in F5V3
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistic above seems to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0b:  // Periodic Breathing, note this is 0xc in F5V2 and 0xa in F5V3
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));  // confirmed to double in F5V0
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0c:  // Large Leak, note this is 0xb in F5V3
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));  // confirmed to double in F5V0
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x0d:  // Statistics
                // These appear every 2 minutes, so presumably summarize the preceding period.
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+0]));              // 00=IPAP
                this->AddEvent(new PRS1IPAPLowEvent(t, data[pos+1]));                  // 01=IAP Low
                this->AddEvent(new PRS1IPAPHighEvent(t, data[pos+2]));                 // 02=IAP High
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+8]));                    // 08=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+9]));              // 09=EPAP average
                this->AddEvent(new PRS1LeakEvent(t, data[pos+0xa]));                   // 0A=Leak (average?) new to F5V1 (originally found in F5V3)
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos));
                ok = false;  // unlike F0V6, we don't know the size of unknown slices, so we can't recover
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


const QVector<PRS1ParsedEventType> ParsedEventsF5V2 = {
    PRS1EPAPSetEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1TimedBreathEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    //PRS1LargeLeakEvent::TYPE,  // not yet seen
    PRS1IPAPAverageEvent::TYPE,
    PRS1IPAPLowEvent::TYPE,
    PRS1IPAPHighEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1LeakEvent::TYPE,
};

// 960T is F5V2
bool PRS1DataChunk::ParseEventsF5V2(void)
{
    if (this->family != 5 || this->familyVersion != 2) {
        qWarning() << "ParseEventsF5V2 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const QMap<int,int> event_sizes = { {0,4}, {1,2}, {8,3}, {9,4}, {0xa,3}, {0xb,2}, {0xc,5}, {0xd,5}, {0xe,0xd}, {0xf,5}, {0x10,5}, {0x11,2}, {0x12,6} };

    if (chunk_size < 1) {
        // This does occasionally happen in F0V6.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    // F5V2 uses a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: this should be parameterized somewhere more logical
    bool ok = true;
    int pos = 0, startpos;
    int code, size;
    int t = 0;
    int elapsed/*, duration, value*/;
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
        if (code != 0 && code != 0x12) {  // These two codes have no timestamp  TODO: verify this applies to F5V012
            t += data[pos] | (data[pos+1] << 8);
            pos += 2;
        }

        switch (code) {
            //case 0x00:  // never seen on F5V2
            //case 0x01:  // never seen on F5V2
            case 0x02:  // Pressure adjustment
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos++], GAIN));
                break;
            case 0x03:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x04:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 0x05:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x06:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x07:  // Hypopnea
                // NOTE: No additional (unknown) first byte as in F5V3 0x07, but see below.
                // This seems closer to F5V3 0x0d or 0x0e.
                // What's different about this an 0x08? This was seen in a PB at least once?
                elapsed = data[pos];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x08:  // Hypopnea, note this is 0x7 in F5V1
                // TODO: How is this hypopnea different from event 0x9 and 0x7?
                // NOTE: No additional (unknown) first byte as in F5V3 0x7, but see below.
                // This seems closer to F5V3 0x0d or 0x0e.
                elapsed = data[pos];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            //case 0x09:  // never seen on F5V2
            case 0x0a:  // Flow Limitation, note this is 0x9 in F5V1 and 0x8 in F5V3
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x0b:  // Vibratory Snore, note this is 0xa in F5V1 and 0x9 in F5V3
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistic above seems to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0c:  // Periodic Breathing, note this is 0xb in F5V1 and 0xa in F5V3
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));  // confirmed to double in F5V0
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            //case 0x0d:  // never seen on F5V2
            case 0x0e:  // Statistics, note this was 0x0d in F5V0 and F5V1
                // These appear every 2 minutes, so presumably summarize the preceding period.
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+0], GAIN));        // 00=IPAP
                this->AddEvent(new PRS1IPAPLowEvent(t, data[pos+1], GAIN));            // 01=IAP Low
                this->AddEvent(new PRS1IPAPHighEvent(t, data[pos+2], GAIN));           // 02=IAP High
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+8]));                    // 08=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+9], GAIN));        // 09=EPAP average
                this->AddEvent(new PRS1LeakEvent(t, data[pos+0xa]));                   // 0A=Leak (average?) new to F5V1 (originally found in F5V3)
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            default:
                DUMP_EVENT();
                UNEXPECTED_VALUE(code, "known event code");
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos));
                ok = false;  // unlike F0V6, we don't know the size of unknown slices, so we can't recover
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
// MARK: DreamStation

// Originally based on ParseSummaryF0V6, with changes observed in ASV sample data
// based on size, slices 0-5 look similar, and it looks like F0V6 slides 8-B are equivalent to 6-9
//
// TODO: surely there will be a way to merge these loops and abstract the device-specific
// encodings into another function or class, but that's probably worth pursuing only after
// the details have been figured out.
bool PRS1DataChunk::ParseSummaryF5V3(void)
{
    if (this->family != 5 || this->familyVersion != 3) {
        qWarning() << "ParseSummaryF5V3 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 1, 0x35, 9, 4, 2, 4, 0x1e, 2, 4, 9 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);
    // NOTE: The sizes contained in hblock can vary, even within a single device, as can the length of hblock itself!

    // TODO: hardcoding this is ugly, think of a better approach
    if (chunk_size < minimum_sizes[0] + minimum_sizes[1] + minimum_sizes[2]) {
        qWarning() << this->sessionid << "summary data too short:" << chunk_size;
        return false;
    }
    // We've once seen a short summary with no mask-on/off: just equipment-on, settings, 9, equipment-off
    // (And we've seen something similar in F3V6.)
    if (chunk_size < 75) UNEXPECTED_VALUE(chunk_size, ">= 75");

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

        int alarm;
        switch (code) {
            case 0:  // Equipment On
                CHECK_VALUE(pos, 1);  // Always first?
                //CHECK_VALUES(data[pos], 1, 7);  // or 3, or 0?  3 when device turned on via auto-on, 1 when turned on via button
                CHECK_VALUE(size, 1);
                break;
            case 1:  // Settings
                ok = this->ParseSettingsF5V3(data + pos, size);
                break;
            case 9:  // new to F5V3 vs. F0V6, comes right after settings, before mask on?
                CHECK_VALUE(data[pos], 0);
                CHECK_VALUE(data[pos+1], 1);
                CHECK_VALUES(data[pos+2], 0, 4);  // Apnea Alarm, 0 = off, 4 = 40
                CHECK_VALUE(data[pos+3], 1);
                CHECK_VALUE(data[pos+4], 1);
                if (data[pos+5] > 3) {
                    UNEXPECTED_VALUE(data[pos+5], "0-3");  // Low Minute Ventilation Alarm, 0 = off, 1-3 = 1-3
                }
                CHECK_VALUE(data[pos+6], 2);
                CHECK_VALUE(data[pos+7], 1);

                alarm = 0;
                switch (data[pos+8]) {
                    case 1: alarm = 15; break;  // 15 sec
                    case 2: alarm = 60; break;  // 60 sec
                    case 0: break;
                    default:
                        UNEXPECTED_VALUE(data[pos+8], "0-2");
                        break;
                }
                if (alarm) {
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_DISCONNECT_ALARM, alarm));
                }

                CHECK_VALUE(size, 9);
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
            case 5:  // ASV pressure stats per mask-on slice
                //CHECK_VALUE(data[pos], 0x28);  // 90% EPAP
                //CHECK_VALUE(data[pos+1], 0x23);  // average EPAP
                //CHECK_VALUE(data[pos+2], 0x24);  // 90% PS
                //CHECK_VALUE(data[pos+3], 0x17);  // average PS
                break;
            case 6:  // Patient statistics per mask-on slice
                // These get averaged on a time-weighted basis in the final report.
                // Where is H count?
                //CHECK_VALUE(data[pos], 0x00);  // probably 16-bit value
                CHECK_VALUE(data[pos+1], 0x00);
                //CHECK_VALUE(data[pos+2], 0x00);  // 16-bit OA count
                //CHECK_VALUE(data[pos+3], 0x00);
                //CHECK_VALUE(data[pos+4], 0x00);  // probably 16-bit value
                CHECK_VALUE(data[pos+5], 0x00);
                //CHECK_VALUE(data[pos+6], 0x00);  // 16-bit CA count
                //CHECK_VALUE(data[pos+7], 0x00);
                //CHECK_VALUE(data[pos+8], 0x00);  // 16-bit minutes in LL
                //CHECK_VALUE(data[pos+9], 0x00);
                //CHECK_VALUE(data[pos+0xa], 0x0f);  // 16-bit minutes in PB
                //CHECK_VALUE(data[pos+0xb], 0x00);
                //CHECK_VALUE(data[pos+0xc], 0x14);  // 16-bit VS count
                //CHECK_VALUE(data[pos+0xd], 0x00);
                //CHECK_VALUE(data[pos+0xe], 0x05);  // 16-bit H count for type 0xd
                //CHECK_VALUE(data[pos+0xf], 0x00);
                //CHECK_VALUE(data[pos+0x10], 0x00);  // 16-bit H count for type 7
                //CHECK_VALUE(data[pos+0x11], 0x00);
                //CHECK_VALUE(data[pos+0x12], 0x02);  // 16-bit FL count
                //CHECK_VALUE(data[pos+0x13], 0x00);
                //CHECK_VALUE(data[pos+0x14], 0x28);  // 0x69 (105)
                //CHECK_VALUE(data[pos+0x15], 0x17);  // average total leak
                //CHECK_VALUE(data[pos+0x16], 0x5b);  // 0x7d (125)
                //CHECK_VALUE(data[pos+0x17], 0x09);  // 16-bit H count for type 0xe
                //CHECK_VALUE(data[pos+0x18], 0x00);
                //CHECK_VALUE(data[pos+0x19], 0x10);  // average breath rate
                //CHECK_VALUE(data[pos+0x1a], 0x2d);  // average TV / 10
                //CHECK_VALUE(data[pos+0x1b], 0x63);  // average % PTB
                //CHECK_VALUE(data[pos+0x1c], 0x07);  // average minute vent
                //CHECK_VALUE(data[pos+0x1d], 0x06);  // average leak
                break;
            case 2:  // Equipment Off
                tt += data[pos] | (data[pos+1] << 8);
                this->AddEvent(new PRS1ParsedSliceEvent(tt, EquipmentOff));
                //CHECK_VALUE(data[pos+2], 0x01);  // 0x08
                //CHECK_VALUE(data[pos+3], 0x17);  // 0x16, 0x18
                //CHECK_VALUE(data[pos+4], 0x00);
                //CHECK_VALUE(data[pos+5], 0x29);  // 0x2a, 0x28, 0x26, 0x36
                //CHECK_VALUE(data[pos+6], 0x01);  // 0x00
                CHECK_VALUE(data[pos+7], 0x00);
                CHECK_VALUE(data[pos+8], 0x00);
                break;
            case 8:  // Humidier setting change
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


// Based initially on ParseSettingsF0V6. Many of the codes look the same, like always starting with 0, 0x35 looking like
// a humidifier setting, etc., but the contents are sometimes a bit different, such as mode values and pressure settings.
//
// new settings to find: breath rate, tubing lock, alarms,
bool PRS1DataChunk::ParseSettingsF5V3(const unsigned char* data, int size)
{
    static const QMap<int,int> expected_lengths = { {0x0a,5}, /*{0x0c,3}, {0x0d,2}, {0x0e,2}, {0x0f,4}, {0x10,3},*/ {0x14,3}, {0x2e,2}, {0x35,2} };
    bool ok = true;

    PRS1Mode cpapmode = PRS1_MODE_UNKNOWN;

    // F5V3 uses a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
    static const float GAIN = 0.125;  // TODO: parameterize this somewhere better

    int max_pressure = 0;
    int min_ps   = 0;
    int max_ps   = 0;
    int min_epap = 0;
    int max_epap = 0;
    int rise_time;
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
                case 0: cpapmode = PRS1_MODE_ASV; break;
                default:
                    UNEXPECTED_VALUE(data[pos], "known device mode");
                    break;
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_CPAP_MODE, (int) cpapmode));
                break;
            case 1: // ???
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 1);  // 1 when when Opti-Start is on? 0 when off?
                /*
                if (data[pos] != 0 && data[pos] != 3) {
                    CHECK_VALUES(data[pos], 1, 2);  // 1 when EZ-Start is enabled? 2 when Auto-Trial? 3 when Auto-Trial is off or Opti-Start isn't off?
                }
                */
                break;
            case 0x0a:  // ASV with variable EPAP pressure setting
                CHECK_VALUE(len, 5);
                CHECK_VALUE(cpapmode, PRS1_MODE_ASV);
                max_pressure = data[pos];
                min_epap = data[pos+1];
                max_epap = data[pos+2];
                min_ps = data[pos+3];
                max_ps = data[pos+4];
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MIN, min_epap, GAIN));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_EPAP_MAX, max_epap, GAIN));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MIN, min_epap + min_ps, GAIN));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_IPAP_MAX, qMin(max_pressure, max_epap + max_ps), GAIN));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MIN, min_ps, GAIN));
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_PS_MAX, max_ps, GAIN));
                break;
            case 0x14:  // ASV backup rate
                CHECK_VALUE(len, 3);
                CHECK_VALUE(cpapmode, PRS1_MODE_ASV);
                switch (data[pos]) {
                //case 0:  // Breath Rate Off in F3V6 setting 0x1e
                case 1:  // Breath Rate Auto
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Auto));
                    CHECK_VALUE(data[pos+1], 0);  // 0 for auto
                    CHECK_VALUE(data[pos+2], 0);  // 0 for auto
                    break;
                case 2:  // Breath Rate (fixed BPM)
                    breath_rate = data[pos+1];
                    timed_inspiration = data[pos+2];
                    if (breath_rate < 4 || breath_rate > 16) UNEXPECTED_VALUE(breath_rate, "4-16");
                    if (timed_inspiration < 12 || timed_inspiration > 30) UNEXPECTED_VALUE(timed_inspiration, "12-30");
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_MODE, PRS1Backup_Fixed));
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_BACKUP_BREATH_RATE, breath_rate));  // BPM
                    this->AddEvent(new PRS1ScaledSettingEvent(PRS1_SETTING_BACKUP_TIMED_INSPIRATION, timed_inspiration, 0.1));
                    break;
                default:
                    CHECK_VALUES(data[pos], 1, 2);  // 1 = auto, 2 = fixed BPM (0 = off in F3V6 setting 0x1e)
                    break;
                }
                break;
            /*
            case 0x2a:  // EZ-Start
                CHECK_VALUE(data[pos], 0x80);  // EZ-Start enabled
                break;
            */
            case 0x2b:  // Ramp Type
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);  // 0 == "Linear", 0x80 = "SmartRamp"
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TYPE, data[pos] != 0));
                break;
            case 0x2c:  // Ramp Time
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == ramp off, and ramp pressure setting doesn't appear
                    if (data[pos] < 5 || data[pos] > 45) UNEXPECTED_VALUE(data[pos], "5-45");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RAMP_TIME, data[pos]));
                break;
            case 0x2d:  // Ramp Pressure (with ASV pressure encoding)
                CHECK_VALUE(len, 1);
                this->AddEvent(new PRS1PressureSettingEvent(PRS1_SETTING_RAMP_PRESSURE, data[pos], GAIN));
                break;
            case 0x2e:  // Flex mode and level (ASV variant)
                CHECK_VALUE(len, 2);
                switch (data[pos]) {
                case 0:  // Bi-Flex
                    // [0x00, N] for Bi-Flex level N
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_MODE, FLEX_BiFlex));
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LEVEL, data[pos+1]));
                    break;
                case 0x20:  // Rise Time
                    // [0x20, 0x03] for no flex, rise time setting = 3, no rise lock
                    rise_time = data[pos+1];
                    if (rise_time < 1 || rise_time > 6) UNEXPECTED_VALUE(rise_time, "1-6");
                    this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_RISE_TIME, rise_time));
                    break;
                default:
                    CHECK_VALUES(data[pos], 0, 0x20);
                    break;
                }
                break;
            case 0x2f:  // Flex lock? (was on F0V6, 0x80 for locked)
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0);
                //this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_FLEX_LOCK, data[pos] != 0));
                break;
            //case 0x30: ASV puts the flex level in the 0x2e setting for some reason
            case 0x35:  // Humidifier setting
                CHECK_VALUE(len, 2);
                this->ParseHumidifierSettingV3(data[pos], data[pos+1], true);
                break;
            case 0x36:  // Mask Resistance Lock
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);  // 0x80 = locked
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_LOCK, data[pos] != 0));
                break;
            case 0x38:  // Mask Resistance
                CHECK_VALUE(len, 1);
                if (data[pos] != 0) {  // 0 == mask resistance off
                    if (data[pos] < 1 || data[pos] > 5) UNEXPECTED_VALUE(data[pos], "1-5");
                }
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_MASK_RESIST_SETTING, data[pos]));
                break;
            case 0x39:
                CHECK_VALUE(len, 1);
                CHECK_VALUE(data[pos], 0);  // 0x80 maybe auto-trial in F0V6?
                break;
            case 0x3b:  // Tubing Type
                CHECK_VALUE(len, 1);
                if (data[pos] > 2) UNEXPECTED_VALUE(data[pos], "0-2");  // 15HT = 2, 15 = 1, 22 = 0, though report only says "15" for 15HT
                this->ParseTubingTypeV3(data[pos]);
                break;
            case 0x3c:  // View Optional Screens
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_SHOW_AHI, data[pos] != 0));
                break;
            case 0x3d:  // Auto On (ASV variant)
                CHECK_VALUE(len, 1);
                CHECK_VALUES(data[pos], 0, 0x80);
                this->AddEvent(new PRS1ParsedSettingEvent(PRS1_SETTING_AUTO_ON, data[pos] != 0));
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


const QVector<PRS1ParsedEventType> ParsedEventsF5V3 = {
    PRS1EPAPSetEvent::TYPE,
    PRS1TimedBreathEvent::TYPE,
    PRS1IPAPAverageEvent::TYPE,
    PRS1IPAPLowEvent::TYPE,
    PRS1IPAPHighEvent::TYPE,
    PRS1TotalLeakEvent::TYPE,
    PRS1RespiratoryRateEvent::TYPE,
    PRS1PatientTriggeredBreathsEvent::TYPE,
    PRS1MinuteVentilationEvent::TYPE,
    PRS1TidalVolumeEvent::TYPE,
    PRS1SnoreEvent::TYPE,
    PRS1EPAPAverageEvent::TYPE,
    PRS1LeakEvent::TYPE,
    PRS1PressurePulseEvent::TYPE,
    PRS1ObstructiveApneaEvent::TYPE,
    PRS1ClearAirwayEvent::TYPE,
    PRS1HypopneaEvent::TYPE,
    PRS1FlowLimitationEvent::TYPE,
    PRS1VibratorySnoreEvent::TYPE,
    PRS1PeriodicBreathingEvent::TYPE,
    PRS1LargeLeakEvent::TYPE,
};

// Outer loop based on ParseSummaryF5V3 along with hint as to event codes from old ParseEventsF5V3,
// except this actually does something with the data.
bool PRS1DataChunk::ParseEventsF5V3(void)
{
    if (this->family != 5 || this->familyVersion != 3) {
        qWarning() << "ParseEventsF5V3 called with family" << this->family << "familyVersion" << this->familyVersion;
        return false;
    }
    const unsigned char * data = (unsigned char *)this->m_data.constData();
    int chunk_size = this->m_data.size();
    static const int minimum_sizes[] = { 2, 3, 3, 0xd, 3, 3, 3, 4, 3, 2, 5, 5, 3, 3, 3, 3 };
    static const int ncodes = sizeof(minimum_sizes) / sizeof(int);

    if (chunk_size < 1) {
        // This does occasionally happen.
        qDebug() << this->sessionid << "Empty event data";
        return false;
    }

    // F5V3 uses a gain of 0.125 rather than 0.1 to allow for a maximum value of 30 cmH2O
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
            case 1:  // Pressure adjustment
                this->AddEvent(new PRS1EPAPSetEvent(t, data[pos++], GAIN));
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos-1, size+1));  // TODO: what is this?
                break;
            case 2:  // Timed Breath
                // TB events have a duration in 0.1s, based on the review of pressure waveforms.
                // TODO: Ideally the starting time here would be adjusted here, but PRS1ParsedEvents
                // currently assume integer seconds rather than ms, so that's done at import.
                duration = data[pos];
                this->AddEvent(new PRS1TimedBreathEvent(t, duration));
                break;
            case 3:  // Statistics
                // These appear every 2 minutes, so presumably summarize the preceding period.
                this->AddEvent(new PRS1IPAPAverageEvent(t, data[pos+0], GAIN));        // 00=IPAP
                this->AddEvent(new PRS1IPAPLowEvent(t, data[pos+1], GAIN));            // 01=IAP Low
                this->AddEvent(new PRS1IPAPHighEvent(t, data[pos+2], GAIN));           // 02=IAP High
                this->AddEvent(new PRS1TotalLeakEvent(t, data[pos+3]));                // 03=Total leak (average?)
                this->AddEvent(new PRS1RespiratoryRateEvent(t, data[pos+4]));          // 04=Breaths Per Minute (average?)
                this->AddEvent(new PRS1PatientTriggeredBreathsEvent(t, data[pos+5]));  // 05=Patient Triggered Breaths (average?)
                this->AddEvent(new PRS1MinuteVentilationEvent(t, data[pos+6]));        // 06=Minute Ventilation (average?)
                this->AddEvent(new PRS1TidalVolumeEvent(t, data[pos+7]));              // 07=Tidal Volume (average?)
                this->AddEvent(new PRS1SnoreEvent(t, data[pos+8]));                    // 08=Snore count  // TODO: not a VS on official waveform, but appears in flags and contributes to overall VS index
                this->AddEvent(new PRS1EPAPAverageEvent(t, data[pos+9], GAIN));        // 09=EPAP average
                this->AddEvent(new PRS1LeakEvent(t, data[pos+0xa]));                   // 0A=Leak (average?)
                this->AddEvent(new PRS1IntervalBoundaryEvent(t));
                break;
            case 0x04:  // Pressure Pulse
                duration = data[pos];  // TODO: is this a duration?
                this->AddEvent(new PRS1PressurePulseEvent(t, duration));
                break;
            case 0x05:  // Obstructive Apnea
                // OA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ObstructiveApneaEvent(t - elapsed, 0));
                break;
            case 0x06:  // Clear Airway Apnea
                // CA events are instantaneous flags with no duration: reviewing waveforms
                // shows that the time elapsed between the flag and reporting often includes
                // non-apnea breathing.
                elapsed = data[pos];
                this->AddEvent(new PRS1ClearAirwayEvent(t - elapsed, 0));
                break;
            case 0x07:  // Hypopnea
                // TODO: How is this hypopnea different from events 0xd and 0xe?
                // TODO: What is the first byte?
                //data[pos+0];  // unknown first byte?
                elapsed = data[pos+1];  // based on sample waveform, the hypopnea is over after this
                this->AddEvent(new PRS1HypopneaEvent(t - elapsed, 0));
                break;
            case 0x08:  // Flow Limitation
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating flow limitations ourselves. Flow limitations aren't
                // as obvious as OA/CA when looking at a waveform.
                elapsed = data[pos];
                this->AddEvent(new PRS1FlowLimitationEvent(t - elapsed, 0));
                break;
            case 0x09:  // Vibratory Snore
                // VS events are instantaneous flags with no duration, drawn on the official waveform.
                // The current thinking is that these are the snores that cause a change in auto-titrating
                // pressure. The snoring statistic above seems to be a total count. It's unclear whether
                // the trigger for pressure change is severity or count or something else.
                // no data bytes
                this->AddEvent(new PRS1VibratorySnoreEvent(t, 0));
                break;
            case 0x0a:  // Periodic Breathing
                // PB events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1PeriodicBreathingEvent(t - elapsed - duration, duration));
                break;
            case 0x0b:  // Large Leak
                // LL events are reported some time after they conclude, and they do have a reported duration.
                duration = 2 * (data[pos] | (data[pos+1] << 8));
                elapsed = data[pos+2];
                this->AddEvent(new PRS1LargeLeakEvent(t - elapsed - duration, duration));
                break;
            case 0x0d:  // Hypopnea
                // TODO: Why does this hypopnea have a different event code?
                // fall through
            case 0x0e:  // Hypopnea
                // TODO: We should revisit whether this is elapsed or duration once (if)
                // we start calculating hypopneas ourselves. Their official definition
                // is 40% reduction in flow lasting at least 10s.
                duration = data[pos];
                this->AddEvent(new PRS1HypopneaEvent(t - duration, 0));
                break;
            case 0x0f:
                // TODO: some other pressure adjustment?
                // Appears near the beginning and end of a session when Opti-Start is on, at least once in middle
                //CHECK_VALUES(data[pos], 0x20, 0x28);
                this->AddEvent(new PRS1UnknownDataEvent(m_data, startpos-1, size+1));
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
