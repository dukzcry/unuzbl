#include "SASMegaRAID.h"
#include "Registers.h"

OSDefineMetaClassAndStructors(SASMegaRAID, IOSCSIParallelInterfaceController)
OSDefineMetaClassAndStructors(mraid_ccbCommand, IOCommand)

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
    sc->sc_ccb_spin = NULL; sc->sc_lock = NULL;
    
    sc->sc_pcq = sc->sc_frames = sc->sc_sense = NULL;
    ccb_inited = false;
    
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
    mraid_ccbCommand *command;
    
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
    if (ccb_inited)
        for (int i = 0; i < sc->sc_max_cmds; i++)
        {
            command = (mraid_ccbCommand *) ccbCommandPool->getCommand(false);
            if (command)
                command->release();
        }
    if (ccbCommandPool) ccbCommandPool->release();
    
    /* Helper Library is not inherited from OSObject */
    /*PCIHelperP->release();*/
    delete PCIHelperP;
    if (sc->sc_iop) IODelete(sc->sc_iop, mraid_iop_ops, 1);
    if(sc->sc_ccb_spin) {
        /*IOSimpleLockUnlock(sc->sc_ccb_spin);*/ IOSimpleLockFree(sc->sc_ccb_spin);
    }
    if (sc->sc_lock) {
        /*IORWLockUnlock(I2C_Lock);*/ IORWLockFree(sc->sc_lock);
    }
    
    if (sc->sc_pcq) FreeMem(sc->sc_pcq);
    if (sc->sc_frames) FreeMem(sc->sc_frames);
    if (sc->sc_sense) FreeMem(sc->sc_sense);
    
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
        IOPrint("::%s: Unknown IOP %d. The driver will unload.\n", __FUNCTION__, mpd->mpd_iop);
        return false;
    }
    
    return true;
}

bool SASMegaRAID::Attach()
{
    UInt32 status, max_segm_count, frames;
    
    sc->sc_iop->init(mpd->mpd_iop);
    if (!this->Probe())
        return false;
    
    DnbgPrint(MRAID_D_MISC, "::%s\n", __FUNCTION__);
    
    if(!Transition_Firmware())
        return false;
    
    ccbCommandPool = IOCommandPool::withWorkLoop(MyWorkLoop);
    if(ccbCommandPool == NULL) {
        DbgPrint("Can't init ccb pool.\n");
        return false;
    }
    
    sc->sc_ccb_spin = IOSimpleLockAlloc(); sc->sc_lock = IORWLockAlloc();
    
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
    
    /* Comms queues memory */
    sc->sc_pcq = AllocMem( (sizeof(UInt32) * sc->sc_max_cmds) + sizeof(struct mraid_prod_cons));
    if (!sc->sc_pcq) {
        IOPrint("Unable to allocate reply queue memory\n");
        return false;
    }
    
    /* Command frames memory */
    frames = (sc->sc_sgl_size * sc->sc_max_sgl + MRAID_FRAME_SIZE - 1) / MRAID_FRAME_SIZE + 1;
    sc->sc_frames_size = frames * MRAID_FRAME_SIZE;
    sc->sc_frames = AllocMem(sc->sc_frames_size * sc->sc_max_cmds);
    if (!sc->sc_frames) {
        IOPrint("Unable to allocate frame memory\n");
        return false;
    }
    /* XXX */
    if (MRAID_DVA(sc->sc_frames) & 0x3f) {
        IOPrint("Wrong frame alignment. Addr 0x%x\n", MRAID_DVA(sc->sc_frames));
        return false;
    }
    
    /* Frame sense memory */
    sc->sc_sense = AllocMem(sc->sc_max_cmds * MRAID_SENSE_SIZE);
    if (!sc->sc_frames) {
        IOPrint("Unable to allocate sense memory\n");
        return false;
    }
    
    /* Init ccbs */
    if (!Initccb()) {
        IOPrint("Unable to init ccb pool\n");
        return false;
    }
    
    /* Init with all pointers */
    if (!Initialize_Firmware()) {
        IOPrint("Unable to init firmware\n");
        return false;
    }
    
    if (!GetInfo()) {
        IOPrint("Unable to get controller info\n");
    }
    
    return true;
}

struct mraid_mem *SASMegaRAID::AllocMem(size_t size)
{
    IOReturn err = kIOReturnSuccess;
    UInt64 offset = 0;
    
    struct mraid_mem *mm;
    
    mm = IONew(mraid_mem, 1);
    if (!mm)
        return NULL;
    
