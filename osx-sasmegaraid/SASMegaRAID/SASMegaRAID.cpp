#include "SASMegaRAID.h"
#include "Registers.h"

OSDefineMetaClassAndStructors(SASMegaRAID, IOSCSIParallelInterfaceController)

bool SASMegaRAID::init (OSDictionary* dict)
{
    if (dict)
        BaseClass::init(dict);
    DbgPrint("init\n");
    
    fPCIDevice = NULL;
    MemDesc = NULL; map = NULL;
    MyWorkLoop = NULL; fInterruptSrc = NULL;
    conf = OSDynamicCast(OSDictionary, getProperty("Settings"));
    
	/* Create an instance of PCI class from Helper Library */
	PCIHelperP = new PCIHelper<SASMegaRAID>;
    
    this->sc = IONew(mraid_softc, 1);
    sc->sc_iop = IONew(mraid_iop_ops, 1);
    sc->sc_lock = NULL;
    
    return true;
}

IOService *SASMegaRAID::probe (IOService* provider, SInt32* score)
{
    IOService *res = BaseClass::probe(provider, score);
    DbgPrint("probe\n");
    return res;
}

//bool SASMegaRAID::start(IOService *provider)
bool SASMegaRAID::InitializeController(void)
{
    IOService *provider = getProvider();
    UInt8 regbar;
    UInt32 type, barval;
    
    //BaseClass::start(provider);
    DbgPrint("InitializeController\n");
    
    if (!(fPCIDevice = OSDynamicCast(IOPCIDevice, provider))) {
        IOPrint("Failed to cast provider\n");
        return false;
    }

    fPCIDevice->retain();
    fPCIDevice->open(this);
    
    if(!(mpd = MatchDevice())) {
        IOPrint("Failure in ::MatchDevice().\n");
        return false;
    }

	/* Choosing BAR register */
	if (mpd->mpd_iop == MRAID_IOP_GEN2 || mpd->mpd_iop == MRAID_IOP_SKINNY)
		regbar = MRAID_BAR_GEN2;
    else
		regbar = MRAID_BAR;

    fPCIDevice->setBusMasterEnable(true);
    /* Figuring out mapping scheme */
    type = PCIHelperP->MappingType(this, regbar, &barval);
    switch(type) {
        case PCI_MAPREG_TYPE_IO:
            fPCIDevice->setIOEnable(true);
            break;
        case PCI_MAPREG_MEM_TYPE_32BIT_1M:
        case PCI_MAPREG_MEM_TYPE_32BIT:
            fPCIDevice->setMemoryEnable(true);

            if (!(MemDesc = IOMemoryDescriptor::withAddressRange(barval, MRAID_PCI_MEMSIZE, kIODirectionOutIn |
                                           kIOMemoryMapperNone, TASK_NULL))) {
                IOPrint("Memory mapping failed\n");
                return false;
            }
            
            if(MemDesc != NULL) {
                map = MemDesc->map();
                if(map != NULL) {
                    vAddr = (void *) map->getVirtualAddress();
                    DbgPrint("Memory mapped at bus address 0x%x, virtual address 0x%x,"
                             " length %d.\n", (UInt32)map->getPhysicalAddress(),
                             (UInt32)map->getVirtualAddress(),
                             (UInt32)map->getLength());
                }
                else {
                    IOPrint("Can't map controller PCI space.\n");
                    return false;
                }
            }
            break;
        case PCI_MAPREG_MEM_TYPE_64BIT:
            /*IOPrint("Only PCI-E cards are supported.\n");
             return false;*/
            
            fPCIDevice->setMemoryEnable(true);
            
            /* Rework: Mapping with 64-bit address. */
            MemDesc = IOMemoryDescriptor::withAddressRange((IOPhysicalAddress64) barval >> 32, MRAID_PCI_MEMSIZE, kIODirectionOutIn |
                                      kIOMemoryMapperNone, TASK_NULL);
            if(MemDesc != NULL) {
                map = MemDesc->map();
                if(map != NULL) {
                    vAddr = (void *) map->getVirtualAddress();
                    DbgPrint("Memory mapped at bus address %d, virtual address %x,"
                             " length %d.\n", (UInt32)map->getPhysicalAddress(),
                             (UInt32)map->getVirtualAddress(),
                             (UInt32)map->getLength());
                }
                else {
                    IOPrint("Can't map controller PCI space.\n");
                    return false;
                }
            }
            break;
        default:
            DbgPrint("Can't find out mapping scheme.\n");
            return false;
    }
    createWorkLoop();
    if (!(MyWorkLoop = (IOWorkLoop *) getWorkLoop()))
        return false;
    OSBoolean *sPreferMSI = conf ? OSDynamicCast(OSBoolean, conf->getObject("PreferMSI")) : NULL;
    bool PreferMSI = true;
    if (sPreferMSI) PreferMSI = sPreferMSI->isTrue();
    if(!PCIHelperP->CreateDeviceInterrupt(this, getProvider(), PreferMSI, &SASMegaRAID::interruptHandler,
                                          &SASMegaRAID::interruptFilter))
        return false;
    
    if(!Attach()) {
     IOPrint("Can't attach device.\n");
     return false;
    }

    return true;
}

