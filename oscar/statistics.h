/* Statistics Report Generator Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SUMMARY_H
#define SUMMARY_H

#include <QObject>
#include <QMainWindow>
#include <QPrinter>
#include <QPainter>
#include <QHash>
#include <QList>


#include "SleepLib/schema.h"
#include "SleepLib/machine.h"


class SummaryInfo
{
public:
    QString display(QString);
    void update(QDate latest, QDate earliest) ;
    int size() {return numDisabledsessions;};
    QDate first() {return _start;};
    QDate start() {return _start;};
    QDate last() {return _last;};
private:
    int totalDays ;
    int daysNoData ;
    int daysOutOfCompliance ;
    int daysInCompliance ;
    int numDisabledsessions ;
    int numDaysWithDisabledsessions ;
    int numDaysDisabledSessionChangedCompliance ;
    double maxDurationOfaDisabledsession ;
    double totalDurationOfDisabledSessions ;
    QDate _start;
    QDate _last;
public:
    void clear (QDate s,QDate l) {
        totalDays = 0;
        daysNoData = 0;
        daysOutOfCompliance = 0;
        daysInCompliance = 0;
        numDisabledsessions = 0;
        numDaysWithDisabledsessions = 0;
        maxDurationOfaDisabledsession = 0;
        numDaysDisabledSessionChangedCompliance = 0;
        totalDurationOfDisabledSessions = 0;
        _start = QDate(s);
        _last = QDate(l);
    };
};




//! \brief Type of calculation on one statistics row
enum StatCalcType {
    SC_UNDEFINED=0, SC_COLUMNHEADERS, SC_HEADING, SC_SUBHEADING, SC_MEDIAN, SC_AVG, SC_WAVG, SC_90P, SC_MIN, SC_MAX, SC_CPH, SC_SPH, SC_AHI_RDI , SC_HOURS, SC_TOTAL_DAYS_PERCENT, SC_DAYS_HEADER , SC_ABOVE, SC_BELOW , SC_WARNING , SC_MESSAGE ,
    SC_TOTAL_DAYS , SC_DAYS_W_DATA , SC_DAYS_WO_DATA , SC_DAYS_GE_COMPLIANCE_HOURS , SC_USED_DAY_PERCENT , SC_DAYS_LT_COMPLAINCE_HOURS , SC_MEDIAN_HOURS , SC_MEDIAN_AHI , SC_AHI_ONLY , SC_SPACE
};

/*! \struct StatisticsRow
    \brief Describes a single row on the statistics page
    */
struct StatisticsRow {
    StatisticsRow() { calc=SC_UNDEFINED; }
    StatisticsRow(QString src, QString calc, QString type) {
        this->src = src;
        this->calc = lookupCalc(calc);
        this->type = lookupType(type);
    }
    StatisticsRow(QString src, StatCalcType calc, MachineType type) {
        this->src = src;
        this->calc = calc;
        this->type = type;
    }
    StatisticsRow(const StatisticsRow &copy) {
        src=copy.src;
        calc=copy.calc;
        type=copy.type;
    }
    StatisticsRow& operator=(const StatisticsRow&) = default;
    ~StatisticsRow() {};
    QString src;
    StatCalcType calc;
    MachineType type;

