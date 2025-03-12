
#define TEST_MACROS_ENABLEDoff
#include <test_macros.h>

#include <QApplication>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QDataStream>
#include <QMessageBox>
#include <QDebug>
#include <cmath>

#include "SleepLib/schema.h"
#include "SleepLib/importcontext.h"
#include "vrem_loader.h"
#include "SleepLib/session.h"
#include "SleepLib/calcs.h"
#include "SleepLib/crypto.h"
#include "rawdata.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QVector>
#include <QMap>
#include <QStringList>
#include <cmath>

#include <QTextStream>
#include <QListWidget>
#include <regex>
#include <string>
#include <vector>
#include <QQueue>
#include <iostream>
#include <fstream>


#ifdef DEBUG_EFFICIENCY
#include <QElapsedTimer>  // only available in 4.8
#endif

ChannelID vREM_Mode, vREM_SmartStart , vREM_Flex , vREM_FlexLevel;
ChannelID vREM_Start,vREM_Stop;

bool vREM_initialised = false;
int intPressure = 0;
qint64 SessionStartTime = 0;
qint64 SessionEndTime = 0;
QString vREMserial = "";
QQueue<qint16> valueQueue;  // Queue to store the last 4 values for moving average

QString VREMLoader::PresReliefLabel() { return QString("");}
ChannelID VREMLoader::PresReliefMode() { return vREM_FlexLevel; }
ChannelID VREMLoader::PresReliefLevel() { return vREM_Flex; }
ChannelID VREMLoader::CPAPModeChannel() { return vREM_Mode; }
VREMLoader::VREMLoader()
{
    m_type = MT_CPAP;
}
VREMLoader::~VREMLoader()
{
}

bool VREMLoader::Detect(const QString & selectedPath)
{
    QString path = selectedPath;
    if (!GetvREMPath(path).isEmpty()) {
        path = GetvREMPath(selectedPath);
        return true;
    }
    return false;
}

QString VREMLoader::GetvREMPath(const QString & path)
{
    QString outpath = "";
    QDir root(path);
    if (!root.exists()) {
        return outpath;
    }
    if (root.dirName().toUpper().startsWith("VREM")) {
        QDir dir(path);
        if (!dir.exists()) {
            qWarning() << "Directory does not exist:" << path;
            return outpath;
        }
        // List all files in the directory
        QStringList files = dir.entryList(QDir::Files);

        bool piExists = files.contains("PI.txt", Qt::CaseInsensitive);
        bool diExists = files.contains("DI.txt", Qt::CaseInsensitive);

        if (piExists && diExists) {
            return path;
        }
    }
    QStringList dirs = root.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Hidden | QDir::NoSymLinks);
    for (auto & dirName : dirs) {
        if (dirName.toUpper().startsWith("VREM")) {
            QDir dir(path + QDir::separator() + dirName);
            if (!dir.exists()) {
                qWarning() << "Directory does not exist:" << dirName;
                return outpath;
            }
            // List all files in the directory
            QStringList files = dir.entryList(QDir::Files);

            bool piExists = files.contains("PI.txt", Qt::CaseInsensitive);
            bool diExists = files.contains("DI.txt", Qt::CaseInsensitive);

            if (piExists && diExists) {
                outpath = path + QDir::separator() + dirName;
                break;
            }
        }
    }
    return outpath;
}

MachineInfo VREMLoader::PeekInfo(const QString & pathPeek)
{
    if (!Detect(pathPeek)) {
        return MachineInfo();
    }
    QString path = "";
    if (!GetvREMPath(pathPeek).isEmpty()) {
        path = GetvREMPath(pathPeek);
    }
    QDir dir(path);
    QString path1;
    if (!dir.isEmpty() && dir.exists()) {
        dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        dir.setSorting(QDir::Name);
        QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (auto & mfi : file_list) {
            if (QDir::match("DI.txt", mfi.fileName())) {
                path1 = mfi.absoluteFilePath();
                break; // Break the loop once the file is found
            }
        }
    }
    MachineInfo info = newInfo();
    if (!path1.isEmpty())
    {
        QFile f(path1);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString line = f.readLine().trimmed();
            f.close();
            info.brand = "vREM";
            info.serial = vREMserial;
            info.version = 1;
            info.type = MachineType::MT_CPAP;
            info.modelnumber = "vREM CPAP "+ line.split("-")[0];
        } else {
            qDebug() << "Failed to open file: " << path1;
        }
    }
    return info;
}

