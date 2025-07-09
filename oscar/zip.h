/* OSCAR ZIP archive creation
 * Provides a Qt-convenient wrapper around miniz, see https://github.com/richgel999/miniz
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include <QObject>
#include <QString>
#include <QDir>
#include <QFile>

class ProgressDialog;

class ZipFile : public QObject
{
    Q_OBJECT

public:
    ZipFile();
    virtual ~ZipFile();
    
    bool Open(const QString & filepath);
    bool AddDirectory(const QString & path, ProgressDialog* progress=nullptr);  // add a directory and recurse
    bool AddDirectory(const QString & path, const QString & archive_name, ProgressDialog* progress=nullptr);  // add a directory and recurse
    bool AddFiles(class FileQueue & queue, ProgressDialog* progress=nullptr);  // add a fixed list of files
    bool AddFile(const QString & path, const QString & archive_name);  // add a single file
    void Close();
    
    bool aborted() const { return m_abort; }

public slots:
    void abort() { m_abort = true; }

signals:
    void setProgressMax(int max);
    void setProgressValue(int val);

protected:
    void* m_ctx;
    QFile m_file;
    bool m_abort;
    quint64 m_progress;
};


class FileQueue
{
    struct Entry
    {
        QString path;
        QString name;
    };
    QList<Entry> m_files;
    int m_dir_count;
    int m_file_count;
    quint64 m_byte_count;

public:
    FileQueue() : m_dir_count(0), m_file_count(0), m_byte_count(0) {}
    ~FileQueue() = default;

    //!brief Remove a file from the queue, return the number of instances removed.
    int Remove(const QString & path, QString* outName=nullptr);
    
    //!brief Recursively add a directory and its contents to the queue along with the prefix to be used in an archive.
    bool AddDirectory(const QString & path, const QString & prefix="");
    
    //!brief Add a file to the queue along with the name to be used in an archive.
    bool AddFile(const QString & path, const QString & archive_name="");

    inline int dirCount() const { return m_dir_count; }
    inline int fileCount() const { return m_file_count; }
    inline quint64 byteCount() const { return m_byte_count; }
    const QList<Entry> & files() const { return m_files; }
    const QString toString() const;
};
