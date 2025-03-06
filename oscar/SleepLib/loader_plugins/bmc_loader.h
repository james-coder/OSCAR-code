#ifndef BMCLOADER_H
#define BMCLOADER_H

#include <QVector>
#include <QFile>
#include "SleepLib/machine.h" // Base class: MachineLoader
#include "SleepLib/machine_loader.h"
#include "SleepLib/profiles.h"
#include "SleepLib/loader_plugins/bmcDataParsing.h"

class EventList;

const int bmc_version = 1;
const QString bmc_class_name = "BMC";





class BmcLoader : public CPAPLoader
{
    Q_OBJECT
public:
    BmcLoader();

    void setSessionMachineSettings(BmcDateSession*, Session*);
    void setSessionRespiratoryEvents(BmcSession*, Session*);
    void setSessionWaveforms(BmcSession*, Session*);

    static void Register();

    virtual MachineInfo PeekInfo(const QString & path);

    virtual bool Detect(const QString & path);

    virtual int Open(const QString &);

    virtual const QString &loaderName() { return bmc_class_name; }

    virtual int Version() { return bmc_version; }

    virtual MachineInfo newInfo() {
            return MachineInfo(MT_CPAP, 0, bmc_class_name, QObject::tr("BMC"), QString(),
            QString(), QString(), QObject::tr("BMC"), QDateTime::currentDateTime(), bmc_version);
        }

    virtual void initChannels();

    virtual QString PresReliefLabel();
    virtual ChannelID PresReliefMode();
    virtual ChannelID PresReliefLevel();
    virtual ChannelID CPAPModeChannel();
};

#endif // BMCLOADER_H
