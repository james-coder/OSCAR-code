/* SleepLib Journal Implementation
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
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <QDir>
#include "journal.h"
#include "daily.h"
#include "SleepLib/common.h"
#include "SleepLib/machine_common.h"
#include "SleepLib/session.h"
#include "SleepLib/profiles.h"
#include "mainwindow.h"

extern MainWindow * mainwin;
const QString OLD_ZOMBIE = QString("zombie");
const QString ZOMBIE = QString("Feelings");
const QString WEIGHT = QString("weight");

bool Journal::BackupJournal(QString filename)
{
    QString outBuf;
    QXmlStreamWriter stream(&outBuf);
    stream.setAutoFormatting(true);
    stream.setAutoFormattingIndent(2);

    stream.writeStartDocument();
//    stream.writeProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
    stream.writeStartElement("OSCAR");
    stream.writeAttribute("created", QDateTime::currentDateTime().toString(Qt::ISODate));

    stream.writeStartElement("Journal");
    stream.writeAttribute("username", p_profile->user->userName());
    stream.writeAttribute("height_cm", QString::number(p_profile->user->height()));
    if (p_profile->general->unitSystem()!=US_Undefined) {
        stream.writeAttribute("systemUnits", ( p_profile->general->unitSystem()==US_Metric?"Metric":"Englsh") );
    }

    QDate first = p_profile->FirstDay(MT_JOURNAL);
    QDate last = p_profile->LastDay(MT_JOURNAL);
    DEBUGFC Q(first) Q(last);


    QDate date = first.addDays(-1);
    int days_saved = 0 ;
    do {
        date = date.addDays(1);

        Day * journal = p_profile->GetDay(date, MT_JOURNAL);
        if (!journal) continue;

        Session * sess = journal->firstSession(MT_JOURNAL);
        if (!sess) continue;

        if (   !journal->settingExists(Journal_Notes)
            && !journal->settingExists(Journal_Weight)
            && !journal->settingExists(Journal_ZombieMeter)
            && !journal->settingExists(LastUpdated)
            && !journal->settingExists(Bookmark_Start)) {
            continue;
        }
        QString weight;
        QString zombie;
        QString notes;
        QString lastupdated;
        QVariantList start;
        int havedata=0;


        if (journal->settingExists(Journal_Weight)) {
            weight = sess->settings[Journal_Weight].toString();
            havedata |= JRNL_Weight ;
        }

        if (journal->settingExists(Journal_ZombieMeter)) {
            zombie = sess->settings[Journal_ZombieMeter].toString();
            havedata |= JRNL_Zombie ;
        }

        if (journal->settingExists(Journal_Notes)) {
            notes = sess->settings[Journal_Notes].toString();
            //notes = Daily::convertHtmlToPlainText(notes).trimmed();
            havedata |= JRNL_Notes ;
        }

        if (journal->settingExists(Bookmark_Start)) {
            start=sess->settings[Bookmark_Start].toList();
            if (start.size()>0) havedata |= JRNL_Bookmarks ;
        }

        if (journal->settingExists(LastUpdated)) {
            QDateTime dt = sess->settings[LastUpdated].toDateTime();
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
            qint64 dtx = dt.toMSecsSinceEpoch()/1000L;
#else
            qint64 dtx = dt.toSecsSinceEpoch();
#endif
            lastupdated = QString::number(dtx);
        }

        if (!havedata) {
            // No data to archive.
            continue ;
        }
        //QString dateStr = ((++count & 1)==0) ? date.toString(/*Qt::ISODate*/) : date.toString(Qt::ISODate) ;
        QString dateStr =date.toString(Qt::ISODate) ;
        stream.writeStartElement("day");
            stream.writeAttribute("date", dateStr);
                if(!weight.isEmpty()) stream.writeAttribute(WEIGHT, weight);
                if(!zombie.isEmpty()) stream.writeAttribute(ZOMBIE, zombie);
                stream.writeAttribute("lastupdated", lastupdated);

            if (!notes.isEmpty() ) {
                stream.writeStartElement("note");
                    stream.writeTextElement("text", notes);
                stream.writeEndElement(); // notes
            }

            if (start.size()>0) {
                QVariantList end=sess->settings[Bookmark_End].toList();
                QStringList notes=sess->settings[Bookmark_Notes].toStringList();
                stream.writeStartElement("bookmarks");
                int size = start.size();
                for (int i=0; i< size; i++) {
                    stream.writeStartElement("bookmark");
                    stream.writeAttribute("notes",notes.at(i));
                    stream.writeAttribute("start",start.at(i).toString());
                    stream.writeAttribute("end",end.at(i).toString());
                    stream.writeEndElement(); // bookmark
                }
                stream.writeEndElement(); // bookmarks
            }
        days_saved++;
        stream.writeEndElement(); // day
    } while (date <= last);
    // //stream.writeAttribute("DaysSaved", QString::number(days_saved));

    stream.writeEndElement(); // Journal
    stream.writeEndElement(); // OSCAR
    stream.writeEndDocument();

    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Couldn't open journal file" << filename << "error code" << file.error() << file.errorString();
        return false;
    }

    QTextStream ts(&file);
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        ts.setCodec("UTF-8");
    #else
        // Qt6 version utf-8 is the default
        // or if not utf-8
        // ts.setEncoding(QStringConverter::Utf8);
    #endif
    ts.setGenerateByteOrderMark(true);
    ts << outBuf;
    file.close();
    QMessageBox::information(nullptr, STR_MessageBox_Information,
        QString(QObject::tr("%1 days Journal Data was saved in file %2")).arg(days_saved).arg(filename) ,
        QMessageBox::Ok);

    return true;
}