int VREMLoader::Open(const QString & selectedPath)
{
    Q_ASSERT(m_ctx);
    if (!Detect(selectedPath)) {
        return -1;
    }
    QString path = selectedPath;
    if (!GetvREMPath(path).isEmpty()) {
        path = GetvREMPath(selectedPath);
    }
    QDir dir(path);

    if (!dir.exists() || (!dir.isReadable())) {
        return 0;
    }

    emit updateMessage(QObject::tr("Getting Ready..."));
    QCoreApplication::processEvents();
    emit setProgressValue(0);

    QStringList Odata;
    QString programfile;
    FindSessionDirsAndPropertiesvREM(path, programfile,Odata);
    if (Odata.isEmpty() )
    {
        return -1;
    }
    int task = 0;
    QVector<vREMData> VREMDATA;
    QFile f(programfile);

    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            vREMData vData;

            QStringList parts = line.split(",");

            if (parts.size() >= 1) {  // Ensure there are enough parts
                vData.start_time = parts[1].toLongLong();
                vData.end_time = parts[2].toLongLong();
                vData.max_pressure =parts[3];
                vData.min_pressure =parts[4];
                vData.ramp_pressure =parts[5];
                vData.rampTime =parts[6];
                vData.flex =parts[7];
                vData.flex_level =parts[8];
                vData.mask_type =parts[9];
                vData.humidifier =parts[10];
                vData.humidifier_level =parts[11];
                vData.mode =parts[12];
                VREMDATA.append(vData);
            } else {
                qWarning() << "Line format incorrect, skipping:" << line;
            }
        }
        f.close();
    } else {
        qWarning() << "Failed to open file:" << programfile;
    }
    for (const QString& ODFolderPath : Odata) {
        QDir dir(ODFolderPath);
        if (!dir.exists()) {
            qDebug() << "Directory does not exist:" << ODFolderPath;
            continue;
        }
        QString folderName = dir.dirName();
        vREMserial = folderName.remove(0,2);
        QStringList files = dir.entryList(QDir::Files);
        QStringList Odatas;
        for (const QString& file : files) {
            QFileInfo fi(dir, file); // QFileInfo object to get full file path
            QString filename = fi.fileName(); // Get the file name
            if (filename.startsWith("OD", Qt::CaseInsensitive)) {
                Odatas.push_back(fi.canonicalFilePath());
            }
        }
        m_ctx->CreateMachineFromInfo(PeekInfo(path));
        Machine *machine = p_profile->CreateMachine(PeekInfo(path));
        task += OscarDataParser(Odatas ,machine , VREMDATA);
    }
    QString backupPath = context()->GetBackupPath() + path.section("/", -1)+"/";
    QDir backupDir(QFileInfo(backupPath).path());
    if (!backupDir.exists()) {
        if (!backupDir.mkpath(backupDir.path())) {
            qWarning() << "Failed to create backup directory: " << backupDir.path();
            return 0;
        }
    }

    if (QDir::cleanPath(path).compare(QDir::cleanPath(backupPath)) != 0) {
        copyPath(path, backupPath);
    }
    if (task == 0) { return -1; }
    return task;
}

double applyGainvREM(QString value, double gain) {
    return (gain*(double)value.toUInt());
}

