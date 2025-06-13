/* SleepLib Progress Dialog Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (C) 2011-2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class ProgressDialog:public QDialog {
Q_OBJECT
public:
    explicit ProgressDialog(QWidget * parent);
    virtual ~ProgressDialog();

    void addAbortButton();

    void setPixmap(QPixmap &pixmap) { imglabel->setPixmap(pixmap); }
    QProgressBar * progress;
public slots:
    void setMessage(QString msg);
    void onAbortClicked();

    void setProgressMax(int max);
    void setProgressValue(int val);

signals:
    void abortClicked();
protected:
    QLabel * statusMsg;
    QHBoxLayout *hlayout;
    QLabel * imglabel;
    QVBoxLayout * vlayout;
    QPushButton * abortButton;

};

#endif // PROGRESSDIALOG_H
