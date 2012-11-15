#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "Hardware.h"
#include "HelperLib.h"

typedef struct {
    IOBufferMemoryDescriptor *bmd;
    IODMACommand *cmd; /* synchronize() */
    IOMemoryMap *map;
    IODMACommand::Segment32 segment;
} mraid_mem;
typedef struct {
    UInt32 numSeg; /* For FreeSGL() */
    UInt64 len; /* For the safe */

    IOBufferMemoryDescriptor *bmd;
    IODMACommand *cmd;
    IOMemoryMap *map;
    IODMACommand::Segment32 *segments;
} mraid_sgl_mem;
void FreeSGL(mraid_sgl_mem *mm)
{
    if (mm->map) {
        mm->map->release();
        mm->map = NULL;
    }
    if (mm->cmd) {
        mm->cmd->clearMemoryDescriptor();
        mm->cmd->release();
        mm->cmd = NULL;
    }
    if (mm->bmd) {
        mm->bmd->complete();
        mm->bmd->release();
        mm->bmd = NULL;
    }
    if (mm->segments) {
        IODelete(mm->segments, IODMACommand::Segment32, mm->numSeg);
        mm->segments = NULL;
    }
}

#define MRAID_DVA(_am) ((_am)->segment.fIOVMAddr)
#define MRAID_KVA(_am) ((_am)->map->getVirtualAddress())

typedef struct {
    struct mraid_iop_ops            *sc_iop;
    
    /* Firmware determined max, totals and other information */
    UInt32                          sc_max_cmds;
    UInt32                          sc_max_sgl;
    UInt32                          sc_sgl_size;
    UInt16                          sc_sgl_flags;
    
    mraid_ctrl_info                 sc_info;
    
    /* Producer/consumer pointers and reply queue */
    mraid_mem                       *sc_pcq;
    
    /* Frame memory */
    mraid_mem                       *sc_frames;
    UInt32                          sc_frames_size;
    
    /* Sense memory */
    mraid_mem                       *sc_sense;
    
    /* gated-get/returnCommand is protected */
    IOSimpleLock                    *sc_ccb_spin;
    /* Management lock */
    IORWLock                        *sc_lock;
} mraid_softc;

#include "ccbCommand.h"

//#define BaseClass IOService
#define BaseClass IOSCSIParallelInterfaceController

class SASMegaRAID: public BaseClass {
	OSDeclareDefaultStructors(SASMegaRAID);
private:
    class PCIHelper<SASMegaRAID>* PCIHelperP;
    
    IOPCIDevice *fPCIDevice;
    IOMemoryMap *map;
    void *vAddr;
    IOWorkLoop *MyWorkLoop;
    IOInterruptEventSource *fInterruptSrc;
    OSDictionary *conf;
    IOCommandPool *ccbCommandPool;
    
    bool fMSIEnabled;
    bool InterruptsActivated;
    const mraid_pci_device *mpd;
    mraid_softc *sc;
    bool ccb_inited;

    friend struct mraid_iop_ops;
    
    /* Helper Library */
	template <typename UserClass> friend
    UInt32 PCIHelper<UserClass>::MappingType(UserClass*, UInt8, UInt32*);
    template <typename UserClass> friend
    bool PCIHelper<UserClass>::CreateDeviceInterrupt(UserClass *, IOService *, bool,
                                                     void(UserClass::*)(OSObject *, void *, IOService *, int),
                                                     bool(UserClass::*)(OSObject *, IOFilterInterruptEventSource *));
    void interruptHandler(OSObject *owner, void *src, IOService *nub, int count);
    bool interruptFilter(OSObject *owner, IOFilterInterruptEventSource *sender);
    /* */
    
    const mraid_pci_device *MatchDevice();
    bool Attach();
    bool Probe();
    bool Transition_Firmware();
    bool Initialize_Firmware();
    bool GetInfo();
    bool Management(UInt32 opc, UInt32 dir, UInt32 len, void *buf, UInt8 *mbox);
    bool Do_Management(mraid_ccbCommand *, UInt32, UInt32 dir, UInt32 len, void *buf, UInt8 *mbox);
    mraid_mem *AllocMem(size_t size);
    void FreeMem(mraid_mem *);
    bool CreateSGL(mraid_ccbCommand *);
    bool GenerateSegments(mraid_ccbCommand *ccb);
    bool Initccb();
    mraid_ccbCommand *Getccb();
    void Putccb(mraid_ccbCommand *);
    UInt32 MRAID_Read(UInt8 offset);
    /*bool*/ void MRAID_Write(UInt8 offset, UInt32 data);
    void MRAID_Poll(mraid_ccbCommand *ccb);
    void MRAID_Exec(mraid_ccbCommand *);
    
