/* PRS1 Unit Tests
 *
 * Copyright (c) 2019-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "prs1tests.h"
#include "sessiontests.h"

#include "../SleepLib/loader_plugins/prs1_loader.h"
#include "../SleepLib/loader_plugins/prs1_parser.h"
#include "../SleepLib/importcontext.h"

#define TESTDATA_PATH "./testdata/"
//#define TEST_OSCAR_CALCS

#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
    #define QTHEX     Qt::hex
    #define QTDEC     Qt::dec
    #define QT_SKIP_EMPTY_PARTS Qt::SkipEmptyParts
#else
    #define QTHEX     hex
    #define QTDEC     dec
    #define QT_SKIP_EMPTY_PARTS QString::SkipEmptyParts
#endif

static PRS1Loader* s_loader = nullptr;
static void iterateTestCards(const QString & root, void (*action)(const QString &));
static QString prs1OutputPath(const QString & inpath, const QString & serial, const QString & basename, const QString & suffix);
static QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix);



void PRS1Tests::initTestCase(void)
{
    p_profile = new Profile(TESTDATA_PATH "profile/", false);

#ifdef TEST_OSCAR_CALCS
    p_pref = new Preferences("Preferences");
    p_pref->Open();
    AppSetting = new AppWideSetting(p_pref);
#endif

    schema::init();
    PRS1Loader::Register();
    s_loader = dynamic_cast<PRS1Loader*>(lookupLoader(prs1_class_name));
}

void PRS1Tests::cleanupTestCase(void)
{
    delete p_profile;
    p_profile = nullptr;
#ifdef TEST_OSCAR_CALCS
    delete p_pref;
#endif
}


// ====================================================================================================

extern PRS1ModelInfo s_PRS1ModelInfo;
void PRS1Tests::testMachineSupport()
{
    QHash<QString,QString> tested = {
        { "ModelNumber", "550P" },
        { "Family", "0" },
        { "FamilyVersion", "3" },
    };
    QHash<QString,QString> supported = {
        { "ModelNumber", "700X999" },
        { "Family", "0" },
        { "FamilyVersion", "6" },
    };
    QHash<QString,QString> unsupported = {
        { "ModelNumber", "550P" },
        { "Family", "0" },
        { "FamilyVersion", "9" },
    };
    
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(5, 3));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(5, 9));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(9, 9));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("550P", 0, 2));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("550P", 0, 3));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("760P", 0, 4));
    Q_ASSERT(s_PRS1ModelInfo.IsTested("700X110", 0, 6));
    Q_ASSERT(!s_PRS1ModelInfo.IsTested("700X999", 0, 6));
    
    Q_ASSERT(s_PRS1ModelInfo.IsTested(tested));
    Q_ASSERT(!s_PRS1ModelInfo.IsTested(supported));
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(tested));
    Q_ASSERT(s_PRS1ModelInfo.IsSupported(supported));
    Q_ASSERT(!s_PRS1ModelInfo.IsSupported(unsupported));
}


// ====================================================================================================


void parseAndEmitSessionYaml(const QString & path)
{
    qDebug() << path;

    ImportContext* ctx = new ProfileImportContext(p_profile);
    s_loader->SetContext(ctx);

    ctx->connect(ctx, &ImportContext::importEncounteredUnexpectedData, [=]() {
        qWarning() << "*** Found unexpected data";
    });

    // This mirrors the functional bits of PRS1Loader::OpenMachine.
    // TODO: Refactor PRS1Loader so that the tests can use the same
    // underlying logic as OpenMachine rather than duplicating it here.

    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    bool supported = s_loader->CreateMachineFromProperties(propertyfile);
    if (!supported) {
        qWarning() << "*** Skipping unsupported machine!";
        return;
    }
    Machine* m = ctx->m_machine;

    s_loader->ScanFiles(paths, sessionid_base);
    
    // Each session now has a PRS1Import object in m_MLtasklist
    QList<ImportTask*>::iterator i;
    while (!s_loader->m_MLtasklist.isEmpty()) {
        ImportTask* task = s_loader->m_MLtasklist.takeFirst();

        // Run the parser
        PRS1Import* import = dynamic_cast<PRS1Import*>(task);
        bool ok = import->ParseSession();
        
        // Emit the parsed session data to compare against our regression benchmarks
        Session* session = import->session;
        QString outpath = prs1OutputPath(path, m->serial(), session->session(), "-session.yml");
#ifdef TEST_OSCAR_CALCS
        session->s_events_loaded = true;
        session->UpdateSummaries();
#endif
        SessionToYaml(outpath, session, ok);
        
        delete session;
        delete task;
    }

    delete ctx;
}

void PRS1Tests::testSessionsToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitSessionYaml);
}


// ====================================================================================================

static QString dur(qint64 msecs)
{
    qint64 s = msecs / 1000L;
    int h = s / 3600; s -= h * 3600;
    int m = s / 60; s -= m * 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

static QString byteList(QByteArray data, int limit=-1)
{
    int count = data.size();
    if (limit == -1 || limit > count) limit = count;
    int first = limit / 2;
    int last = limit - first;
#if 1
    // Optimized after profiling regression tests.
    QString s;
    s.reserve(3 * limit + 4);  // "NN " for each byte + possible "... " in the middle
    const unsigned char* b = (const unsigned char*) data.constData();
    for (int i = 0; i < first; i++) {
        s.append(QString::asprintf("%02X ", b[i]));
    }
    if (limit < count) {
        s.append(QStringLiteral("... "));
    }
    for (int i = count - last; i < count; i++) {
        s.append(QString::asprintf("%02X ", b[i]));
    }
    s.resize(s.size() - 1);  // remove trailing space
#else
    // Unoptimized original, slows down regression tests.
    QStringList l;
    for (int i = 0; i < first; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    if (limit < count) l.push_back("...");
    for (int i = count - last; i < count; i++) {
        l.push_back(QString( "%1" ).arg((int) data[i] & 0xFF, 2, 16, QChar('0') ).toUpper());
    }
    QString s = l.join(" ");
#endif
    return s;
}

void ChunkToYaml(QTextStream & out, PRS1DataChunk* chunk, bool ok)
{
    // chunk header
    out << "chunk:" << '\n';
    out << "  at: " << QTHEX << chunk->m_filepos << '\n';
    out << "  parsed: " << ok << '\n';
    out << "  version: " << QTDEC << chunk->fileVersion << '\n';
    out << "  size: " << chunk->blockSize << '\n';
    out << "  htype: " << chunk->htype << '\n';
    out << "  family: " << chunk->family << '\n';
    out << "  familyVersion: " << chunk->familyVersion << '\n';
    out << "  ext: " << chunk->ext << '\n';
    out << "  session: " << chunk->sessionid << '\n';
    out << "  start: " << ts(chunk->timestamp * 1000L) << '\n';
    out << "  duration: " << dur(chunk->duration * 1000L) << '\n';

    // hblock for V3 non-waveform chunks
    if (chunk->fileVersion == 3 && chunk->htype == 0) {
        out << "  hblock:" << '\n';
        QMapIterator<unsigned char, short> i(chunk->hblock);
        while (i.hasNext()) {
            i.next();
            out << "    " << (int) i.key() << ": " << i.value() << '\n';
        }
    }

    // waveform chunks
    if (chunk->htype == 1) {
        out << "  intervals: " << chunk->interval_count << '\n';
        out << "  intervalSeconds: " << (int) chunk->interval_seconds << '\n';
        out << "  interleave:" << '\n';
        for (int i=0; i < chunk->waveformInfo.size(); i++) {
            const PRS1Waveform & w = chunk->waveformInfo.at(i);
            out << "    " << i << ": " << w.interleave << '\n';
        }
        out << "  end: " << ts((chunk->timestamp + chunk->duration) * 1000L) << '\n';
    }
    
    // header checksum
    out << "  checksum: " << QTHEX << chunk->storedChecksum << '\n';
    if (chunk->storedChecksum != chunk->calcChecksum) {
        out << "  calcChecksum: " << QTHEX << chunk->calcChecksum << '\n';
    }
    
    // data
    bool dump_data = true;
    if (chunk->m_parsedData.size() > 0) {
        dump_data = false;
        out << "  events:" << '\n';
        for (auto & e : chunk->m_parsedData) {
            QString name = e->typeName();
            if (name == "raw" || name == "unknown") {
                dump_data = true;
            }
            QMap<QString,QString> contents = e->contents();
            if (name == "setting" && contents.size() == 1) {
                out << "  - set_" << contents.firstKey() << ": " << contents.first() << '\n';
            }
            else {
                out << "  - " << name << ":" << '\n';
                
                // Always emit start first if present
                if (contents.contains("start")) {
                    out << "      " << "start" << ": " << contents["start"] << '\n';
                }
                for (auto & key : contents.keys()) {
                    if (key == "start") continue;
                    out << "      " << key << ": " << contents[key] << '\n';
                }
            }
        }
    }
    if (dump_data || !ok) {
        out << "  data: " << byteList(chunk->m_data, 100) << '\n';
    }
    
    // data CRC
    out << "  crc: " << QTHEX << chunk->storedCrc << '\n';
    if (chunk->storedCrc != chunk->calcCrc) {
        out << "  calcCrc: " << QTHEX << chunk->calcCrc << '\n';
    }
    out << '\n';
}

void parseAndEmitChunkYaml(const QString & path)
{
    bool FV = false;  // set this to true to emit family/familyVersion for this path
    qDebug() << path;

    ImportContext* ctx = new ProfileImportContext(p_profile);
    s_loader->SetContext(ctx);

    QHash<QString,QSet<quint64>> written;
    QStringList paths;
    QString propertyfile;
    int sessionid_base;
    sessionid_base = s_loader->FindSessionDirsAndProperties(path, paths, propertyfile);

    bool supported = s_loader->CreateMachineFromProperties(propertyfile);
    if (!supported) {
        qWarning() << "*** Skipping unsupported machine!";
        return;
    }
    Machine* m = ctx->m_machine;

    // This mirrors the functional bits of PRS1Loader::ScanFiles.
    
    QDir dir;
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    int size = paths.size();

    // for each p0/p1/p2/etc... folder
    for (int p=0; p < size; ++p) {
        dir.setPath(paths.at(p));
        if (!dir.exists() || !dir.isReadable()) {
            qWarning() << dir.canonicalPath() << "can't read directory";
            continue;
        }
        QFileInfoList flist = dir.entryInfoList();

        // Scan for individual .00X files
        for (int i = 0; i < flist.size(); i++) {
            QFileInfo fi = flist.at(i);
            QString inpath = fi.canonicalFilePath();
            bool ok;
            
            if (fi.fileName() == ".DS_Store") {
                continue;
            }

            QString ext_s = fi.fileName().section(".", -1);
            if (ext_s.toUpper().startsWith("B")) {  // .B01, .B02, etc.
                ext_s = ext_s.mid(1);
            }
            int ext = ext_s.toInt(&ok);
            if (!ok) {
                // not a numerical extension
                qInfo() << inpath << "unexpected filename";
                continue;
            }

            QString session_s = fi.fileName().section(".", 0, -2);
            int sessionid = session_s.toInt(&ok, sessionid_base);
            if (!ok) {
                // not a numerical session ID
                qInfo() << inpath << "unexpected filename";
                continue;
            }
            
            // Create the YAML file.
            QString suffix = QString(".%1-chunks.yml").arg(ext, 3, 10, QChar('0'));
            QString outpath = prs1OutputPath(path, m->serial(), sessionid, suffix);
            QFile file(outpath);
            // Truncate the first time we open this file, to clear out any previous test data.
            // Otherwise append, allowing session chunks to be split among multiple files.
            if (!file.open(QFile::WriteOnly | (written.contains(outpath) ? QFile::Append : QFile::Truncate))) {
                qDebug() << outpath;
                Q_ASSERT(false);
            }
            QTextStream out(&file);

            // keep only P1234568/Pn/00000000.001
            QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QT_SKIP_EMPTY_PARTS);
            QString relative = pathlist.mid(pathlist.size()-3).join(QDir::separator());
            bool first_chunk_from_file = true;

            // Parse the chunks in the file.
            QList<PRS1DataChunk *> chunks = s_loader->ParseFile(inpath);
            for (int i=0; i < chunks.size(); i++) {
                PRS1DataChunk * chunk = chunks.at(i);
                if (i == 0 && FV) { qWarning() << QString("F%1V%2").arg(chunk->family).arg(chunk->familyVersion); FV=false; }
                // Only write unique chunks to the file.
                if (written[outpath].contains(chunk->hash()) == false) {
                    if (first_chunk_from_file) {
                        out << "file: " << relative << '\n';
                        first_chunk_from_file = false;
                    }
                    bool ok = true;
                    
                    // Parse the inner data.
                    switch (chunk->ext) {
                        case 0: ok = chunk->ParseCompliance(); break;
                        case 1: ok = chunk->ParseSummary(); break;
                        case 2: ok = chunk->ParseEvents(); break;
                        case 5: break;  // skip flow/pressure waveforms
                        case 6: break;  // skip oximetry data
                        default:
                            qWarning() << relative << "unexpected file type";
                            break;
                    }
                    
                    // Emit the YAML.
                    ChunkToYaml(out, chunk, ok);
                    written[outpath] += chunk->hash();
                }
                delete chunk;
            }
            
            file.close();
        }
    }
    
    delete ctx;
    
    p_profile->removeMachine(m);
    delete m;
}

void PRS1Tests::testChunksToYaml()
{
    iterateTestCards(TESTDATA_PATH "prs1/input/", parseAndEmitChunkYaml);
}


// ====================================================================================================

QString prs1OutputPath(const QString & inpath, const QString & serial, int session, const QString & suffix)
{
    QString basename = QString("%1").arg(session, 8, 10, QChar('0'));
    return prs1OutputPath(inpath, serial, basename, suffix);
}

QString prs1OutputPath(const QString & inpath, const QString & serial, const QString & basename, const QString & suffix)
{
    // Output to prs1/output/FOLDER/SERIAL-000000(-session.yml, etc.)
    QDir path(inpath);
    QStringList pathlist = QDir::toNativeSeparators(inpath).split(QDir::separator(), QT_SKIP_EMPTY_PARTS);
    pathlist.pop_back();  // drop serial number directory
    pathlist.pop_back();  // drop P-Series directory
    QString foldername = pathlist.last();

    QDir outdir(TESTDATA_PATH "prs1/output/" + foldername);
    outdir.mkpath(".");
    
    QString filename = QString("%1-%2%3")
                        .arg(serial)
                        .arg(basename)
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
        QStringList machinePaths = s_loader->FindMachinesOnCard(fi.canonicalFilePath());

        // Tests should be run newest to oldest, since older sets tend to have more
        // complete data. (These are usually previously cleared data in the Clear0/Cn
        // directories.) The machines themselves will write out the summary data they
        // remember when they see an empty folder, without event or waveform data.
        // And since these tests (by design) overwrite existing output, we want the
        // earlier (more complete) data to be what's written last.
        //
        // Since the loader itself keeps only the first set of data it sees for a session,
        // we want to leave its earliest-to-latest ordering in place, and just reverse it
        // here.
        for (auto i = machinePaths.crbegin(); i != machinePaths.crend(); i++) {
            action(*i);
        }
    }
}