int packetType(QByteArray hexByte)
{
    if (hexByte.isEmpty()) {
        return 0; // Handle empty input
    }
    QString hexString = "4f-50-45-41";
    QStringList hexList = hexString.split('-');
    for (const QString &hex : hexList) {
        if (hex == hexByte.toHex())
        {
            return 9;
        }else if (hexByte.toHex() == "46") // flow -46
        {
            return 29;
        }  
    }
    return 0;
}

QPair<QByteArray, int> reduce7BBytes(const QByteArray &byteArray) {
    QByteArray result;
    bool lastWas7B = false;
    QByteArray SecondByte = byteArray.mid(1, 1);
    int reductionCount = 0;
    for (int i = 0; i < byteArray.size(); ++i) {
        char currentByte = byteArray[i];
        int asciiValue = static_cast<int>(currentByte);
        // Compare with the hex value 0x7b (which is the ASCII value for '{')
        if (asciiValue == 0x7b) { // Check if the current byte is 0x7B
            if (lastWas7B) {
                // If the last byte was also 0x7B, skip this byte
                lastWas7B = false;
                reductionCount++;
                continue;
            } else {
                // Otherwise, add the current byte and mark lastWas7B as true
                result.append(currentByte);
                lastWas7B = true;
            }
        } else if (i > 3 && lastWas7B && packetType(SecondByte) == 0)
        {
            qWarning() << byteArray.toHex()  <<"Invalid bytes detected: a start byte is found in the middle of the array.";
        } else {
            // If the current byte is not 0x7B, add it and reset lastWas7B
            result.append(currentByte);
            lastWas7B = false;
        }
    }

    return qMakePair(result, reductionCount);
}

void flowParser(const QByteArray &flowArray,EventList* flow,/*EventList* leak,*/qint64 time){
    QByteArray flowValuesArray = flowArray.mid(2, 50);
    qint64 flowTime = (time);
    qint16* waveformValues = new qint16[50];
    qint16* averageValues = new qint16[50];
    for (int i = 0; i < flowValuesArray.size(); ++i) {
        uint8_t currentByte = static_cast<uint8_t>(flowValuesArray[i]);
        int8_t signedByte = static_cast<int8_t>(currentByte);
        // Convert to signed int
        int signedValue = static_cast<int>(signedByte);
        waveformValues[i] = static_cast<qint16>(signedValue);
        valueQueue.enqueue(waveformValues[i]);
        // If the queue has more than 4 values, remove the oldest
        if (valueQueue.size() > 4) {
            valueQueue.dequeue();
        }
        // Calculate the moving average of the last 4 values
        int sum = 0;
        for (qint16 val : valueQueue) {
            sum += val;
        }
        averageValues[i] = sum / valueQueue.size();
        
    }
    flow->AddWaveform(flowTime, averageValues,50 , 20);
}
void updatePressure(qint64 time ,EventList* pressure){
    if (intPressure != 0)
    {
        qint16 waveformValue = static_cast<qint16>(intPressure);
        pressure->AddWaveform(time, &waveformValue,1 , 1000);
    }
}

void eventParser(const QByteArray &eventArray, Session* session){
    QByteArray eventIdentifier = eventArray.mid(1, 1);
    QByteArray eventValuesArray = eventArray.mid(2, 4);
    uint32_t value = 0;
    uint8_t unitTime[4] ; 
    for (int i = 0; i < eventValuesArray.size(); i++)
    {
        unitTime[i] = static_cast<uint8_t>(eventValuesArray[i]);
    }
    value = (unitTime[3] << 24) |(unitTime[2] << 16) |(unitTime[1] << 8) |(unitTime[0]);
    QDateTime dateTime;
    dateTime.setSecsSinceEpoch(static_cast<qint64>(value));
    QByteArray eventValue = eventArray.mid(6,1);
    if (eventValuesArray.size() != 4) {
        qDebug() << "Incorrect byte array size.";
        return;
    }
    char currentByte = eventIdentifier[0];
    int asciiValue = static_cast<int>(currentByte);
    if (asciiValue == 0x50)
    {
        uint8_t byteValue = static_cast<uint8_t>(eventValue[0]);
        intPressure = static_cast<int>(byteValue);
    }else if (asciiValue == 0x45)
    {
        if (eventValue.toHex() == "01")
        {
            EventList* apnea = session->AddEventList(CPAP_Hypopnea, EVL_Event, 1);
            apnea->AddEvent(static_cast<qint64>(value)*1000, 10);
        }
        else if (eventValue.toHex() == "02")
        {
            EventList* Hapnea = session->AddEventList( CPAP_AllApnea, EVL_Event, 1);
            Hapnea->AddEvent(static_cast<qint64>(value)*1000, 10);   
        }
    }
}

