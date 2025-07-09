/* Create New Profile Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QFile>
#include <QDesktopServices>
#include <QSettings>
#include "SleepLib/profiles.h"

#include "newprofile.h"
#include "staticQMessageBox.h"
#include "ui_newprofile.h"
#include "mainwindow.h"
#include "version.h"

extern MainWindow *mainwin;
extern bool openOk;


NewProfile::NewProfile(QWidget *parent, const QString *user) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
    ui(new Ui::NewProfile)
{
    ui->setupUi(this);
    if (user) {
      originalProfileName=*user;
      ui->userNameEdit->setText(*user);
//    ui->userNameEdit->setText(getUserName());
    }
    QLocale locale = QLocale::system();
    QString shortformat = locale.dateFormat(QLocale::ShortFormat);

    if (!shortformat.toLower().contains("yyyy")) {
        shortformat.replace("yy", "yyyy");
    }

    ui->dobEdit->setDisplayFormat(shortformat);
    ui->dateDiagnosedEdit->setDisplayFormat(shortformat);
    m_firstPage = 0;
    ui->backButton->setEnabled(false);
    ui->nextButton->setEnabled(false);

    ui->stackedWidget->setCurrentIndex(0);
    on_cpapModeCombo_activated(0);
    m_passwordHashed = false;
    ui->heightEdit2->setVisible(false);
    ui->heightEdit->setDecimals(0);
    ui->heightEdit->setSuffix(QString(" %1").arg(STR_UNIT_CM));

    {
        // process countries list
        QFile f(":/docs/countries.txt");
        openOk = f.open(QFile::ReadOnly);
        QTextStream cnt(&f);
        QString a;
        ui->countryCombo->clear();
        ui->countryCombo->addItem(tr("Select Country"));

        do {
            a = cnt.readLine();

            if (a.isEmpty()) { break; }

            ui->countryCombo->addItem(a);
        } while (1);

        f.close();
    }
    {
        // timezone list
        QFile f(":/docs/tz.txt");
        openOk = f.open(QFile::ReadOnly);
        QTextStream cnt(&f);
        QString a;
        ui->timezoneCombo->clear();

        //ui->countryCombo->addItem("Select TimeZone");
        do {
            a = cnt.readLine();

            if (a.isEmpty()) { break; }

            QStringList l;
            l = a.split("=");
            ui->timezoneCombo->addItem(l[1], l[0]);
        } while (1);

        f.close();
    }
    ui->versionLabel->setText("");

    ui->textBrowser->setHtml(getIntroHTML());
}

NewProfile::~NewProfile()
{
    delete ui;
}


QString NewProfile::getIntroHTML()
{
    return "<html>"
           "<body>"
           "<div align=center><h1>" + tr("Welcome to the Open Source CPAP Analysis Reporter") + "</h1></div>"

           "<p>" + tr("This software is being designed to assist you in reviewing the data produced by your CPAP Devices and related equipment.")
           + "</p>"

           "<p>" + tr("OSCAR has been released freely under the <a href='qrc:/COPYING'>GNU Public License v3</a>, and comes with no warranty, and without ANY claims to fitness for any purpose.")
           + "</p>"
           "<div align=center><font color=\"red\"><h2>" + tr("PLEASE READ CAREFULLY") + "</h2></font></div>"
           "<p>" + tr("OSCAR is intended merely as a data viewer, and definitely not a substitute for competent medical guidance from your Doctor.")
           + "</p>"

           "<p>" + tr("Accuracy of any data displayed is not and can not be guaranteed.") + "</p>"

           "<p>" + tr("Any reports generated are for PERSONAL USE ONLY, and NOT IN ANY WAY fit for compliance or medical diagnostic purposes.")
           + "</p>"

           "<p>" + tr("The authors will not be held liable for <u>anything</u> related to the use or misuse of this software.")
           + "</p>"

           "<div align=center>"
           "<p><b><font size=+1>" + tr("Use of this software is entirely at your own risk.") +
           "</font></b></p>"

           "<p><i>" + tr("OSCAR is copyright &copy;2011-2018 Mark Watkins and portions &copy;2019-2025 The OSCAR Team") + "<i></p>"
           "</div>"
           "</body>"
           "</html>";
}

#include <cmath>
int cmToFeetInch( double cm, double& inches ) {
    int tenthInches = std::round(cm * inches_per_cm * 10);
    int feet = std::floor(((double)tenthInches) / 120);
    inches = double(tenthInches - (feet *120))/10;
    return feet;
}

double feetInchToCm( int feet , double inches ) {
    double mm = std::round(10*cms_per_inch*(inches + (double)(feet *12)));
    return mm/10;
}

void NewProfile::on_nextButton_clicked()
{
    const QString xmlext = ".xml";

    int index = ui->stackedWidget->currentIndex();

    switch (index) {
    case 0:
        if (!ui->agreeCheckbox->isChecked()) {
            return;
        }

        // Reload Preferences object
        break;

    case 1:
        if (ui->userNameEdit->text().trimmed().isEmpty()) {
            staticQMessageBox::information(this, STR_MessageBox_Error, tr("Please provide a username for this profile"), QMessageBox::Ok);
            return;
        }

        if (ui->genderCombo->currentIndex() == 0) {
            //QMessageBox::information(this,tr("Notice"),tr("You did not specify Gender."),QMessageBox::Ok);
        }

        if (ui->passwordGroupBox->isChecked()) {
            if (ui->passwordEdit1->text() != ui->passwordEdit2->text()) {
                staticQMessageBox::information(this, STR_MessageBox_Error, tr("Passwords don't match"), QMessageBox::Ok);
                return;
            }

            if (ui->passwordEdit1->text().isEmpty()) {
                ui->passwordGroupBox->setChecked(false);
            }
        }

        break;

    case 2:
        break;

    case 3:
        break;

    default:
        break;
    }

    int max_pages = ui->stackedWidget->count() - 1;

    if (index < max_pages) {
        index++;
        ui->stackedWidget->setCurrentIndex(index);
    } else {
        // Finish button clicked.
        newProfileName = ui->userNameEdit->text().simplified();
        QString profileName;
        if (originalProfileName.isEmpty() ) {
            profileName = newProfileName;
        } else {
            profileName = originalProfileName;
            ui->userNameEdit->setText(originalProfileName);
            //QString profileName = originalProfileName.isEmpty()? newProfileName : originalProfileName;
        }
        //QString profileName = originalProfileName.isEmpty()? newProfileName : originalProfileName;

        if ( staticQMessageBox::question(this, tr("Profile Changes"), tr("Accept and save this information?"),
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {

            Profile *profile = Profiles::Get(profileName);
            if (!profile) { // No profile, create one.
                profile = Profiles::Create(profileName);
            }

            Profile &prof = *profile;
            profile->user->setFirstName(ui->firstNameEdit->text());
            profile->user->setLastName(ui->lastNameEdit->text());
            profile->user->setDOB(ui->dobEdit->date());
            profile->user->setEmail(ui->emailEdit->text());
            profile->user->setPhone(ui->phoneEdit->text());
            profile->user->setAddress(ui->addressEdit->toPlainText());

            if (ui->passwordGroupBox->isChecked()) {
                if (!m_passwordHashed) {
                    profile->user->setPassword(ui->passwordEdit1->text().toUtf8());
                }
            } else {

                prof.Erase(STR_UI_Password);
            }

            profile->user->setGender((Gender)ui->genderCombo->currentIndex());

            profile->cpap->setDateDiagnosed(ui->dateDiagnosedEdit->date());
            profile->cpap->setUntreatedAHI(ui->untreatedAHIEdit->value());
            profile->cpap->setMode((CPAPMode)ui->cpapModeCombo->currentIndex());
            profile->cpap->setMinPressure(ui->minPressureEdit->value());
            profile->cpap->setMaxPressure(ui->maxPressureEdit->value());
            profile->cpap->setNotes(ui->cpapNotes->toPlainText());
            profile->doctor->setName(ui->doctorNameEdit->text());
            profile->doctor->setPracticeName(ui->doctorPracticeEdit->text());
            profile->doctor->setAddress(ui->doctorAddressEdit->toPlainText());
            profile->doctor->setPhone(ui->doctorPhoneEdit->text());
            profile->doctor->setEmail(ui->doctorEmailEdit->text());
            profile->doctor->setPatientID(ui->doctorPatientIDEdit->text());
            profile->user->setTimeZone(ui->timezoneCombo->itemData(
                                           ui->timezoneCombo->currentIndex()).toString());
            profile->user->setCountry(ui->countryCombo->currentText());
            profile->user->setDaylightSaving(ui->DSTcheckbox->isChecked());

            UnitSystem us = US_Metric;
            if (ui->heightCombo->currentIndex() == 1) { us = US_English; };
            if (profile->general->unitSystem() != us) {
                profile->general->setUnitSystem(us);
                if (mainwin && mainwin->getDaily()) { mainwin->getDaily()->UnitsChanged(); }
            }

            if (m_height_modified) {
                profile->user->setHeight(m_tmp_height_cm);
                // also call unitsChanged if height also changed. Need for update BMI.
                if (mainwin && mainwin->getDaily()) { mainwin->getDaily()->UnitsChanged(); }
            }
            AppSetting->setProfileName(profileName);

            profile->Save();
            if ( !originalProfileName.isEmpty() && !newProfileName.isEmpty() && (originalProfileName != newProfileName)) {
                QString originalProfileFullName = p_pref->Get("{home}/Profiles/") + originalProfileName;
                QString newProfileFullName = p_pref->Get("{home}/Profiles/") + newProfileName;
                QFile file(originalProfileFullName);
                if (file.exists()) {
                    bool status = file.rename(newProfileFullName);
                    if (status) {  // successful rename
                        Profiles::profiles[newProfileName] = p_profile;
                        AppSetting->setProfileName(newProfileName);
                        if (mainwin) mainwin->CloseProfile();
                        QCoreApplication::processEvents();
                        mainwin->RestartApplication(true,"-l");
                        QCoreApplication::processEvents();
                        exit(0);
                    } else {
                        staticQMessageBox::question(this,
                            tr("Duplicate or Invalid User Name"),
                            tr("Please Change User Name "),
                            QMessageBox::Ok);
                        index=1;
                        ui->stackedWidget->setCurrentIndex(index);
                        ui->userNameEdit->setText(newProfileName);
                    }
                } else {
                    qWarning() << "Rename Profile failed";
                }
            } else {
                if (mainwin) {
                    mainwin->GenerateStatistics();
                }
                this->accept();
            }
        }
    }

    if (index >= max_pages) {
        ui->nextButton->setText(tr("Finish"));
    } else {
        ui->nextButton->setText(tr("Next"));
    }

    ui->backButton->setEnabled(true);

}

void NewProfile::on_backButton_clicked()
{
    ui->nextButton->setText(tr("Next"));

    if (ui->stackedWidget->currentIndex() > m_firstPage) {
        ui->stackedWidget->setCurrentIndex(ui->stackedWidget->currentIndex() - 1);
    }

    if (ui->stackedWidget->currentIndex() == m_firstPage) {
        ui->backButton->setEnabled(false);
    } else {
        ui->backButton->setEnabled(true);
    }


}


void NewProfile::on_cpapModeCombo_activated(int index)
{
    if (index == 0) {
        ui->maxPressureEdit->setVisible(false);
    } else {
        ui->maxPressureEdit->setVisible(true);
    }
}

void NewProfile::on_agreeCheckbox_clicked(bool checked)
{
    ui->nextButton->setEnabled(checked);
}

void NewProfile::skipWelcomeScreen()
{
    ui->agreeCheckbox->setChecked(true);
    ui->stackedWidget->setCurrentIndex(m_firstPage = 1);
    ui->backButton->setEnabled(false);
    ui->nextButton->setEnabled(true);
}

void NewProfile::edit(const QString name)
{
    skipWelcomeScreen();
    Profile *profile = Profiles::Get(name);

    if (!profile) {
        profile = Profiles::Create(name);
    }

    ui->userNameEdit->setText(name);
    // ui->userNameEdit->setReadOnly(true);
    ui->firstNameEdit->setText(profile->user->firstName());
    ui->lastNameEdit->setText(profile->user->lastName());

    if (profile->contains(STR_UI_Password)
            && !profile->p_preferences[STR_UI_Password].toString().isEmpty()) {
        // leave the password box blank..
        QString a = "******";
        ui->passwordEdit1->setText(a);
        ui->passwordEdit2->setText(a);
        ui->passwordGroupBox->setChecked(true);
        m_passwordHashed = true;
    }

    ui->dobEdit->setDate(profile->user->DOB());

    if (profile->user->gender() == Male) {
        ui->genderCombo->setCurrentIndex(1);
    } else if (profile->user->gender() == Female) {
        ui->genderCombo->setCurrentIndex(2);
    } else { ui->genderCombo->setCurrentIndex(0); }

    ui->heightEdit->setValue(profile->user->height());
    ui->addressEdit->setText(profile->user->address());
    ui->emailEdit->setText(profile->user->email());
    ui->phoneEdit->setText(profile->user->phone());
    ui->dateDiagnosedEdit->setDate(profile->cpap->dateDiagnosed());
    ui->cpapNotes->clear();
    ui->cpapNotes->appendPlainText(profile->cpap->notes());
    ui->minPressureEdit->setValue(profile->cpap->minPressure());
    ui->maxPressureEdit->setValue(profile->cpap->maxPressure());
    ui->untreatedAHIEdit->setValue(profile->cpap->untreatedAHI());
    ui->cpapModeCombo->setCurrentIndex((int)profile->cpap->mode());

    on_cpapModeCombo_activated(profile->cpap->mode());

    ui->doctorNameEdit->setText(profile->doctor->name());
    ui->doctorPracticeEdit->setText(profile->doctor->practiceName());
    ui->doctorPhoneEdit->setText(profile->doctor->phone());
    ui->doctorEmailEdit->setText(profile->doctor->email());
    ui->doctorAddressEdit->setText(profile->doctor->address());
    ui->doctorPatientIDEdit->setText(profile->doctor->patientID());

    ui->DSTcheckbox->setChecked(profile->user->daylightSaving());
    int i = ui->timezoneCombo->findData(profile->user->timeZone());
    ui->timezoneCombo->setCurrentIndex(i);
    i = ui->countryCombo->findText(profile->user->country());
    ui->countryCombo->setCurrentIndex(i);

    UnitSystem us = profile->general->unitSystem();
    i = (int)us - 1;

    if (i < 0) { i = 0; }

    ui->heightCombo->setCurrentIndex(i);

    m_tmp_height_cm = profile->user->height();
    m_height_modified = false;
    on_heightCombo_currentIndexChanged(i);
}

void NewProfile::on_passwordEdit1_editingFinished()
{
    m_passwordHashed = false;
}

void NewProfile::on_passwordEdit2_editingFinished()
{
    m_passwordHashed = false;
}

void NewProfile::on_heightCombo_currentIndexChanged(int index)
{
    ui->heightEdit->blockSignals(true);
    ui->heightEdit2->blockSignals(true);
    if (index == 0) {
        //metric
        ui->heightEdit->setDecimals(1);
        ui->heightEdit->setSuffix(QString(" %1").arg(STR_UNIT_CM));
        ui->heightEdit->setValue(m_tmp_height_cm);
        ui->heightEdit2->setVisible(false);
    } else {        //english
        ui->heightEdit->setDecimals(0);  // feet are always a whole number.
        ui->heightEdit2->setDecimals(1);  // inches can be seen as double.
        ui->heightEdit->setSuffix(QString(" %1").arg(STR_UNIT_FOOT));
        ui->heightEdit2->setVisible(true);
        ui->heightEdit2->setSuffix(QString(" %1").arg(STR_UNIT_INCH));
        double inches=0;
        ui->heightEdit->setValue(cmToFeetInch(m_tmp_height_cm,inches));
        ui->heightEdit2->setValue(inches);
    }
    ui->heightEdit->blockSignals(false);
    ui->heightEdit2->blockSignals(false);
}

void NewProfile::on_heightEdit_valueChanged(double ) {
    m_height_modified = true;
    double cm = ui->heightEdit->value();
    if (ui->heightCombo->currentIndex() == 1) {
        //US_English;
        cm = feetInchToCm (cm,ui->heightEdit2->value());
    };
    m_tmp_height_cm = cm;
}

void NewProfile::on_heightEdit2_valueChanged(double value) {
    on_heightEdit_valueChanged(value);
}

void NewProfile::on_textBrowser_anchorClicked(const QUrl &arg1)
{
    QDialog *dlg = new QDialog(this);
    dlg->setMinimumWidth(600);
    dlg->setMinimumHeight(500);
    QVBoxLayout *layout = new QVBoxLayout();
    dlg->setLayout(layout);
    QTextBrowser *browser = new QTextBrowser(this);
    dlg->layout()->addWidget(browser);
    QPushButton *button = new QPushButton(tr("Close this window"), browser);

    QFile f(arg1.toString().replace("qrc:", ":"));
    openOk = f.open(QIODevice::ReadOnly);
    QTextStream ts(&f);
    QString text = ts.readAll();
    connect(button, SIGNAL(clicked()), dlg, SLOT(close()));
    dlg->layout()->addWidget(button);
    browser->setPlainText(text);
    dlg->exec();

    delete dlg;
}