    mm->bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryPhysicallyContiguous
                                                               | kIOMemoryMapperNone
            , size, 0x00000000FFFFF000ULL);
    if (!mm->bmd) {
        goto free;
    }
    
    //mm->cmd = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, 0, IODMACommand::kMapped, 0, 1);
    mm->cmd = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, 0);
    if (!mm->cmd)
        goto bmd_free;
    
    err = mm->cmd->setMemoryDescriptor(mm->bmd);
    if (err != kIOReturnSuccess)
        goto cmd_free;
    
    while ((err == kIOReturnSuccess) && (offset < mm->bmd->getLength()))
    {
        UInt32 numSeg = 1;
        
        err = mm->cmd->gen32IOVMSegments(&offset, &mm->segments[0], &numSeg);
        DnbgPrint(MRAID_D_MEM, "gen32IOVMSegments(%x) phys 0x%x, len %d, nsegs %d\n",
              err, mm->segments[0].fIOVMAddr, mm->segments[0].fLength, numSeg);
    }
    if (err == kIOReturnSuccess) {
        mm->map = mm->bmd->map();
        memset((void *) mm->map->getVirtualAddress(), '\0', size);
        return mm;
    }
    
cmd_free:
    mm->cmd->release();
bmd_free:
    mm->bmd->release();
free:
    IODelete(mm, mraid_mem, 1);
    
    return NULL;
}
void SASMegaRAID::FreeMem(struct mraid_mem *mm)
{
    mm->cmd->clearMemoryDescriptor();
    mm->cmd->release();
    mm->bmd->release();
    IODelete(mm, mraid_mem, 1);
    mm = NULL;
}
void SASMegaRAID::FreeDMAMap(struct mraid_ccb_mem *cm)
{
    cm->bmd->release();
}

bool SASMegaRAID::Initccb()
{
    mraid_ccbCommand *ccb;
    int i, j = 0;
    
    DnbgPrint(MRAID_D_CCB, "::%s\n", __FUNCTION__);
    
    for (i = 0; i < sc->sc_max_cmds; i++) {
        ccb = mraid_ccbCommand::NewCommand();

        /* Pick i'th frame & i'th sense */
        
        ccb->ccb_frame = (union mraid_frame *) (MRAID_KVA(sc->sc_frames) + sc->sc_frames_size * i);
        ccb->ccb_pframe = MRAID_DVA(sc->sc_frames) + sc->sc_frames_size * i;
        ccb->ccb_pframe_offset = sc->sc_frames_size * i;
        
        ccb->ccb_sense = (struct mraid_sense *) (MRAID_KVA(sc->sc_sense) + MRAID_SENSE_SIZE * i);
        ccb->ccb_psense = MRAID_DVA(sc->sc_sense) + MRAID_SENSE_SIZE * i;
        ccb->ccb_dmamap.bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
                                                                    kIOMemoryPhysicallyContiguous
                                                                   | kIOMemoryMapperNone
                                                                   , MAXPHYS, 0x00000000FFFFF000ULL);
        if (!ccb->ccb_dmamap.bmd) {
            IOPrint("Can't create ccb dmamap\n");
            goto free;
        }

        /*DnbgPrint(MRAID_D_CCB, "ccb(%p) frame: %p (%lx) sense: %p (%lx)\n",
                  ccb, ccb->ccb_frame, ccb->ccb_pframe, ccb->ccb_sense, ccb->ccb_psense);*/
        
        ccbCommandPool->returnCommand(ccb);
    }
    
    ccb_inited = true;
    return true;
free:
    while (j < i)
    {
        ccb = (mraid_ccbCommand *) ccbCommandPool->getCommand(false);
        if (ccb)
            ccb->release();
        j++;
    }
    
    return false;
}

