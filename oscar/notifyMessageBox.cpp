/* Daily Panel
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "notifyMessageBox.h"

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>


int defaultTimeoutSeconds = 4 ;
//QString defaultTimeoutMessage = QObject::tr("Notifcation expires in %1 seconds.\nTo Dismiss: Press Escape or Enter.");
//QString timeoutStoppedMessage = QObject::tr("Timer Stopped.\nTo Dismiss: Press Escape or Enter.");
QString defaultTimeoutMessage = QObject::tr("Notifcation expires in %1 seconds.");
QString timeoutStoppedMessage = QObject::tr("");

    NotifyMessageBox::NotifyMessageBox(const QString& title, const QString& message, int timeoutSeconds,  const QString& timeoutMessage, QWidget* parent)
        : QObject(parent), m_title(title), m_message(message), m_timeoutSeconds(timeoutSeconds), m_timeoutMessage(timeoutMessage), m_state(nmb_init)
    {
        if (timeoutSeconds<defaultTimeoutSeconds) m_timeoutSeconds=defaultTimeoutSeconds;
        if ( timeoutMessage.isEmpty() ) m_timeoutMessage = defaultTimeoutMessage;
        setupTimer();

    }

    NotifyMessageBox::~NotifyMessageBox() 
    {
    };

    void NotifyMessageBox::setupMessageBox()
    {
        m_msgBox = new QMessageBox(QMessageBox::Information,m_title,m_message);
        //QFont boldFont;
        //boldFont.setBold(true);
        //m_msgBox->setFont(boldFont);
        m_msgBox->setWindowTitle(m_title);
        m_msgBox->setText(m_message);

        #ifndef STOPTIMER
            m_msgBox->setStandardButtons(QMessageBox::Ok);
        #else 

            // Set action as the default button
            QPushButton *terminateB = m_msgBox->addButton("Dismiss", QMessageBox::RejectRole);
            m_msgBox->setDefaultButton(terminateB);
            connect(m_msgBox, SIGNAL(rejected()), this, SLOT(onTerminate()));
            if (m_state == nmb_stopped) {
                m_msgBox->setInformativeText(timeoutStoppedMessage);
            } else {
                m_msgBox->setInformativeText(m_timeoutMessage.arg(m_timeoutSeconds));
                m_msgBox->addButton("Stop Timer", QMessageBox::AcceptRole);
                m_msgBox->setEscapeButton(QMessageBox::Cancel);
                connect(m_msgBox, SIGNAL(accepted()), this, SLOT(onStop()));
            }

        #endif
        m_msgBox->show();
        m_msgBox->raise();
    }


#if defined(STOPTIMER)
    void NotifyMessageBox::onStop()
    {
        m_msgBox->close();
        m_state = nmb_stopped;
        setupMessageBox();
    }

    void NotifyMessageBox::onTerminate()
    {
        releaseResources();
    }
#endif

    void NotifyMessageBox::onTimeout()
    {
        switch (m_state) {
            case nmb_init:
                m_timer->setInterval(1000);
                setupMessageBox();
                m_state = nmb_running;
                break;
            case nmb_running:
                m_timer->setInterval(1000);
                m_timeoutSeconds--;
                if (m_timeoutSeconds == 0) {
                    m_state = nmb_stopped;
                    releaseResources();
                } else {
                    m_msgBox->setInformativeText(m_timeoutMessage.arg(m_timeoutSeconds));
                }
                break;
            default:
            case nmb_stopped:
                break;
        }
    }

    void NotifyMessageBox::setupTimer()
    {
        #if defined(STOPTIMER)
        m_timer = new QTimer(this);
        m_timer->setInterval(1);
        m_timer->setSingleShot(false);

        connect(m_timer, &QTimer::timeout, this, &NotifyMessageBox::onTimeout);
        m_timer->start();
        #endif
    }


    void NotifyMessageBox::releaseResources()
    {
        m_msgBox->close();
#if defined(STOPTIMER)
        m_timer->deleteLater();
#endif
    }

    NotifyMessageBox* createNotifyMessageBox(
        const QString& title, 
        const QString& message, 
        int timeoutSeconds, 
        const QString& timeoutMessage, 
        QWidget* parent)
    {
        NotifyMessageBox* msgBox = new NotifyMessageBox(title, message, timeoutSeconds, timeoutMessage,  parent);
        return msgBox;
    }

NotifyMessageBox* createNotifyMessageBox (
        QWidget* parent ,
        const QString& title, 
        const QString& message, 
        int timeoutSeconds ,
        const QString& timeoutMessage 
        ) {
            return createNotifyMessageBox(title, message, timeoutSeconds, timeoutMessage,  parent);
        };
#if 0
NotifyMessageBox* createNotifyMessageBox (
        QWidget* parent ,
        const QString& title, 
        const QString& message, 
        enum QMessageBox::Icon msgIcon,
        int timeoutSeconds ,
        const QString& timeoutMessage 
        ) {
            return createNotifyMessageBox(title, message, timeoutSeconds, timeoutMessage,  parent);
        };
#endif



