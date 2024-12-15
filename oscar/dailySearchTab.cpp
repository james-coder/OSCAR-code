/* user graph settings Implementation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

// Issues no clear of results between runs or between passes.
// automatically open event. in graph view for events TAB

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QProgressBar>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPixmap>
#include <QSize>
#include <QChar>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QtGlobal>
#include <QHeaderView>
#include <QCoreApplication>
#include "dailySearchTab.h"
#include "SleepLib/day.h"
#include "SleepLib/profiles.h"
#include "SleepLib/journal.h"
#include "daily.h"
#include "SleepLib/machine_common.h"

enum DS_COL { DS_COL_LEFT=0, DS_COL_RIGHT, DS_COL_MAX };
enum DS_ROW{ DS_ROW_HEADER, DS_ROW_DATA };
#define DS_ROW_MAX         (passDisplayLimit+DS_ROW_DATA)


/*
 *
 *


clear button
	CLear button is always enabled and visible.
	resets everything
	next state: Match State
match button
	open list of items to search
	next state:Wait for start
	next state:Searching
		Same actions taken by clicking on Start Button.
Start Button
	Starts or continues searching
	disabled during searching
	next state:Searching
Another Match Button	
	dispalys green if space available. Red if no more space.
	disabled during searching
	Saves Current match allowing for multiple  matches
	Display saved match.
	State Not Changed.
	open list of items to search
Results
	column 1 (left most) loads date
	column 2 (right most) loads date  & open a different tab (Detailed, Events, Notes, Bookmark)
	mark the check item selected. Can re-executed if necessary
	
States for search.
0) init state
	goto ready to Match state
1) Match State (waitForSearchParameters)
	Always Enabled except when searching. if searching then nextState is init;
	start  Button disabled && visible
	AnotheMatch Button Visible with already saved matches
	clear button
	match selection for what to find
	clears selections.
3) waitForStart allows match ,operation and value to be set
	 if no operation  or value is required then nextState:Searching
	Allows changing match, operation, or value.
	 Start && anotherMatch Buttons are enabled and green
     next state: searching	
4) searching
	updates progress bar.
	Updates result table.
	 Start && anotherMatch Buttons are disabled and gray.
	next states : endOfSearch  or Wait for Continuel
5) end of seaching
	 Start Button is red disables EndOfSearch.
	 anotherMatch Buttons is disabled and display red
	 next State : match.
6) WaitForContinue;
	 AnotherMatch is disabled (gtrayed out)
	 Start button displays continue search
	 Result Table is enabled.
	Start  Button is enabled & green
	AnotherMatch button is diabled.
	NextState: searching




 */

/* layout  of searchTAB
+========================+
|           HELP         |
+========================+
|       HELPText         |
+========================+
| Match | Clear | start  |
|------------------------|
| control:cmd op value   |
+========================+
| saved cmd op values    |
+========================+
|     Progress Bar       |
+========================+
|          Summary       |
+========================+
|          RESULTS       |
+========================+
*/


DailySearchTab::DailySearchTab(Daily* daily , QWidget* searchTabWidget  , QTabWidget* dailyTabWidget)  :
                        daily(daily) , parent(daily) , searchTabWidget(searchTabWidget) ,dailyTabWidget(dailyTabWidget)
{
    m_icon_selected      = new QIcon(":/icons/checkmark.png");
    m_icon_notSelected   = new QIcon(":/icons/empty_box.png");
    m_icon_configure     = new QIcon(":/icons/cog.png");

    createUi();
    connectUi(true);
}

DailySearchTab::~DailySearchTab() {
    connectUi(false);
    delete m_icon_selected;
    delete m_icon_notSelected;
    delete m_icon_configure ;
};

void    DailySearchTab::createUi() {

        searchTabLayout  = new QVBoxLayout(searchTabWidget);

        resultTable      = new QTableWidget(DS_ROW_MAX,DS_COL_MAX,searchTabWidget);

        helpButton       = new QPushButton(searchTabWidget);
        helpText         = new QTextEdit(searchTabWidget);

        startWidget      = new QWidget(searchTabWidget);
        startLayout      = new QHBoxLayout;
        matchButton      = new QPushButton( startWidget);
        addMatchButton   = new QPushButton( startWidget);
        clearButton      = new QPushButton( startWidget);
        startButton      = new QPushButton( startWidget);


        commandWidget    = new QWidget(searchTabWidget);
        commandLayout    = new QHBoxLayout();
        commandButton    = new QPushButton(commandWidget);
        buttonGroup      = new QButtonGroup(commandWidget);
        operationCombo   = new QComboBox(commandWidget);
        operationButton  = new QPushButton(commandWidget);
        selectDouble     = new QDoubleSpinBox(commandWidget);
        selectInteger    = new QSpinBox(commandWidget);
        selectString     = new QLineEdit(commandWidget);
        selectUnits      = new QLabel(commandWidget);

        commandList      = new QListWidget(resultTable);

        cmdDescList      = new QFrame(searchTabWidget);
        cmdDescLayout    = new QVBoxLayout(cmdDescList);
        cmdDescLabelsUsed = 0;

        summaryWidget    = new QWidget(searchTabWidget);
        summaryLayout    = new QHBoxLayout();
        summaryProgress  = new QPushButton(summaryWidget);
        summaryFound     = new QPushButton(summaryWidget);
        summaryMinMax    = new QPushButton(summaryWidget);
        progressBar      = new QProgressBar(searchTabWidget);

        populateControl();

        searchTabLayout->addSpacing(2);
        searchTabLayout->setContentsMargins(2, 2, 2, 2);

        startLayout->addWidget(matchButton);
        startLayout->addWidget(addMatchButton);
        startLayout->addWidget(clearButton);
        startLayout->addWidget(startButton);
        startLayout->addStretch(0);
        startLayout->addSpacing(2);
        startLayout->setContentsMargins(2, 2, 2, 2);
        startWidget->setLayout(startLayout);

        commandLayout->addWidget(commandButton);
        commandLayout->addWidget(operationCombo);
        commandLayout->addWidget(operationButton);
        commandLayout->addWidget(selectInteger);
        commandLayout->addWidget(selectString);
        commandLayout->addWidget(selectDouble);
        commandLayout->addWidget(selectUnits);

        commandLayout->setContentsMargins(2, 2, 2, 2);
        commandLayout->setSpacing(2);
        commandLayout->addStretch(0);
        commandWidget->setLayout(commandLayout);

        summaryLayout->addWidget(summaryProgress);
        summaryLayout->addWidget(summaryFound);
        summaryLayout->addWidget(summaryMinMax);

        summaryLayout->setContentsMargins(2, 2, 2, 2);
        summaryLayout->setSpacing(2);
        summaryWidget->setLayout(summaryLayout);


        QString styleSheetWidget = QString("border: 1px solid black; padding:none;");
        startWidget->setStyleSheet(styleSheetWidget);
        searchTabLayout ->addWidget(helpButton);
        searchTabLayout ->addWidget(helpText);
        searchTabLayout ->addWidget(startWidget);
        searchTabLayout ->addWidget(commandWidget);
        searchTabLayout ->addWidget(commandList);
        searchTabLayout ->addWidget(cmdDescList);
        searchTabLayout ->addWidget(progressBar);
        searchTabLayout ->addWidget(summaryWidget);
        searchTabLayout ->addWidget(resultTable);

        // End of UI creatation
        //Setup each BUtton / control item

        QString styleButton=QString(
            "QPushButton { color: black; border: 1px solid black; padding: 5px ;background-color:white; }"
            "QPushButton:disabled { background-color: #EEEEFF;}"
            //"QPushButton:disabled { color: #333333; border: 1px solid #333333; background-color: #dddddd;}"
            );
        QSizePolicy sizePolicyEP = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        QSizePolicy sizePolicyEE = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
        QSizePolicy sizePolicyEM = QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
        QSizePolicy sizePolicyMM = QSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        QSizePolicy sizePolicyMxMx = QSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
        Q_UNUSED (sizePolicyEP);
        Q_UNUSED (sizePolicyMM);
        Q_UNUSED (sizePolicyMxMx);

        helpText->setReadOnly(true);
        helpText->setLineWrapMode(QTextEdit::NoWrap);
        QSize size = QFontMetrics(this->font()).size(0, helpString);
        size.rheight() += 35 ; // scrollbar
        size.rwidth()  += 35 ; // scrollbar
        helpText->setText(helpString);
        helpText->setMinimumSize(textsize(this->font(),helpString));
        helpText->setSizePolicy( sizePolicyEE );

        helpButton->setStyleSheet( styleButton );
        // helpButton->setText(tr("Help"));
        helpMode = true;
        on_helpButton_clicked();

        matchButton->setIcon(*m_icon_configure);
        matchButton->setStyleSheet( styleButton );
        clearButton->setStyleSheet( styleButton );
        addMatchButton->setStyleSheet( styleButton );
        setText(matchButton,tr("Match"));
        setText(clearButton,tr("Clear"));


        commandButton->setStyleSheet("border:none;");

        float height = float(commandList->count())*commandListItemHeight ;
        commandList->setMinimumHeight(height);
        commandList->setMinimumWidth(commandListItemMaxWidth);
        setCommandPopupEnabled(false);

        cmdDescLayout->addStretch(5);
        cmdDescLayout->setSpacing(0);
        cmdDescLayout->setContentsMargins(0,0,0,0);
        cmdDescList->setLayout(cmdDescLayout);
        cmdDescList->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
        cmdDescList->show();

        setText(operationButton,"");
        operationButton->setStyleSheet("border:none;");
        operationButton->hide();
        operationCombo->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setOperationPopupEnabled(false);

        selectDouble->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectInteger->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectString->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        selectUnits->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
        setText(selectUnits,"");

        progressBar->setStyleSheet(
            "QProgressBar{border: 1px solid black; text-align: center;}"
            "QProgressBar::chunk { border: none; background-color: #ccddFF; } ");


        summaryProgress->setStyleSheet( styleButton );
        summaryFound->setStyleSheet( styleButton );
        summaryMinMax->setStyleSheet( styleButton );
        summaryProgress->setSizePolicy( sizePolicyEM ) ;
        summaryFound->setSizePolicy( sizePolicyEM ) ;
        summaryMinMax->setSizePolicy( sizePolicyEM ) ;

        resultTable->horizontalHeader()->hide();   // hides numbers above each column
        resultTable->horizontalHeader()->setStretchLastSection(true);
        // get rid of selection coloring
        resultTable->setStyleSheet("QTableView{background-color: white; selection-background-color: white; }");

        float width = 14/*styleSheet:padding+border*/ + QFontMetrics(this->font()).size(Qt::TextSingleLine ,"WWW MMM 99 2222").width();
        resultTable->setColumnWidth(DS_COL_LEFT, width);

        width = 30/*iconWidthPlus*/+QFontMetrics(this->font()).size(Qt::TextSingleLine ,clearButton->text()).width();
        //width = clearButton->width();
        resultTable->setShowGrid(false);


        setResult(DS_ROW_HEADER,0,QDate(),tr("DATE\nJumps to Date"));
        setState( reset );
}

