/* Check for Updates
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef CHECKUPDATES_H
#define CHECKUPDATES_H

#include <QNetworkAccessManager>
#include <QMainWindow>
#include <QElapsedTimer>
#include <QProgressDialog>

/*! \class CheckUpdates
    \brief Check-for-Updates Module for OSCAR

    This class handles the Check-for-Updates process in OSCAR: it does the network checks,
    parses the version.xml file, checks for any new updates, and advises the user if updates are available.
  */
class CheckUpdates : public QMainWindow
{
    Q_OBJECT

  public:
    explicit CheckUpdates(QWidget *parent = 0);
    ~CheckUpdates();

    //! Start the check
    void checkForUpdates(bool showWhenCurrent);

    //! See if running version is current and prepare message if not
    void compareVersions();

    //! Show message to user, if it is available
    //! If shown, clear the "message ready" flag
    void showMessage();

  protected slots:
    void replyFinished(QNetworkReply *reply);

  private:
    QNetworkAccessManager *manager;

    QElapsedTimer readTimer;
    float elapsedTime;

    QString msg;                // Message to show to user
    bool msgIsReady = false;    // Message is ready to be displayed
    bool showIfCurrent = false; // show a message if running current release
    bool showTestVersion = false; // Show message if test version is available
    QProgressDialog * checkingBox;// Looking for updates message

    QNetworkReply *reply;
};

#endif // CHECKUPDATES_H
