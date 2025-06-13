/* search  GUI Headers
 *
 * Copyright (c) 2024-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef NOTIFYMESSAGEBOX_H
#define NOTIFYMESSAGEBOX_H

#define STOPTIMER

#include <QMessageBox>
#include <QFont>
#include <QDebug>

#ifdef STOPTIMER
#include <QTimer>
#include <QPushButton>
#endif




class NotifyMessageBox : public QObject
{
    Q_OBJECT

public:
enum NMB_STATE { nmb_init, nmb_running, nmb_stopped };
    NotifyMessageBox(const QString& title, 
        const QString& message, 
        int timeoutSeconds = 0 , 
        const QString& timeoutMessage = "" , 
        QWidget* parent = nullptr);
    virtual ~NotifyMessageBox();

private:
    void setupMessageBox();
    void setupTimer();

private slots:
    void releaseResources();

    void onTimeout();
#if defined(STOPTIMER)
    void onStop() ;
    void onTerminate() ;
#endif

private:
    QWidget* m_parent;
    QString m_title;
    QString m_message;
    QMessageBox* m_msgBox;
    int     m_timeoutSeconds;
    QString m_timeoutMessage;
    NMB_STATE m_state = nmb_init;
#if defined(STOPTIMER)
    QTimer* m_timer;
    QPushButton *stopB ;
    QPushButton *terminateB ;
#endif
};


NotifyMessageBox* createNotifyMessageBox (
        QWidget* parent ,
        const QString& title, 
        const QString& message, 
        int timeoutSeconds = 0 ,
        const QString& timeoutMessage = "" ) ;

#if 0
NotifyMessageBox* createNotifyMessageBox (
        const QString& title, 
        const QString& message, 
        int timeoutSeconds = 0 , 
        const QString& timeoutMessage = "" , 
        QWidget* parent = nullptr);

NotifyMessageBox* createNotifyMessageBox (
        const QString& title, 
        const QString& message, 
        int timeoutSeconds = 0 , 
        enum QMessageBox::Icon msgIcon = 
        const QString& timeoutMessage = "" , 
        QWidget* parent = nullptr);
#endif

#endif // NOTIFYMESSAGEBOX_H


