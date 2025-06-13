/* Multilingual Support files
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QApplication>
#include <QDebug>
#include <QStringList>
#include <QList>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QSettings>
#include <QTranslator>
#include <QListWidget>
#include <QLibraryInfo>

#include "SleepLib/common.h"
#include "translation.h"

QHash<QString, QString> langNames;

QString currentLanguage()
{
    QSettings settings;
    return settings.value(LangSetting).toString();
}
QString lookupLanguageName(QString language)
{
    auto it = langNames.find(language);
    if (it != langNames.end()) {
        return it.value();
    }
    return language;
}

void initTranslations()
{
    // Add any languages with need for a special character set to this list
    langNames["af"] = "Afrikaans";
    langNames["ar"] = "\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a (Arabic)";
    langNames["bg"] = "\xd0\xb1\xd1\x8a\xd0\xbb\xd0\xb3\xd0\xb0\xd1\x80\xd1\x81\xd0\xba\xd0\xb8 (Bulgarian)";
    langNames["da"] = "Dansk";
    langNames["de"] = "Deutsch";
    langNames["el"] = "\xce\x95\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac (Greek)";
    langNames["en_UK"] = "English (UK)";
    langNames["es"] = "Español";
    langNames["es_MX"] = "Español (Mexico)";
    langNames["fi"] = "Suomen kieli";
    langNames["fil"] = "Filipino";
    langNames["fr"] = "Français";
    langNames["he"] = "\xd7\xa2\xd7\x91\xd7\xa8\xd7\x99\xd7\xaa (Hebrew)";
    langNames["hu"] = "Magyar";
    langNames["it"] = "Italiano";
    langNames["ja"] = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e (Japanese)";
    langNames["ko"] = "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4 (Korean)";
    langNames["nl"] = "Nederlands";
    langNames["no"] = "Norsk";
    langNames["pl"] = "Polski";
    langNames["pt"] = "Português";
    langNames["pt_BR"] = "Português (Brazil)";
    langNames["ro"] = "Românește";
    langNames["ru"] = "\xd1\x80\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9 (Russian)";
    langNames["th"] = "\xe0\xb8\xa0\xe0\xb8\xb2\xe0\xb8\xa9\xe0\xb8\xb2\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2 (Thai)";
    langNames["tr"] = "Türkçe";
    langNames["zh_CN"] = "\xe5\x8d\x8e\xe8\xaf\xad\xe7\xae\x80\xe4\xbd\x93\xe5\xad\x97 \x2d \xe4\xb8\xad\xe5\x9b\xbd (Chinese Simpl)";
    langNames["zh_TW"] = "\xe8\x8f\xaf\xe8\xaa\x9e\xe6\xad\xa3\xe9\xab\x94\xe5\xad\x97 \x2d \xe8\x87\xba\xe7\x81\xa3 (Chinese Trad)";

    langNames[DefaultLanguage]="English (US)";

    QHash<QString, QString> langFiles;
    QHash<QString, QString> langPaths;

    langFiles[DefaultLanguage] = "English (US).en_US.qm";

    QSettings settings;
    QString language = settings.value(LangSetting).toString();


    QString inbuiltPath = ":/translations";
    QStringList inbuilt(DefaultLanguage);

    QDir dir(inbuiltPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.qm"));

    qDebug() << "number of built-in *.qm files" << dir.count();
    QFileInfoList list = dir.entryInfoList();
    for (const auto & fi : list) {
        if (fi.fileName().indexOf("oscar_qt", 0) == 0)     // skip files named QT...  These are supplemental files for QT strings
            continue;
        QString code = fi.fileName().section('.', 1, 1);

        if (!langNames.contains(code)) langNames[code]=fi.fileName().section('.', 0, 0);

        inbuilt.push_back(code);

        langFiles[code] = fi.fileName();
        langPaths[code] = inbuiltPath;
    }
    std::sort(inbuilt.begin(), inbuilt.end());
    qDebug() << "Inbuilt Translations:" << QString(inbuilt.join(", ")).toLocal8Bit().data();

    QString externalPath = appResourcePath() +"/Translations";
    dir.setPath(externalPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.qm"));
    list = dir.entryInfoList();
    qDebug() << "number of external *.qm files" << dir.count();

    // Scan through available translations, and add them to the list
    QStringList extratrans, replaced;
    int numExternal = 0;
    for (const auto & fi : list) {
        if (fi.fileName().indexOf("oscar_qt", 0) == 0)     // skip files named QT...  These are supplemental files for QT strings
            continue;
        QString code = fi.fileName().section('.', 1, 1);

        numExternal++;
        if(!langNames.contains(code)) langNames[code] = fi.fileName().section('.', 0, 0);
        if (inbuilt.contains(code)) replaced.push_back(code); else extratrans.push_back(code);

        langFiles[code] = fi.fileName();
        langPaths[code] = externalPath;
    }
    std::sort(replaced.begin(), replaced.end());
    std::sort(extratrans.begin(), extratrans.end());
    qDebug() << "Number of external translations is" << numExternal;
    if (replaced.size()>0) qDebug() << "Overridden Tranlsations:" << QString(replaced.join(", ")).toLocal8Bit().data();
    if (extratrans.size()>0) qDebug() << "Extra Translations:" << QString(extratrans.join(", ")).toLocal8Bit().data();

    if (language.isEmpty() || !langNames.contains(language)) {
        QDialog langsel(nullptr, Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        QFont font;
        font.setPointSize(12);
        langsel.setFont(font);
        langsel.setWindowTitle("Language / Taal / Sprache / Langue / \xe8\xaf\xad\xe8\xa8\x80 / ... ");
        QHBoxLayout lang_layout(&langsel);

        QLabel img;
        img.setPixmap(QPixmap(":/icons/logo-lg.png"));

        QPushButton lang_okbtn("->", &langsel); // hard coded non translatable

        QVBoxLayout layout1;
        QVBoxLayout layout2;

        layout2.setContentsMargins(6, 6, 6, 6);
        lang_layout.setContentsMargins(6, 6, 6, 6);
        layout2.setSpacing(6);
        QListWidget langlist;

        lang_layout.addLayout(&layout1);
        lang_layout.addLayout(&layout2);

        layout1.addWidget(&img);
        layout2.addWidget(&langlist, 1);

        langlist.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        int row = 0;
        for (QHash<QString, QString>::iterator it = langNames.begin(); it != langNames.end(); ++it) {
            const QString & code = it.key();
            const QString & name = it.value();
            if (!langFiles.contains(code) || langFiles[code].isEmpty())
                continue;

            QListWidgetItem *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, code);
            langlist.insertItem(row++, item);
            // Todo: Use base system language code if available.
            if (code.compare(DefaultLanguage) == 0) {
                langlist.setCurrentItem(item);
            }
        }
        langlist.sortItems();
        layout2.addWidget(&lang_okbtn);

        langsel.connect(&langlist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), &langsel, SLOT(close()));
        langsel.connect(&lang_okbtn, SIGNAL(clicked()), &langsel, SLOT(close()));

        langsel.raise();

        langsel.exec();

        langsel.disconnect(&lang_okbtn, SIGNAL(clicked()), &langsel, SLOT(close()));
        langsel.disconnect(&langlist, SIGNAL(itemDoubleClicked(QListWidgetItem*)), &langsel, SLOT(close()));
        language = langlist.currentItem()->data(Qt::UserRole).toString();
        settings.setValue(LangSetting, language);
    }

    QString langname=langNames[language];
    QString langfile=langFiles[language];
    QString langpath=langPaths[language];

    if (language.compare(DefaultLanguage) != 0) {
        // Install QT translation files
        QString qtLang = language.left(2);
        if ( qtLang.compare("zh") == 0 )      // QT-supplied translation files have both _CN and _TW, but are the same for our purposes
            qtLang.append("_CN");
        QString qtLangFile = "qt_" + qtLang + ".qm";
        if (!QFileInfo(qtLangFile).exists()) {
            qtLang = qtLang.left(2);        // Undo QT suffix for zh; we don't use that for our file
            qtLangFile = "oscar_qt_" + qtLang + ".qm";
        }
        qDebug() << "Loading" << langname << "QT translation" << qtLangFile.toLocal8Bit().data() << "from" << langpath.toLocal8Bit().data();
        QTranslator * qtranslator = new QTranslator();

        if (!qtLangFile.isEmpty() && !qtranslator->load(qtLangFile, langpath)) {
             qWarning() << "Could not load QT translation" << qtLangFile << "reverting to english :(";
        }

        qApp->installTranslator(qtranslator);

        // Install OSCAR translation files
        qDebug() << "Loading" << langname << "OSCAR translation" << langfile.toLocal8Bit().data() << "from" << langpath.toLocal8Bit().data();
        QTranslator * translator = new QTranslator();

        if (!langfile.isEmpty() && !translator->load(langfile, langpath)) {
            qWarning() << "Could not load OSCAR translation" << langfile << "reverting to english :(";
        }

        qApp->installTranslator(translator);
    } else {
        qDebug() << "Using default language" << language.toLocal8Bit().data();
    }
}
