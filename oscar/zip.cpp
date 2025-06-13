/* OSCAR ZIP archive creation
 * Provides a Qt-convenient wrapper around miniz, see https://github.com/richgel999/miniz
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "zip.h"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include "SleepLib/progressdialog.h"

static const quint64 PROGRESS_SCALE = 1024;  // QProgressBar only holds an int, so report progress in KiB.

// Static functions to abstract the details of miniz from the primary logic.
static void* zip_init();
static bool zip_open(void* ctx, QFile & file);
static bool zip_add(void* ctx, const QString & archive_name, const QByteArray & data, const QDateTime & modified);
static void zip_close(void* ctx);
static void zip_done(void* ctx);


ZipFile::ZipFile()
{
    m_ctx = zip_init();
}

ZipFile::~ZipFile()
{
    Close();
    zip_done(m_ctx);
}

bool ZipFile::Open(const QString & filepath)
{
    m_file.setFileName(filepath);
    bool ok = m_file.open(QIODevice::WriteOnly);
    if (!ok) {
        qWarning() << "Could not open" << m_file.fileName() << "for writing, error code" << m_file.error() << m_file.errorString();
//        qWarning() << "unable to open" << m_file.fileName();
        return false;
    }
    ok = zip_open(m_ctx, m_file);
    return ok;
}

void ZipFile::Close()
{
    if (m_file.isOpen()) {
        zip_close(m_ctx);
        m_file.close();
    }
}

bool ZipFile::AddDirectory(const QString & path, ProgressDialog* progress)
{
    return AddDirectory(path, "", progress);
}

bool ZipFile::AddDirectory(const QString & path, const QString & prefix, ProgressDialog* progress)
{
    bool ok;
    FileQueue queue;
    queue.AddDirectory(path, prefix);
    ok = AddFiles(queue, progress);
    return ok;
}

bool ZipFile::AddFiles(FileQueue & queue, ProgressDialog* progress)
{
    bool ok;
    
    // Exclude the zip file that's being created (if it happens to be in the list).
    queue.Remove(QFileInfo(m_file).canonicalFilePath());

    qDebug().noquote() << "Adding" << queue.toString();
    m_abort = false;
    m_progress = 0;

    if (progress) {
        progress->addAbortButton();
        progress->setWindowModality(Qt::ApplicationModal);
        progress->open();
        connect(this, SIGNAL(setProgressMax(int)), progress, SLOT(setProgressMax(int)));
        connect(this, SIGNAL(setProgressValue(int)), progress, SLOT(setProgressValue(int)));
        connect(progress, SIGNAL(abortClicked()), this, SLOT(abort()));
    }

    // Always emit, since the caller may have configured and connected a progress dialog manually.
    emit setProgressValue(m_progress/PROGRESS_SCALE);
    emit setProgressMax((queue.byteCount() + queue.dirCount())/PROGRESS_SCALE);
    QCoreApplication::processEvents();

    for (auto & entry : queue.files()) {
        ok = AddFile(entry.path, entry.name);
        if (!ok || m_abort) {
            break;
        }
    }
    
    if (progress) {
        disconnect(progress, SIGNAL(abortClicked()), this, SLOT(abort()));
        disconnect(this, SIGNAL(setProgressMax(int)), progress, SLOT(setProgressMax(int)));
        disconnect(this, SIGNAL(setProgressValue(int)), progress, SLOT(setProgressValue(int)));
        progress->close();
        progress->deleteLater();
    }

    if (!ok) {
        qWarning().noquote() << "Unable to create" << m_file.fileName();
        Close();
        m_file.remove();
    } else if (aborted()) {
        qDebug().noquote() << "User canceled zip creation.";
        Close();
        m_file.remove();
    } else {
        qDebug().noquote() << "Created" << m_file.fileName() << m_file.size() << "bytes";
    }

    return ok;
}

bool ZipFile::AddFile(const QString & path, const QString & name)
{
    if (!m_file.isOpen()) {
        qWarning() << m_file.fileName() << "has not been opened for writing";
        return false;
    }

    QFileInfo fi(path);
    QByteArray data;
    QString archive_name = name;
    if (archive_name.isEmpty()) archive_name = fi.fileName();

    if (fi.isDir()) {
        archive_name += "/";
        m_progress += 1;
    } else {
        // Open and read file into memory.
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << path << "can't open";
            return false;
        }
        data = f.readAll();
        m_progress += data.size();
    }

    //qDebug() << "attempting to add" << archive_name << ":" << data.size() << "bytes";

    bool ok = zip_add(m_ctx, archive_name, data, fi.lastModified());

    emit setProgressValue(m_progress/PROGRESS_SCALE);
    QCoreApplication::processEvents();
    
    return ok;
}


// ==================================================================================================

bool FileQueue::AddDirectory(const QString & path, const QString & prefix)
{
    QDir dir(path);
    if (!dir.exists() || !dir.isReadable()) {
        qWarning() << dir.canonicalPath() << "can't read directory";
#if defined(Q_OS_MACOS)
        // If this is a directory known to be protected by macOS "Full Disk Access" permissions,
        // skip it but don't consider it an error.
        static const QSet<QString> s_macProtectedDirs = { ".fseventsd", ".Spotlight-V100", ".Trashes" };
        if (s_macProtectedDirs.contains(dir.dirName())) {
            return true;
        }
#endif
        return false;
    }
    QString base = prefix;
    if (base.isEmpty()) base = dir.dirName();

    // Add directory entry
    bool ok = AddFile(dir.canonicalPath(), base);
    if (!ok) {
        return false;
    }

    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();

    for (auto & fi : flist) {
        QString canonicalPath = fi.canonicalFilePath();
        QString relative_path = base + "/" + fi.fileName();
        if (fi.isSymLink()) {
            qWarning() << "skipping symlink" << canonicalPath << fi.symLinkTarget();
        } else if (fi.isDir()) {
            // Descend and recurse
            ok &= AddDirectory(canonicalPath, relative_path);
        } else {
            // Add the file to the zip
            ok &= AddFile(canonicalPath, relative_path);
        }
        // Don't stop in our tracks when we hit an error.
    }

    return ok;
}

bool FileQueue::AddFile(const QString & path, const QString & prefix)
{
    QFileInfo fi(path);
    QString canonicalPath = fi.canonicalFilePath();
    QString archive_name = prefix;

    if (archive_name.isEmpty()) archive_name = fi.fileName();

    if (fi.isDir()) {
        m_dir_count++;
    } else if (fi.exists()) {
        m_file_count++;
        m_byte_count += fi.size();
    } else {
        qWarning() << "file doesn't exist" << canonicalPath;
        return false;
    }
    Entry entry = { canonicalPath, archive_name };
    m_files.append(entry);
    QCoreApplication::processEvents();
    return true;
}

int FileQueue::Remove(const QString & path, QString* outName)
{
    QFileInfo fi(path);
    QString canonicalPath = fi.canonicalFilePath();
    int removed = 0;

    QMutableListIterator<Entry> i(m_files);
    while (i.hasNext()) {
        Entry & entry = i.next();
        if (entry.path == canonicalPath) {
            if (outName) {
                // If the caller cares about the name, it will most likely be re-added later rather than skipped.
                *outName = entry.name;
            } else {
                qDebug().noquote() << "skipping file:" << path;
            }

            if (fi.isDir()) {
                m_dir_count--;
            } else {
                m_file_count--;
                m_byte_count -= fi.size();
            }
            i.remove();
            removed++;
        }
    }
    
    if (removed > 1) {
        qWarning().noquote() << removed << "copies found in zip queue:" << path;
    }
    return removed;
}

const QString FileQueue::toString() const
{
    return QString("%1 directories, %2 files, %3 bytes").arg(m_dir_count).arg(m_file_count).arg(m_byte_count);
}


// ==================================================================================================
// Static functions to abstract the details of miniz from the primary logic.

#include "SleepLib/thirdparty/miniz.h"

// Callback for miniz to write compressed data
static size_t zip_write(void *pOpaque, mz_uint64 /*file_ofs*/, const void *pBuf, size_t n)
{
    if (pOpaque == nullptr) {
        qCritical() << "null pointer passed to ZipFile::Write!";
        return 0;
    }
    QFile* file = (QFile*) pOpaque;
    size_t written = file->write((const char*) pBuf, n);
    if (written < n) {
        qWarning() << "error writing to" << file->fileName();
    }
    return written;
}

