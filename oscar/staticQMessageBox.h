/* Create staticQMessage Header
 *
 * Copyright (c) 2023-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

/*
The sole purpose of this file is to recreate static messagebox functions that were deprecated in qt6.
The static method  question and information and other will be recreated.

This will create a subclass of QMessageBox/QWidget creating the static methods that were dropped.
This will have minimal impact on code changes.
*/

#ifndef STATIC_MESSAGE_BOX_H
#define STATIC_MESSAGE_BOX_H

#include <QMessageBox>
#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6,2,0)
    #define staticQMessageBox QMessageBox
#else

class staticQMessageBox : public QWidget
{
private:
    static QMessageBox::StandardButton custom( QMessageBox::Icon icon,
            QWidget *parent, const QString &title, const QString &text,
            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) ;

public:
    static QMessageBox::StandardButton question( QWidget *parent, const QString &title, const QString &text,
            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) ;

    static QMessageBox::StandardButton information( QWidget *parent, const QString &title, const QString &text,
            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) ;

    static QMessageBox::StandardButton warning( QWidget *parent, const QString &title, const QString &text,
            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) ;

   QMessageBox::StandardButton critical( QWidget *parent, const QString &title, const QString &text,
            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) ;
};
#endif      // QT_VERSION
#endif      // STATIC_MESSAGE_BOX_H
