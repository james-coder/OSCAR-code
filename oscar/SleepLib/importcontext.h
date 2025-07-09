/* SleepLib Import Context Header
 *
 * Copyright (c) 2021-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef IMPORTCONTEXT_H
#define IMPORTCONTEXT_H

#include "SleepLib/machine_loader.h"


class ImportContext : public QObject
{
    Q_OBJECT
public:
    ImportContext();
    virtual ~ImportContext();

    // Loaders will call this directly. It manages the device's stored set of previously seen messages
    // and will emit an importEncounteredUnexpectedData signal in its dtor if any are new.
    void LogUnexpectedMessage(const QString & message);

signals:
    void importEncounteredUnexpectedData(const MachineInfo & info, QSet<QString> & newMessages);

public:
    // Emit the importEncounteredUnexpectedData signal if there are any new messages and clear the list.
    // TODO: This will no longer need to be public once a context doesn't get reused between devices.
    void FlushUnexpectedMessages();

    virtual bool ShouldIgnoreOldSessions() { return false; }
    virtual QDateTime IgnoreSessionsOlderThan() { return QDateTime(); }
    
    // TODO: Isolate the Machine object from the loader rather than returning it.
    virtual Machine* CreateMachineFromInfo(const MachineInfo & info) = 0;
    
    // TODO: Eventually backup (and rebuild) should be handled invisibly to loaders.
    virtual QString GetBackupPath();

    virtual bool SessionExists(SessionID sid);
    
    // Create an in-memory Session object for the importer to fill out.
    virtual Session* CreateSession(SessionID sid);

    // Write the session to disk and release its memory, adding it to the queue to be committed.
    virtual bool AddSession(Session* session);
    
    // Update the database to include all the newly added sessions.
    virtual bool Commit();

protected:
    QMutex m_logMutex;
    QSet<QString> m_unexpectedMessages;

    MachineInfo m_machineInfo;
    Machine* m_machine;

    QMutex m_sessionMutex;
    QMap<SessionID, Session *> m_sessions;
};


// Loaders and parsers #define IMPORT_CTX and SESSIONID based on their internal data structures.
#define UNEXPECTED_VALUE(SRC, VALS) { \
    QString message = QString("%1:%2: %3 = %4 != %5").arg(__func__).arg(__LINE__).arg(#SRC).arg(SRC).arg(VALS); \
    qWarning() << SESSIONID << message; \
    IMPORT_CTX->LogUnexpectedMessage(message); \
    }
#define CHECK_VALUE(SRC, VAL) if ((SRC) != (VAL)) UNEXPECTED_VALUE(SRC, VAL)
#define CHECK_VALUES(SRC, VAL1, VAL2) if ((SRC) != (VAL1) && (SRC) != (VAL2)) UNEXPECTED_VALUE(SRC, #VAL1 " or " #VAL2)
// For more than 2 values, just write the test manually and use UNEXPECTED_VALUE if it fails.
#define HEX(SRC) { qWarning() << SESSIONID << QString("%1:%2: %3 = %4").arg(__func__).arg(__LINE__).arg(#SRC).arg((SRC & 0xFF), 2, 16, QChar('0')); }


// ProfileImportContext isolates the loader from Profile and its storage.
class Profile;
class ProfileImportContext : public ImportContext
{
    Q_OBJECT
public:
    ProfileImportContext(Profile* profile);
    virtual ~ProfileImportContext() {};

    virtual bool ShouldIgnoreOldSessions();
    virtual QDateTime IgnoreSessionsOlderThan();

    virtual Machine* CreateMachineFromInfo(const MachineInfo & info);

protected:
    Profile* m_profile;
};


// TODO: Add a TestImportContext that writes the data to YAML for regression tests.


// TODO: Once all loaders support context and UI, move this into the main application
// and refactor its import UI logic into this class.
class ImportUI : public QObject
{
    Q_OBJECT
public:
    ImportUI(Profile* profile);
    virtual ~ImportUI() {}

public slots:
    void onUnexpectedData(const MachineInfo & machine, QSet<QString> & newMessages);
    void onDeviceReportsUsageOnly(const MachineInfo & info);
    void onDeviceIsUntested(const MachineInfo & info);
    void onDeviceIsUnsupported(const MachineInfo & info);

protected:
    Profile* m_profile;
};


#endif // IMPORTCONTEXT_H
