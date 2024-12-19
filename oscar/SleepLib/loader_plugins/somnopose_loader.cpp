/* SleepLib Somnopose Loader Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

//********************************************************************************************
// Please only INCREMENT the somnopose_data_version in somnopose_loader.h when making changes
// that change loader behaviour or modify channels in a manner that fixes old data imports.
// Note that changing the data version will require a reimport of existing data for which OSCAR
// does not keep a backup - so it should be avoided if possible.
// i.e. there is no need to change the version when adding support for new devices
//********************************************************************************************

#include <QDir>
#include <QTextStream>
#include <QTimeZone>
#include "somnopose_loader.h"
#include "SleepLib/machine.h"

SomnoposeLoader::SomnoposeLoader()
{
    m_type = MT_POSITION;
}

SomnoposeLoader::~SomnoposeLoader()
{
}

int SomnoposeLoader::OpenFile(const QString & filename)
{
    QFile file(filename);

    if (filename.toLower().endsWith(".csv")) {
        if (!file.open(QFile::ReadOnly)) {
            qDebug() << "Couldn't open Somnopose data file" << filename;
            return -1;
        }
    } else {
        return -1;
    }

    qDebug() << "Opening file" << filename;
    QTextStream ts(&file);

    // Read header line and determine order of fields
    QString hdr = ts.readLine();
    QStringList headers = hdr.split(",");
    QString model = "";
    QString serial = "";

    int col_inclination = -1, col_orientation = -1, col_timestamp = -1, col_movement = -1;

    int hdr_size = headers.size();

    for (int i = 0; i < hdr_size; i++) {
        // Optional header model=<model>
        if (headers.at(i).startsWith("model=", Qt::CaseInsensitive)) {
            model=headers.at(i).split("=")[1];
        }

        // Optional header serial=<serial>
        if (headers.at(i).startsWith("serial=", Qt::CaseInsensitive)) {
            serial=headers.at(i).split("=")[1];
        }

        if (headers.at(i).compare("timestamp", Qt::CaseInsensitive) == 0) {
            col_timestamp = i;
        }

        if (headers.at(i).compare("inclination", Qt::CaseInsensitive) == 0) {
            col_inclination = i;
        }

        if (headers.at(i).compare("orientation", Qt::CaseInsensitive) == 0) {
            col_orientation = i;
        }

        if (headers.at(i).compare("movement", Qt::CaseInsensitive) == 0) {
            col_movement = i;
        }
    }

    // Check we have all fields available
    if (col_timestamp < 0) {
        qDebug() << "Header missing timestamp";
        return -1;
    }

    if ((col_inclination < 0) && (col_orientation < 0) && (col_movement < 0)) {
        qDebug() << "Header missing all of inclination, orientation, movement (at least one must be present)";
        return -1;
    }

     #if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
         QDateTime epoch(QDate(2001, 1, 1).startOfDay(QTimeZone("UTC")));
         qint64 ep = epoch.toMSecsSinceEpoch() , time=0;
     #else
        QDateTime epoch(QDate(2001, 1, 1));
        qint64 ep = qint64(epoch.toSecsSinceEpoch()+epoch.offsetFromUtc()) * 1000, time=0;
    #endif
    qDebug() << "Epoch starts at" << epoch.toString();

    double timestamp, orientation=0, inclination=0, movement=0;
    QString data;
    QStringList fields;
    bool ok, orientation_ok, inclination_ok, movement_ok;

    bool first = true, skip_session = false;
    int session_count = 0;
    MachineInfo info = newInfo();
    info.model = model;
    info.serial = serial;
    Machine *mach = p_profile->CreateMachine(info);
    Session *sess = nullptr;
    SessionID sid;

    EventList *ev_orientation = nullptr, *ev_inclination = nullptr, *ev_movement = nullptr;

    while (!(data = ts.readLine()).isEmpty()) {
        fields = data.split(",");

        if (fields.size() >= col_timestamp && fields[col_timestamp] == "-") {
            // Flag end of session...
            if (sess) {
                if (ev_orientation) {
                    sess->setMin(POS_Orientation, ev_orientation->Min());
                    sess->setMax(POS_Orientation, ev_orientation->Max());
                }
                if (ev_inclination) {
                    sess->setMin(POS_Inclination, ev_inclination->Min());
                    sess->setMax(POS_Inclination, ev_inclination->Max());
                }
                if (ev_movement) {
                    sess->setMin(POS_Movement, ev_movement->Min());
                    sess->setMax(POS_Movement, ev_movement->Max());
                }
                sess->really_set_last(time);
                sess->SetChanged(true);

                mach->AddSession(sess);
                session_count++;
            }
            // Prepare for potential next session...
            sess = nullptr;
            ev_orientation = ev_inclination = ev_movement = nullptr;
            first = true;
            skip_session = false;
            continue;
        }

        if (skip_session) {
            continue;
        }

        if (fields.size() < hdr_size) { // missing fields.. skip this record
            continue;
        }

        timestamp = fields[col_timestamp].toDouble(&ok);

        if (!ok) { continue; }
        orientation_ok = inclination_ok = movement_ok = false;

        if (col_orientation >= 0) {
            orientation = fields[col_orientation].toDouble(&orientation_ok);
        }

        if (col_inclination >= 0) {
            inclination = fields[col_inclination].toDouble(&inclination_ok);
        }

        if (col_movement >= 0) {
            movement = fields[col_movement].toDouble(&movement_ok);
        }

        if (!orientation_ok && !inclination_ok && !movement_ok) {
            continue;
        }
        // convert to milliseconds since epoch
        time = (timestamp * 1000.0) + ep;

        if (first) {
            first = false;
            sid = time / 1000;
            qDebug() << "First timestamp is" << QDateTime::fromMSecsSinceEpoch(time).toString();

            if (mach->SessionExists(sid)) {
                qDebug() << "File " << filename << " session " << sid << " already loaded... skipping";
                // Continue processing file to allow for case where new sessions are added to a file
                skip_session = true;
                continue;
            }

            sess = new Session(mach, sid);
            sess->really_set_first(time);
            if (col_orientation >= 0) {
                ev_orientation = sess->AddEventList(POS_Orientation, EVL_Event, 1, 0, 0, 0);
            }
            if (col_inclination >= 0) {
                ev_inclination = sess->AddEventList(POS_Inclination, EVL_Event, 1, 0, 0, 0);
            }
            if (col_movement >= 0) {
                ev_movement = sess->AddEventList(POS_Movement, EVL_Event, 1, 0, 0, 0);
            }
        }

        sess->set_last(time);
        if (ev_orientation && orientation_ok) {
            ev_orientation->AddEvent(time, orientation);
        }
        if (ev_inclination && inclination_ok) {
            ev_inclination->AddEvent(time, inclination);
        }
        if (ev_movement && movement_ok) {
            ev_movement->AddEvent(time, movement);
        }
    }

    if (sess) {
        if (ev_orientation) {
            sess->setMin(POS_Orientation, ev_orientation->Min());
            sess->setMax(POS_Orientation, ev_orientation->Max());
        }
        if (ev_inclination) {
            sess->setMin(POS_Inclination, ev_inclination->Min());
            sess->setMax(POS_Inclination, ev_inclination->Max());
        }
        if (ev_movement) {
            sess->setMin(POS_Movement, ev_movement->Min());
            sess->setMax(POS_Movement, ev_movement->Max());
        }
        sess->really_set_last(time);
        sess->SetChanged(true);

        mach->AddSession(sess);
        session_count++;
    }

    if (session_count) {
        mach->Save();
        // Adding these to hopefully make data persistent...
        mach->SaveSummaryCache();
        p_profile->StoreMachines();
    }

    return session_count;
}


static bool somnopose_initialized = false;

void SomnoposeLoader::Register()
{
    if (somnopose_initialized) { return; }

    qDebug("Registering SomnoposeLoader");
    RegisterLoader(new SomnoposeLoader());
    //InitModelMap();
    somnopose_initialized = true;
}

