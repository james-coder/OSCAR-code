/* Events Tab Unit Tests
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef EVENTSTABTESTS_H
#define EVENTSTABTESTS_H

#include <QObject>
#include <QStringList>
#include <QtGlobal>    // Provides QtMessageHandler for custom message handling

#include "tests/AutoTest.h"
#include "daily.h"

// Forward declaration of the custom message handler
void capturingMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

class EventsTabTests : public QObject
{
    Q_OBJECT

public:
    EventsTabTests();
    ~EventsTabTests();

    // Helper functions
    void freeObjects();
    void compareCapturedMessages(const QStringList& expectedMessages);
    void loadSampleData(bool consolidated, double event_post_context_secs);

private slots:
    // QTest functions executed before and after test cases
    void init();
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    
    // Test functions
    void testDefaults();
    void testOptions();
    void testHtmlSummary();

private:
    // Member variables
    Daily* m_daily = nullptr;
    QApplication* m_app = nullptr;
    gGraphView* m_graphview = nullptr;
    QDate m_target_date;
    MainWindow* m_save_mainwin = nullptr;
    Preferences* m_save_p_pref = nullptr;
    AppWideSetting* m_save_AppSetting = nullptr;
    Profile* m_save_p_profile = nullptr;
    ProgressDialog* m_progress = nullptr;
    QString m_save_app_data;
};

// Register the test class with the AutoTest framework
DECLARE_TEST(EventsTabTests)

#endif // EVENTSTABTESTS_H
