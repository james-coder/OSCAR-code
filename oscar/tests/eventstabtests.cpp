/* Events Tab Unit Tests
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

// Include the test class header
#include "eventstabtests.h"

#include <QDebug>
#include <QProgressBar>
#include <QSettings>
#include "mainwindow.h"
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"
#include "SleepLib/loader_plugins/resmed_loader.h"

#define TEST_MACROS_ENABLEDoff
#include "test_macros.h"

#define TESTDATA_PATH "./testdata/"

#define APP_DATA_SETTING "Settings/AppData"

extern MainWindow * mainwin;

// Initialize globals for log capturing
QStringList capturedDebugMessages;
QtMessageHandler originalMessageHandler = nullptr;

// Custom log message handler implementation
void capturingMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Capture only Debug messages
    if (type == QtDebugMsg) {
        capturedDebugMessages.append(msg);
    }

    // Call the original handler so messages still appear in the console/debugger
    // You might want to remove this line if you only want to capture for tests
    if (originalMessageHandler) {
        originalMessageHandler(type, context, msg); // Corrected to call original handler
    }
}

// Constructor: keep track of OSCAR globals overwritten
EventsTabTests::EventsTabTests()
{
    DEBUGXD O("EventsTabTests::EventsTabTests");
    m_save_mainwin = mainwin;
    m_save_p_pref = p_pref;
    m_save_p_profile = p_profile;
    m_save_AppSetting = AppSetting;
    QSettings settings;
    m_save_app_data = settings.value(APP_DATA_SETTING).toString();
}

EventsTabTests::~EventsTabTests()
{
    DEBUGXD O("EventsTabTests::~EventsTabTests");
}

// Overall initialization: opens event-tab sample profile and thus
// indirectly creates the infrastructure for the events tab display.
// This includes a hidden QApplication.
// TODO: Move infrastructure init to a supporting module for OSCAR tests.
void EventsTabTests::initTestCase()
{
    DEBUGXD O("EventsTabTests::initTestCase");

    // note: need dummy app for Qt even though hidden
    int argc = 0;
    char** argv = nullptr;
    m_app = new QApplication(argc, argv);

    // Load in profile with sample data.
    // See http://www.tomasohara.trade/misc/testdata-22may25.tar.gz
    qDebug() << "loading mocked up profile";
    p_profile = new Profile(TESTDATA_PATH "profile/tpo-25apr25", true);
    Q_ASSERT(p_profile);

    // Make sure global preferences initialized
    // note: based on ResmedTests::initTestCase
    qDebug() << "loading mocked up preferences";
    p_pref = new Preferences("Preferences", TESTDATA_PATH "Preferences.xml");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    // Set Qt application data
    QSettings settings;
    settings.setValue(APP_DATA_SETTING, TESTDATA_PATH);
    
    // Make sure main window unset
    mainwin = nullptr;

# if 0
    // Alternative code using hidden main window
    // NOTE: Not quite functional but would allow for better code coverage.

    // Scan for user profiles (for automatic loading via mainwin)
    Profiles::Scan();

    // Startup hidden application
    // note: The Daily view assumes various UI control setup
    mainwin = new MainWindow();
    mainwin->SetupGUI();                // needed for profile initialization
    mainwin->hide();
# endif

    // Explicitly load machine data
    qDebug() << "loading mocked up machine data";
    m_progress = new ProgressDialog(m_app->activeWindow());
    p_profile->LoadMachineData(m_progress);
    
    qDebug() << "loading mocked up events";
    m_target_date = QDate().fromString("2025-04-25", "yyyy-MM-dd");

    // Sanity check on session loading
    Day *day = p_profile->GetDay(m_target_date, MT_CPAP);
    if (!day) {
        qWarning() << "No sessions found for" << m_target_date.toString();
    } else {
        qDebug() << "Found" << day->sessions.count() << "sessions for" << m_target_date.toString();
    }

    // Needed unless using hidden mainwin (which does this automatically)
    qDebug() << "initializing Daily view for" << m_target_date;
    m_graphview = new gGraphView();
    m_daily = new Daily(nullptr, m_graphview);
}

// Free up the object and reset to null
# define DELETE_OBJ(p_obj)              \
    if (p_obj) {                        \
        DEBUGXD O("deleting " #p_obj);  \
        delete p_obj;                   \
        p_obj = nullptr;                \
    }

void EventsTabTests::freeObjects()
{
    DEBUGXD O("EventsTabTests::freeObjects");

    DELETE_OBJ(m_daily);
    // TODO: fix ~gGraphView to handle this
    DestroyGraphGlobals();
    DELETE_OBJ(m_graphview);
    DELETE_OBJ(AppSetting);
    DELETE_OBJ(p_profile);
    DELETE_OBJ(p_pref);
    DELETE_OBJ(m_progress);
    DELETE_OBJ(m_app);
}

// Overall cleanup
void EventsTabTests::cleanupTestCase()
{
    DEBUGXD O("EventsTabTests::cleanupTestCase");
    
    // Cleanup objects
    freeObjects();

    // Restore OSCAR globals
    mainwin = m_save_mainwin;
    p_pref = m_save_p_pref;
    p_profile = m_save_p_profile;
    AppSetting = m_save_AppSetting;

    // Likewise for Qt
    QSettings settings;
    settings.setValue(APP_DATA_SETTING, m_save_app_data);
}

// Per-test initialization
void EventsTabTests::init()
{
    DEBUGXD O("EventsTabTests::init");
    // Install the custom message handler and store the original one
    originalMessageHandler = qInstallMessageHandler(capturingMessageOutput);
    capturedDebugMessages.clear(); // Ensure the list is empty before running tests
}

// Per-test cleanup
void EventsTabTests::cleanup()
{
    DEBUGXD O("EventsTabTests::cleanup");
    // Restore the original message handler
    qInstallMessageHandler(originalMessageHandler);

    // Clear the list after running tests
    capturedDebugMessages.clear();
}

// Helper function to compare captured messages, checking if expectedMessages is a subsequence
void EventsTabTests::compareCapturedMessages(const QStringList& expectedMessages)
{
    DEBUGXD O("EventsTabTests::compareCapturedMessages");
    int capturedIndex = 0;
    bool allFound = true;

    for (const QString& expectedMsg : expectedMessages) {
        bool found = false;
        for (int i = capturedIndex; i < capturedDebugMessages.size(); ++i) {
            if (capturedDebugMessages.at(i).contains(expectedMsg)) {
                capturedIndex = i + 1; // Start searching from the next message
                found = true;
                break;
            }
        }
        if (!found) {
            allFound = false;
            QFAIL(qPrintable(QString("Expected message not found in captured output (starting from index %1): \"%2\"").arg(capturedIndex).arg(expectedMsg)));
            // No need to check further expected messages if one is missing
            break;
        }
    }

    QVERIFY(allFound);
}

// Loads target date with multiple event types (i.e., 04/25/2025)
// note: Profile loading now done via initTestCase().
void EventsTabTests::loadSampleData(bool consolidated, double event_post_context_secs)
{
    DEBUGXD O("EventsTabTests::loadSampleData") O(consolidated) O(event_post_context_secs);
    if (! p_profile) {
        qWarning() << "Unexpected condition in loadSampleData";
        initTestCase();
    }
    p_profile->cpap->setConsolidateEvents(consolidated);
    p_profile->cpap->setEventPostcontext(event_post_context_secs);
 
    m_daily->Load(m_target_date);
}

void EventsTabTests::testDefaults()
{
    DEBUGXD O("EventsTabTests::testDefaults");
    capturedDebugMessages.clear(); // Clear messages before this specific test

    // Load data with default settings
    qDebug() << "Loading without consolidation and without post-context";
    loadSampleData(false, 0);
    
    // Define the expected debug messages
    QStringList expectedMessages;
    expectedMessages << "max_t_post_context: 1745679339000"  // 2025-04-26 09:55:39
                     << "consolidate: false";

    // Compare the captured messages with the expected messages using the helper
    compareCapturedMessages(expectedMessages);
}

void EventsTabTests::testOptions()
{
    DEBUGXD O("EventsTabTests::testOptions");
    capturedDebugMessages.clear(); // Clear messages before this specific test

    // Load data with consolidation and with post-context
    qDebug() << "Loading with consolidation and 15sec post-context";
    loadSampleData(true, 15);

    // Define the expected debug messages
    QStringList expectedMessages;
    expectedMessages << "max_t_post_context: 1745679354000"  // 2025-04-26 09:55:54
                     << "consolidate: true"
                     << "\"All\"";

    // Compare the captured messages with the expected messages using the helper
    compareCapturedMessages(expectedMessages);
}
