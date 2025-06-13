/* SleepLib Import Context Implementation
*
* Copyright (c) 2021-2025 The OSCAR Team
*
* This file is subject to the terms and conditions of the GNU General Public
* License. See the file COPYING in the main directory of the source code
* for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QApplication>
#include <QMessageBox>

#include "SleepLib/importcontext.h"


ImportContext::ImportContext()
    : m_machine(nullptr)
{
}

ImportContext::~ImportContext()
{
    FlushUnexpectedMessages();
}

void ImportContext::LogUnexpectedMessage(const QString & message)
{
    m_logMutex.lock();
    m_unexpectedMessages += message;
    m_logMutex.unlock();
}

void ImportContext::FlushUnexpectedMessages()
{
    if (m_unexpectedMessages.count() > 0 && m_machine) {
        // Compare this to the list of messages previously seen for this device
        // and only alert if there are new ones.
        QSet<QString> newMessages = m_unexpectedMessages - m_machine->previouslySeenUnexpectedData();
        if (newMessages.count() > 0) {
            emit importEncounteredUnexpectedData(m_machine->getInfo(), newMessages);
            m_machine->previouslySeenUnexpectedData() += newMessages;
        }
    }
    m_unexpectedMessages.clear();
}

QString ImportContext::GetBackupPath()
{
    Q_ASSERT(m_machine);
    return m_machine->getBackupPath();
}

bool ImportContext::SessionExists(SessionID sid)
{
    Q_ASSERT(m_machine);
    return m_machine->SessionExists(sid);
}

Session* ImportContext::CreateSession(SessionID sid)
{
    Q_ASSERT(m_machine);
    return new Session(m_machine, sid);
}

bool ImportContext::AddSession(Session* session)
{
    // Make sure the session will be saved.
    session->SetChanged(true);

    // Update indexes, process waveform and perform flagging.
    session->UpdateSummaries();

    // Write the session file to disk.
    bool ok = session->Store(session->machine()->getDataPath());
    if (!ok) {
        qWarning() << "Failed to store session" << session->session();
    }

    // Unload the memory-intensive data now that it's written to disk.
    session->TrashEvents();
    
    // TODO: Remove MachineLoader::addSession once all loaders use this.
    // Add the session to the database
    m_sessionMutex.lock();
    m_sessions[session->session()] = session;
    m_sessionMutex.unlock();

    return ok;
}

bool ImportContext::Commit()
{
    bool ok = true;
    
    // TODO: Remove MachineLoader::finishAddingSessions once all loaders use this.
    // Using a map specifically so they are inserted in order.
    for (auto session : m_sessions) {
        bool added = session->machine()->AddSession(session);
        if (!added) {
            qWarning() << "Session" << session->session() << "was not addded";
            ok = false;
        }
    }
    m_sessions.clear();

    // TODO: Move what we can from finishCPAPImport into here,
    // e.g. Profile::StoreMachines and Machine::SaveSummaryCache.
    return ok;
}

ProfileImportContext::ProfileImportContext(Profile* profile)
    : m_profile(profile)
{
    Q_ASSERT(m_profile);
}

bool ProfileImportContext::ShouldIgnoreOldSessions()
{
    return m_profile->session->ignoreOlderSessions();
}

QDateTime ProfileImportContext::IgnoreSessionsOlderThan()
{
    return m_profile->session->ignoreOlderSessionsDate();
}

Machine* ProfileImportContext::CreateMachineFromInfo(const MachineInfo & info)
{
    if (m_machine) {
        // TODO: Ultimately a context will probably take MachineInfo as a constructor,
        // once all loaders fully populate MachineInfo prior to import.
        qWarning() << "ProfileImportContext::CreateMachineFromInfo called more than once for this context!";
    }
    m_machineInfo = info;
    m_machine = m_profile->CreateMachine(m_machineInfo);
    return m_machine;
}


// MARK: -

ImportUI::ImportUI(Profile* profile)
    : m_profile(profile)
{
    Q_ASSERT(m_profile);
}

void ImportUI::onUnexpectedData(const MachineInfo & info, QSet<QString> & /*newMessages*/)
{
    QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Untested Data"),
                             QObject::tr("Your %1 %2 (%3) generated data that OSCAR has never seen before.").arg(info.brand).arg(info.model).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("The imported data may not be entirely accurate, so the developers would like a .zip copy of this device's SD card and matching clinician .pdf reports to make sure OSCAR is handling the data correctly.")
                             ,QMessageBox::Ok);
}

void ImportUI::onDeviceReportsUsageOnly(const MachineInfo & info)
{
    if (m_profile->cpap->brickWarning()) {
        QApplication::processEvents();
        QMessageBox::information(QApplication::activeWindow(),
                                 QObject::tr("Non Data Capable Device"),
                                 QString(QObject::tr("Your %1 CPAP Device (Model %2) is unfortunately not a data capable model.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                                         QObject::tr("I'm sorry to report that OSCAR can only track hours of use and very basic settings for this device."))
                                 ,QMessageBox::Ok);
        m_profile->cpap->setBrickWarning(false);
    }
}

void ImportUI::onDeviceIsUntested(const MachineInfo & info)
{
    Machine* m = m_profile->CreateMachine(info);
    if (m_profile->session->warnOnUntestedMachine() && m->warnOnUntested()) {
        m->suppressWarnOnUntested();  // don't warn the user more than once
        QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Device Untested"),
                             QObject::tr("Your %1 CPAP Device (Model %2) has not been tested yet.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("It seems similar enough to other devices that it might work, but the developers would like a .zip copy of this device's SD card and matching clinician .pdf reports to make sure it works with OSCAR.")
                             ,QMessageBox::Ok);
    }
}

void ImportUI::onDeviceIsUnsupported(const MachineInfo & info)
{
    QMessageBox::information(QApplication::activeWindow(),
                             QObject::tr("Device Unsupported"),
                             QObject::tr("Sorry, your %1 CPAP Device (%2) is not supported yet.").arg(info.brand).arg(info.modelnumber) +"\n\n"+
                             QObject::tr("The developers need a .zip copy of this device's SD card and matching clinician .pdf reports to make it work with OSCAR.")
                             ,QMessageBox::Ok);
}