//void SASMegaRAID::stop(IOService *provider)
void SASMegaRAID::TerminateController(void)
{
    DbgPrint("TerminateController\n");
    
    //BaseClass::stop(provider);
}

void SASMegaRAID::free(void)
{
    DbgPrint("free\n");
    
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
    }
    if(MemDesc) MemDesc->release();
    if(map) map->release();
    if (fInterruptSrc && MyWorkLoop)
        MyWorkLoop->removeEventSource(fInterruptSrc);
    if (fInterruptSrc) fInterruptSrc->release();
    
    /* Helper Library is not inherited from OSObject */
    /*PCIHelperP->release();*/
    delete PCIHelperP;
    if (sc->sc_iop) IODelete(sc->sc_iop, mraid_iop_ops, 1);
    if (sc->sc_lock) {
        /*IORWLockUnlock(I2C_Lock);*/ IORWLockFree(sc->sc_lock);
    }
    if (sc) IODelete(sc, mraid_softc, 1);
    
    BaseClass::free();
}

/* */

bool SASMegaRAID::createWorkLoop(void)
{
    MyWorkLoop = IOWorkLoop::workLoop();
    
    return (MyWorkLoop != 0);
}
IOWorkLoop *SASMegaRAID::getWorkLoop(void) const
{
    return MyWorkLoop;
}
bool SASMegaRAID::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *sender)
{
    /* Check by which device the interrupt line is occupied (mine or other) */
    
    if(mraid_my_intr() != TRUE)
        return false;
    
    return true;
}
void SASMegaRAID::interruptHandler(OSObject *owner, void *src, IOService *nub, int count)
{
    return;
}

/* */

const struct mraid_pci_device* SASMegaRAID::MatchDevice()
{
	using mraid_structs::mraid_pci_devices;
    
	const struct mraid_pci_device *mpd;
	UInt16 VendorId, DeviceId;
	
	VendorId = fPCIDevice->configRead16(kIOPCIConfigVendorID);
	DeviceId = fPCIDevice->configRead16(kIOPCIConfigDeviceID);
	
	for (int i = 0; i < nitems(mraid_pci_devices); i++) {
		mpd = &mraid_pci_devices[i];
		
		if (mpd->mpd_vendor == VendorId &&
			mpd->mpd_product == DeviceId)
            return(mpd);
	}
	return(NULL);
}