void Journal::getJournal(Daily*& daily,QDate& date,Session* & journal) {
    if (journal) return;
    journal = daily->GetJournalSession(date,true);
    if (journal) return;
    journal = daily->CreateJournalSession(date);
}


bool Journal::RestoreDay (QDomElement& dayElement,QDate& date,QString& filename) {
    Daily* daily = mainwin->getDaily();
    if (!daily) return false;
    Session* journal = nullptr;
    bool changed = false;

    // handle zombie - feelings
    bool ok = false;
    int zombie = 0;
    zombie =  (dayElement.attribute(ZOMBIE)).toInt(&ok);
    if (!ok) { zombie=0; }
    if (zombie == 0 ) {
        zombie =  (dayElement.attribute(OLD_ZOMBIE)).toInt(&ok);
        if (!ok) { zombie=0; }
    }
    DEBUGFC O("\n\n") ;
    DEBUGFC O(date) O(filename); Q_UNUSED(filename);
    if (zombie>0) {
        int jvalue =  0 ;
        getJournal(daily,date,journal);
        if (journal->settings.contains(Journal_ZombieMeter)) {
            jvalue = journal->settings[Journal_ZombieMeter].toInt();
        }
        if (jvalue == 0)  {
            DEBUGFC O(date) Q(zombie);
            daily->set_JournalZombie(date,zombie);
            if (date == daily->getDate()) daily->set_ZombieUI(zombie);
            changed = true;
        }
    }

    // handle Weight
    double weight =  (dayElement.attribute(WEIGHT)).toDouble(&ok);
    if (weight>zeroD) {
        getJournal(daily,date,journal);
        double jvalue = 0.0 ;
        if (journal->settings.contains(Journal_Weight)) {
            jvalue = journal->settings[Journal_Weight].toDouble();
        }
        if (jvalue < zeroD )  {
            DEBUGFC O(date) Q(weight);
            daily->set_JournalWeightValue(date,weight);
            if (date == daily->getDate()) daily->set_WeightUI(weight);
            changed = true;
        }
    }

    // Handle Notes.
    QDomElement noteText = dayElement.elementsByTagName("note").at(0).toElement().elementsByTagName("text").at(0).toElement();
    if (!noteText.text().isEmpty() ) {
        getJournal(daily,date,journal);
        // there are characters in notes. maybe just spaces. Ignore spaces.
        QString plainTextToAdd = Daily::convertHtmlToPlainText(noteText.text());

        // get existing note if any.
        QString currNoteHtml = journal->settings[Journal_Notes].toString();
        QString currNotePlainText = Daily::convertHtmlToPlainText(currNoteHtml);
        if (currNotePlainText.contains(plainTextToAdd) ) {
            // plainText to add is equal to curr or is it a subset of the current Note
            // use the current notes.. ignore text in backup.
        } else {
            // plainText is not equal to curr nor is it a subset of the current Note
            if (plainTextToAdd.contains(currNotePlainText) ) {
                //curr note is a subset of the new - so use new.
                currNoteHtml = noteText.text();
            } else {
                // ToAdd text and Current text are different.
                // use previous verson append with  current 
                currNoteHtml.prepend(noteText.text());
            }
            daily->set_JournalNotesHtml(date,currNoteHtml);
            if (date == daily->getDate()) daily->set_NotesUI(noteText.text());
            DEBUGFC O(date) Q((void*)journal) O( )Daily::convertHtmlToPlainText(currNoteHtml);
            changed = true;
        }
    }

    QDomNodeList bookmarks = dayElement.elementsByTagName("bookmarks").at(0).toElement().elementsByTagName("bookmark");
    if (bookmarks.size()>0) {
        DEBUGFC Q(bookmarks.size());
        getJournal(daily,date,journal);
        // get list of bookmarks for journal. These will not be removed.
        QVariantList start;
        QVariantList end;
        QStringList notes;
        if (journal->settings.contains(Bookmark_Start)) {
            //DEBUGFC;
            start=journal->settings[Bookmark_Start].toList();
            end=journal->settings[Bookmark_End].toList();
            notes=journal->settings[Bookmark_Notes].toStringList();
        }
        // check if arcived bookmark is current list
        bool bmChanged = false;
        for (int idx=0 ; idx < bookmarks.size() ; idx++) {
            // get archived bookmark
            //DEBUGFC Q(idx) Q(bookmarks.size()) Q(start.size()) ;
            QDomElement bookmark = bookmarks.at(idx).toElement();
            qint64 archiveStart = bookmark.attribute("start").toLongLong();
            qint64 archiveEnd   = bookmark.attribute("end").toLongLong();
            QString archiveNote = bookmark.attribute("notes");
            // check if bookmark already exists.

            bool duplicate = false;
            bool bNoteChanged = false;
            for (int idy=0 ; idy < start.size() ; idy++) {
                //DEBUGFC Q(idy);
                qint64 bmStart = start.at(idy).toLongLong();
                qint64 bmEnd   = end.at(idy).toLongLong();
                if ( (bmStart == archiveStart) && (bmEnd == archiveEnd) ) {
                    duplicate = true;
                    //DEBUGFC Q(idx) Q(idy);
                    // have same bookmark - new check if notes need merging
                    QString aNote = archiveNote.simplified();
                    QString bmNote = notes.at(idy);
                    QString bNote = bmNote.simplified();
                    DEBUGFC  Q(aNote) Q(bNote) Q(bNote.contains(aNote)) Q(aNote.contains(bNote));
                    if (bNote.contains(aNote) ) {
                        // no action needed.
                        //DEBUGFC Q(idx) Q(idy) Q(bNote);;
                        break;
                    } else {
                        // updated existing bookmark label
                        //DEBUGFC Q(idx) Q(idy);
                        // if an append is needed.
                        if (aNote.contains(bNote)) {
                            // use archived note
                            //DEBUGFC Q(idx) Q(idy);
                        } else {
                            //DEBUGFC Q(idx) Q(idy);
                            // prepend  archive note to bmNote
                            archiveNote.append(" :: ").append(bmNote);
                        }
                        // use archiveNote.
                        notes[idy]= archiveNote;
                        bmChanged = true;
                        bNoteChanged = true;
                    }
                }
            }
            if (!duplicate) {
                DEBUGFC Q(date) Q(idx) Q(archiveNote);
                // here if need to add archive bookmark.
                // add archive bookmark to currrent list
                start.push_back(archiveStart);
                end.push_back(archiveEnd);
                notes.push_back(archiveNote);
                bmChanged = true;
            } else if (bNoteChanged) {
                DEBUGFC Q(date) Q(idx) Q(archiveNote);
            }
        }
       if (bmChanged) {
            //DEBUGFC Q(bmChanged);
            getJournal(daily,date,journal);
            journal->settings[Bookmark_Start]=start;
            journal->settings[Bookmark_End]=end;
            journal->settings[Bookmark_Notes]=notes;
            if (date == daily->getDate()) daily->set_BookmarksUI(start,end,notes,0);
        } else {
            DEBUGFC O("bookmark ignored") ;
        }

    }

    if (!journal) {
        return true;
    }
    if (!journal->machine()) {
        return true;
    }
    if (changed) {
        journal->SetChanged(true);
        journal->settings[LastUpdated] = QDateTime::currentDateTime();
        journal->machine()->SaveSummaryCache();
        journal->SetChanged(false);
    }
    DEBUGFC O(date) O("\n\n");
    return true;
}


