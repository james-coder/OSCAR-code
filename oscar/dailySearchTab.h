/* search  GUI Headers
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SEARCHDAILY_H
#define SEARCHDAILY_H

#include <QDate>
#include <QTextDocument>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRegularExpression>       // QRegexp is obsoluted.
#include <QList>
#include <QFrame>
#include <QWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QTabWidget>
#include <QMap>
#include <QTextEdit>
#include "SleepLib/common.h"
#include "SleepLib/machine_common.h"

class GPushButton;
class QWidget ;
class QDialog ;
class QComboBox ;
class QListWidget ;
class QProgressBar ;
class QHBoxLayout ;
class QVBoxLayout ;
class QLabel ;
class QDoubleSpinBox ;
class QSpinBox ;
class QLineEdit ;
class QTableWidget ;
class QTableWidgetItem ;
class QSizeF ;

class Day;  //forward declaration.
class Daily;  //forward declaration.

enum ValueMode { invalidValueMode , notUsed , minutesToMs  , hoursToMs , hundredths , opWhole , displayWhole , opString , displayString, secondsDisplayString};

enum SearchTopic { ST_NONE , ST_DAYS_SKIPPED , ST_DISABLED_SESSIONS  , ST_JOURNAL , ST_NOTES  , ST_NOTES_STRING , ST_BOOKMARKS , ST_BOOKMARKS_STRING , ST_AHI , ST_SESSION_LENGTH , ST_SESSIONS_QTY , ST_DAILY_USAGE , ST_APNEA_LENGTH , ST_APNEA_ALL , ST_CLEAR , ST_EVENT };

enum OpCode {
    //DO NOT CHANGE NUMERIC OP CODES because THESE VALUES impact compare operations.
    // start of fixed codes - do not modifyusage bit1(1)==> Less ; bit2(2)==>greater  bit3(4)==>Equal;
    OP_INVALID , OP_LT , OP_GT , OP_NE , OP_EQ , OP_LE , OP_GE , OP_END_NUMERIC ,
    // end of fixed codes
    OP_CONTAINS , OP_WILDCARD , OP_NO_PARMS /*Self starting*/};

class Match
{
    public:
	void stateMachine();
    Match(bool valid=false ) {this->valid=valid;};
    virtual ~Match( ) {};
    bool isValid() {return valid;};
    bool isEmpty() {return !valid  || searchTopic==ST_NONE;};

    QString     matchName;
    QString     opCodeStr;
    QString     compareString;
    QString     units;
    QString     createMatchDescription();

    SearchTopic searchTopic = ST_NONE;
    ChannelID   channelId = 0;
    OpCode      operationOpCode = OP_INVALID;

    ValueMode   valueMode;
    qint32      foundValue ;
    QString     foundString;
    bool        minMaxValid  = false;
    qint32      minInteger ;
    qint32      maxInteger ;
    qint32      compareValue=0;

    void        updateMinMaxValues(qint32 value) ;
    bool        compare(int,int);
    bool        compare(QString aa , QString bb);
    QRegularExpression     searchPatterToRegex (QString searchPattern);
    QString     formatTime (qint32) ;
    int         nextTab;
    QString     valueToString(int value, QString empty = "");
    double      maxDouble;
    double      minDouble;
    QString     label;

    private:
    bool valid=false;
};

class Matches
{
public:
    Matches() ;
    ~Matches() ;
    Match* addMatch();
    Match* reset() {clear();return addMatch();};
    void clear() ;
    Match* at(int offset) {
        if (offset<0 || offset>= size()) return empty();
        return matchList[offset];
    };
    Match* empty();
    int size() {return inuse;};
private:
    int inuse = 0;
    int created() {return matchList.size();};
    Match _empty = Match();
    QList<Match*>  matchList;
};

class DailySearchTab : public QWidget
{
	Q_OBJECT
public:
    DailySearchTab ( Daily* daily , QWidget* ,  QTabWidget* ) ;
    ~DailySearchTab();
    void updateEvents(ChannelID id,QString fullname);

private:
    enum STATE { reset = 0 ,
    waitForSearchTopic   = 1 ,
    matching = 2 ,
    multpileMatches =3 ,
    waitForStart =4 ,
    autoStart =5 ,
    searching =6 ,
    endOfSeaching =7 ,
    waitForContinue =8 ,
    noDataFound =9 };
	STATE state = waitForSearchTopic;

    QString red =  "#ff8080";
    QString green="#80ff80";
    QString grey= "#c0c0c0";
    QString blue= "#8080ff";

    // these values are hard coded. because dynamic translation might not do the proper assignment.
    // Dynamic code is commented out using c preprocess #if #endif
    const int          TW_NONE       = -1;
    const int          TW_DETAILED   =  0;
    const int          TW_EVENTS     =  1;
    const int          TW_NOTES      =  2;
    const int          TW_BOOKMARK   =  3;
    const int          TW_SEARCH     =  4;

    const int     dateRole = Qt::UserRole;
    const int     valueRole = 1+Qt::UserRole;
    const int     passDisplayLimit = 30;
    const int     stringDisplayLen = 80;

