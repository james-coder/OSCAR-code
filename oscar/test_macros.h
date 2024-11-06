/* Test macros Implemntation
 *
 * Copyright (c) 2019-2024 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

/*
These functions will display formatted debug information.
The macro TEST_MACROS_ENABLED will enable these macros to display information
When The macro TEST_MACROS_ENABLED is undefined then these marcos will expand to white space.

When these macos are used then debugging is disabled
When only these macos are used then debugging is disabled.
SO for production code rename TEST_MACROS_ENABLED to TEST_MACROS_ENABLEDoff

###########################################
The the following to source cpp files
to turn on the debug macros for use.

#define TEST_MACROS_ENABLED
#include <test_macros.h>

To turn off the the test macros.
#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>
###########################################

These macrio are used to help in understanding the exist code.
The data in displayed in the debug log file.

All debug statements can be turned or off by a single variable in each file.

if TEST_MACROS_ENABLED is defined then then test macros are enabled.
#define TEST_MACROS_ENABLEDoff      #to turn off test macros
#define TEST_MACROS_ENABLED         # to on macro
#include <test_macros.h>            # to include.

The macros are designed to to be easy to be seen and found.
1) Basic debug I use criical so there are easily forund in the log file.
2) The file name , line number and function/method name can be displayed.
3) macros can be used to display data.
4) There are display macros display data as needed.
5) if TEST_MACROS_ENABLED is not defined then all macros and there contents produce nothiing.



Examples Code               Log FIle
======================      =========================================================

API OPTIONS
================
// API DEBUG macros start with DEBUGC so that they can always be easily found in code.
DEBUGCI         //  Critical with information (Filename,lineNumber,method/fuction)
DEBUGCIS        //  Critical with information (Filename,lineNumber)
DEBUGCT         //  Critical with time and information(Filename,lineNumber)

Examples
DEBUGCIS ;                              Critical: MinutesAtPressure[218]
DEBUGCI ;                               Critical: MinutesAtPressure[219]PressureInfo
DEBUGCT ;                               Critical: 10:44:43.942 MinutesAtPressure[220]PressureInfo

DISPLAY OPTIONS
================
EXPRESSION are valid C expressions taht can be display using <<
Display Values
--------------

Q( EXPRESSION )                         Display EXPRESSION quoted followed by its value
O( EXPRESSION )                         Display the EXRESSIONS value  - no label
Z( EXPRESSION )                         ALWAYS return empty space
Add a description
QQ( TEXT , EXPRESSION )                 Displays EXPRESSION with the Description TEXT (quoted)
OO( TEXT , EXPRESSION )                 Display the EXRESSIONS value  - no Description
ZZ( TEXT , EXPRESSION )                 ALWAYS return empty space

Examples
DEBUGCI Q(code) QQ(Channelid,code);     Critical: MinutesAtPressure[221]PressureInfo code: 4364 Channelid: 4364
DEBUGCI O(code) OO(Channelid,code);     Critical: MinutesAtPressure[221]PressureInfo 4364 4364
DEBUGCI Z(code) ZZ(Channelid,code);     Critical: MinutesAtPressure[222]PressureInfo

Display Schema codes
--------------------
SCHEMA_CODE_ID may be either the Channel id or code.
DEBUGCI FULLNAME( SCHEMA_CODE_ID ) ;    Displays the FULLNAME of the channel (translateabled)
DEBUGCI NAME( SCHEMA_CODE_ID ) ;       Displays the is in hex and the name of the code.

Examples
DEBUGCI FULLNAME(code) ;                Critical: MinutesAtPressure[224]PressureInfo EPAP
DEBUGCI NAME(code) ;                    Critical: MinutesAtPressure[225]PressureInfo EPAP:0x110E

Display Date Time macro from EPOCH
----------------------------------
DEBUGCI DATE( EPOCH ) ;                 Display the DATE
DEBUGCI TIME( EPOCH ) ;                 Display the time
DEBUGCI DATETIME( EPOCH ) ;             Displays the full date time
DEBUGCI DATETIMEUTC( EPOCH ) ;          Displays the full date in UTC

Examples
DEBUGCI DATE(minTime) ;                 Critical: MinutesAtPressure[226]PressureInfo 31Jan2024
DEBUGCI TIME(minTime) ;                 Critical: MinutesAtPressure[227]PressureInfo 13:02:00.000
DEBUGCI DATETIME(minTime) ;             Critical: MinutesAtPressure[228]PressureInfo 31Jan2024 13:02:00.000
DEBUGCI DATETIMEUTC(minTime) ;          Critical: MinutesAtPressure[229]PressureInfo 31Jan2024 18:02:00.000 UTC

Display utilities
----------------
DEBUGCI COMPILER ;                      Display the version of the clang or gcc compiler. other wise nothing

Example
DEBUGCI COMPILER ;                      Critical: MinutesAtPressure[230]PressureInfo clang++:18.1.3 (1ubuntu1)

Combined Examples
-----------------
DEBUGCI FULLNAME(code) DATETIME(minTime) QQ(Minutes, ((maxTime-minTime)/60000) ) ;
                                        Critical: MinutesAtPressure[231]PressureInfo EPAP 31Jan2024 13:02:00.000 Minutes: 1120

Control feature
---------------
IF                                      Expands to "if" or to empty space. Needs to test for conditions.

Examples
--------
IF (day) DEBUGCI O(day->date());        Critical: MinutesAtPressure[659]SetDay QDate(2024-01-31)
                                        Sometimes day is null at this line.
*/

