#ifndef _SASMRAID_
#define _SASMRAID_

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#include <IOKit/IOKitKeys.h>
/* This is already exists in HelperLib */
/*#include <IOKit/IOLib.h>*/
namespace IOTypes {
    typedef UInt32 IOUInt32;
}
bool _megaraid_was_initf_called = FALSE;
bool _megaraid_was_probef_called = FALSE;

#include "ccbPool.h"
#include "HelperLib/HelperLib.h"

#define BaseClass IOSCSIParallelInterfaceController

enum dmawidth {
    DMA32BIT,
    DMA64BIT
};
/* Templates are denied, but let's break the
 * rules */
class SASMegaRAID: public BaseClass 
{
	OSDeclareDefaultStructors(SASMegaRAID);
private:
	class PCIHelper<SASMegaRAID>* PCIHelperP;

	IOPCIDevice *fPCIDevice;
    IOMemoryDescriptor *MemDesc;
    IOMemoryMap *map;
    IOWorkLoop *MyWorkLoop;
    IOInterruptEventSource *fInterruptSrc;
    IOCommandPool *ccbCommandPool;
    bool fMSIEnabled;
    
    struct mraid_softc *sc;
    const struct mraid_pci_device *mpd;
    
	const struct mraid_pci_device *MatchDevice();
    void AllocMem(dmawidth width); // temp decl
    bool Attach();
    bool free_start();

    bool Transition_Firmware();
    UInt32 MRAID_Read(IOByteCount offset);
    /*bool*/ void MRAID_Write(IOByteCount offset, IOByteCount data);
    
    void Done(struct mraid_ccb *ccb);    
    bool mraid_xscale_intr();
    UInt32 mraid_xscale_fw_state();
    bool mraid_ppc_intr();
    UInt32 mraid_ppc_fw_state();
    bool mraid_gen2_intr();
    UInt32 mraid_gen2_fw_state();
protected:
	virtual void free();
public:
	virtual bool init();
    virtual IOService *probe(IOService *provider, SInt32 *score);

    friend struct mraid_iop_ops;
    /* Helper Library is allowed to touch private methods */
	template <typename UserClass> friend 
    UInt32 PCIHelper<UserClass>::MappingType(UserClass*, UInt8, UInt32*);
    template <typename UserClass> friend
    bool PCIHelper<UserClass>::CreateDeviceInterrupt(UserClass *, IOService *, bool,
        void(UserClass::*)(OSObject *, void *, IOService *, int),
        bool(UserClass::*)(OSObject *, IOFilterInterruptEventSource *));
    
	//bool start(IOService *provider);
    virtual bool InitializeController(void);
	virtual void stop(IOService *provider);
    virtual void TerminateController(void);
    
    virtual bool createWorkLoop(void);
    /* Protect class members from this func */
    virtual IOWorkLoop *getWorkLoop(void) const;
    
    void interruptHandler(OSObject *owner, void *src, IOService *nub, int count);
    //virtual void HandleInterruptRequest(void);
    bool interruptFilter(OSObject *owner, IOFilterInterruptEventSource *sender);
    //virtual bool FilterInterruptRequest(void);
protected:
    virtual IOInterruptEventSource* CreateDeviceInterrupt(
        IOInterruptEventSource::Action,
        IOFilterInterruptEventSource::Filter,
        IOService *);
    virtual SCSIServiceResponse ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest);

    virtual SCSIDeviceIdentifier ReportHighestSupportedDeviceID(void);
    IOTypes::IOUInt32 ReportMaximumTaskCount(void);
    void ReportHBAConstraints(OSDictionary *constraints);
    IOTypes::IOUInt32 ReportHBASpecificTaskDataSize(void);
    bool DoesHBAPerformDeviceManagement(void);
};

struct mraid_softc {
    //struct device                sc_dev;
    void                            *sc_ih;
    //struct scsi_link             sc_link;
    
    struct mraid_iop_ops            *sc_iop;
    
    //bus_space_tag_t              sc_iot;
    //bus_space_handle_t           sc_ioh;
    //bus_space_tag_t              sc_dmat;
    
    /* Save some useful information for logical drives that is missing
     * in sc_ld_list struct */
    struct {
        UInt32                      ld_present;
        //TO-DO: It's a Mac OS X ;)
        //char                        ld_dev[16]; /* Device name sd? */
    } sc_ld[MRAID_MAX_LD];
    
    /* Scsi ioctl from sd device */
    //int                             (*sc_ioctl)(struct device *, u_long, caddr_t);
    
    /* Firmware determined max, totals and other information */
    UInt32                          sc_max_cmds;
    UInt32                          sc_max_sgl;
    UInt32                          sc_sgl_size;
    UInt32                          sc_max_ld;
    UInt32                          sc_ld_cnt;
    
    UInt16                          sc_sgl_flags;
    UInt16                          sc_reserved;
    
    // TO-DO: replace bio stuff with API for future MegaCLI management utility
    // P.S.: linux-megacli should fit my needs
    // struct mraid_conf            *sc_cfg;
    // struct mraid_ctrl_info       sc_ld_list;
    // struct mraid_ld_list         sc_ld_list;
    // struct mraid_ld_details      *sc_ld_details; // Array to all logical disks
    // int                          sc_no_pd; // Used physical disks
    // int                          sc_ld_sz; // sizeof(sc_ld_details)*/
    
    /* All commands */
    struct mraid_ccb                *sc_ccb;
    
    /* Producer/consumer pointers and reply queue */
    struct mraid_mem                *sc_pcq;
    
    /* Frame memory */
    struct mraid_mem                *sc_frames;
    UInt32                          sc_frames_size;
    
    /* Sense memory */
    struct mraid_mem                *sc_sense;
    
    IOSimpleLock                    *sc_ccb_spin;
    /* Management lock */
    IOLock                          *sc_lock;
    
    //TO-DO: Implement hardware sensors */
    //struct ksensor                  *sc_sensors;
    //struct ksensordev               sc_sensordev;
};

#define mraid_my_intr() ((this->*sc->sc_iop->mio_intr)())
#define mraid_fw_state() ((this->*sc->sc_iop->mio_fw_state)())
/* Different IOPs means different bunch of handling. Means: firmware, interrupts, POST. */
struct mraid_iop_ops {
    mraid_iop_ops() : mio_intr(NULL) {}
    bool is_set() { return (mio_intr == NULL ? FALSE : TRUE); } /* Enough */
    void init(enum mraid_iop iop) {
        switch(iop) {
            case MRAID_IOP_XSCALE:
                mio_intr = &SASMegaRAID::mraid_xscale_intr;
                mio_fw_state = &SASMegaRAID::mraid_xscale_fw_state;
                //
                break;
            case MRAID_IOP_PPC:
                mio_intr = &SASMegaRAID::mraid_ppc_intr;
                mio_fw_state = &SASMegaRAID::mraid_ppc_fw_state;
                //
                break;
            case MRAID_IOP_GEN2:
                mio_intr = &SASMegaRAID::mraid_gen2_intr;
                mio_fw_state = &SASMegaRAID::mraid_gen2_fw_state;
                //
                break;
        }
    }
    UInt32      (SASMegaRAID::*mio_fw_state)(void);
    //void        (*mio_intr_ena)(struct mraid_softc *);
    bool         (SASMegaRAID::*mio_intr)(void);
    //void        (*mio_post)(struct mraid_softc *, struct mraid_ccb *);
};
#endif /* ifndef */
