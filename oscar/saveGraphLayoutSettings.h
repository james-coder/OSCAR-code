/* Overview GUI Headers
 *
 * Copyright (c) 2022-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef SAVEGRAPHLAYOUTSETTINGS_H
#define SAVEGRAPHLAYOUTSETTINGS_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
class QVBoxLayout;
class QListWidget;
class QListWidgetItem;
class QRegularExpression;
class QDir;
class QPushButton;

#include "Graphs/gGraphView.h"

class DescriptionMap
{
public:
    DescriptionMap(QDir* dir, QString filename) ;
    virtual ~DescriptionMap();
    void add(QString key,QString desc);
    void remove(QString key);
    QString get(QString key);
    void load();
    void save();
private:
    QString filename;
    QMap <QString,QString> descriptions;
    const QRegularExpression* parseDescriptionsRe;
    const QChar delimiter = QChar(':');
};

class HelpData {
public:
    void setDialog(QString windowsTitle);
    void setDialog(QWidget*,QString windowsTitle );
    void finishInit();

    QLabel title ;
    QLabel message ;

    QDialog*dialog = nullptr;
    bool    open = false;
    QVBoxLayout* layout =nullptr;

    QFont   font ;
    QFont   fontBold ;
    QLabel* label ;

    // For frameless
    QPushButton* exitBtn=nullptr;
    QHBoxLayout* headerLayout=nullptr;

};

class SaveGraphLayoutSettings : public QWidget
{
	Q_OBJECT
public:
    SaveGraphLayoutSettings(QString title, QWidget* parent) ;
    ~SaveGraphLayoutSettings();
    void triggerLayout(gGraphView* graphView);
    void    hintHelp();
protected:
    HelpData hint;
    HelpData help;
    HelpData menu;
    QIcon*  m_icon_return       = new QIcon(":/icons/return.png");
    QIcon*  m_icon_help         = new QIcon(":/icons/question_mark.png");
    QIcon*  m_icon_exit         = new QIcon(":/icons/exit.png");
    QIcon*  m_icon_delete       = new QIcon(":/icons/trash_can.png");
    QIcon*  m_icon_update       = new QIcon(":/icons/update.png");
    QIcon*  m_icon_restore      = new QIcon(":/icons/restore.png");
    QIcon*  m_icon_rename       = new QIcon(":/icons/rename.png");
    QIcon*  m_icon_add          = new QIcon(":/icons/plus.png");

private:
    const static int fileNumMaxLength = 3;
    const static int maxFiles = 30;     // Max supported design limited is 1000 10**fileNumMaxLength(3).
    const static int iconWidthMessageBox = 50;
    const static int maxDescriptionLen = 80;
    const QString fileBaseName = QString("layout");
    const int fileNameRole = Qt::UserRole;
    int   horizontalWidthAdjustment=60;     // this seem to make menu size changes work. Testing says it is 60 but what causes it is unknown.

    QSize minMenuListSize = QSize(0,0);
    QSize minMenuDialogSize = QSize(0,0);

    QSize dialogListDiff = QSize(0,0);
    QSize menuDialogSize = QSize(0,0);
    QSize menuListSize = QSize(0,0);
    void  initminMenuListSize();
    QSize calculateMenuDialogSize();
    QSize maxSize(const QSize AA , const QSize BB ) ;
    bool  sizeEqual(const QSize AA , const QSize BB ) ;

    const QRegularExpression* singleLineRe;
    const QRegularExpression* fileNumRe;
    const QRegularExpression* parseFilenameRe;


    QWidget*            parent;
    const   QString     title;
    gGraphView*         graphView = nullptr;

    void         createHint();
    QString      hintInfo();
    QListWidget* menuList;

    QPushButton* menuAddFullBtn;   // Must be first item for workaround.
    QPushButton* menuAddBtn;
    QPushButton* menuDeleteBtn;
    QPushButton* menuRestoreBtn;
    QPushButton* menuUpdateBtn;
    QPushButton* menuRenameBtn;
    QPushButton* menuHelpBtn;

    QHBoxLayout* menuLayoutButtons;

    void         createHelp();
    void         createHelp(HelpData & help , QString title , QString text);
    void         initDialog(HelpData & help , QString title );
    void         helpDestructor();
    QString      helpInfo();

    QDir*   dir = nullptr;
    QString dirName;
    int     nextNumToUse;
    QListWidgetItem* updateFileList(QString find = QString());
    QListWidgetItem* widestItem=nullptr;
    QString styleOn;
    QString styleOff;
    QString styleExit;
    QString styleMessageBox;
    QString styleDialog;

    QString calculateButtonStyle(bool on,bool border);
    void    looksOn(QPushButton* button,bool on);
    DescriptionMap* descriptionMap;
    bool    confirmAction(QString name,QString question,QIcon* icon,
                QMessageBox::StandardButtons flags = (QMessageBox::Cancel|QMessageBox::Yes) ,
                QMessageBox::StandardButton adefault = QMessageBox::Cancel,
                QMessageBox::StandardButton success = QMessageBox::Yes
                );
    bool    verifyItem(QListWidgetItem* item,QString name,QIcon* icon) ;

    const QString calculateStyleMessageBox( QFont* font, QString& s1, QString& s2);

    void     displaywidgets(QWidget* widget);
    QSize    calculateParagraphSize(QString& text,QFont& font, QString& );

    void    createMenu();
    void    createStyleSheets();
    void    createSaveFolder();
    QPushButton*  newBtnRtn(QHBoxLayout*, QString name, QIcon* icon, QString style,QSizePolicy::Policy hPolicy,QString tooltip);
    QPushButton*  menuBtn(                QString name, QIcon* icon, QString style,QSizePolicy::Policy hPolicy,QString tooltip);

    void    manageButtonApperance();
    void    resizeMenu();

    int     fileNum(QString fileName);
    void    writeSettings(QString filename);
    void    loadSettings(QString filename);
    void    deleteSettings(QString filename);
    void    closeMenu();
    void    closeHelp();

public slots:
private slots:
    void    add_feature();
    void    addFull_feature();
    void    restore_feature();
    void    rename_feature();
    void    update_feature();
    void    help_feature();
    void    help_exit_feature();
    void    delete_feature();
    void    itemChanged(QListWidgetItem *item);
    void    itemSelectionChanged();

    void    closeHint();
};

#endif // SAVEGRAPHLAYOUTSETTINGS_H

