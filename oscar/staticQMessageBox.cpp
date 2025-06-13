/* staticMessageBox Implementation
 *
 * Copyright (c) 2024-2025 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include "staticQMessageBox.h"

/*
The sole purpose of this file is to recreate static messagebox functions that were deprecated in qt6.
The static method  question and information and other will be recreated.

This will create a subclass of QMessageBox/QWidget creating the static methods that were dropped.
This will have minimal impact on code changes.
*/

#if ! defined(staticQMessageBox)

QMessageBox::StandardButton staticQMessageBox::custom(
    QMessageBox::Icon icon,
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton
    )
{
    QMessageBox box( icon, title, text, buttons, parent);
    box.setDefaultButton(defaultButton);
    QMessageBox::StandardButton result = (QMessageBox::StandardButton)box.exec();
    return result;
};

QMessageBox::StandardButton staticQMessageBox::question( QWidget *parent, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons ,
        QMessageBox::StandardButton defaultButton ) {
    return custom( QMessageBox::Question, parent, title, text, buttons , defaultButton ) ;
}

QMessageBox::StandardButton staticQMessageBox::information( QWidget *parent, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons ,
        QMessageBox::StandardButton defaultButton ) {
    return custom(QMessageBox::Information, parent, title, text, buttons , defaultButton ) ;
}

QMessageBox::StandardButton staticQMessageBox::warning( QWidget *parent, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons ,
        QMessageBox::StandardButton defaultButton ) {
    return custom( QMessageBox::Warning, parent, title, text, buttons , defaultButton ) ;
}

QMessageBox::StandardButton staticQMessageBox::critical( QWidget *parent, const QString &title, const QString &text,
        QMessageBox::StandardButtons buttons ,
        QMessageBox::StandardButton defaultButton ) {
    return custom( QMessageBox::Critical, parent, title, text, buttons , defaultButton ) ;
}


#endif      // QT_VERSION
