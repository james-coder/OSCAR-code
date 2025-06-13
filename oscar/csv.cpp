/* OSCAR CSV Reader Implementation
 *
 * Copyright (c) 2020-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "csv.h"
#include <QDebug>

CSVReader::CSVReader(QIODevice & input, const QString & delim, const QString & comment)
    : m_stream(&input), m_delim(delim), m_comment(comment)
{
}

void CSVReader::setFieldNames(QStringList & header)
{
    m_field_names = header;
}

// This is a very simplistic reader that splits lines on the delimiter and truncates
// lines after the comment sequence (if specified). It doesn't do any quote handling.
// If that's ultimately necessary, either rewrite it or subclass it.
//
// For a public domain version that handles RFC 4180, see:
// https://stackoverflow.com/questions/27318631/parsing-through-a-csv-file-in-qt/40229435#40229435
//
bool CSVReader::readRow(QStringList & fields)
{
    QString line;
    fields.clear();
    
    // Read until the next non-empty/non-comment line.
    do {
        line = m_stream.readLine();
        if (line.isNull()) {
            return false;
        }
        if (m_comment.isNull() == false) {
            line = line.section(m_comment, 0, 0);
        }
    } while (line.isEmpty());

    fields = line.split(m_delim);
    return true;
}

bool CSVReader::readRow(QHash<QString,QString> & row)
{
    QStringList fields;
    row.clear();
    
    if (!readRow(fields)) {
        return false;
    }
    if (fields.size() > m_field_names.size()) {
        qWarning() << "row has too many columns";
        return false;
    }
    for (int i = 0; i < fields.size(); i++) {
        row[m_field_names.at(i)] = fields.at(i);
    }
    return true;
}
