/* CheckUpdates
 *
 * Copyright (c) 2011-2018 Mark Watkins 
 * Copyright (c) 2020-2025 OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QDesktopServices>
#include <QResource>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDate>
#include <QDebug>
#include <QXmlStreamReader>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif
#include <QProcess>

#include "checkupdates.h"
#include "version.h"
#include "mainwindow.h"

extern MainWindow *mainwin;

struct VersionInfo {
    QString group;  // test or release
    QString platform; // All or Requested platform
    QString version; // version number
    QString urlInstaller; // URL for installer page
    QString notes; // any notes
};

CheckUpdates::CheckUpdates(QWidget *parent) :
    QMainWindow(parent)
{
    manager = new QNetworkAccessManager(this);
}

CheckUpdates::~CheckUpdates()
{
    disconnect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(replyFinished(QNetworkReply *)));
}

QString platformStr()
{
    static QString platform;

#if defined(Q_OS_WIN64)
    platform="win64";
#elif defined(Q_OS_WIN)
    platform="win32";
#elif defined(Q_OS_MACOS)
    platform="macOS";
#elif defined(Q_OS_LINUX)
    platform="linux";
#else
    platform="unknown";
#endif
    return platform;
}

static const QString OSCAR_Version_File = "http://apneaboard.net/OSCAR/versions.xml";

static QString versionXML;

/*! \fn readLocalVersions
    \brief Reads versions.xml from local OSCAR Data directory
    */
QString readLocalVersions() {
    // Connect and read XML file from disk
    QString filename = GetAppData() + "/versions.xml";
    qDebug() << "Local version control file at" << filename;
    QFile file(filename);
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() << "Cannot open local version control file" << filename << "-" << file.errorString() << "version check disabled";
        return QString();
    }
    QByteArray qba = file.readAll();
    QFileDevice::FileError error = file.error();
    file.close();
    if (error != QFile::NoError) {
        qDebug() << "Error reading local version control file" << filename << "-" << file.errorString() << "version check disabled";
        qDebug() << "versionXML" << versionXML;
        return QString();
    }
    return QString(qba).toLower();
}

/*! \fn GetVersionInfo
    \brief Extracts newer version info for this platform
    If returned versionInfo.version is empty, no newer version was found
    */
VersionInfo getVersionInfo (QString type, QString platform) {
    VersionInfo foundInfo;

    QXmlStreamReader reader(versionXML);

       if (reader.readNextStartElement()) {
           if (reader.name() == QStringLiteral("oscar")){
                //qDebug() << "expecting oscar, read" << reader.name();
               while(reader.readNextStartElement()){
                    //qDebug() << "expecting group, read" << reader.name() << "with id" << reader.attributes().value("id").toString();
                   if(reader.name() == QStringLiteral("group") &&
                        reader.attributes().hasAttribute("id")){
                        if (reader.attributes().value("id").toString() == type) {
                            while(reader.readNextStartElement()) {
                                //qDebug() << "expecting url or platform, read" << reader.name();
                                if (reader.name() == QStringLiteral("installers"))
                                    foundInfo.urlInstaller = reader.readElementText();
                                if (reader.name() == QStringLiteral("platform")) {
                                    QString plat=reader.attributes().value("id").toString();
                                    //qDebug() << "expecting platform, read " << reader.name()  << "with id" << reader.attributes().value("id").toString();

                                    if ((plat == platform) || (plat == "all" && foundInfo.platform.length() == 0)) {
                                        foundInfo.platform = plat;
                                        while(reader.readNextStartElement()) {
                                            //qDebug() << "expecting version or notes, read" << reader.name();
                                            if (reader.name() == QStringLiteral("version")) {
                                                QString fileVersion = reader.readElementText();
                                                Version fv(fileVersion);
                                                if (!fv.IsValid()) {
                                                    qWarning() << "Server file versions.xml contains an invalid version code" << fileVersion;
                                                } else {
                                                    if (fv > getVersion())
                                                        foundInfo.version = fileVersion;  // We found a more recent version
                                                }
                                            }
                                            else if (reader.name() == QStringLiteral("notes")) {
                                                foundInfo.notes = reader.readElementText();
                                            }
                                            else
                                                reader.skipCurrentElement();
                                        }
                                    }
                                }
                            }
                        }
                        else
                            reader.skipCurrentElement();
                   }
                   else
                       reader.skipCurrentElement();
               }
           }
           else {
               qWarning() << "Versions.xml file improperly formed --" << reader.errorString();
               reader.raiseError(QObject::tr("New versions file improperly formed"));
           }
       }
    return foundInfo;
}