static void* zip_init()
{
    mz_zip_archive* pZip = new mz_zip_archive();  // zero-initializes struct
    pZip->m_pWrite = zip_write;
    return pZip;
}

static void zip_done(void* ctx)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    delete pZip;
}

static bool zip_open(void* ctx, QFile & file)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;

    pZip->m_pIO_opaque = &file;
    bool ok = mz_zip_writer_init_v2(pZip, 0, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (!ok) {
        mz_zip_error mz_err = mz_zip_get_last_error(pZip);
        qWarning() << "unable to initialize miniz writer" << MZ_VERSION << mz_zip_get_error_string(mz_err);
    }
    return ok;
}

static bool zip_add(void* ctx, const QString & archive_name, const QByteArray & data, const QDateTime & modified)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    
    // Add to .zip
    time_t last_modified = modified.toSecsSinceEpoch();  // technically deprecated, but miniz expects a time_t
    bool ok = mz_zip_writer_add_mem_ex_v2(pZip, archive_name.toLocal8Bit(), data.constData(), data.size(),
                                          nullptr, 0,  // no comment
                                          MZ_DEFAULT_COMPRESSION,
                                          0, 0,  // not used when compressing data
                                          &last_modified,
                                          nullptr, 0,  // no user extra data
                                          nullptr, 0   // no user extra data central
                                         );
    if (!ok) {
        mz_zip_error mz_err = mz_zip_get_last_error(pZip);
        qWarning() << "unable to add" << archive_name << ":" << data.size() << "bytes" << mz_zip_get_error_string(mz_err);
    }
    return ok;
}

static void zip_close(void* ctx)
{
    Q_ASSERT(ctx);
    mz_zip_archive* pZip = (mz_zip_archive*) ctx;
    mz_zip_writer_finalize_archive(pZip);
    mz_zip_writer_end(pZip);
}