    Daily*        daily;
    QWidget*      parent;
    QWidget*      searchTabWidget;
    QTabWidget*   dailyTabWidget;
    QVBoxLayout*  searchTabLayout;

    QTableWidget* resultTable;

    QFrame*       cmdDescList;
    quint32       cmdDescLabelsUsed = 0;
    QVBoxLayout*  cmdDescLayout;
    QVector<QLabel*> cmdDescLabels;
    QLabel*       getCmdDescLabel();
    void          clrCmdDescList();

    // start Widget
    QWidget*      startWidget;
    QHBoxLayout*  startLayout;
    QPushButton*  matchButton;
    QPushButton*  clearButton;
    QPushButton*  startButton;
    QPushButton*  addMatchButton;

    // Command command Widget
    QWidget*      commandWidget;
    QHBoxLayout*  commandLayout;

    QPushButton*  helpButton;
    QTextEdit*    helpText;

    QProgressBar* progressBar;

    // control Widget

    QWidget*      summaryWidget;
    QHBoxLayout*  summaryLayout;

    // Command Widget
    QListWidget*  commandList;
    QButtonGroup* buttonGroup;
    QAbstractButton* lastButton = nullptr;
    qint32        lastTopic = ST_NONE;
    QPushButton*  commandButton;
    QComboBox*    operationCombo;
    QPushButton*  operationButton;
    QLabel*       selectUnits;
    QDoubleSpinBox* selectDouble;
    QSpinBox*     selectInteger;
    QLineEdit*    selectString;

    QPushButton*  summaryProgress;
    QPushButton*  summaryFound;
    QPushButton*  summaryMinMax;

    QIcon*        m_icon_selected;
    QIcon*        m_icon_notSelected;
    QIcon*        m_icon_configure;

    QMap <QString,OpCode> opCodeMap;
    QString     opCodeStr(OpCode);
    QVector<ChannelID> apneaLikeChannels;

    bool        helpMode=false;
    QString     helpString = helpStr();
    void        clearMatch();
    void        setState(STATE);
    void        createUi();
    void        populateControl();
    void        initApneaLikeChannels();
    QSize       setText(QPushButton*,QString);
    QSize       setText(QLabel*,QString);
    QSize       textsize(QFont font ,QString text);
    void        setColor(QPushButton*,QString);
    Matches     matches;
    Match*      match = matches.empty();
    void        search(QDate date);
    void        find(QDate&);
    bool        matchFind(Match* myMatch ,Day* day,QDate& date , Qt::Alignment& alignment);
    void        criteriaChanged();
    void        endOfPass();
    void        displayStatistics();
    void        clearStatistics();
    void        setResult(int row,int column,QDate date,QString value);


    void        addItem(QDate date, QString value, Qt::Alignment alignment);
    void        setCommandPopupEnabled(bool );
    void        setOperationPopupEnabled(bool );
    void        setOperation( );
    void        hideResults(bool);
    void        hideCommand(bool showcommand=false);
    void        showOnlyAhiChannels(bool);
    void        connectUi(bool);


    QString     helpStr();
    QString     centerLine(QString line);
    QRegularExpression     searchPatterToRegex (QString wildcard);
    void        addCommandItem(QString str,int topic);
    float       commandListItemMaxWidth = 0;
    float       commandListItemHeight = 0;
    QSet<QString> commandEventList;

    EventDataType calculateAhi(Day* day);

    bool        createUiFinished=false;
    bool        startButton_1stPass = true;
    bool        commandPopupEnabled=false;

    QDate       earliestDate ;
    QDate       latestDate ;
    QDate       nextDate;

    //
    int         daysTotal;
    int         daysSkipped;
    int         daysProcessed;
    int         daysFound;
    int         passFound;
    int         DaysWithFileErrors;

    void        setoperation(OpCode opCode,ValueMode mode) ;

void process_match_info(QString text, int topic);

public slots:
private slots:
    void on_matchGroupButton_toggled(QAbstractButton* );
    void on_startButton_clicked();
    void on_clearButton_clicked();
    void on_matchButton_clicked();
    void on_addMatchButton_clicked();
    void on_helpButton_clicked();

    void on_commandButton_clicked();
    void on_operationButton_clicked();

    void on_operationCombo_activated(int index);

    void on_intValueChanged(int);
    void on_doubleValueChanged(double);
    void on_textEdited(QString);

    void on_activated(GPushButton*);
};


class GPushButton : public QPushButton
{
	Q_OBJECT
public:
    GPushButton (int,int,QDate,DailySearchTab* parent, ChannelID code=0, quint32 offset=0);
    virtual ~GPushButton();
    int row() { return _row;};
    int column() { return _column;};
    QDate date() { return _date;};
    ChannelID code() {return _code;};
    quint32 offset() {return _offset;};
    void setDate(QDate date) {_date=date;};
    void setChannelId(ChannelID code) {_code=code;};
    void setOffset(quint32 offset) {_offset=offset;};
private:
    const DailySearchTab* _parent;
    const int _row;
    const int _column;
    QDate _date;
    ChannelID _code;
    quint32 _offset;
signals:
    void activated(GPushButton*);
public slots:
    void on_clicked();
};

#endif // SEARCHDAILY_H