    bool mraid_xscale_intr();
    UInt32 mraid_xscale_fw_state();
    void mraid_xscale_post(mraid_ccbCommand *);
    bool mraid_ppc_intr();
    UInt32 mraid_ppc_fw_state();
    void mraid_ppc_post(mraid_ccbCommand *);
    bool mraid_gen2_intr();
    UInt32 mraid_gen2_fw_state();
    bool mraid_skinny_intr();
    UInt32 mraid_skinny_fw_state();
    void mraid_skinny_post(mraid_ccbCommand *);
protected:
    virtual bool init(OSDictionary *dict = NULL);
    
    virtual IOService* probe (IOService* provider, SInt32* score);
    virtual void free(void);
    
    /*virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);*/
    virtual bool InitializeController(void);
    virtual void TerminateController(void);
    virtual bool StartController() {return true;};
    virtual void StopController() {};
private:
    /* Unimplemented */
    virtual SCSILogicalUnitNumber	ReportHBAHighestLogicalUnitNumber ( void ) {};
    virtual bool	DoesHBASupportSCSIParallelFeature (
                                                       SCSIParallelFeature 		theFeature ) {};
    
    virtual bool	InitializeTargetForID (
                                           SCSITargetIdentifier 		targetID ) {};
    virtual SCSIServiceResponse	AbortTaskRequest (
                                                  SCSITargetIdentifier 		theT,
                                                  SCSILogicalUnitNumber		theL,
                                                  SCSITaggedTaskIdentifier	theQ ) {};
    virtual	SCSIServiceResponse AbortTaskSetRequest (
                                                     SCSITargetIdentifier 		theT,
                                                     SCSILogicalUnitNumber		theL ) {};
	virtual	SCSIServiceResponse ClearACARequest (
                                                 SCSITargetIdentifier 		theT,
                                                 SCSILogicalUnitNumber		theL ) {};
	
	virtual	SCSIServiceResponse ClearTaskSetRequest (
                                                     SCSITargetIdentifier 		theT,
                                                     SCSILogicalUnitNumber		theL ) {};
	
	virtual	SCSIServiceResponse LogicalUnitResetRequest (
                                                         SCSITargetIdentifier 		theT,
                                                         SCSILogicalUnitNumber		theL ) {};
	
	virtual	SCSIServiceResponse TargetResetRequest (
                                                    SCSITargetIdentifier 		theT ) {};
    virtual SCSIInitiatorIdentifier	ReportInitiatorIdentifier ( void ) {};
    virtual SCSIDeviceIdentifier	ReportHighestSupportedDeviceID ( void ) {};
    virtual UInt32		ReportMaximumTaskCount ( void ) {};
    virtual UInt32		ReportHBASpecificTaskDataSize ( void ) {};
    virtual UInt32		ReportHBASpecificDeviceDataSize ( void ) {};
    virtual bool		DoesHBAPerformDeviceManagement ( void ) {};
    virtual void	HandleInterruptRequest ( void ) {};
    virtual SCSIServiceResponse ProcessParallelTask (
                                                     SCSIParallelTaskIdentifier parallelRequest ) {};
    /* */
};

#define mraid_my_intr() ((this->*sc->sc_iop->mio_intr)())
#define mraid_fw_state() ((this->*sc->sc_iop->mio_fw_state)())
#define mraid_start(_c) { sc->sc_frames->cmd->synchronize(kIODirectionOutIn); (this->*sc->sc_iop->mio_post)(_c); }
/* Different IOPs means different bunch of handling. Means: firmware, interrupts, POST. */
typedef struct mraid_iop_ops {
    mraid_iop_ops() : mio_intr(NULL) {}
    bool is_set() { return (mio_intr == NULL ? FALSE : TRUE); } /* Enough */
    void init(mraid_iop iop) {
        switch(iop) {
            case MRAID_IOP_XSCALE:
                mio_intr = &SASMegaRAID::mraid_xscale_intr;
                mio_fw_state = &SASMegaRAID::mraid_xscale_fw_state;
                mio_post = &SASMegaRAID::mraid_xscale_post;
                break;
            case MRAID_IOP_PPC:
                mio_intr = &SASMegaRAID::mraid_ppc_intr;
                mio_fw_state = &SASMegaRAID::mraid_ppc_fw_state;
                mio_post = &SASMegaRAID::mraid_ppc_post;
                break;
            case MRAID_IOP_GEN2:
                mio_intr = &SASMegaRAID::mraid_gen2_intr;
                mio_fw_state = &SASMegaRAID::mraid_gen2_fw_state;
                mio_post = &SASMegaRAID::mraid_ppc_post; /* Same as for PPC */
                break;
            case MRAID_IOP_SKINNY:
                mio_intr = &SASMegaRAID::mraid_skinny_intr;
                mio_fw_state = &SASMegaRAID::mraid_skinny_fw_state;
                mio_post = &SASMegaRAID::mraid_skinny_post;
                break;
        }
    }
    UInt32      (SASMegaRAID::*mio_fw_state)(void);
    //void        (*mio_intr_ena)(mraid_softc *);
    bool         (SASMegaRAID::*mio_intr)(void);
    void        (SASMegaRAID::*mio_post)(mraid_ccbCommand *);
} mraid_iop_ops;