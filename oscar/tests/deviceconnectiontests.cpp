/* Device Connection Unit Tests
 *
 * Copyright (c) 2020-2022 The OSCAR Team
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#include "deviceconnectiontests.h"
#include "SleepLib/deviceconnection.h"

// TODO: eventually this should move to serialoximeter.h
#include "SleepLib/loader_plugins/cms50f37_loader.h"

#include <QTemporaryFile>

void DeviceConnectionTests::testSerialPortInfoSerialization()
{
    QString serialized;
    
    // With VID and PID
    const QString tag = R"(<serial portName="cu.SLAB_USBtoUART" systemLocation="/dev/cu.SLAB_USBtoUART" description="CP210x USB to UART Bridge Controller" manufacturer="Silicon Labs" serialNumber="0001" vendorIdentifier="0x10C4" productIdentifier="0xEA60"/>)";
    SerialPortInfo info = SerialPortInfo(tag);
    Q_ASSERT(info.isNull() == false);
    Q_ASSERT(info.portName() == "cu.SLAB_USBtoUART");
    Q_ASSERT(info.systemLocation() == "/dev/cu.SLAB_USBtoUART");
    Q_ASSERT(info.description() == "CP210x USB to UART Bridge Controller");
    Q_ASSERT(info.manufacturer() == "Silicon Labs");
    Q_ASSERT(info.serialNumber() == "0001");
    Q_ASSERT(info.hasVendorIdentifier());
    Q_ASSERT(info.hasProductIdentifier());
    Q_ASSERT(info.vendorIdentifier() == 0x10C4);
    Q_ASSERT(info.productIdentifier() == 0xEA60);
    serialized = info;
    Q_ASSERT(serialized == tag);
    
    // Without VID or PID
    const QString tag2 = R"(<serial portName="cu.Bluetooth-Incoming-Port" systemLocation="/dev/cu.Bluetooth-Incoming-Port" description="incoming port - Bluetooth-Incoming-Port" manufacturer="" serialNumber=""/>)";
    SerialPortInfo info2 = SerialPortInfo(tag2);
    Q_ASSERT(info2.isNull() == false);
    Q_ASSERT(info2.portName() == "cu.Bluetooth-Incoming-Port");
    Q_ASSERT(info2.systemLocation() == "/dev/cu.Bluetooth-Incoming-Port");
    Q_ASSERT(info2.description() == "incoming port - Bluetooth-Incoming-Port");
    Q_ASSERT(info2.manufacturer() == "");
    Q_ASSERT(info2.serialNumber() == "");
    Q_ASSERT(info2.hasVendorIdentifier() == false);
    Q_ASSERT(info2.hasProductIdentifier() == false);
    serialized = info2;
    Q_ASSERT(serialized == tag2);

    // Empty
    const QString tag3 = R"(<serial/>)";
    SerialPortInfo info3 = SerialPortInfo(tag3);
    Q_ASSERT(info3.isNull() == true);
    serialized = info3;
    Q_ASSERT(serialized == tag3);
}

void DeviceConnectionTests::testSerialPortScanning()
{
    QString string;

    DeviceConnectionManager & devices = DeviceConnectionManager::getInstance();
    devices.record(string);
    auto list1 = SerialPortInfo::availablePorts();
    auto list2 = SerialPortInfo::availablePorts();
    devices.record(nullptr);
    // string now contains the recorded XML.
    qDebug().noquote() << string;

    devices.replay(string);
    Q_ASSERT(list1 == SerialPortInfo::availablePorts());
    Q_ASSERT(list2 == SerialPortInfo::availablePorts());
    Q_ASSERT(list2 == SerialPortInfo::availablePorts());  // replaying past the recording should return the final state
    devices.replay(nullptr);  // turn off replay
    auto list3 = SerialPortInfo::availablePorts();

    // Test file-based recording/playback
    QTemporaryFile recording;
    Q_ASSERT(recording.open());
    devices.record(&recording);
    list1 = SerialPortInfo::availablePorts();
    list2 = SerialPortInfo::availablePorts();
    devices.record(nullptr);

    recording.seek(0);
    devices.replay(&recording);
    Q_ASSERT(list1 == SerialPortInfo::availablePorts());
    Q_ASSERT(list2 == SerialPortInfo::availablePorts());
    Q_ASSERT(list2 == SerialPortInfo::availablePorts());  // replaying past the recording should return the final state
    devices.replay(nullptr);  // turn off replay
    list3 = SerialPortInfo::availablePorts();
}

#define ENABLE 0

#if ENABLE
static void testDownload(const QString & loaderName)
{
    SerialOximeter * oxi = qobject_cast<SerialOximeter*>(lookupLoader(loaderName));
    Q_ASSERT(oxi);

    if (oxi->openDevice()) {
        bool open = true;
        oxi->resetDevice();
        int session_count = oxi->getSessionCount();
        qDebug() << session_count << "sessions";
        for (int i = 0; i < session_count; i++) {
            qDebug() << i+1 << oxi->getDateTime(i) << oxi->getDuration(i);
            oxi->Open("import");
            if (oxi->commandDriven()) {
                oxi->getSessionData(i);
                while (!oxi->isImporting() && !oxi->isAborted()) {
                    //QThread::msleep(10);
                    QCoreApplication::processEvents();
                }
                while (oxi->isImporting() && !oxi->isAborted()) {
                    //QThread::msleep(10);
                    QCoreApplication::processEvents();
                }
            }
            open = oxi->openDevice();  // annoyingly import currently closes the device, so reopen it
        }
        if (open) {
            oxi->closeDevice();
        }
    }
    oxi->trashRecords();
}
#endif

void DeviceConnectionTests::testOximeterConnection()
{
    // Initialize main event loop to initialize threads and enable signals and slots.
    int argc = 1;
    const char* argv = "test";
    QCoreApplication app(argc, (char**) &argv);
    
#if ENABLE
    CMS50F37Loader::Register();

    DeviceConnectionManager & devices = DeviceConnectionManager::getInstance();
    /*
    QString string;
    devices.record(string);

    // new API
    QString portName = "cu.SLAB_USBtoUART";
    {
        QScopedPointer<DeviceConnection> conn(devices.openConnection("serial", portName));
        Q_ASSERT(conn);
        Q_ASSERT(devices.openConnection("serial", portName) == nullptr);
    }
    {
        QScopedPointer<SerialPortConnection> conn(devices.openSerialPortConnection(portName));
        Q_ASSERT(conn);
        Q_ASSERT(devices.openSerialPortConnection(portName) == nullptr);
    }
    // legacy API
    SerialPort port;
    port.setPortName(portName);
    if (port.open(QSerialPort::ReadWrite)) {
        qDebug() << "port opened";
        port.close();
    }

    devices.record(nullptr);
    
    qDebug().noquote() << string;
    */
    
    QFile out("test.xml");
    Q_ASSERT(out.open(QFile::ReadWrite));
    devices.record(&out);

    QFile file("cms50f37.xml");
    if (!file.exists()) {
        qDebug() << "Recording oximeter connection";
        Q_ASSERT(file.open(QFile::ReadWrite));
        devices.record(&file);
        testDownload(cms50f37_class_name);
        devices.record(nullptr);
        file.close();
    }

    qDebug() << "Replaying oximeter connection";
    Q_ASSERT(file.open(QFile::ReadOnly));
    devices.replay(&file);
    testDownload(cms50f37_class_name);
    devices.replay(nullptr);
    file.close();
#endif
}