void DailySearchTab::connectUi(bool doConnect) {
    if (doConnect) {
        daily->connect(startButton,     SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
        daily->connect(buttonGroup,     SIGNAL(buttonReleased(QAbstractButton*)), this, SLOT(on_matchGroupButton_toggled(QAbstractButton*)));
        daily->connect(clearButton,     SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
        daily->connect(matchButton,     SIGNAL(clicked()), this, SLOT(on_matchButton_clicked()) );
        daily->connect(addMatchButton,  SIGNAL(clicked()), this, SLOT(on_addMatchButton_clicked()) );
        daily->connect(helpButton   ,   SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );

        daily->connect(commandButton,   SIGNAL(clicked()), this, SLOT(on_commandButton_clicked()) );
        daily->connect(operationButton, SIGNAL(clicked()), this, SLOT(on_operationButton_clicked()) );
        daily->connect(operationCombo,  SIGNAL(activated(int)), this, SLOT(on_operationCombo_activated(int)   ));

        daily->connect(selectInteger,   SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
        daily->connect(selectDouble,    SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
        daily->connect(selectString,    SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );

    } else {
        daily->disconnect(startButton,     SIGNAL(clicked()), this, SLOT(on_startButton_clicked()) );
        daily->disconnect(buttonGroup,     SIGNAL(buttonReleased(QAbstractButton*)), this, SLOT(on_matchGroupButton_toggled(QAbstractButton*)));
        daily->disconnect(clearButton,     SIGNAL(clicked()), this, SLOT(on_clearButton_clicked()) );
        daily->disconnect(matchButton,     SIGNAL(clicked()), this, SLOT(on_matchButton_clicked()) );
        daily->disconnect(addMatchButton,  SIGNAL(clicked()), this, SLOT(on_addMatchButton_clicked()) );
        daily->disconnect(helpButton   ,   SIGNAL(clicked()), this, SLOT(on_helpButton_clicked()) );

        daily->disconnect(commandButton,   SIGNAL(clicked()), this, SLOT(on_commandButton_clicked()) );
        daily->disconnect(operationButton, SIGNAL(clicked()), this, SLOT(on_operationButton_clicked()) );
        daily->disconnect(operationCombo,  SIGNAL(activated(int)), this, SLOT(on_operationCombo_activated(int)   ));

        daily->disconnect(selectInteger,   SIGNAL(valueChanged(int)), this, SLOT(on_intValueChanged(int)) );
        daily->disconnect(selectDouble,    SIGNAL(valueChanged(double)), this, SLOT(on_doubleValueChanged(double)) );
        daily->disconnect(selectString,    SIGNAL(textEdited(QString)), this, SLOT(on_textEdited(QString)) );

    }
}

void DailySearchTab::addCommandItem(QString str,int topic) {
    float scaleX= (float)(QWidget::logicalDpiX()*100.0)/(float)(QWidget::physicalDpiX());
    float percentX = scaleX/100.0;
    float width = QFontMetricsF(this->font()).size(Qt::TextSingleLine , str).width();
    width += 30 ; // account for scrollbar width;
    commandListItemMaxWidth = max (commandListItemMaxWidth, (width*percentX));
    commandListItemHeight = QFontMetricsF(this->font()).size(Qt::TextSingleLine , str).height();
    QAbstractButton* topicButton ;
    //topicButton = new QCheckBox(str,commandList);
    topicButton = new QRadioButton(str,commandList);
    buttonGroup->addButton(topicButton,topic);

    QListWidgetItem* item = new QListWidgetItem(commandList);
    item->setData(Qt::UserRole,topic);
    commandList->setItemWidget(item, topicButton);
}

void DailySearchTab::showOnlyAhiChannels(bool ahiOnly) {
    for (int ind = 0; ind <commandList->count() ; ++ind) {
        QListWidgetItem *item = commandList->item(ind);
        QAbstractButton* topicButton = dynamic_cast<QAbstractButton*>(commandList->itemWidget(item));
        if (!topicButton) continue;
        int topic = buttonGroup->id(topicButton);
        if (apneaLikeChannels.contains((ChannelID)topic)) {
            item->setHidden(false);
        } else {
            if (topic == ST_CLEAR) {
                topicButton->setChecked(true);
                item->setHidden(true);
            } else if (topic == ST_APNEA_ALL) {
               item->setHidden(!ahiOnly);
            } else {
                item->setHidden(ahiOnly);
            }
        }
    }
    if (!ahiOnly) {
        lastButton = nullptr;
        lastTopic = ST_NONE ;
    };
};

void DailySearchTab::on_matchGroupButton_toggled(QAbstractButton* topicButton ) {
    if (topicButton) {
        int topic = buttonGroup->id(topicButton);
        if (lastTopic == ST_APNEA_LENGTH ) {
            //menu was only ahi channels
            apneaLikeChannels.clear();
            if (topic == ST_APNEA_ALL ) {
                initApneaLikeChannels();
            } else {
                // topic is ChannelIDA
                apneaLikeChannels.push_back(topic);
            }
            process_match_info(lastButton->text(), ST_APNEA_LENGTH );
            return;
        }
        lastButton = topicButton;
        lastTopic = buttonGroup->id(topicButton);
        if (lastTopic == ST_APNEA_LENGTH ) {
            initApneaLikeChannels();
            showOnlyAhiChannels(true);
            return;
        }
    } else {
        return;
    }
    process_match_info(lastButton->text(), lastTopic );
}

void DailySearchTab::initApneaLikeChannels() {
    apneaLikeChannels = QVector<ChannelID>(ahiChannels);
    if (p_profile->cpap->userEventFlagging()) {
        apneaLikeChannels.push_back(CPAP_UserFlag1);
        apneaLikeChannels.push_back(CPAP_UserFlag2);
    }
}

void DailySearchTab::updateEvents(ChannelID id,QString fullname) {
    if (commandEventList.contains(fullname)) return;
    commandEventList.insert(fullname);
    addCommandItem(fullname,id);
}

void DailySearchTab::populateControl() {
        opCodeMap.clear();
        opCodeMap.insert( opCodeStr(OP_LT),OP_LT);
        opCodeMap.insert( opCodeStr(OP_GT),OP_GT);
        opCodeMap.insert( opCodeStr(OP_LE),OP_LE);
        opCodeMap.insert( opCodeStr(OP_GE),OP_GE);
        opCodeMap.insert( opCodeStr(OP_EQ),OP_EQ);
        opCodeMap.insert( opCodeStr(OP_NE),OP_NE);

        // The order here is the order in the popup box
        operationCombo->clear();
        operationCombo->addItem(opCodeStr(OP_LT));
        operationCombo->addItem(opCodeStr(OP_GT));
        operationCombo->addItem(opCodeStr(OP_LE));
        operationCombo->addItem(opCodeStr(OP_GE));
        operationCombo->addItem(opCodeStr(OP_EQ));
        operationCombo->addItem(opCodeStr(OP_NE));

        // Now add events
        commandList->clear();
        addCommandItem(tr("Journal"),ST_JOURNAL);
        addCommandItem(tr("Notes"),ST_NOTES);
        addCommandItem(tr("Notes containing"),ST_NOTES_STRING);
        addCommandItem(tr("Bookmarks"),ST_BOOKMARKS);
        addCommandItem(tr("Bookmarks containing"),ST_BOOKMARKS_STRING);
        addCommandItem(tr("AHI "),ST_AHI);
        addCommandItem(tr("Daily Duration"),ST_DAILY_USAGE);
        addCommandItem(tr("Session Duration" ),ST_SESSION_LENGTH);
        addCommandItem(tr("Days Skipped"),ST_DAYS_SKIPPED);
        addCommandItem(tr("Apnea Length"),ST_APNEA_LENGTH);
        qint32 chans = schema::SPAN | schema::FLAG | schema::MINOR_FLAG;
        if ( !p_profile->cpap->clinicalMode() ) {
            addCommandItem(tr("Disabled Sessions"),ST_DISABLED_SESSIONS);
        }
        addCommandItem(tr("Number of Sessions"),ST_SESSIONS_QTY);

        QDate date = p_profile->LastDay(MT_CPAP);
        if ( !date.isValid()) return;
        Day* day = p_profile->GetDay(date);
        if (!day) return;

        if (p_profile->general->showUnknownFlags()) chans |= schema::UNKNOWN;
        QList<ChannelID> available;
        available.append(day->getSortedMachineChannels(chans));

        for (int i=0; i < available.size(); ++i) {
            ChannelID id = available.at(i);
            schema::Channel chan = schema::channel[ id  ];
            updateEvents(id,chan.fullname());
        }
        // these must be at end
        addCommandItem(tr("All Apnea"),ST_APNEA_ALL);
        addCommandItem("CLEAR",ST_CLEAR);
}

void    DailySearchTab::setState(STATE newState) {
            STATE prev=state;
            state = newState;
            //enum STATE { reset , waitForSearchTopic   ,  matching , multpileMatches , waitForStart ,  autoStart , searching ,  endOfSeaching ,  waitForContinue , noDataFound};
            switch (state) {
                case multpileMatches :
                    break;
                case matching :
                    if (prev == multpileMatches) {
                        matchButton->show();
                        addMatchButton->hide();
                    } else {
                        setState(reset);
                    }
                    break;
                case reset :
                    clrCmdDescList();
                    match = matches.reset();
                    clearMatch() ;
                    setState(waitForSearchTopic);
                    break;
                case waitForSearchTopic :
                    setColor(matchButton,green);
                    matchButton->show();
                    addMatchButton->hide();
                    startButton->hide();
                    hideResults(true);
                    progressBar->hide();
                    summaryWidget->hide();
                    setCommandPopupEnabled(false);
                    startButton_1stPass=true;
                    showOnlyAhiChannels(false);
                    break;
                case waitForStart :
                    matchButton->hide();
                    addMatchButton->show();
                    startButton->show();
                    break;
                case autoStart :
                    break;
                case searching :
                    //if (prev == searching) break;
                    matchButton->hide();
                    addMatchButton->hide();
                    hideResults(true);
                    progressBar->show();
                    summaryWidget->show();
                    break;
                case waitForContinue :
                    startButton->setEnabled(true);
                    startButton_1stPass=false;
                    setText(startButton,(tr("Continue Search")));
                    setColor(startButton,green);
                    matchButton->hide();
                    addMatchButton->hide();
                    hideResults(false);
                    break;
                case endOfSeaching :
                    startButton->setEnabled(false);
                    setText(startButton,tr("End of Search"));
                    setColor(startButton,red);
                    matchButton->show();
                    addMatchButton->hide();
                    hideResults(false);
                    break;
                case noDataFound :
                    startButton->setEnabled(false);
                    setText(startButton,tr("No Matches"));
                    setColor(startButton,red);
                    matchButton->show();
                    addMatchButton->hide();
                    hideResults(true);
                    break;
            };
        };
// QRegexp is obsoluted.
QRegularExpression Match::searchPatterToRegex (QString searchPattern) {

        const static QChar bSlash('\\');
        const static QChar asterisk('*');
        const static QChar qMark('?');
        const static QChar emptyChar('\0');
        const QString emptyStr("");
        const QString anyStr(".*");
        const QString singleStr(".");


        searchPattern = searchPattern.simplified();
        //QString wilDebug = searchPattern;
        //
        // wildcard searches uses  '*' , '?' and '\'
        // '*' will match zero or more characters. '?' will match one character. the escape character '\' always matches the next character.
        //  '\\' will match '\'. '\*' will match '*'. '\?' mach for '?'. otherwise the '\' is ignored

        // The user request will be mapped into a valid QRegularExpression. All RegExp meta characters in the request must be handled.
        // Most of the meta characters will be escapped. backslash, asterisk, question mark will be treated.
        // '\*' -> '\*'
        // '\?' -> '\?'
        // '*'  -> '.*'      // really  asterisk followed by asterisk or questionmark -> as a single asterisk.
        // '?'  -> '.'
        // '.'  -> '\.'  // default for all other  meta characters.
        // '\\' -> '[\\]'

        // QT documentation states regex reserved characetrs are $ () * + . ? [ ] ^ {} |   // seems to be missing / \ -
        // Regular expression reserved characters / \ [ ] () {} | + ^ . $ ? * -


        static const QString metaClass = QString( "[ / \\\\ \\[ \\]  ( ) { } | + ^ . $ ? * - ]").replace(" ",""); // slash,bSlash,[,],(,),{,},+,^,.,$,?,*,-,|
        static const QRegularExpression metaCharRegex(metaClass);
        #if 0
        // Verify search pattern
        if (!metaCharRegex.isValid()) {
            return QRegularExpression();
        }
        #endif

        // regex meta characetrs. all regex meta character must be acounts to us regular expression to make wildcard work.
        // they will be escaped. except for * and ? which will be treated separately
        searchPattern = searchPattern.simplified();               // remove witespace at ends. and multiple white space to a single space.

        // now handle each meta character requested.
        int pos=0;
        int len=1;
        QString replace;
        QChar metaChar;
        QChar nextChar;
        while (pos < (len = searchPattern.length()) ) {
            pos = searchPattern.indexOf(metaCharRegex,pos);
            if (pos<0) break;
            metaChar = searchPattern.at(pos);
            if (pos+1<len) {
                nextChar = searchPattern.at(pos+1);
            } else {
                nextChar = emptyChar;
            }

            int replaceCount = 1;
            if (metaChar == asterisk ){
                int next=pos+1;
                // discard any successive wildcard type of characters.
                while ( (nextChar == asterisk ) || (nextChar == qMark ) ) {
                    searchPattern.remove(next,1);
                    len = searchPattern.length();
                    if (next>=len) break;
                    nextChar = searchPattern.at(next);
                }
                replace = anyStr;            // if asterisk then write dot asterisk
            } else if (metaChar == qMark ) {
                replace = singleStr;
            } else {
                if ((metaChar == bSlash ) ) {
                    if ( ((nextChar == bSlash ) || (nextChar == asterisk ) || (nextChar == qMark ) ) )  {
                        pos+=2; continue;
                    }
                    replace = emptyStr;         //match next character. same as deleteing the backslash
                } else {
                    // Now have a regex reserved character that needs escaping.
                    // insert an escape '\' before meta characters.
                    replace = QString("\\%1").arg(metaChar);
                }
            }
            searchPattern.replace(pos,replaceCount,replace);
            pos+=replace.length();  // skip over characters added.
        }


        // from AI
        // QRegex convertedRegex =QRegex(searchPattern,Qt::CaseInsensitive, QRegex::RegExp);
        // QRegularExpression convertedRegex(QRegularExpression::escape(searchPattern), QRegularExpression::CaseInsensitiveOption);

        // searchPattern = QString("^.*%1.*$").arg(searchPattern);       // add asterisk to end end points.
        QRegularExpression convertedRegex =QRegularExpression(searchPattern,QRegularExpression::CaseInsensitiveOption);
        // verify convertedRegex to use
        if (!convertedRegex.isValid()) {
            qWarning() << QFileInfo( __FILE__).baseName() <<"[" << __LINE__ << "] " <<  convertedRegex.errorString() << convertedRegex ;
            return QRegularExpression();
        }
        return convertedRegex;
}

bool    Match::compare(QString find , QString target)
{
        OpCode opCode = operationOpCode;
        bool ret=false;
        if (opCode==OP_CONTAINS) {
            ret =  target.contains(find,Qt::CaseInsensitive);
        } else if (opCode==OP_WILDCARD) {
            QRegularExpression regex =  searchPatterToRegex(find);
            ret =  target.contains(regex);
        }
        return ret;
}

bool    Match::compare(int aa , int bb)
{
        // OP_INVALID=0 , OP_LT=1 , OP_GT=2 , OP_NE=3 , OP_EQ=4 , OP_LE=5 , OP_GE=6 , OP_END_NUMERIC ,
        OpCode opCode = operationOpCode;
        if (opCode>=OP_END_NUMERIC) return false;
        int mode=0;
        if (aa <bb ) {
            mode |= OP_LT;
        } else if (aa >bb ) {
            mode |= OP_GT;
        } else {
            mode |= OP_EQ;
        }
        bool result = ( (mode & (int)opCode)!=0);
        return result;
}

QString Match::valueToString(int value, QString defaultValue) {
        switch (valueMode) {
            case hundredths :
                return QString("%1").arg( (double(value)/100.0),0,'f',2);
                break;
            case hoursToMs:
            case minutesToMs:
                return formatTime(value);
                break;
            case displayWhole:
            case opWhole:
                return QString("%1").arg(value);
                break;
            case secondsDisplayString:
            case displayString:
            case opString:
                return foundString;
                break;
            case invalidValueMode:
            case notUsed:
                break;
        }
        return defaultValue;
}

QSize DailySearchTab::setText(QLabel* label ,QString text) {
        QSize size = textsize(label->font(),text);
        int width = size.width();
        width += 20 ; //margings
        label->setMinimumWidth(width);
        label->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        label->setText(text);
        return size;
}

QSize DailySearchTab::setText(QPushButton* but ,QString text) {
        QSize size = textsize(but->font(),text);
        int width = size.width();
        width += but->iconSize().width();
        width += 4 ; //margings
        but->setMinimumWidth(width);
        but->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
        but->setText(text);
        return size;
}

void DailySearchTab::setResult(int row,int column,QDate date,QString text) {
    if(column<0 || column>1) {
        return;
    } else if ( row < DS_ROW_HEADER || row >= DS_ROW_MAX)  {
        return;
    }

    QWidget* header = resultTable->cellWidget(row,column);
    GPushButton* item;
    if (header == nullptr) {
        item = new GPushButton(row,column,date,this);
        //item->setStyleSheet("QPushButton {text-align: left;vertical-align:top;}");
        item->setStyleSheet(
            "QPushButton { text-align: left;color: black; border: 1px solid black; padding: 5px ;background-color:white; }" );
        resultTable->setCellWidget(row,column,item);
    } else {
        item = dynamic_cast<GPushButton *>(header);
        if (item == nullptr) {
            return;
        }
        item->setDate(date);
    }
    if (row == DS_ROW_HEADER) {
        QSize size=setText(item,text);
        resultTable->setRowHeight(DS_ROW_HEADER,8/*margins*/+size.height());
        return; // skip make row visible if header. will be made visible with hideResults method
    } else {
        item->setIcon(*m_icon_notSelected);
        if (column == 0) {
            setText(item,date.toString());
        } else {
            setText(item,text);
        }
    }
    if ( row == DS_ROW_DATA )  {
        resultTable->setRowHidden(DS_ROW_HEADER,false);
    }
    resultTable->setRowHidden(row,false);
}

void Match::updateMinMaxValues(qint32 value)
{
        foundValue = value;
        if (!minMaxValid ) {
            minMaxValid = true;
            minInteger = value;
            maxInteger = value;
        } else if (  value < minInteger ) {
            minInteger = value;
        } else if (  value > maxInteger ) {
            maxInteger = value;
        }
}

bool DailySearchTab::matchFind(Match* myMatch ,Day* day, QDate& date, Qt::Alignment& alignment) {
        bool found=false;
        switch (myMatch->searchTopic) {
            case ST_DAYS_SKIPPED :
                found=!day;
                break;
            case ST_DISABLED_SESSIONS :
                {
                qint32 numDisabled=0;
                QList<Session *> sessions = day->getSessions(MT_CPAP,true);
                for (auto & sess : sessions) {
                    if (!sess->enabled()) {
                        numDisabled ++;
                        found=true;
                    }
                }
                myMatch->updateMinMaxValues(numDisabled);
                }
                break;
            case ST_JOURNAL :
                {
                Session* journal=daily->GetJournalSession(date,false);
                if (journal) {
                    myMatch->foundString = journal->settings[LastUpdated].toDateTime().toString();
                    myMatch->foundString.append(" ");
                    bool empty =true;
                    if ( journal->settings.contains(Bookmark_Start)  ) {
                        if (journal->settings[Bookmark_Start].toList().size()>0) {
                            empty = false;
                            myMatch->foundString.append("B");
                            /* Not reuired.
                            if ( journal->settings.contains(Bookmark_Notes)  ) {
                                if (journal->settings[Bookmark_Start].toList().size()>0) {
                                    myMatch->foundString.append("n");
                                }
                            }
                            */
                        }
                    }
                    if ( journal->settings.contains(Journal_Notes) ) {
                        myMatch->foundString.append("N");
                        empty = false;
                    }
                    if ( journal->settings.contains(Journal_Weight) ) {
                        myMatch->foundString.append("W");
                        empty = false;
                    }
                    if ( journal->settings.contains(Journal_ZombieMeter) ) {
                        myMatch->foundString.append("F");
                        empty = false;
                    }
                    if (empty) {
                        break;
                        myMatch->foundString.append(tr("Empty"));
                    }
                    found=true;
                    alignment=Qt::AlignLeft;
                }
                }
                break;
            case ST_NOTES :
                {
                Session* journal=daily->GetJournalSession(date,false);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = Daily::convertHtmlToPlainText(journal->settings[Journal_Notes].toString());
                    myMatch->foundString = jcontents.simplified().left(stringDisplayLen).simplified();
                    found=true;
                    alignment=Qt::AlignLeft;
                }
                }
                break;
            case ST_BOOKMARKS :
                {
                Session* journal=daily->GetJournalSession(date,false);
                if (journal && journal->settings.contains(Bookmark_Notes)) {
                    found=true;
                    QStringList notes = journal->settings[Bookmark_Notes].toStringList();
                    for (   const auto & note : notes) {
                       myMatch->foundString = note.simplified().left(stringDisplayLen).simplified();
                       alignment=Qt::AlignLeft;
                       break;
                    }
                }
                }
                break;
            case ST_BOOKMARKS_STRING :
                {
                Session* journal=daily->GetJournalSession(date,false);
                if (journal && journal->settings.contains(Bookmark_Notes)) {
                    QStringList notes = journal->settings[Bookmark_Notes].toStringList();
                    QString findStr = selectString->text();
                    for (   const auto & note : notes) {
                        if (myMatch->compare(findStr , note))
                        {
                           found=true;
                           myMatch->foundString = note.simplified().left(stringDisplayLen).simplified();
                           alignment=Qt::AlignLeft;
                           break;
                        }
                    }
                }
                }
                break;
            case ST_NOTES_STRING :
                {
                Session* journal=daily->GetJournalSession(date,false);
                if (journal && journal->settings.contains(Journal_Notes)) {
                    QString jcontents = Daily::convertHtmlToPlainText(journal->settings[Journal_Notes].toString());
                    QString findStr = selectString->text();
                    if (jcontents.contains(findStr,Qt::CaseInsensitive) ) {
                       found=true;
                       myMatch->foundString = jcontents.simplified().left(stringDisplayLen).simplified();
                       alignment=Qt::AlignLeft;
                    }
                }
                }
                break;
            case ST_AHI :
                {
                EventDataType dahi =calculateAhi(day);
                dahi += 0.005;
                dahi *= 100.0;
                int ahi = (int)dahi;
                myMatch->updateMinMaxValues(ahi);
                found = myMatch->compare (ahi , myMatch->compareValue);
                myMatch->foundString = myMatch->valueToString( ahi,"----");
                }
                break;
            case ST_CLEAR :
            case ST_APNEA_ALL :
                break; // should never get here
            case ST_APNEA_LENGTH :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                QMap<ChannelID,int> values;
                // find possible channeld to use
                QList<ChannelID> chans;
                for( auto code : apneaLikeChannels) {
                    if (day->count(code)) { chans.push_back(code); }
                }
                bool errorFound = false;
                QString result;
                if (!apneaLikeChannels.isEmpty()) {
                    for (Session* sess : sessions ) {
                        if (!sess->enabled()) continue;
                        auto keys = sess->eventlist.keys();
                        if (keys.size() <= 0) {
                            bool ok = sess->LoadSummary(false);
                            bool ok1 = sess->OpenEvents(false);
                            keys = sess->eventlist.keys();
                            if ((keys.size() <= 0) || !ok || !ok1 ) {
                                if ((keys.size() <= 0) || !ok || !ok1 ) {
                                    errorFound |= true;
                                    // skip this channel
                                    continue;
                                }
                            }
                        }
                        for( ChannelID code : apneaLikeChannels) {
                            auto evlist = sess->eventlist.find(code);
                            if (evlist == sess->eventlist.end()) {
                                continue;
                            }
                            // Now we go through the event list for the *session* (not for the day)
                            for (int z=0;z<evlist.value().size();z++) {
                                EventList & ev=*(evlist.value()[z]);
                                for (quint32 o=0;o<ev.count();o++) {
                                    int sec = evlist.value()[z]->raw(o);
                                    //qint64 time = evlist.value()[z]->time(o);
                                    if (apneaLikeChannels.size()==1) {
                                        myMatch->updateMinMaxValues(sec);
                                    }
                                    if (myMatch->compare (sec , myMatch->compareValue)) {
                                        // save value in map
                                        auto it = values.find(code);
                                        if (it == values.end() ) {
                                            values.insert(code,sec);
                                        } else {
                                            // save max or min value in map
                                            int saved_sec = it.value();
                                            // save highest or lowest value.
                                            if (myMatch->compare (sec ,saved_sec) ) {
                                                *it = sec;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                for ( auto it= values.begin() ; it != values.end() ; it++) {
                    ChannelID code = it.key();
                    int value = it.value();
                    result += QString("%1:%2 ").arg(schema::channel[ code  ].label()).arg(value);
                }
                if (errorFound) {
                    daysSkipped++;
                    return false;
                };
                found = !result.isEmpty();
                if (found) {
                    myMatch->foundString = result;
                    alignment=Qt::AlignLeft;
                }
                }
                break;
            case ST_SESSION_LENGTH :
                {
                bool valid=false;
                qint64 value=0;
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                for (auto & sess : sessions) {
                    qint64 ms = sess->length();
                    myMatch->updateMinMaxValues(ms);
                    if (myMatch->compare (ms , myMatch->compareValue) ) {
                        found =true;
                    }
                    if (!valid) {
                        valid=true;
                        value=ms;
                    } else if (myMatch->compare (ms , value) ) {
                        value=ms;
                    }
                }
                if (valid) myMatch->updateMinMaxValues(value);
                myMatch->foundString = myMatch->valueToString( value,"----");
                }
                break;
            case ST_SESSIONS_QTY :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint32 size = sessions.size();
                myMatch->updateMinMaxValues(size);
                found=myMatch->compare (size , myMatch->compareValue);
                myMatch->foundString = myMatch->valueToString( size,"----");
                }
                break;
            case ST_DAILY_USAGE :
                {
                QList<Session *> sessions = day->getSessions(MT_CPAP);
                qint64 sum = 0  ;
                for (auto & sess : sessions) {
                    sum += sess->length();
                }
                myMatch->updateMinMaxValues(sum);
                found=myMatch->compare (sum , myMatch->compareValue);
                myMatch->foundString = myMatch->valueToString( sum,"----");
                }
                break;
            case ST_EVENT :
                {
                qint32 count = day->count(myMatch->channelId);
                myMatch->updateMinMaxValues(count);
                found=myMatch->compare (count , myMatch->compareValue);
                myMatch->foundString = myMatch->valueToString( count,"----");
                }
                break;
            case ST_NONE :
                break;
        }
        return found;
};

void DailySearchTab::find(QDate& date) {
        QCoreApplication::processEvents();
        Day* day = p_profile->GetDay(date);
        if ( (!day) && (match->searchTopic != ST_DAYS_SKIPPED)) { daysSkipped++; return;};
        Qt::Alignment alignment=Qt::AlignCenter;
        bool found=false;
        QString result;
        for (int idx = 0 ; idx < matches.size() ; ++idx ) {
            if ((!day) && (match->searchTopic != ST_DAYS_SKIPPED)) break;
            Match* tmpMatch = matches.at(idx);
            if (tmpMatch->isEmpty()) { continue; };

            found = matchFind(tmpMatch,day,date,alignment);
            if (!found) return ;
            result += tmpMatch->foundString;
            result += "  ";
        };
        if (!found) return ;
        addItem(date , result,alignment );
        passFound++;
        daysFound++;
        return ;
};

void DailySearchTab::search(QDate date) {
        setState( searching );
        if (!date.isValid()) {
            qWarning() << "DailySearchTab::find invalid date." << date;
            return;
        }
		startButton->setEnabled(false);
        match->foundString.clear();
        passFound=0;
        while (date >= earliestDate) {
            nextDate = date;
            if (passFound >= passDisplayLimit)  break;

            find(date);
            progressBar->setValue(++daysProcessed);
            date=date.addDays(-1);
        }
        endOfPass();
        return ;
};

void DailySearchTab::addItem(QDate date, QString value,Qt::Alignment alignment) {
        int row = passFound;
        Q_UNUSED(alignment);
        setResult(DS_ROW_DATA+row,0,date,value);
        setResult(DS_ROW_DATA+row,1,date,value);
}

void    DailySearchTab::endOfPass() {
        cmdDescList->show();
        QString display;
        if ((passFound >= passDisplayLimit) && (daysProcessed<daysTotal)) {
            setState( waitForContinue );
        } else if (daysFound>0) {
            setState( endOfSeaching );
        } else {
            setState( noDataFound );
        }
        displayStatistics();
}

void    DailySearchTab::hideCommand(bool showButton) {
        //operationCombo->hide();
        //operationCombo->clear();
        operationButton->hide();
        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();
        commandWidget->setVisible(showButton);
        commandButton->setVisible(showButton);
};

void    DailySearchTab::setCommandPopupEnabled(bool on) {
        if (on) {
            commandPopupEnabled=true;
            hideCommand();
            commandList->show();
        } else {
            commandPopupEnabled=false;
            commandList->hide();
            hideCommand(true/*show button*/);
        }
}

void    DailySearchTab::process_match_info(QString text, int topic) {
        // here to select new search criteria
        // must reset all variables and label, button, etc
        clearMatch() ;
        commandWidget->show();
        match->matchName = text;
        // get item selected
        if (topic>=ST_EVENT) {
            match->channelId = topic;
            match->searchTopic = ST_EVENT;
        } else {
            match->searchTopic = (SearchTopic)topic;
        }

        match->valueMode = notUsed;
        match->compareValue = 0;

        // workaround for combo box alignmnet and sizing.
        // copy selections to a pushbutton. hide combobox and show pushButton. Pushbutton activation can show popup.
        // always hide first before show. allows for best fit
        setText(commandButton, text);
        setCommandPopupEnabled(false);

        match->operationOpCode = OP_INVALID;
        match->nextTab = TW_NONE;
        match->compareString = "9999";

        switch (match->searchTopic) {
            case ST_NONE :
                // should never get here.
                setResult(DS_ROW_HEADER,1,QDate(),"");
                match->nextTab = TW_NONE ;
                setoperation( OP_INVALID ,notUsed);
                break;
            case ST_DAYS_SKIPPED :
                setResult(DS_ROW_HEADER,1,QDate(),"");
                // there is no graphs available in daily. or in the lefti sidebar so no jump match->nextTab = TW_DETAILED ;
                setoperation(OP_NO_PARMS,notUsed);
                break;
            case ST_DISABLED_SESSIONS :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number Disabled Session\nJumps to Date's Details "));
                match->nextTab = TW_DETAILED ;
                selectInteger->setValue(0);
                setoperation(OP_NO_PARMS,displayWhole);
                break;
            case ST_JOURNAL :
                setResult(DS_ROW_HEADER,1,QDate(),tr("JUmps\nJumps to Date's Notes"));
                match->nextTab = TW_NOTES ;
                setoperation( OP_NO_PARMS ,displayString);
                break;
            case ST_NOTES :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Note\nJumps to Date's Notes"));
                match->nextTab = TW_NOTES ;
                setoperation( OP_NO_PARMS ,displayString);
                break;
            case ST_BOOKMARKS :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Bookmark\nJumps to Date's Bookmark"));
                match->nextTab = TW_BOOKMARK ;
                setoperation( OP_NO_PARMS ,displayString);
                break;
            case ST_BOOKMARKS_STRING :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Bookmark\nJumps to Date's Bookmark"));
                match->nextTab = TW_BOOKMARK ;
                setoperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case ST_NOTES_STRING :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Note\nJumps to Date's Notes"));
                match->nextTab = TW_NOTES ;
                setoperation(OP_WILDCARD,opString);
                selectString->clear();
                break;
            case ST_AHI :
                setResult(DS_ROW_HEADER,1,QDate(),tr("AHI\nJumps to Date's Details"));
                match->nextTab = TW_DETAILED ;
                setoperation(OP_GT,hundredths);
                setText(selectUnits,tr(" EventsPerHour"));
                selectDouble->setValue(5.0);
                selectDouble->setSingleStep(0.1);
                break;
            case ST_CLEAR :
            case ST_APNEA_ALL :
                break; // should never get here
            case ST_APNEA_LENGTH :
                DaysWithFileErrors = 0;

                setResult(DS_ROW_HEADER,1,QDate(),tr("Set of Apnea:Length\nJumps to Date's Events"));
                match->nextTab = TW_EVENTS ;
                setoperation(OP_GE,secondsDisplayString);

                selectInteger->setRange(0,999);
                selectInteger->setValue(25);
                setText(selectUnits,tr(" Seconds"));
                break;
            case ST_SESSION_LENGTH :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Session Duration\nJumps to Date's Details"));
                match->nextTab = TW_DETAILED ;
                setoperation(OP_LT,minutesToMs);
                selectDouble->setValue(5.0);
                setText(selectUnits,tr(" Minutes"));
                selectDouble->setSingleStep(0.1);
                selectInteger->setValue((int)selectDouble->value()*60000.0);   //convert to ms
                break;
            case ST_SESSIONS_QTY :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of Sessions\nJumps to Date's Details"));
                match->nextTab = TW_DETAILED ;
                setoperation(OP_GT,opWhole);
                selectInteger->setRange(0,999);
                selectInteger->setValue(2);
                setText(selectUnits,tr(" Sessions"));
                break;
            case ST_DAILY_USAGE :
                setResult(DS_ROW_HEADER,1,QDate(),tr("Daily Duration\nJumps to Date's Details"));
                match->nextTab = TW_DETAILED ;
                setoperation(OP_LT,hoursToMs);
                selectDouble->setValue(p_profile->cpap->complianceHours());
                selectDouble->setSingleStep(0.1);
                selectInteger->setValue((int)selectDouble->value()*3600000.0);   //convert to ms
                setText(selectUnits,tr(" Hours"));
                break;
            case ST_EVENT:
                // Have an Event
                setResult(DS_ROW_HEADER,1,QDate(),tr("Number of events\nJumps to Date's Events"));
                match->nextTab = TW_EVENTS ;
                setoperation(OP_GT,opWhole);
                selectInteger->setValue(0);
                setText(selectUnits,tr(" Events"));
                break;
        }
        criteriaChanged();
        match->opCodeStr = opCodeStr(match->operationOpCode);
        QString uni = selectUnits->text();
        match->units = uni;

        addMatchButton->setText(tr("add another match?"));
        addMatchButton->setVisible(true);
        setState( waitForStart );

        if (match->operationOpCode == OP_NO_PARMS ) {
            // auto start searching
            setState (autoStart);
            //match->units = "";
            //match->compareString="";
            on_startButton_clicked();
            return;
        }
        setColor(startButton,green);
        setColor(addMatchButton , green);
        return;
}

void    DailySearchTab::on_operationButton_clicked() {
        setState( waitForStart );
        // only gets here for string operations
        if (match->operationOpCode == OP_CONTAINS ) {
            match->operationOpCode = OP_WILDCARD;
        } else if (match->operationOpCode == OP_WILDCARD) {
            match->operationOpCode = OP_CONTAINS ;
        } else {
            setOperationPopupEnabled(true);
        }
        QString text=opCodeStr(match->operationOpCode);
        setText(operationButton,text);
        criteriaChanged();
};

void    DailySearchTab::on_operationCombo_activated(int index) {
        // only gets here for numeric comparisions.
        setState( waitForStart );
        QString text = operationCombo->itemText(index);
        OpCode opCode = opCodeMap[text];
        match->operationOpCode = opCode;
        match->opCodeStr = opCodeStr(match->operationOpCode);
        setText(operationButton,match->opCodeStr);
        setOperationPopupEnabled(false);
        criteriaChanged();
};

void    DailySearchTab::on_matchButton_clicked() {
        setState (matching);
        setCommandPopupEnabled(!commandPopupEnabled);
}

void    DailySearchTab::on_commandButton_clicked() {
        setCommandPopupEnabled(true);
}

void    DailySearchTab::on_helpButton_clicked() {
        helpMode = !helpMode;
        if (helpMode) {
            resultTable->setMinimumSize(QSize(50,200)+textsize(helpText->font(),helpString));
            resultTable->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
            helpButton->setText(tr("Click HERE to close Help"));
            helpText ->show();
        } else {
            resultTable->setMinimumWidth(250);
            helpText ->hide();
            helpButton->setText(tr("Help"));
        }
}

void    DailySearchTab::on_clearButton_clicked()
{
        setState( reset);
}

void    DailySearchTab::setOperationPopupEnabled(bool on) {
        if (on) {
            operationButton->hide();
            operationCombo->show();
            operationCombo->showPopup();
        } else {
            operationCombo->hidePopup();
            operationCombo->hide();
            operationButton->show();
        }
}

void    DailySearchTab::setoperation(OpCode opCode,ValueMode mode)  {
        match->valueMode = mode;
        match->operationOpCode = opCode;
        setText(operationButton,opCodeStr(match->operationOpCode));
        setOperationPopupEnabled(false);

        if (opCode > OP_INVALID && opCode <OP_END_NUMERIC)  {
            // Have numbers
            selectDouble->setDecimals(2);
            selectDouble->setRange(0,999);
            selectDouble->setDecimals(2);
            selectInteger->setRange(0,999);
            selectDouble->setSingleStep(0.1);
        }
        switch (match->valueMode) {
            case hundredths :
                selectUnits->show();
                selectDouble->show();
                break;
            case hoursToMs:
                setText(selectUnits,tr(" Hours"));
                selectUnits->show();
                selectDouble->show();
                break;
            case secondsDisplayString:
                setText(selectUnits,tr(" Seconds"));
                selectUnits->show();
                selectInteger->show();
                break;
            case minutesToMs:
                setText(selectUnits,tr(" Minutes"));
                selectUnits->show();
                selectDouble->setRange(0,9999);
                selectDouble->show();
                break;
            case opWhole:
                selectUnits->show();
                selectInteger->show();
                break;
            case displayWhole:
                selectInteger->hide();
                break;
            case opString:
                operationButton->show();
                selectString ->show();
                break;
            case displayString:
                selectString ->hide();
                break;
            case invalidValueMode:
            case notUsed:
                break;
        }

}

void    DailySearchTab::hideResults(bool hide) {
        if (hide) {
            for (int index = DS_ROW_HEADER; index<DS_ROW_MAX;index++) {
                resultTable->setRowHidden(index,true);
            }
        }
        progressBar->setMinimumHeight(matchButton->height());
        //summaryWidget->setVisible(!hide);
}

QSize   DailySearchTab::textsize(QFont font ,QString text) {
        return QFontMetrics(font).size(0 , text);
}

void    DailySearchTab::clearMatch()
{
        commandWidget->hide();
        DaysWithFileErrors = 0 ;
        match->searchTopic = ST_NONE;
        match->matchName.clear();
        match->opCodeStr.clear();
        match->compareString.clear();
        match->units.clear();
        match->foundString.clear();
        match->label.clear();

        // make these button text back to start.
        addMatchButton->setVisible(matches.size()>1);

        startButton->setText(tr("Start Search"));
        startButton->setEnabled( false);

        // hide widgets
        //Reset Select area
        commandList->hide();
        setCommandPopupEnabled(false);
        setText(commandButton,"");
        commandButton->show();

        operationCombo->hide();
        setOperationPopupEnabled(false);
        operationButton->hide();
        selectDouble->hide();
        selectInteger->hide();
        selectString->hide();
        selectUnits->hide();
        selectUnits->clear();

}

QString Match::createMatchDescription() {
        label = QString("%1 %2 %3 %4 " ).arg(matchName).arg(opCodeStr).arg(compareString).arg(units);
        return label;
}

void    DailySearchTab::on_addMatchButton_clicked() {
        if (match->matchName.isEmpty()) { return; };
        match->createMatchDescription();
        QLabel* label = getCmdDescLabel();
        label->setText(match->label);
        label->setVisible(true);
        QCoreApplication::processEvents();

        Match* nmatch = matches.addMatch();
        match = nmatch;
        clearMatch();
        setState(multpileMatches);
        on_matchButton_clicked();
}

void    DailySearchTab::on_startButton_clicked() {
        setState( searching );
        clearStatistics();
        if (startButton_1stPass) {

            match->createMatchDescription();
            QLabel* label = getCmdDescLabel();
            label->setText(match->label);
            label->setVisible(true);

            commandWidget->hide();
            cmdDescList->show();
            search (latestDate );
            startButton_1stPass=false;
        } else {
            search (nextDate );
        }
}

void    DailySearchTab::on_intValueChanged(int ) {
        selectInteger->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_doubleValueChanged(double ) {
        selectDouble->findChild<QLineEdit*>()->deselect();
        criteriaChanged();
}

void    DailySearchTab::on_textEdited(QString ) {
        criteriaChanged();
}

void    DailySearchTab::on_activated(GPushButton* item ) {
        int row=item->row();
        int col=item->column();
        if (row<DS_ROW_DATA) return;
        if (row>=DS_ROW_MAX) return;
        row-=DS_ROW_DATA;

        item->setIcon (*m_icon_selected);
        if (match->searchTopic == ST_DAYS_SKIPPED) return;
        daily->LoadDate( item->date() );
        if ((col!=0) &&  match->nextTab>=0 && match->nextTab < dailyTabWidget->count())  {
            dailyTabWidget->setCurrentIndex(match->nextTab);    // 0 = details ; 1=events =2 notes ; 3=bookarks;
        }
}

void    DailySearchTab::clrCmdDescList() {
        cmdDescLabelsUsed = 0 ;
        for (int i = 0 ; i< cmdDescLabels.size(); i++) {
            QLabel* label = cmdDescLabels[i];
            label->setVisible(false);
            label->setText("");
        }
};

QLabel* DailySearchTab::getCmdDescLabel() {
        quint32 size = cmdDescLabels.size();
        QLabel* label ;
        if (cmdDescLabelsUsed >= size ) {
            QString styleLabel=QString(
                "QLabel { color: black; border: 1px solid black; padding: 5px ;background-color:white; }"
                "QLabel:disabled { background-color: #EEEEFF;}"
                );
            label = new QLabel();
            cmdDescLabels.append(label);
            label ->setStyleSheet( styleLabel );
            label ->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            cmdDescLayout ->addWidget(label);
        }
        label = cmdDescLabels[cmdDescLabelsUsed++];
        label ->setVisible( true );
        return label;
}

void    DailySearchTab::setColor(QPushButton* button,QString color)  {
        QString style=QString(
            "QPushButton { color: black; border: 1px solid black; padding: 5px ;background-color:%1; }").arg(color);
        button->setStyleSheet(style);
        QCoreApplication::processEvents();
}

void    DailySearchTab::clearStatistics() {
        summaryProgress->hide();
        summaryFound->hide();
        summaryMinMax->hide();
}

void    DailySearchTab::displayStatistics() {
        QString extra;
        summaryProgress->show();

        // display days searched
        QString skip= daysSkipped==0?"":QString(tr(" Skip:%1")).arg(daysSkipped);
        setText(summaryProgress,centerLine(QString(tr("%1/%2%3 days")).arg(daysProcessed).arg(daysTotal).arg(skip) ));

        // display days found
        setText(summaryFound,centerLine(QString(tr("Found %1 ")).arg(daysFound) ));

        // display associated value
        extra ="";
        if (match->minMaxValid) {
            if (match->valueMode == secondsDisplayString)  {
                extra = QString("%1 / %2").arg(match->minInteger).arg(match->maxInteger);
            } else {
                QString amin = match->valueToString(match->minInteger);
                QString amax = match->valueToString(match->maxInteger);
                extra = QString("%1 / %2").arg(amin).arg(amax);
            }
        }
        if (extra.size()>0) {
            setText(summaryMinMax,extra);
            summaryMinMax->show();
        } else {
            if (DaysWithFileErrors) {
                QString msg = tr("File errors:%1");
                setText(summaryMinMax, QString(msg).arg(DaysWithFileErrors));
                summaryMinMax->show();
            } else {
                summaryMinMax->hide();
            }
        }
        summaryProgress->show();
        summaryFound->show();
}

void    DailySearchTab::criteriaChanged() {
        // setup before start button
        match->compareString = "";

        if (match->valueMode != notUsed ) {
            setText(operationButton,opCodeStr(match->operationOpCode));
            operationButton->show();
        }
        switch (match->valueMode) {
            case hundredths :
                match->compareValue = (int)(selectDouble->value()*100.0);   //convert to hundreths of AHI.
                match->compareString = QString::number(selectDouble->value());
            break;
            case minutesToMs:
                match->compareValue = (int)(selectDouble->value()*60000.0);   //convert to ms
                match->compareString = QString::number(selectDouble->value());
            break;
            case hoursToMs:
                match->compareValue = (int)(selectDouble->value()*3600000.0);   //convert to ms
                match->compareString = QString::number(selectDouble->value());
            break;
            case secondsDisplayString:
            case displayWhole:
            case opWhole:
                match->compareValue = selectInteger->value();
                match->compareString = QString::number(selectInteger->value());
            break;
            case opString:
                match->compareString = selectString->text();
            break;
            case displayString:
            case invalidValueMode:
            case notUsed:
            break;
        }

        commandList->hide();
        commandButton->show();

        setText(startButton,tr("Start Search"));
        setColor(startButton,green);
        setColor(addMatchButton , green);
        startButton->setEnabled( true);


        match->minMaxValid = false;
        match->minInteger = 0;
        match->maxInteger = 0;
        match->minDouble = 0.0;
        match->maxDouble = 0.0;
        earliestDate = p_profile->FirstDay(MT_CPAP);
        latestDate = p_profile->LastDay(MT_CPAP);
        daysTotal= 1+earliestDate.daysTo(latestDate);
        daysFound=0;
        daysSkipped=0;
        daysProcessed=0;

        //initialize progress bar.

        progressBar->setMinimum(0);
        progressBar->setMaximum(daysTotal);
        progressBar->setTextVisible(true);
        progressBar->setMinimumHeight(commandListItemHeight);
        progressBar->setMaximumHeight(commandListItemHeight);
        progressBar->reset();
}

// inputs  character string.
// outputs cwa centered html string.
// converts \n to <br>
QString DailySearchTab::centerLine(QString line) {
        return line;
        return QString( "<center>%1</center>").arg(line).replace("\n","<br>");
}

QString DailySearchTab::helpStr()
{
    QStringList str; str
    <<tr("Finds days that match specified criteria.") <<"\n"
    <<tr("  Searches from last day to first day.") <<"\n"
    <<tr("  Skips Days with no graphing data.") <<"\n"
    <<"\n"
    <<tr("First click on Match Button then select topic.") <<"\n"
    <<tr("  Then click on the operation to modify it.") <<"\n"
    <<tr("  or update the value") <<"\n"
    <<"\n"
    <<tr("Topics without operations will automatically start.") <<"\n"
    <<"\n"
    <<tr("Compare Operations: numberic or character. ") <<"\n"
    <<tr("  Numberic  Operations: ") <<"   >  ,  >=  ,  <  ,  <=  ,  ==  ,  != " <<"\n"
    <<tr("  Character Operations: ") <<"   ==  ,  *? " <<"\n"
    <<"\n"
    <<tr("Summary Line") <<"\n"
    <<tr("  Left:Summary - Number of Day searched") <<"\n"
    <<tr("  Center:Number of Items Found") <<"\n"
    <<tr("  Right:Minimum/Maximum for item searched") <<"\n"
    <<"\n"
    <<tr("Result Table") <<"\n"
    <<tr("  Column One: Date of match. Click selects date.") <<"\n"
    <<tr("  Column two: Information. Click selects date.") <<"\n"
    <<tr("    Then Jumps the appropiate tab.") <<"\n"
    <<"\n"
    <<tr("Wildcard Pattern Matching:") <<" *? " <<"\n"
    <<tr("  Wildcards use 3 characters:") <<"\n"
    <<tr("  Asterisk") <<" *  " <<"   "
    <<tr("  Question Mark") <<" ? " <<"   "
    <<tr("  Backslash.") <<" \\ " <<"\n"
    <<tr("  Asterisk matches any number of characters.") <<"\n"
    <<tr("  Question Mark matches a single character.") <<"\n"
    <<tr("  Backslash matches next character.") <<"\n\n"
    ;
    return str.join("");
}

QString Match::formatTime (qint32 ms) {
        qint32 hours = ms / 3600000;
        ms = ms % 3600000;
        qint32 minutes = ms / 60000;
        ms = ms % 60000;
        qint32 seconds = ms /1000;
        return QString("%1:%2:%3").arg(hours).arg(minutes,2,10,QLatin1Char('0')).arg(seconds,2,10,QLatin1Char('0'));
}

QString DailySearchTab::opCodeStr(OpCode opCode) {
        switch (opCode) {
            case OP_LT : return "< ";
            case OP_GT : return "> ";
            case OP_GE : return ">=";
            case OP_LE : return "<=";
            case OP_EQ : return "==";
            case OP_NE : return "!=";
            case OP_CONTAINS : return "==";
            case OP_WILDCARD : return "*?";
            case OP_INVALID:
            case OP_END_NUMERIC:
            case OP_NO_PARMS : // return tr("Automatic Starting");
            break;
        }
        return QString();
};

EventDataType  DailySearchTab::calculateAhi(Day* day) {
        if (!day) return 0.0;
        // copied from daily.cpp
        double  hours=day->hours(MT_CPAP);
        if (hours<=0) return 0;
        EventDataType  ahi=day->count(AllAhiChannels);
        if (p_profile->general->calculateRDI()) ahi+=day->count(CPAP_RERA);
        ahi/=hours;
        return ahi;
}

GPushButton::GPushButton (int row,int column,QDate date,DailySearchTab* parent , ChannelID code, quint32 offset) :
    QPushButton(parent), _parent(parent), _row(row), _column(column), _date(date), _code(code) , _offset(offset)
{
        connect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        connect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};

GPushButton::~GPushButton()
{
        //the following disconnects trigger a crash during exit  or profile change. - anytime daily is destroyed.
        //disconnect(this, SIGNAL(clicked()), this, SLOT(on_clicked()));
        //disconnect(this, SIGNAL(activated(GPushButton*)), _parent, SLOT(on_activated(GPushButton*)));
};

void GPushButton::on_clicked() {
        emit activated(this);
};

Match* Matches::empty() {return &_empty;};

Matches::Matches() {
        inuse = 0;
        matchList.clear();
}

Matches::~Matches() {
        clear();
        while ( !matchList.isEmpty() ) {
            Match* next = matchList.takeLast();
            delete next;
        }
}

void    Matches::clear() {
        for (int idx=0; idx < matchList.size(); idx++) {
            Match* next = matchList.at(idx);
            next->label = "";
        }
        inuse = 0;
}

Match*   Matches::addMatch() {
    if (inuse >= matchList.size()) {
        Match* match = new Match(true);
        matchList.push_back(match);
    }
    if (inuse<matchList.size()) {
        Match* next = matchList.at(inuse);
        next->label = next->createMatchDescription();
        inuse ++;
        return next;
    }
    return empty();
}

