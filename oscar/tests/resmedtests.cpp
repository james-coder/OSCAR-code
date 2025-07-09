/* ResMed Unit Tests
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "resmedtests.h"
#include "sessiontests.h"
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    #define QTHEX     Qt::hex
    #define QTDEC     Qt::dec
    #define QT_SKIP_EMPTY_PARTS Qt::SkipEmptyParts
#else
    #define QTHEX     hex
    #define QTDEC     dec
    #define QT_SKIP_EMPTY_PARTS QString::SkipEmptyParts
#endif


#define TESTDATA_PATH "./testdata/"

static ResmedLoader* s_loader = nullptr;
static void iterateTestCards(const QString & root, void (*action)(const QString &));
static QString resmedOutputPath(const QString & inpath, int session, const QString & suffix);

void ResmedTests::initTestCase(void)
{
    p_profile = new Profile(TESTDATA_PATH "profile/", false);
    p_profile->session->setBackupCardData(false);

    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);

    schema::init();
    ResmedLoader::Register();
    s_loader = dynamic_cast<ResmedLoader*>(lookupLoader(resmed_class_name));
}

void ResmedTests::cleanupTestCase(void)
{
    delete AppSetting;
    delete p_profile;
    p_profile = nullptr;
    delete p_pref;
}


// ====================================================================================================

static QString s_currentPath;

static void emitSessionYaml(ResmedLoader* /*loader*/, Session* session)
{
    // Emit the parsed session data to compare against our regression benchmarks
    QString outpath = resmedOutputPath(s_currentPath, session->session(), "-session.yml");
    SessionToYaml(outpath, session, true);
}

static void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    // TODO: Refactor Resmed so that passing callbacks and using static globals isn't
    // necessary for testing. Both are used for now in order to introduce the minimal
    // set of changes into the Resmed loader needed for testing.
    s_currentPath = path;
    s_loader->OpenWithCallback(path, emitSessionYaml);
}

void ResmedTests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "resmed/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

QString resmedOutputPath(const QString & inpath, int session, const QString & suffix)
{
    // Output to resmed/output/FOLDER/000000(-session.yml, etc.)
    QDir path(inpath);
    QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QT_SKIP_EMPTY_PARTS);
    QString foldername = pathlist.last();

    QDir outdir(TESTDATA_PATH "resmed/output/" + foldername);
    outdir.mkpath(".");
    
    QString filename = QString("%1%2")
                        .arg(session)
                        .arg(suffix);
    return outdir.path() + QDir::separator() + filename;
}

void iterateTestCards(const QString & root, void (*action)(const QString &))
{
    QDir dir(root);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    // Look through each folder in the given root
    for (auto & fi : flist) {
        // If it contains a DATALOG folder, it's a ResMed SD card
        QDir datalog(fi.canonicalFilePath() + QDir::separator() + "DATALOG");
        if (datalog.exists()) {
            // Confirm that it contains the file that the ResMed loader expects
            QFileInfo idfile(fi.canonicalFilePath() + QDir::separator() + "Identification.tgt");
            if (idfile.exists()) {
                action(fi.canonicalFilePath());
            }
        }
    }
}
