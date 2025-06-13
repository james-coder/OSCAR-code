/* OSCAR AboutDialog Implementation
 *
 * Date created: 7/5/2018
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Source Directory. */

#include <QDesktopServices>
#include <QFile>
#include <QDebug>
#include <QMessageBox>

#include "version.h"
#include "SleepLib/appsettings.h"
#include "aboutdialog.h"
#include "ui_aboutdialog.h"


AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    ui->aboutText->setHtml(getAbout());
    ui->creditsText->setHtml(getCredits());
    ui->licenseText->setHtml(getLicense());
    ui->relnotesText->setHtml(getRelnotes());
    ui->versionLabel->setText("");

    QString path = GetAppData();
    // TODO: consider replacing gitrev below with a link or button to the System Information window
    QString text = /* gitrev + */ "<br/><br/><a href=\"file:///"+path+"\">"+tr("Show data folder")+"</a>";
    ui->infoLabel->setText(text);


    setWindowTitle(tr("About OSCAR %1").arg(getVersion().displayString()));
    setMinimumSize(QSize(400,400));
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    connect(ui->closeButton, SIGNAL(clicked(bool)), this, SLOT(accept()));

    int idx=AppSetting->showAboutDialog();
    if (idx<0) idx = 0; // start in about tab
    ui->tabWidget->setCurrentIndex(idx);
}

AboutDialog::~AboutDialog()
{
    disconnect(ui->closeButton, SIGNAL(clicked(bool)), this, SLOT(accept()));
    delete ui;
}

/***************************************************
void AboutDialog::on_donateButton_clicked()
{
    QMessageBox::information(nullptr, STR_MessageBox_Information, tr("Donations are not yet implemented"));
}
******************************************************/

QString AboutDialog::getFilename(QString name)
{
    QString filename;
    QString language = AppSetting->language();
    QString docRoot = appResourcePath() + "/Html/";
    if (language == "en_US") {
        filename = docRoot+name+".html";
    } else {
        filename = docRoot + name + "-" + language + ".html";
        if ( ! QFile::exists(filename) )
            filename = docRoot+name+".html";
    }
    qDebug() << "Looking for " + filename;
    return filename;
}

QString transLink (QString text) {
    return text.replace("This page in other languages:", QObject::tr("This page in other languages:"));
}

QString AboutDialog::getAbout()
{
    QString aboutFile = getFilename("about");
    QFile clfile(aboutFile);
    QString text = tr("Sorry, could not locate About file.");
    if (clfile.open(QIODevice::ReadOnly)) {
        text = clfile.readAll();
        text = transLink(text);
    } else
        qWarning() << "Could not open" << aboutFile << "for reading, error code" << clfile.error() << clfile.errorString();
//        qDebug() << "Failed to open About file";

    return text;
}

QString AboutDialog::getCredits()
{
    QString creditsFile = getFilename("credits");
    QFile clfile(creditsFile);
    QString text = tr("Sorry, could not locate Credits file.");
    if (clfile.open(QIODevice::ReadOnly)) {
        text = clfile.readAll();
        text = transLink(text);
    } else {
        qWarning() << "Could not open" << creditsFile << "for reading, error code" << clfile.error() << clfile.errorString();
    }

    return text;
}

QString AboutDialog::getRelnotes()
{
    QString relNotesFile = getFilename("release_notes");
    QFile clfile(relNotesFile);
    QString changeLog = tr("Sorry, could not locate Release Notes.");
    if (clfile.open(QIODevice::ReadOnly)) {
        //Todo, write XML parser and only show the latest..
        //QTextStream ts(&clfile);
        changeLog = clfile.readAll();
    }
    else
        qWarning() << "Could not open" << relNotesFile << "for reading, error code" << clfile.error() << clfile.errorString();

    QString text = "<html>"
    "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head>"
    "<body><span style=\" font-size:20pt;\">"+tr("Release Notes")+"</span><br/>"
    "<span style=\" font-size:14pt;\">"+QString("OSCAR %1").arg(getVersion())+"</span>"
    "<hr/>";
    if (getVersion().IsReleaseVersion() == false) {
        text += "<p><font color='red' size=+1><b>"+tr("Important:")+"</b></font> "
        "<font size=+1><i>"+tr("As this is a pre-release version, it is recommended that you <b>back up your data folder manually</b> before proceeding, because attempting to roll back later may break things.")+"</i></font></p><hr/>";
    }
    text += changeLog;
    text += "</body></html>";
    text = transLink(text);
    return text;
}

QString AboutDialog::getLicense()
{
    QString text;
    QString licenseFile = ":/docs/GPLv3-"+AppSetting->language();
    if (!QFile::exists(licenseFile)) {
        ui->licenceLabel->setText(tr("To see if the license text is available in your language, see %1.").arg("<a href=\"https://www.gnu.org/licenses/translations.en.html\">https://www.gnu.org/licenses/translations.en.html</a>"));
        ui->licenceLabel->setVisible(true);
        licenseFile = ":/docs/GPLv3-en_US";
    } else {
        ui->licenceLabel->setVisible(false);
    }

    QFile file(licenseFile);
    if (file.open(QIODevice::ReadOnly)) {
        text = "<div align=center>"+QString(file.readAll()).replace("\n","<br/>")+"</div>";
    }
    return text;
}