bool SASMegaRAID::Probe()
{
    bool uiop = (mpd->mpd_iop != MRAID_IOP_XSCALE && mpd->mpd_iop != MRAID_IOP_PPC && mpd->mpd_iop != MRAID_IOP_GEN2
                 && mpd->mpd_iop != MRAID_IOP_SKINNY);
    if(sc->sc_iop->is_set() && uiop) {
        IOPrint("::Probe(): Unknown IOP %d. The driver will unload.\n", mpd->mpd_iop);
        return false;
    }
    
    return true;
}

bool SASMegaRAID::Attach()
{
    UInt32 status, max_segm_count;
    
    sc->sc_iop->init(mpd->mpd_iop);
    if (!this->Probe())
        return false;
    
    DnbgPrint(MRAID_D_MISC, "::Attach()\n");
    
    if(!Transition_Firmware())
        return false;
    
    sc->sc_lock = IORWLockAlloc();
    
    /* Get constraints forming frame pool contiguous memory */
    status = mraid_fw_state();
    sc->sc_max_cmds = status & MRAID_STATE_MAXCMD_MASK;
    max_segm_count = (status & MRAID_STATE_MAXSGL_MASK) >> 16;
    if(IOPhysSize == 64) {
        OSNumber *byteCount = OSDynamicCast(OSNumber, getProperty(kIOMaximumSegmentByteCountReadKey));
        sc->sc_max_sgl = byteCount? eqmin(max_segm_count, (UInt32) byteCount->unsigned32BitValue() / PAGE_SIZE) :
        max_segm_count;
        sc->sc_sgl_size = sizeof(struct mraid_sg64);
        sc->sc_sgl_flags = MRAID_FRAME_SGL64;
    } else {
        sc->sc_max_sgl = max_segm_count;
        sc->sc_sgl_size = sizeof(struct mraid_sg32);
        sc->sc_sgl_flags = MRAID_FRAME_SGL32;
    }
    DnbgPrint(MRAID_D_MISC, "DMA: %d-bit, max commands: %u, max SGL segment count: %u.\n", IOPhysSize, sc->sc_max_cmds,
              sc->sc_max_sgl);
    
    return true;
}

IOBufferMemoryDescriptor *AllocMem(size_t size)
{
    DnbgPrint(MRAID_D_MEM, "::AllocMem(): %zd\n", size);
    
    //return IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryMapperNone, size, 0x00000000FFFFF000ULL);
}

bool SASMegaRAID::Transition_Firmware()
{
    UInt32 fw_state, cur_state;
    int max_wait;
    
    fw_state = mraid_fw_state() & MRAID_STATE_MASK;
    
    DnbgPrint(MRAID_D_CMD, "::Transition_Firmware(): Firmware state %#x.\n", fw_state);
    
    while(fw_state != MRAID_STATE_READY) {
        DnbgPrint(MRAID_D_CMD, "Waiting for firmware to become ready.\n");
        cur_state = fw_state;
        switch(fw_state) {
            case MRAID_STATE_FAULT:
                IOPrint("Firmware fault.\n");
                return FALSE;
            case MRAID_STATE_WAIT_HANDSHAKE:
                MRAID_Write(MRAID_IDB, MRAID_INIT_CLEAR_HANDSHAKE);
                max_wait = 2;
                break;
            case MRAID_STATE_OPERATIONAL:
                MRAID_Write(MRAID_IDB, MRAID_INIT_READY);
                max_wait = 10;
                break;
            case MRAID_STATE_UNDEFINED:
            case MRAID_STATE_BB_INIT:
                max_wait = 2;
                break;
            case MRAID_STATE_FW_INIT:
            case MRAID_STATE_DEVICE_SCAN:
            case MRAID_STATE_FLUSH_CACHE:
                max_wait = 20;
                break;
            default:
                IOPrint("Unknown firmware state.\n");
                return FALSE;
        }
        for(int i = 0; i < (max_wait * 10); i++) {
            fw_state = mraid_fw_state() & MRAID_STATE_MASK;
            if(fw_state == cur_state)
                IODelay(100000); /* 100 msec */
            else break;
        }
        if(fw_state == cur_state) {
            IOPrint("Firmware stuck in state: %#x.\n", fw_state);
            return FALSE;
        }
    }
    
    return TRUE;
}

