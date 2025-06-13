/* SleepLib Journal Implementation
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */


#ifndef JOURNAL_H
#define JOURNAL_H

class Session;
#include <QString>
#include <QDate>
#include <QDomDocument>
#include "daily.h"

#include "SleepLib/profiles.h"
class Journal {
public:
    enum { JRNL_Zombie = 1 , JRNL_Weight = 2 , JRNL_Notes = 4 , JRNL_Bookmarks = 8 };
    static bool BackupJournal(QString filename);
    static bool RestoreJournal(QString filename);
private:
    constexpr static double zeroD = 0.0001;
    static void getJournal(Daily*& daily,QDate& date,Session* & journal);
    static bool RestoreDay (QDomElement& dayElement,QDate& date,QString& filename) ;
};

#endif // JOURNAL_H