bool SASMegaRAID::Transition_Firmware()
{
    UInt32 fw_state, cur_state;
    int max_wait;
    
    fw_state = mraid_fw_state() & MRAID_STATE_MASK;
    
    DnbgPrint(MRAID_D_CMD, "::%s: Firmware state %#x.\n", __FUNCTION__, fw_state);
    
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

void mraid_empty_done(mraid_ccbCommand *)
{
    __asm__ volatile("nop");
}

bool SASMegaRAID::Initialize_Firmware()
{
    bool res = true;
    mraid_ccbCommand* ccb;
    struct mraid_init_frame *init;
    struct mraid_init_qinfo *qinfo;
    
    DnbgPrint(MRAID_D_MISC, "::%s\n", __FUNCTION__);
    
    ccb = (mraid_ccbCommand *) ccbCommandPool->getCommand(false);
    
    init = &ccb->ccb_frame->mrr_init;
    qinfo = (struct mraid_init_qinfo *)((UInt8 *) init + MRAID_FRAME_SIZE);
    
    qinfo->miq_rq_entries = htole32(sc->sc_max_cmds + 1);
    qinfo->miq_rq_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_reply_q));
    
	qinfo->miq_pi_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_producer));
	qinfo->miq_ci_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_consumer));
    
    init->mif_header.mrh_cmd = MRAID_CMD_INIT;
    init->mif_header.mrh_data_len = htole32(sizeof(*qinfo));
    init->mif_qinfo_new_addr = htole64(ccb->ccb_pframe + MRAID_FRAME_SIZE);
    
    sc->sc_pcq->cmd->synchronize(kIODirectionOutIn);
    
    ccb->setDone(mraid_empty_done);
    MRAID_Poll(ccb);
    if (init->mif_header.mrh_cmd_status != MRAID_STAT_OK)
        res = false;
    
    ccbCommandPool->returnCommand(ccb);
    
    return res;
}

bool SASMegaRAID::GetInfo()
{
    return true;
}

UInt32 SASMegaRAID::MRAID_Read(UInt8 offset)
{
    UInt32 data;
    /*if(MemDesc->readBytes(offset, &data, 4) != 4) {
     DbgPrint("%s[%p]::Read(): Unsuccessfull.\n", getName(), this);
     return(0);
     }*/
    data = OSReadLittleInt32(vAddr, offset);
    DnbgPrint(MRAID_D_RW, "::%s: offset 0x%x data 0x08%x.\n", __FUNCTION__, offset, data);
    
    return data;
}
/*bool*/
void SASMegaRAID::MRAID_Write(UInt8 offset, uint32_t data)
{
    DnbgPrint(MRAID_D_RW, "::%s: offset 0x%x data 0x08%x.\n", __FUNCTION__, offset, data);
    OSWriteLittleInt32(vAddr, offset, data);
    OSSynchronizeIO();
    
    /*if(MemDesc->writeBytes(offset, &data, 4) != 4) {
     DbgPrint("%s[%p]::Write(): Unsuccessfull.\n", getName(), this);
     return FALSE;
     }
     
     return TRUE;*/
}

void SASMegaRAID::MRAID_Poll(mraid_ccbCommand *ccb)
{
    struct mraid_frame_header *hdr;
    int to = 0;
    
    DnbgPrint(MRAID_D_CMD, "::%s\n", __FUNCTION__);
    
    hdr = &ccb->ccb_frame->mrr_header;
    hdr->mrh_cmd_status = 0xff;
    hdr->mrh_flags |= MRAID_FRAME_DONT_POST_IN_REPLY_QUEUE;
    
    MRAID_Start(ccb);
    
    while (1) {
        IODelay(1000);
        
        sc->sc_frames->cmd->synchronize(kIODirectionOutIn);
        
        if (hdr->mrh_cmd_status != 0xff)
            break;
        
        if (to++ > 5000) {
            IOPrint("ccb timeout\n");
            ccb->ccb_flags = MRAID_CCB_F_ERR;
            break;
        }
        
        sc->sc_frames->cmd->synchronize(kIODirectionOutIn);
    }
    
    if (ccb->getLen() > 0) {
        ccb->ccb_dmamap.cmd->synchronize((ccb->getDirection() & MRAID_DATA_IN) ?
                                         kIODirectionIn : kIODirectionOut);
        FreeDMAMap(&ccb->ccb_dmamap);
    }
    
    ccb->ccb_done(ccb);
}
void SASMegaRAID::MRAID_Start(mraid_ccbCommand *ccb)
{
    sc->sc_frames->cmd->synchronize(kIODirectionOutIn);
    
    mraid_post(ccb);
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
void SASMegaRAID::mraid_xscale_post(mraid_ccbCommand *ccb)
{
    MRAID_Write(MRAID_IQP, (ccb->ccb_pframe >> 3) | ccb->getExtraFrames());
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
void SASMegaRAID::mraid_ppc_post(mraid_ccbCommand *ccb)
{
    MRAID_Write(MRAID_IQP, 0x1 | ccb->ccb_pframe | (ccb->getExtraFrames() << 1));
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
void SASMegaRAID::mraid_skinny_post(mraid_ccbCommand *ccb)
{
    MRAID_Write(MRAID_IQPL, 0x1 | ccb->ccb_pframe | (ccb->getExtraFrames() << 1));
    MRAID_Write(MRAID_IQPH, 0x00000000);
}