    //! \brief Looks up calculation type for this row
    StatCalcType lookupCalc(QString calc)
    {
        if (calc.compare("avg",Qt::CaseInsensitive)==0) {
            return SC_AVG;
        } else if (calc.compare("w-avg",Qt::CaseInsensitive)==0) {
            return SC_WAVG;
        } else if (calc.compare("median",Qt::CaseInsensitive)==0) {
            return SC_MEDIAN;
        } else if (calc.compare("90%",Qt::CaseInsensitive)==0) {
            return SC_90P;
        } else if (calc.compare("min", Qt::CaseInsensitive)==0) {
            return SC_MIN;
        } else if (calc.compare("max", Qt::CaseInsensitive)==0) {
            return SC_MAX;
        } else if (calc.compare("cph", Qt::CaseInsensitive)==0) {
            return SC_CPH;
        } else if (calc.compare("sph", Qt::CaseInsensitive)==0) {
            return SC_SPH;
        } else if (calc.compare("ahi", Qt::CaseInsensitive)==0) {
            return SC_AHI_RDI;
        } else if (calc.compare("hours", Qt::CaseInsensitive)==0) {
            return SC_HOURS;
        #if 0
        } else if (calc.compare("compliance", Qt::CaseInsensitive)==0) {
            return SC_TOTAL_DAYS_PERCENT;
        } else if (calc.compare("days", Qt::CaseInsensitive)==0) {
            return SC_DAYS;
        } else if (calc.compare("heading", Qt::CaseInsensitive)==0) {
            return SC_HEADING;
        } else if (calc.compare("subheading", Qt::CaseInsensitive)==0) {
            return SC_SUBHEADING;
        #endif
        }
        return SC_UNDEFINED;
    }

    //! \brief Look up device type
    MachineType lookupType(QString type)
    {
        if (type.compare("cpap", Qt::CaseInsensitive)==0) {
            return MT_CPAP;
        } else if (type.compare("oximeter", Qt::CaseInsensitive)==0) {
            return MT_OXIMETER;
        } else if (type.compare("sleepstage", Qt::CaseInsensitive)==0) {
            return MT_SLEEPSTAGE;
        }
        return MT_UNKNOWN;
    }


    ChannelID channel() {
        return schema::channel[src].id();
    }

    QString value(QDate start, QDate end );
};

//! \class Prescription (device) setting
class RXItem {
public:
    RXItem() {
        machine = nullptr;
        ahi = rdi = 0;
        hours = 0;
    }
    RXItem(const RXItem & copy) {
        start = copy.start;
        end = copy.end;
        days = copy.days;
        s_count = copy.s_count;
        s_sum = copy.s_sum;
        ahi = copy.ahi;
        rdi = copy.rdi;
        hours = copy.hours;
        machine = copy.machine;
        relief = copy.relief;
        mode = copy.mode;
        pressure = copy.pressure;
        dates = copy.dates;
    }
    RXItem& operator=(const RXItem&) = default;
    inline quint64 count(ChannelID id) const {
        QHash<ChannelID, quint64>::const_iterator it = s_count.find(id);
        if (it == s_count.end()) return 0;
        return it.value();
    }
    inline double sum(ChannelID id) const{
        QHash<ChannelID, double>::const_iterator it = s_sum.find(id);
        if (it == s_sum.end()) return 0;
        return it.value();
    }
    QDate start;
    QDate end;
    int days;
    QHash<ChannelID, quint64> s_count;
    QHash<ChannelID, double> s_sum;
    quint64 ahi;
    quint64 rdi;
    double hours;
    Machine * machine;
    QString relief;
    QString mode;
    QString pressure;
    QMap<QDate, Day *> dates;
};



class Statistics : public QObject
{
    Q_OBJECT
  public:
    explicit Statistics(QObject *parent = 0);

    QString GenerateHTML();

    QString UpdateRecordsBox();

    static void printReport(QWidget *parent = nullptr);

    static void updateReportDate();

    void adjustRange(QDate& start , QDate& last);

  protected:
    void loadRXChanges();
    void saveRXChanges();
    void updateRXChanges();

    QString getUserInfo();
    QString getRDIorAHIText();

    QString htmlNoData();
    QString generateHeader(bool showheader);
    QString generateFooter(bool showinfo=true);

    QString GenerateMachineList();
    QString GenerateRXChanges();
    QString GenerateCPAPUsage();

    // Using a map to maintain order
    QList<StatisticsRow> rows;
    QMap<StatCalcType, QString> calcnames;
    QMap<MachineType, QString> machinenames;

    QMap<QDate, RXItem> rxitems;

    QList<QDate> record_best_ahi;
    QList<QDate> record_worst_ahi;

  signals:

  public slots:

};

#endif // SUMMARY_H