qint32 StartTime(QByteArray startBytes)
{
    uint32_t value = 0;
    uint8_t unitTime[4] ; 
    for (int i = 0; i < startBytes.size(); i++)
    {
        unitTime[i] = static_cast<uint8_t>(startBytes[i]);
    }

    value = (unitTime[3] << 24) |(unitTime[2] << 16) |(unitTime[1] << 8) |(unitTime[0]);
    return static_cast<qint32>(value);
}
int VREMLoader::OscarDataParser(QStringList OdataList,Machine* machine,QVector<vREMData> &vREMdata)
{
    int task = 0;
    bool isValidData = false;
    for (const QString &Odata : OdataList) {
        QFile file(Odata);
        qint64 time = 0;
        
        if (!file.open(QIODevice::ReadOnly)) {  // Open in binary mode, no QIODevice::Text
            qWarning() << "Failed to open file:" << Odata;
            return 0;
        }

        const qint64 bufferSize = 1024; // Buffer size of 1 KB
        QByteArray buffer(bufferSize, 0); // Initialize buffer with zeros
        QByteArray byteArray; // To accumulate all read data

        qint64 bytesRead = 0;
        while ((bytesRead = file.read(buffer.data(), bufferSize)) > 0) {
            // Append the read bytes to byteArray
            byteArray.append(buffer.left(bytesRead)); // Use left() to only append the bytes read
        }
        file.close();
        int index = 0;
        EventList * flow = NULL;
        EventList * pressure = NULL;
        for (; index < byteArray.size(); ++index) {
            QByteArray hexByte; 
            QByteArray Byte2;
            if (index + 1 < byteArray.size()) {
                hexByte = QByteArray(1, byteArray[index]);
                Byte2 = QByteArray(1, byteArray[index + 1]);
            }
            if (hexByte.toHex() == "7b" && Byte2.toHex() == "4f" && isValidData == false)
            {
                QByteArray subArray = byteArray.mid(index, 9);
                QPair<QByteArray, int> result = reduce7BBytes(subArray);
                QByteArray reducedArray = result.first;
                int reductionCount = result.second;
                index+=9;
                while (reductionCount > 1)
                {
                    QPair<QByteArray, int> result = reduce7BBytes(byteArray.mid(index, reductionCount));
                    reducedArray.append(result.first);
                    reductionCount = result.second;
                    index += reductionCount;
                }
                if (reductionCount == 1)
                {
                    reducedArray.append(byteArray.mid(index, reductionCount));
                }
                index-=9;
                qint32 value  = StartTime(reducedArray.mid(2, 4));
                qint64 timeinmillies = value;
                // if (context()->SessionExists(value)) {
                //     continue;
                // }
                
                time = timeinmillies*1000;
                for (auto data : vREMdata)
                {
                    if (data.start_time == time && data.start_time < data.end_time)
                    {
                        int mode = data.mode.toInt();
                        session = new Session(machine,value);
                        session->SetChanged(true);
                        SessionStartTime = data.start_time;
                        SessionEndTime = data.end_time;
                        session->set_first(SessionStartTime);
                        intPressure = 0;
                        session->set_last(SessionEndTime);
                        session->settings[CPAP_PressureMin] = data.min_pressure.toFloat();
                        session->settings[CPAP_PressureMax] = data.max_pressure.toFloat();
                        session->settings[CPAP_RampTime] = data.rampTime.toInt();
                        session->settings[CPAP_RampPressure] = data.ramp_pressure.toInt();
                        if (mode == 1)
                        {
                            session->settings[vREM_Flex] = 0;
                            session->settings[CPAP_Mode] = 1;
                            session->settings[vREM_Mode] = 1;
                        } else if (mode == 2)
                        {
                            session->settings[vREM_Flex] = 1;
                            session->settings[vREM_FlexLevel] = data.flex_level.toInt();
                            session->settings[CPAP_Mode] = 1;
                            session->settings[vREM_Mode] = 2;
                        } else if (mode == 3)
                        {
                            session->settings[vREM_Flex] = 0;
                            session->settings[CPAP_Mode] = 2;
                            session->settings[vREM_Mode] = 2;
                        } else if (mode == 4)
                        {
                            session->settings[vREM_Flex] = 1;
                            session->settings[vREM_FlexLevel] = data.flex_level.toInt();
                            session->settings[CPAP_Mode] = 2;
                            session->settings[vREM_Mode] = 2;
                        } else
                        {
                            session->settings[CPAP_Mode] = 0;
                            session->settings[vREM_Mode] = 0;
                        }
                        task+=1;
                        isValidData = true;
                        flow = session->AddEventList(CPAP_FlowRate, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, 20);
                        // leak = session->AddEventList(CPAP_Leak, EVL_Waveform, 1.0f, 0.0f, 0.0f, 0.0f, 1000);
                        pressure = session->AddEventList(CPAP_Pressure, EVL_Waveform, 0.1f, 0.0f, 0.0f, 0.0f, 1000);
                    }
                    
                };
                if (!isValidData) {
                    qDebug() << "No matching time found for "<< time << " in Program Information.";
                }
            } else if (hexByte.toHex() == "7b" && Byte2.toHex() == "4f" && isValidData == true)
            {
                qDebug() << "Error Occur on Start/End packet.";
            }
            int packageTypeInt = packetType(Byte2);
            if (hexByte.toHex() == "7b" && (packageTypeInt == 9 || packageTypeInt == 29) && isValidData == true)
            {
                if (packageTypeInt == 9)
                {
                    QByteArray subArray = byteArray.mid(index, 9);
                    QPair<QByteArray, int> result = reduce7BBytes(subArray);
                    QByteArray reducedArray = result.first;
                    int reductionCount = result.second;
                    index+=9;
                    while (reductionCount > 1)
                    {
                        QPair<QByteArray, int> result = reduce7BBytes(byteArray.mid(index, reductionCount));
                        reducedArray.append(result.first);
                        reductionCount = result.second;
                        index += reductionCount;
                    }
                    if (reductionCount == 1)
                    {
                        reducedArray.append(byteArray.mid(index, reductionCount));
                        index+=1;
                    }
                    index-=9;
                    eventParser(reducedArray, session);
                }else if (packageTypeInt == 29 && time != 0)
                {
                    QByteArray subArray = byteArray.mid(index, 54);
                    QPair<QByteArray, int> result = reduce7BBytes(subArray);
                    QByteArray reducedArray = result.first;
                    int reductionCount = result.second;
                    index+=54;
                    while (reductionCount > 1)
                    {
                        QPair<QByteArray, int> result = reduce7BBytes(byteArray.mid(index, reductionCount));
                        reducedArray.append(result.first);
                        reductionCount = result.second;
                        index += reductionCount;
                    }
                    if (reductionCount == 1)
                    {
                        reducedArray.append(byteArray.mid(index, reductionCount));
                        index+=1;
                    }
                    index-=54;
                    flowParser(reducedArray, flow, time );
                    updatePressure(time , pressure);
                    time += 1000;
                }
            }
            if (hexByte.toHex() == "7b" && Byte2.toHex() == "41" && isValidData == true)
            {
                QByteArray subArray = byteArray.mid(index, 9);
                    QPair<QByteArray, int> result = reduce7BBytes(subArray);
                    QByteArray reducedArray = result.first;
                    int reductionCount = result.second;
                    index+=9;
                    while (reductionCount > 1)
                    {
                        QPair<QByteArray, int> result = reduce7BBytes(byteArray.mid(index, reductionCount));
                        reducedArray.append(result.first);
                        reductionCount = result.second;
                        index += reductionCount;
                    }
                    if (reductionCount == 1)
                    {
                        reducedArray.append(byteArray.mid(index, reductionCount));
                        index+=1;
                    }
                    index-=10;
                isValidData = false;
                valueQueue.clear();
                session->UpdateSummaries();
                session->Store(machine->getDataPath());
                machine->AddSession(session);
            }
        }
    }
    return task;
}
void VREMLoader::FindSessionDirsAndPropertiesvREM(const QString & path, QString & propertyfile, QStringList & Odata)
{
    QDir dir(path);
    dir.setFilter(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);
    QFileInfoList flist = dir.entryInfoList();
    QString filename;
    for (int i = 0; i < flist.size(); i++) {
        QFileInfo fi = flist.at(i);
        filename = fi.fileName();
        if (fi.isDir()) {
            if (filename.startsWith("OD", Qt::CaseInsensitive)) {
                Odata.push_back(fi.canonicalFilePath());
            }
        }
        if (filename.compare("PI.txt",Qt::CaseInsensitive) == 0) {
            propertyfile = fi.canonicalFilePath();
        }
    }
}

