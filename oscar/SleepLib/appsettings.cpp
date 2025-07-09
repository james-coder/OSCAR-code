/* SleepLib AppSettings Initialization
 *
 * This isolates the initialization and its dependencies from the header file,
 * which is widely included.
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the sourcecode. */

#include "appsettings.h"
#include "version.h"

AppWideSetting::AppWideSetting(Preferences *pref) : PrefSettings(pref)
{
//    m_multithreading = initPref(STR_IS_Multithreading, idealThreads() > 1).toBool();
    m_multithreading = false;     // too dangerous to allow
    m_showPerformance = initPref(STR_US_ShowPerformance, false).toBool();
    m_showDebug = initPref(STR_US_ShowDebug, false).toBool();
    initPref(STR_AS_CalendarVisible, false);
    m_scrollDampening = initPref(STR_US_ScrollDampening, (int)50).toInt();
    m_tooltipTimeout = initPref(STR_US_TooltipTimeout, (int)2500).toInt();
    m_graphHeight=initPref(STR_AS_GraphHeight, 180).toInt();
    initPref(STR_AS_DailyPanelWidth, 250.0);
    initPref(STR_AS_RightPanelWidth, 230.0);
    m_antiAliasing=initPref(STR_AS_AntiAliasing, true).toBool();
//    initPref(STR_AS_GraphSnapshots, true);
    initPref(STR_AS_IncludeSerial, false);
    initPref(STR_AS_MonochromePrinting, false);
    //initPref(STR_AS_EventFlagSessionBar, false);
    initPref(STR_AS_DisableDailyGraphTitles, false);
    #if defined(STEADY_BREATHING)
    initPref(STR_AS_SteadyBreathing, SB_OFF);
    #if defined(STEADY_BREATHING_ENHANCED_TESTING)
    initPref(STR_AS_SteadyBreathingThreshold, steadyBreathingDefaultThreshold());
    initPref(STR_AS_SteadyBreathingDuration, steadyBreathingDefaultDuration());
    #endif
    #endif
    initPref(STR_AS_ShowPieChart, false);
    m_animations = initPref(STR_AS_Animations, true).toBool();
    m_squareWavePlots = initPref(STR_AS_SquareWave, false).toBool();
    initPref(STR_AS_AllowYAxisScaling, true);
    m_graphTooltips = initPref(STR_AS_GraphTooltips, true).toBool();
    m_usePixmapCaching = initPref(STR_AS_UsePixmapCaching, false).toBool();
    m_odt = (OverlayDisplayType)initPref(STR_AS_OverlayType, (int)ODT_Bars).toInt();
    initPref(STR_AS_GraphTooltips, 0);
    m_alternatingColorsCombo = initPref(STR_AS_setAlternatingColorsCombo, 0).toInt(); 
#ifndef REMOVE_FITNESS
    m_olm = (OverviewLinechartModes)initPref(STR_AS_OverviewLinechartMode, (int)OLC_Bartop).toInt();
#endif
    m_lineThickness=initPref(STR_AS_LineThickness, 1.0).toFloat();
    m_lineCursorMode = initPref(STR_AS_LineCursorMode, true).toBool();
    initPref(STR_AS_RightSidebarVisible, false);
    initPref(STR_CS_UserEventPieChart, false);
    initPref(STR_US_ShowSerialNumbers, false);
    initPref(STR_US_ShowPersonalData, true);
    initPref(STR_US_OpenTabAtStart, 1);
    initPref(STR_US_OpenTabAfterImport, 0);
    initPref(STR_US_AutoLaunchImport, false);
    m_cacheSessions = initPref(STR_IS_CacheSessions, false).toBool();
    initPref(STR_US_RemoveCardReminder, true);
    initPref(STR_US_NotifyMessagBoxOption, false);
    initPref(STR_US_DontAskWhenSavingScreenshots, false);
    m_profileName = initPref(STR_GEN_Profile, "").toString();
    initPref(STR_GEN_AutoOpenLastUsed, true);

#ifndef NO_CHECKUPDATES
    initPref(STR_GEN_UpdatesAutoCheck, true);
    initPref(STR_GEN_UpdateCheckFrequency, 14);
    initPref(STR_PREF_AllowEarlyUpdates, false);
    initPref(STR_GEN_UpdatesLastChecked, QDateTime());
#endif
    initPref(STR_PREF_VersionString, getVersion().toString());
    m_language = initPref(STR_GEN_Language, "en_US").toString();
    initPref(STR_GEN_ShowAboutDialog, 0);  // default to about screen, set to -1 afterwards
}