UInt32 SASMegaRAID::MRAID_Read(UInt8 offset)
{
    UInt32 data;
    /*if(MemDesc->readBytes(offset, &data, 4) != 4) {
     DbgPrint("%s[%p]::Read(): Unsuccessfull.\n", getName(), this);
     return(0);
     }*/
    data = OSReadLittleInt32(vAddr, offset);
    DnbgPrint(MRAID_D_RW, "::Read(): offset 0x%x data 0x08%x.\n", offset, data);
    
    return data;
}
/*bool*/
void SASMegaRAID::MRAID_Write(UInt8 offset, uint32_t data)
{
    DnbgPrint(MRAID_D_RW, "::Write(): offset 0x%x data 0x08%x.\n", offset, data);
    OSWriteLittleInt32(vAddr, offset, data);
    OSSynchronizeIO();
    
    /*if(MemDesc->writeBytes(offset, &data, 4) != 4) {
     DbgPrint("%s[%p]::Write(): Unsuccessfull.\n", getName(), this);
     return FALSE;
     }
     
     return TRUE;*/
}
bool SASMegaRAID::mraid_xscale_intr()
{
    UInt32 Status;
    
    Status = MRAID_Read(MRAID_OSTS);
    if(ISSET(Status, MRAID_OSTS_INTR_VALID) == 0)
        return FALSE;
    
    /* Write status back to acknowledge interrupt */
    /*if(!MRAID_Write(MRAID_OSTS, Status))
     return FALSE;*/
    MRAID_Write(MRAID_OSTS, Status);
    
    return TRUE;
}
UInt32 SASMegaRAID::mraid_xscale_fw_state()
{
    return(MRAID_Read(MRAID_OMSG0));
}
bool SASMegaRAID::mraid_ppc_intr()
{
    UInt32 Status;
    
    Status = MRAID_Read(MRAID_OSTS);
    if(ISSET(Status, MRAID_OSTS_PPC_INTR_VALID) == 0)
        return FALSE;
    
    /* Write status back to acknowledge interrupt */
    /*if(!MRAID_Write(MRAID_ODC, Status))
     return FALSE;*/
    MRAID_Write(MRAID_ODC, Status);
    
    return TRUE;
}
UInt32 SASMegaRAID::mraid_ppc_fw_state()
{
    return(MRAID_Read(MRAID_OSP));
}
bool SASMegaRAID::mraid_gen2_intr()
{
    UInt32 Status;
    
    Status = MRAID_Read(MRAID_OSTS);
    if(ISSET(Status, MRAID_OSTS_GEN2_INTR_VALID) == 0)
        return FALSE;
    
    /* Write status back to acknowledge interrupt */
    /*if(!MRAID_Write(MRAID_ODC, Status))
     return FALSE;*/
    MRAID_Write(MRAID_ODC, Status);
    
    return TRUE;
}
UInt32 SASMegaRAID::mraid_gen2_fw_state()
{
    return(MRAID_Read(MRAID_OSP));
}
bool SASMegaRAID::mraid_skinny_intr()
{
    UInt32 Status;
    
    Status = MRAID_Read(MRAID_OSTS);
    if(ISSET(Status, MRAID_OSTS_SKINNY_INTR_VALID) == 0)
        return FALSE;
    
    /* Write status back to acknowledge interrupt */
    /*if(!MRAID_Write(MRAID_ODC, Status))
     return FALSE;*/
    MRAID_Write(MRAID_ODC, Status);
    
    return TRUE;
}
UInt32 SASMegaRAID::mraid_skinny_fw_state()
{
    return(MRAID_Read(MRAID_OSP));
}