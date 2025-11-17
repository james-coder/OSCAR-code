#-------------------------------------------------
#
# Project created by QtCreator 2011-06-20T22:05:30
#
#-------------------------------------------------

message(Platform is $$QMAKESPEC )
#message("CONFIG: $$CONFIG")

# the debug option is set by default by qmake
# to enable debug builds from the command line a new option "crash" is enable
# crash was used since debug mode is always used to identify the cause of crashes.
# a better name might be possible.
# this can be enabled by CONFIG += crash in the qmake make.
# qmake <buldFolderPath> OSCAR-code/OSCAR_QT.pro "CONFIG+=crash"  
contains(CONFIG, crash) {
    message("DEBUG BUILDS - set by OPTION  'crash' ")
}


lessThan(QT_MAJOR_VERSION,5) {
    error("You need Qt 5.8 or newer to build OSCAR");
}

if (equals(QT_MAJOR_VERSION,5)) {
    lessThan(QT_MINOR_VERSION,9) {
        message("You need Qt 5.9 to build OSCAR with Help Pages")
        DEFINES += helpless
    }
    lessThan(QT_MINOR_VERSION,7) {
        error("You need Qt 5.8 or newer to build OSCAR");
    }
}

# get rid of the help browser, at least for now
DEFINES += helpless

QT += core gui network xml printsupport serialport widgets help
contains(DEFINES, helpless) {
    QT -= help
}

DEFINES += QT_DEPRECATED_WARNINGS

# Enable this to turn off Check for Updates feature
# DEFINES += NO_CHECKUPDATES

#OSCAR requires OpenGL 2.0 support to run smoothly
#On platforms where it's not available, it can still be built to work
#provided the BrokenGL DEFINES flag is passed to qmake (eg, qmake [specs] /path/to/OSCAR_QT.pro DEFINES+=BrokenGL) (hint, Projects button on the left)
contains(DEFINES, NoGL) {
    message("Building with QWidget gGraphView to support systems without ANY OpenGL")
    DEFINES += BROKEN_OPENGL_BUILD
    DEFINES += NO_OPENGL_BUILD
} else:contains(DEFINES, BrokenGL) {
    DEFINES += BROKEN_OPENGL_BUILD
    message("Building with QWidget gGraphView to support systems with legacy graphics")
    DEFINES-=BrokenGL
} else {
    QT += opengl
    greaterThan(QT_MAJOR_VERSION, 5) {
        QT += openglwidgets
    }
    message("Building with regular OpenGL gGraphView")
}

## qt6 requires c++17
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
## Added for Qt6-Clang compiler - Crimson Nape 25-11-17
    CONFIG += warn_off
    QMAKE_CXXFLAGS += -Wno-nonportable-include-path

} else {
    CONFIG += c++11
    #keep old configuration for qt5.
    #needs verification to use higher compiler.
    if (false) {
        greaterThan(QT_MINOR_VERSION, 11) {
            CONFIG += c++17
        } else {
            CONFIG += c++1z
            QMAKE_CXXFLAGS += -std=c++17
        }
    }
}

DEFINES += LOCK_RESMED_SESSIONS
CONFIG += rtti
!contains(CONFIG, crash) {
    CONFIG -= debug_and_release
} else {
    # This requires CONFIG = debug set in qmake command
    #message("DEBUG BUILDS - set by OPTION  'crash' ")

    CONFIG -= crash  # Enable debug mode
    CONFIG += debug  # Enable debug mode
    # Debugging Configurations
    CONFIG -= release  # Ensure release mode is disabled

    # Debugging Symbols
    QMAKE_CXXFLAGS += -g3  # Most comprehensive debugging symbols
    QMAKE_CXXFLAGS += -ggdb  # GDB-specific debug information

    # Optimization Level
    #QMAKE_CXXFLAGS_DEBUG += -O0  # Disable optimizations completely

    # Additional Debugging Flags
    DEFINES += QT_ENABLE_GLIB  # Enable additional debugging support
    DEFINES += QT_MESSAGELOGCONTEXT  # Include source file and line number in debug messages
}