//==========================================================================
//==========================================================================
// START OF MACRO DEFINITIONS

#ifndef TEST_MACROS_FILE_ONCE
#define TEST_MACROS_FILE_ONCE

// This enables or disbale the test macros.
#if defined( TEST_MACROS_ENABLED ) || false
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include <QDateTime>
#include "SleepLib/schema.h"


// define submacros to call logging functions
#define DEBUGXD   qDebug().noquote(
#define DEBUGXC   qCritical().noquote()
#define DEBUGXW   qWarning().noquote()

// define sub-macros for what data to display
// filename
#define DEBUGXF         <<QString("%1").arg(QFileInfo( __FILE__).baseName())

// filename linenumber
#define DEBUGXFL        <<QString("%1[%2]").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)

// filename linenumber Method
#define DEBUGXFLM       <<QString("%1[%2]%3").arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)

// buildTime
#define DEBUGXT         <<QString("%1").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz"))

// buildTime filename
#define DEBUGXTF        <<QString("%1 %2").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName())

// buildTime filename linenumber
#define DEBUGXTFL       <<QString("%1 %2[%3]").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__)

// buildTime filename linenumber Method
#define DEBUGXTFLM      <<QString("%1 %2[%3]%4").arg(QDateTime::currentDateTime().time().toString("hh:mm:ss.zzz")).arg(QFileInfo( __FILE__).baseName()).arg(__LINE__).arg(__func__)

// END internal use in the file

// Note users could make their own API using DEBUGXD, DEBUGXC , or DEBUGXW followed by DEBUGXF.. or DEBUGXT..
// since all macros will be empty when test macros are turned off. so the end user does not have make them empry by default.

// define API macro to display data

// Do nothing - quickly take an item temporally from display
#define Z( EXPRESSION ) 			/* comment out display of variable */
#define ZZ( A ,EXPRESSION ) 		/* comment out display of variable */

// Macros to display variables
#define Q( EXPRESSION ) 			<<  "" #EXPRESSION ":" << EXPRESSION
#define O( EXPRESSION ) 			<<  EXPRESSION
#define QQ( TEXT , EXPRESSION) 		<<  #TEXT ":" << EXPRESSION
#define OO( TEXT , EXPRESSION) 		<<  EXPRESSION


// Macros to display chanel name and hex id
#define NAME( SCHEMA_CODE_ID ) <<  QString(("%1:0x%2")) .arg(schema::channel[ SCHEMA_CODE_ID  ].label()) .arg( QString::number(schema::channel[ SCHEMA_CODE_ID  ].id(),16).toUpper() )
#define FULLNAME( SCHEMA_CODE_ID ) 		 <<  schema::channel[ SCHEMA_CODE_ID ].fullname()

//display the date of an epoch time stamp "qint64"
#define DATE( EPOCH ) 			<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("ddMMMyyyy")
#define TIME( EPOCH ) 		    <<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("hh:mm:ss.zzz")
#define DATETIME( EPOCH ) 		<<  QDateTime::fromMSecsSinceEpoch( EPOCH ).toString("ddMMMyyyy hh:mm:ss.zzz")
#define DATETIMEUTC( EPOCH ) 	<<  QDateTime::fromMSecsSinceEpoch( EPOCH ,Qt::UTC).toString("ddMMMyyyy hh:mm:ss.zzz UTC")

//Condition execution
#define IF( EXPRESSION )    if (EXPRESSION )

//Define compiler version.
#ifdef __clang__
    #define COMPILER O(QString("clang++:%1").arg(__clang_version__) );
#elif __GNUC_VERSION__
    #define COMPILER O(QString("GNUC++:%1").arg("GNUC").arg(__GNUC_VERSION__)) ;
#else
    #define COMPILER
#endif

//==========================================================================
#else       // TEST_MACROS_ENABLED
// Turn debugging off.  macros expands to white space

#define DEBUGXD
#define DEBUGXW
#define DEBUGXC

#define DEBUGXF
#define DEBUGXFL
#define DEBUGXFLM
#define DEBUGXT
#define DEBUGXTF
#define DEBUGXTFL
#define DEBUGXTFLM

#define Z( XX )
#define ZZ( XX , YY)
#define Q( XX )
#define QQ( XX , YY )
#define O( XX )
#define OO( XX , YY )
#define NAME( id)
#define FULLNAME( id)
#define DATE( XX )
#define TIME( XX )
#define DATETIME( XX )
#define DATETIMEUTC( XX )
#define COMPILER
#define IF( XX )

#endif  // TEST_MACROS_ENABLED

#endif  // TEST_MACROS_FILE_ONCE

//defined API macros
//API should start with DEBUGC so that they can always be easily found in code.

#define DEBUGCI         DEBUGXC DEBUGXFLM
#define DEBUGCIS        DEBUGXC DEBUGXFL
#define DEBUGCT         DEBUGXC DEBUGXTFLM

// obsoluted macros that are still usd.
#define DEBUGF          DEBUGXC DEBUGXFL
#define DEBUGFC         DEBUGXC DEBUGXFLM