void VREMLoader::Register()
{
    if (vREM_initialised) { return; }
    qDebug() << "Registering VREMLoader";
    RegisterLoader(new VREMLoader());
    vREM_initialised = true;
}

using namespace schema;

Channel vREMChannels;

void VREMLoader::initChannels() {
    using namespace schema;
    int vREM_CHANNELS = 0xeA56;

    Channel * chan = new Channel(vREM_Mode = vREM_CHANNELS , SETTING, MT_CPAP,   SESSION,
        "vREM_Mode", QObject::tr("Mode"), QObject::tr("CPAP Mode"), QObject::tr("Mode"), "", LOOKUP, Qt::green);
    channel.add(GRP_CPAP, chan);

    chan->addOption(0, STR_TR_None);
    chan->addOption(1, STR_TR_CPAP);    // strings have already been translated
    chan->addOption(2, STR_TR_APAP);    // strings have already been translated

    channel.add(GRP_CPAP, chan = new Channel(vREM_FlexLevel = vREM_CHANNELS+1, SETTING,  MT_CPAP,  SESSION,
        "vREMFlexLevel", QObject::tr("Flex Level"),
        QObject::tr("vREM pressure relief mode."),
        QObject::tr("Flex Level"),
        "", LOOKUP, Qt::green));
    chan->addOption(0, STR_TR_None);
    chan->addOption(1,QObject::tr("1"));
    chan->addOption(2,QObject::tr("2"));
    chan->addOption(3,QObject::tr("3"));
    chan->addOption(4,QObject::tr("4"));
    chan->addOption(5,QObject::tr("5"));
    chan->addOption(6,QObject::tr("6"));
    chan->addOption(7,QObject::tr("7"));

    channel.add(GRP_CPAP, chan = new Channel(vREM_Flex = vREM_CHANNELS+2 , SETTING, MT_CPAP,   SESSION,
        "vREMFlexSet",
        QObject::tr("Flex Mode"),
        QObject::tr("vREM pressure relief setting."),
        QObject::tr("Flex Mode"),
        "", LOOKUP, Qt::blue));
    chan->addOption(false, STR_TR_None);
    chan->addOption(true, STR_TR_On);
    chan->setShowInOverview(true);
}