contains(DEFINES, STATIC) {
    static {
        CONFIG += static
        QTPLUGIN += qgif qpng

        message("Static build.")
    }
}

DEFINES += STEADY_BREATHING
#### STEADY_BREATHING_ENHANCED_TESTING should NOT be defined for offical builds.
#### STEADY_BREATHING_ENHANCED_TESTING requires STEADY_BREATHING
####  DEFINES += STEADY_BREATHING_ENHANCED_TESTING


TARGET = OSCAR
unix:!macx:!haiku {
    TARGET.path=/usr/bin
}

TEMPLATE = app

gitinfotarget.target = git_info.h
gitinfotarget.depends = FORCE

win32 {
    system("$$_PRO_FILE_PWD_/update_gitinfo.bat");
    message("Updating gitinfo.h for Windows build")
    gitinfotarget.commands = "$$_PRO_FILE_PWD_/update_gitinfo.bat"
} else {
    system("/bin/bash $$_PRO_FILE_PWD_/update_gitinfo.sh");
    message("Updating gitinfo.h for non-Windows build")
    gitinfotarget.commands = "/bin/bash $$_PRO_FILE_PWD_/update_gitinfo.sh"
}

PRE_TARGETDEPS += git_info.h
QMAKE_EXTRA_TARGETS += gitinfotarget

!contains(DEFINES, helpless) {
#Build the help documentation
    message("Generating help files");
    qtPrepareTool(QCOLGENERATOR, qcollectiongenerator)

    command=$$QCOLGENERATOR $$PWD/help/index.qhcp -o $$PWD/help/index.qhc
    system($$command)|error("Failed to run: $$command")
    message("Finished generating help files");
}

QMAKE_TARGET_PRODUCT = OSCAR
QMAKE_TARGET_COMPANY = The OSCAR Team
QMAKE_TARGET_COPYRIGHT = © 2019-2024 The OSCAR Team
QMAKE_TARGET_DESCRIPTION = "OpenSource CPAP Analysis Reporter"
_VERSION_FILE = $$cat(./VERSION)
VERSION = $$section(_VERSION_FILE, '"', 1, 1)
win32 {
    VERSION = $$section(VERSION, '-', 0, 0)
}
RC_ICONS = ./icons/logo.ico

macx  {
  QMAKE_TARGET_BUNDLE_PREFIX = "org.oscar-team"
# QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13
  LIBS             += -lz
  ICON              = icons/OSCAR.icns
} else:haiku {
    LIBS            += -lz -lGLU
    DEFINES         += _TTY_POSIX_
} else:unix {
    LIBS            += -lX11 -lz -lGLU
    DEFINES         += _TTY_POSIX_
} else:win32 {
    DEFINES          += WINVER=0x0501 # needed for mingw to pull in appropriate dbt business...probably a better way to do this
    LIBS             += -lsetupapi

    INCLUDEPATH += $$PWD
    INCLUDEPATH += $$[QT_INSTALL_PREFIX]/../src/qtbase/src/3rdparty/zlib

    if (*-msvc*):!equals(TEMPLATE_PREFIX, "vc") {
        LIBS += -ladvapi32
    } else {
        # MingW needs this
        LIBS += -lz
    }

    if (*-msvc*) {
        CONFIG += precompile_header
        PRECOMPILED_HEADER = pch.h
        HEADERS += pch.h

    }

    CONFIG(release, debug|release) {
        contains(DEFINES, OfficialBuild) {
           QMAKE_POST_LINK += "$$PWD/../../scripts/release_tool.sh --testing --source \"$$PWD/..\" --binary \"$${OUT_PWD}/$${TARGET}.exe\""
        }
    }
}