bool Journal::RestoreJournal(QString filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Could not open" << filename.toLocal8Bit().data() << "for reading, error code" << file.error() << file.errorString();
        return false;
    }

    QDomDocument doc("machines.xml");

    if (!doc.setContent(&file)) {
        qWarning() << "Invalid XML Content in" << filename.toLocal8Bit().data();
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement(); // Get the root element of the document
    if (root.isNull()) {
        return false;
    }
    QDomNodeList days = doc.elementsByTagName("day");
    if (days.isEmpty()) {
        return false;
    }

    QDate first = p_profile->FirstDay();
    QDate last = p_profile->LastDay();
    QDate current = QDate::currentDate();
    //int used=0;
    for (int idx=0 ; idx < days.size() ; idx++) {
        QDomElement dayElement = days.at(idx).toElement();

        QString dateStr = dayElement.attribute("date") ;
        QDate   date    = QDate::fromString(dateStr,Qt::ISODate) ;
        if (!date.isValid()) {
            QDate newdate   = QDate::fromString(dateStr) ;  // read original date format 
            date = newdate;
        }
        if (!date.isValid()) {
            continue;
        }
        if (date < first) {
            continue;
        }
        if (date > last) {
            continue;
        }
        if (date > current) {
            continue;
        }
        if ( RestoreDay(dayElement,date,filename) ) {
           //used++;
        }
    }
    double user_height_cm  = p_profile->user->height();
    if (user_height_cm<zeroD) {
        QDomElement journal = root.elementsByTagName("Journal").at(0).toElement();
        double height_cm = journal.attribute("height_cm").toDouble();
        DEBUGFC  Q(journal.tagName()) Q(height_cm);
        if (height_cm>=zeroD) p_profile->user->setHeight(height_cm);
    }
    return true;
}

