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

class BmcLoader;
class BmcDataLink;
class BmcData;

class BmcLoaderTask: public ImportTask
{
public:
    BmcLoaderTask(BmcLoader* machineLoader, Machine* machine, BmcData* bmcData, BmcDataLink *dataLink, int totalLinkCount, int currentLinkIdx):
        bmcLoader(machineLoader),
        mach(machine),
        bmc(bmcData),
        bmcLink(dataLink),
        totalLinksToImport(totalLinkCount),
        currentLinkIndex(currentLinkIdx)
    {}

    BmcLoader* bmcLoader;
    Machine* mach;
    BmcData* bmc;
    BmcDataLink* bmcLink;
    int totalLinksToImport;
    int currentLinkIndex;

    virtual void run();
};



class BmcLoader : public CPAPLoader
{    
public:
    BmcLoader();

    void setSessionMachineSettings(BmcDateSession*, Session*);
    void setSessionRespiratoryEvents(BmcSession*, Session*);
    void setSessionWaveforms(BmcSession*, Session*);
        

    virtual bool Detect(const QString & path);

    virtual MachineInfo PeekInfo(const QString & path);

    virtual void initChannels();

    virtual int Open(const QString &);

    static void Register();

    virtual const QString &loaderName() { return bmc_class_name; }

    virtual int Version() { return bmc_version; }

    virtual MachineInfo newInfo() {
            return MachineInfo(MT_CPAP, 0, bmc_class_name, QObject::tr("BMC"), QString(),
            QString(), QString(), QObject::tr("BMC"), QDateTime::currentDateTime(), bmc_version);
        }

    

    

    virtual QString PresReliefLabel();
    virtual ChannelID PresReliefMode();
    virtual ChannelID PresReliefLevel();
    virtual ChannelID CPAPModeChannel();

    int sessionsLoaded;
};

#endif // BMCLOADER_H