TRANSLATIONS = $$files($$PWD/../Translations/*.ts)
TRANSLATIONS += $$files($$PWD/../Translations/qt/*.ts)

qtPrepareTool(LRELEASE, lrelease)

for(file, TRANSLATIONS) {

 qmfile = $$absolute_path($$basename(file), $$PWD/translations/)
 qmfile ~= s,.ts$,.qm,

 qmdir = $$PWD/translations
 !exists($$qmdir) {
     mkpath($$qmdir)|error("Aborting.")
 }
 qmout = $$qmfile
 command = $$LRELEASE -removeidentical $$file -qm $$qmfile
 system($$command)|error("Failed to run: $$command")
 TRANSLATIONS_FILES += $$qmfile
}

HTML_FILES = $$files($$PWD/../Htmldocs/*.html)

#copy the Translation and Help files to where the test binary wants them
message("Setting up Translations & Help Transfers")
macx {
    !contains(DEFINES, helpless) {
        HelpFiles.files = $$files($$PWD/help/*.qch)
        HelpFiles.path = Contents/Resources/Help
        QMAKE_BUNDLE_DATA += HelpFiles
    }

# Removed because we are not using QT's translation files
#    QtTransFiles.files = $$files($$[QT_INSTALL_TRANSLATIONS]/qt*.qm)
#    QtTransFiles.path = Contents/translations
#    QMAKE_BUNDLE_DATA += QtTransFiles

    TransFiles.files = $${TRANSLATIONS_FILES}
    TransFiles.path = Contents/Resources/translations
    QMAKE_BUNDLE_DATA += TransFiles

    HtmlFiles.files = $${HTML_FILES}
    HtmlFiles.path = Contents/Resources/html
    QMAKE_BUNDLE_DATA += HtmlFiles
} else {
    !contains(DEFINES, helpless) {
        HELPDIR = $$OUT_PWD/Help
        HELP_FILES += $$PWD/help/*.qch
    }
    DDIR = $$OUT_PWD/Translations
    HTMLDIR = $$OUT_PWD/Html

    TRANS_FILES = $${TRANSLATIONS_FILES}

    win32 {
        TRANS_FILES_WIN = $${TRANS_FILES}
        TRANS_FILES_WIN ~= s,/,\\,g
        DDIR ~= s,/,\\,g
        !exists($$quote($$DDIR)): system(mkdir $$quote($$DDIR))
        for(FILE,TRANS_FILES_WIN) {
            system(xcopy /y $$quote($$FILE) $$quote($$DDIR))
        }

        HTML_FILES_WIN = $${HTML_FILES}
        HTML_FILES_WIN ~= s,/,\\,g
        HTMLDIR ~= s,/,\\,g
        !exists($$quote($$HTMLDIR)): system(mkdir $$quote($$HTMLDIR))
        for(FILE,HTML_FILES_WIN) {
            system(xcopy /y $$quote($$FILE) $$quote($$HTMLDIR))
        }

        !contains(DEFINES, helpless) {
            HELP_FILES_WIN = $${HELP_FILES}
            HELP_FILES_WIN ~= s,/,\\,g
            HELPDIR ~= s,/,\\,g
            !exists($$quote($$HELPDIR)): system(mkdir $$quote($$HELPDIR))
            for(FILE,HELP_FILES_WIN) {
                system(xcopy /y $$quote($$FILE) $$quote($$HELPDIR))
            }
        }
    } else {
        system(mkdir -p $$quote($$DDIR))
        for(FILE,TRANS_FILES) {
            system(cp $$quote($$FILE) $$quote($$DDIR))
        }

        system(mkdir -p $$quote($$HTMLDIR))
        for(FILE,HTML_FILES) {
            system(cp $$quote($$FILE) $$quote($$HTMLDIR))
        }

        !contains(DEFINES, helpless) {
            system(mkdir -p $$quote($$HELPDIR))
            for(FILE,HELP_FILES) {
                system(cp $$quote($$FILE) $$quote($$HELPDIR))
            }
        }
    }
}

SOURCES += \
    aboutdialog.cpp \
    checkupdates.cpp \
    common_gui.cpp \
    cprogressbar.cpp \
    csv.cpp \
    daily.cpp \
    dailySearchTab.cpp \
    exportcsv.cpp \
    highresolution.cpp \
    logger.cpp \
    main.cpp \
    mainwindow.cpp \
    newprofile.cpp \
    notifyMessageBox.cpp \
    overview.cpp \
    oximeterimport.cpp \
    preferencesdialog.cpp \
    profileselector.cpp \
    rawdata.cpp \
    reports.cpp \
    saveGraphLayoutSettings.cpp \
    sessionbar.cpp \
    staticQMessageBox.cpp \
    statistics.cpp \
    translation.cpp \
    version.cpp \
    welcome.cpp \
    zip.cpp \
    Graphs/gAHIChart.cpp \
    Graphs/gdailysummary.cpp \
    Graphs/gFlagsLine.cpp \
    Graphs/gFooBar.cpp \
    Graphs/gGraph.cpp \
    Graphs/gGraphView.cpp \
    Graphs/glcommon.cpp \
    Graphs/gLineChart.cpp \
    Graphs/gLineOverlay.cpp \
    Graphs/gOverviewGraph.cpp \
    Graphs/gPressureChart.cpp \
    Graphs/gSegmentChart.cpp \
    Graphs/gSessionTimesChart.cpp \
    Graphs/gspacer.cpp \
    Graphs/gStatsLine.cpp \
    Graphs/gSummaryChart.cpp \
    Graphs/gTTIAChart.cpp \
    Graphs/gUsageChart.cpp \
    Graphs/gXAxis.cpp \
    Graphs/gYAxis.cpp \
    Graphs/layer.cpp \
    Graphs/MinutesAtPressure.cpp \
    SleepLib/appsettings.cpp \
    SleepLib/calcs.cpp \
    SleepLib/common.cpp \
    SleepLib/day.cpp \
    SleepLib/deviceconnection.cpp \
    SleepLib/event.cpp \
    SleepLib/importcontext.cpp \
    SleepLib/journal.cpp \
    SleepLib/loader_plugins/bmcDataParsing.cpp \
    SleepLib/loader_plugins/bmc_loader.cpp \
    SleepLib/loader_plugins/cms50f37_loader.cpp \
    SleepLib/loader_plugins/cms50_loader.cpp \
    SleepLib/loader_plugins/dreem_loader.cpp \
    SleepLib/loader_plugins/edfparser.cpp \
    SleepLib/loader_plugins/icon_loader.cpp \
    SleepLib/loader_plugins/intellipap_loader.cpp \
    SleepLib/loader_plugins/md300w1_loader.cpp \
    SleepLib/loader_plugins/mseries_loader.cpp \
    SleepLib/loader_plugins/prisma_loader.cpp \
    SleepLib/loader_plugins/vrem_loader.cpp \
    SleepLib/loader_plugins/prs1_loader.cpp \
    SleepLib/loader_plugins/prs1_parser_asv.cpp \
    SleepLib/loader_plugins/prs1_parser.cpp \
    SleepLib/loader_plugins/prs1_parser_vent.cpp \
    SleepLib/loader_plugins/prs1_parser_xpap.cpp \
    SleepLib/loader_plugins/resmed_EDFinfo.cpp \
    SleepLib/loader_plugins/resmed_loader.cpp \
    SleepLib/loader_plugins/resvent_loader.cpp \
    SleepLib/loader_plugins/sleepstyle_EDFinfo.cpp \
    SleepLib/loader_plugins/sleepstyle_loader.cpp \
    SleepLib/loader_plugins/somnopose_loader.cpp \
    SleepLib/loader_plugins/viatom_loader.cpp \
    SleepLib/loader_plugins/weinmann_loader.cpp \
    SleepLib/loader_plugins/zeo_loader.cpp \
    SleepLib/machine_common.cpp \
    SleepLib/machine.cpp \
    SleepLib/machine_loader.cpp \
    SleepLib/preferences.cpp \
    SleepLib/profiles.cpp \
    SleepLib/progressdialog.cpp \
    SleepLib/schema.cpp \
    SleepLib/serialoximeter.cpp \
    SleepLib/session.cpp \
    SleepLib/thirdparty/miniz.c \
    SleepLib/xmlreplay.cpp
!contains(DEFINES, helpless) {
    SOURCES += help.cpp
}

# The crypto libraries need to be optimized to avoid a 5x slowdown or worse.
# Don't use this for everything, as it interferes with debugging.
SOURCES_OPTIMIZE = \
    SleepLib/thirdparty/botan_all.cpp \
    SleepLib/crypto.cpp
optimize.name = optimize
optimize.input = SOURCES_OPTIMIZE
optimize.dependency_type = TYPE_C
optimize.variable_out = OBJECTS
optimize.output = ${QMAKE_FILE_IN_BASE}$${first(QMAKE_EXT_OBJ)}
optimize.commands = $${QMAKE_CXX} -c $(CXXFLAGS) -O3 $(INCPATH) -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_IN}
QMAKE_EXTRA_COMPILERS += optimize

HEADERS  += \
    checkupdates.h \
    notifyMessageBox.h \
    highresolution.h \
    dailySearchTab.h \
    daily.h \
    saveGraphLayoutSettings.h \
    overview.h \
    common_gui.h \
    cprogressbar.h \
    exportcsv.h \
    mainwindow.h \
    newprofile.h \
    preferencesdialog.h \
#    psettings.h \
    reports.h \
    sessionbar.h \
#    updateparser.h \
    version.h \
    VERSION \
    Graphs/gFlagsLine.h \
    Graphs/gFooBar.h \
    Graphs/gGraph.h \
    Graphs/gGraphView.h \
    Graphs/glcommon.h \
    Graphs/gLineChart.h \
    Graphs/gLineOverlay.h \
    Graphs/gSegmentChart.h\
    Graphs/gspacer.h \
    Graphs/gStatsLine.h \
    Graphs/gSummaryChart.h \
    Graphs/gAHIChart.h \
    Graphs/gTTIAChart.h \
    Graphs/gUsageChart.h \
    Graphs/gSessionTimesChart.h \
    Graphs/gPressureChart.h \
    Graphs/gOverviewGraph.h \
    Graphs/gXAxis.h \
    Graphs/gYAxis.h \
    Graphs/layer.h \
    SleepLib/calcs.h \
    SleepLib/common.h \
    SleepLib/day.h \
    SleepLib/event.h \
    SleepLib/machine.h \
    SleepLib/machine_common.h \
    SleepLib/machine_loader.h \
    SleepLib/importcontext.h \
    SleepLib/preferences.h \
    SleepLib/profiles.h \
    SleepLib/schema.h \
    SleepLib/session.h \
    SleepLib/loader_plugins/bmcDataParsing.h \
    SleepLib/loader_plugins/bmc_loader.h \
    SleepLib/loader_plugins/cms50_loader.h \
    SleepLib/loader_plugins/dreem_loader.h \
    SleepLib/loader_plugins/icon_loader.h \
    SleepLib/loader_plugins/sleepstyle_loader.h \
    SleepLib/loader_plugins/sleepstyle_EDFinfo.h \
    SleepLib/loader_plugins/intellipap_loader.h \
    SleepLib/loader_plugins/mseries_loader.h \
    SleepLib/loader_plugins/prisma_loader.h \
    SleepLib/loader_plugins/vrem_loader.h \
    SleepLib/loader_plugins/prs1_loader.h \
    SleepLib/loader_plugins/prs1_parser.h \
    SleepLib/loader_plugins/resmed_loader.h \
    SleepLib/loader_plugins/resmed_EDFinfo.h \
    SleepLib/loader_plugins/somnopose_loader.h \
    SleepLib/loader_plugins/viatom_loader.h \
    SleepLib/loader_plugins/zeo_loader.h \
    SleepLib/loader_plugins/resvent_loader.h \
    SleepLib/thirdparty/botan_all.h \
    SleepLib/thirdparty/botan_windows.h \
    SleepLib/thirdparty/botan_linux.h \
    SleepLib/thirdparty/botan_macos.h \
    SleepLib/crypto.h \
    zip.h \
    SleepLib/thirdparty/miniz.h \
    csv.h \
    rawdata.h \
    translation.h \
    statistics.h \
    oximeterimport.h \
    SleepLib/deviceconnection.h \
    SleepLib/xmlreplay.h \
    SleepLib/serialoximeter.h \
    SleepLib/loader_plugins/md300w1_loader.h \
    logger.h \
    SleepLib/loader_plugins/weinmann_loader.h \
    Graphs/gdailysummary.h \
    Graphs/MinutesAtPressure.h \
    SleepLib/journal.h \
    SleepLib/progressdialog.h \
    SleepLib/loader_plugins/cms50f37_loader.h \
    profileselector.h \
    SleepLib/appsettings.h \
    SleepLib/loader_plugins/edfparser.h \
    aboutdialog.h \
    welcome.h \
    mytextbrowser.h \
    staticQMessageBox.h \
    git_info.h
!contains(DEFINES, helpless) {
    HEADERS += help.h
}

FORMS += \
    daily.ui \
    overview.ui \
    mainwindow.ui \
    oximetry.ui \
    preferencesdialog.ui \
    newprofile.ui \
    exportcsv.ui \
#    UpdaterWindow.ui \
    oximeterimport.ui \
    profileselector.ui \
    aboutdialog.ui \
    welcome.ui
!contains(DEFINES, helpless) {
    FORMS += help.ui
}
equals(QT_MAJOR_VERSION,5) {
    lessThan(QT_MINOR_VERSION,12) {
        FORMS += reports.ui
    }
}

RESOURCES += \
    Resources.qrc

OTHER_FILES += \
    docs/index.html \
    docs/schema.xml \
    docs/graphs.xml \
    docs/channels.xml \
    docs/startup_tips.txt \
    docs/countries.txt \
    docs/tz.txt \
    ../LICENSE.txt \
    docs/tooltips.css \
    docs/script.js \
    ../update.xml \
    docs/changelog.txt \
    docs/intro.html \
    docs/statistics.xml \
    update_gitinfo.bat \
    update_gitinfo.sh

DISTFILES += \
    ../README
!contains(DEFINES, helpless) {
DISTFILES += help/default.css \
    help/help_en/daily.html \
    help/help_en/glossary.html \
    help/help_en/import.html \
    help/help_en/index.html \
    help/help_en/overview.html \
    help/help_en/oximetry.html \
    help/help_en/statistics.html \
    help/help_en/supported.html \
    help/help_en/gettingstarted.html \
    help/help_en/tipsntricks.html \
    help/help_en/faq.html \
    help/help_nl/daily.html \
    help/help_nl/faq.html \
    help/help_nl/gettingstarted.html \
    help/help_nl/glossary.html \
    help/help_nl/import.html \
    help/help_nl/index.html \
    help/help_nl/overview.html \
    help/help_nl/oximetry.html \
    help/help_nl/statistics.html \
    help/help_nl/supported.html \
    help/help_nl/tipsntricks.html \
    help/help_en/reportingbugs.html \
    help/help_nl/OSCAR_Guide_nl.qhp \
    help/help_en/OSCAR_Guide_en.qhp \
    help/index.qhcp
}

message("CXXFLAGS pre-mods $$QMAKE_CXXFLAGS ")

# Always treat warnings as errors, even (especially!) in release
QMAKE_CFLAGS += -Werror
QMAKE_CXXFLAGS += -Werror

gcc | clang {
    COMPILER_VERSION = $$system($$QMAKE_CXX " -dumpversion")
    COMPILER_MAJOR = $$split(COMPILER_VERSION, ".")
    COMPILER_MAJOR = $$first(COMPILER_MAJOR)

    message("$$QMAKE_CXX major version $$COMPILER_MAJOR")
}

if (false) {
  if (equals(QT_MAJOR_VERSION,6)) {
   if (greaterThan(QT_MINOR_VERSION,8)) {
    ## allow deprecated warings for TESTING
    message("ALLOW DEPRECATED WARNINGS FOR TESTING ")
    # Make deprecation warnings just warnings
    QMAKE_CFLAGS += -Wno-error=deprecated-declarations
    QMAKE_CXXFLAGS += -Wno-error=deprecated-declarations
} } }

message("CXXFLAGS post-mods $$QMAKE_CXXFLAGS ")
message("CXXFLAGS_WARN_ON $$QMAKE_CXXFLAGS_WARN_ON")

lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
    QMAKE_CFLAGS += -Wno-error=strict-aliasing
    QMAKE_CXXFLAGS += -Wno-error=strict-aliasing
}

# Create a debug GUI build by adding "CONFIG+=memdebug" to your qmake command
memdebug {
    CONFIG += debug
    ## there is an error in qt. qlist.h uses an implicitly defined operator=
    ## allow this for debug
    QMAKE_CFLAGS += -Wno-error=deprecated-copy
    QMAKE_CXXFLAGS += -Wno-error=deprecated-copy
    !win32 {  # add memory checking on Linux and macOS debug builds
        QMAKE_CFLAGS += -g -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
            QMAKE_CFLAGS -= -fsanitize-address-use-after-scope
        }
        QMAKE_CXXFLAGS += -g -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
            QMAKE_CXXFLAGS -= -fsanitize-address-use-after-scope
        }
        QMAKE_LFLAGS += -fsanitize=address
    }
}

# Turn on unit testing by adding "CONFIG+=test" to your qmake command
test {
    TARGET = test
    DEFINES += UNITTEST_MODE

    QT += testlib
    QT -= gui
    CONFIG += console debug
    CONFIG -= app_bundle
    !win32 {  # add memory checking on Linux and macOS test builds
        QMAKE_CFLAGS += -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
            QMAKE_CFLAGS -= -fsanitize-address-use-after-scope
        }
        QMAKE_CXXFLAGS += -Werror -fsanitize=address -fno-omit-frame-pointer -fno-common -fsanitize-address-use-after-scope
        lessThan(QT_MAJOR_VERSION,5)|lessThan(QT_MINOR_VERSION,9) {
            QMAKE_CXXFLAGS -= -fsanitize-address-use-after-scope
        }
        QMAKE_LFLAGS += -fsanitize=address
    }

    SOURCES += \
        tests/prs1tests.cpp \
        tests/rawdatatests.cpp \
        tests/resmedtests.cpp \
        tests/sessiontests.cpp \
        tests/versiontests.cpp \
        tests/viatomtests.cpp \
        tests/deviceconnectiontests.cpp \
        tests/cryptotests.cpp \
        tests/dreemtests.cpp \
        tests/eventstabtests.cpp \
        tests/zeotests.cpp

    HEADERS += \
        tests/AutoTest.h \
        tests/prs1tests.h \
        tests/rawdatatests.h \
        tests/resmedtests.h \
        tests/sessiontests.h \
        tests/versiontests.h \
        tests/viatomtests.h \
        tests/deviceconnectiontests.h \
        tests/cryptotests.h \
        tests/dreemtests.h \
        tests/eventstabtests.h \
        tests/zeotests.h
}

macx {
    app_bundle {
        # On macOS put a custom Info.plist into the bundle that disables dark mode on Mojave.
        QMAKE_INFO_PLIST = "../Building/MacOS/Info.plist.in"

        # Add the git revision to the Info.plist.
        Info_plist.target = Info.plist
        Info_plist.depends = $${TARGET}.app/Contents/Info.plist
        Info_plist.commands = $$_PRO_FILE_PWD_/../Building/MacOS/finalize_plist $$_PRO_FILE_PWD_ $${TARGET}.app/Contents/Info.plist
        QMAKE_EXTRA_TARGETS += Info_plist
        PRE_TARGETDEPS += $$Info_plist.target
    }

    # Add a dist-mac target to build the distribution .dmg.
    QMAKE_EXTRA_TARGETS += dist-mac
    dist-mac.commands = QT_BIN=$$[QT_INSTALL_PREFIX]/bin $$_PRO_FILE_PWD_/../Building/MacOS/create_dmg $${TARGET} $${TARGET}.app $$_PRO_FILE_PWD_/../Building/MacOS/README.rtfd $$_PRO_FILE_PWD_/../Building/MacOS/background.png
    dist-mac.depends = $${TARGET}.app/Contents/MacOS/$${TARGET}
}