void CheckUpdates::compareVersions () {
#ifndef NO_CHECKUPDATES
    // Get any more recent versions available
    VersionInfo releaseVersion = getVersionInfo ("release", platformStr());
    VersionInfo testVersion = getVersionInfo ("test", platformStr());

    msg = "";
    if (!showTestVersion)
        testVersion.version = "";

    if (testVersion.version.length() == 0 && releaseVersion.version.length() == 0) {
        if (showIfCurrent) {
            QString txt = getVersion().IsReleaseVersion()?QObject::tr("release"):QObject::tr("test version");
            QString txt2 = QObject::tr("You are running the latest %1 of OSCAR").arg(txt);
            msg = txt2 + "<p>" + QObject::tr("You are running OSCAR %1").arg(getVersion()) + "</p>";
        }
    } else {
        msg = QObject::tr("A more recent version of OSCAR is available");
        msg += "<p>" + QObject::tr("You are running OSCAR %1").arg(getVersion()) + "</p>";
        if (releaseVersion.version.length() > 0) {
            msg += "<p>" + QObject::tr("OSCAR %1 is available <a href='%2'>here</a>.")
                               .arg(releaseVersion.version,
                               releaseVersion.urlInstaller) + "</p>";
        }
        if (showTestVersion && (testVersion.version.length() > 0)) {
            msg += "<p>" + QObject::tr("Information about more recent test version %1 is available at <a href='%2'>%2</a>")
                               .arg(testVersion.version,
                                testVersion.urlInstaller) + "</p>";
        }
    }

    if (msg.length() > 0) {
        // Add elapsed time in test versions only
//        if (elapsedTime > 0 && !getVersion().IsReleaseVersion())
//            msg += "<font size='-1'><p>" + QString(QObject::tr("(Reading %1 took %2 seconds)")).arg("versions.xml").arg(elapsedTime) + "</p></font>";
        msgIsReady = true;
    }

    AppSetting->setUpdatesLastChecked(QDateTime::currentDateTime());

    return;
#endif
}

void CheckUpdates::showMessage()
{
    if (!msgIsReady)
        return;

    if (showIfCurrent) {
        if (checkingBox != nullptr)
            checkingBox->reset();
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle(QObject::tr("Check for OSCAR Updates"));
    msgBox.setTextFormat(Qt::RichText);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // For Ubuntu 22 on Qt6, we have to show the message box before inserting the message to get the box sized correctly.
    // This will cause a flicker on Ubuntu platform but at least the message box will look appropriate after flickering.
    if (QSysInfo::prettyProductName().contains("Ubuntu 22", Qt::CaseInsensitive)) {
        msgBox.show();
        msgBox.setText(msg);
    }
#endif
    if (msgBox.text().size() <= 0)
        msgBox.setText(msg);
    msgBox.exec();

    msgIsReady = false;
}

void CheckUpdates::checkForUpdates(bool showWhenCurrent)
{
    showIfCurrent = showWhenCurrent;
    showTestVersion = false;

    // If running a test version of OSCAR, try reading versions.xml from OSCAR_Data directory
    // and force display of any new test version
    if (!getVersion().IsReleaseVersion()) {
        showTestVersion = true;
        versionXML = readLocalVersions();
        if (versionXML.length() > 0) {
            compareVersions();
            elapsedTime = 0;
            checkingBox = nullptr;
            showMessage();
            return;
        }
    } else {
        showTestVersion = AppSetting->allowEarlyUpdates();
    }

    readTimer.start();

    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));

    manager->get(QNetworkRequest(QUrl(OSCAR_Version_File)));

    if (showIfCurrent) {
        checkingBox = new QProgressDialog (this);
//        checkingBox->setWindowModality(Qt::WindowModal);
        checkingBox->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
        checkingBox->setWindowFlags(this->windowFlags() & ~Qt::WindowMinMaxButtonsHint);
        checkingBox->setLabelText(tr("Checking for newer OSCAR versions"));
        checkingBox->setMinimumDuration(500);
        checkingBox->setRange(0,0);
        checkingBox->setCancelButton(nullptr);
        checkingBox->setWindowTitle(getAppName());
        checkingBox->exec();
    }

    qDebug() << "Starting network request for" << OSCAR_Version_File;

    return;
}

void CheckUpdates::replyFinished(QNetworkReply *reply)
{
    if (showIfCurrent) {
        if (checkingBox != nullptr)
            checkingBox->reset();
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Update Check Error:" << reply->errorString();
        reply->deleteLater();
        if (!showIfCurrent) {  // for automatic checks, don't show anything
            return;
        }
        msg = QObject::tr("Unable to check for updates. Please try again later.");
        msgIsReady = true;
    } else {
//      qDebug() << reply->header(QNetworkRequest::ContentTypeHeader).toString();
//      qDebug() << reply->header(QNetworkRequest::LastModifiedHeader).toDateTime().toString();
//      qDebug() << reply->header(QNetworkRequest::ContentLengthHeader).toULongLong();
      qDebug() << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      qDebug() << reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

        versionXML = reply->readAll().toLower();
        reply->deleteLater();

        // Only calculate elapsed time for Help/Check for Updates
        // (Auto-update time would include profile opening time)
        if (showIfCurrent) {
            elapsedTime = readTimer.elapsed() / 1000.0;
            qDebug() << "Elapsed time to read versions.XML from web:" << elapsedTime << "seconds";
        }
        else
            elapsedTime = 0;

        compareVersions();
    }

    if (showIfCurrent)
        showMessage();

    return;
}
