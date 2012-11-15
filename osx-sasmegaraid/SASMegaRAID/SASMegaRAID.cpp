#include "SASMegaRAID.h"
#include "Registers.h"

OSDefineMetaClassAndStructors(SASMegaRAID, IOSCSIParallelInterfaceController)
OSDefineMetaClassAndStructors(mraid_ccbCommand, IOCommand)

bool SASMegaRAID::init (OSDictionary* dict)
{
    if (dict)
        BaseClass::init(dict);
    //DbgPrint("IOService->init\n");
    
    fPCIDevice = NULL;
    map = NULL;
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
    //DbgPrint("IOService->probe\n");
    return res;
}

//bool SASMegaRAID::start(IOService *provider)
bool SASMegaRAID::InitializeController(void)
{
    IOService *provider = getProvider();
    IOMemoryDescriptor *MemDesc;
    UInt8 regbar;
    UInt32 type, barval;
    
    //BaseClass::start(provider);
    DbgPrint("super->InitializeController\n");
    
    if (!(fPCIDevice = OSDynamicCast(IOPCIDevice, provider))) {
        IOPrint("Failed to cast provider\n");
        return false;
    }

    fPCIDevice->retain();
    fPCIDevice->open(this);
    
    if(!(mpd = MatchDevice())) {
        IOPrint("Device matching failed\n");
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
                MemDesc->release();
                if(map != NULL) {
                    vAddr = (void *) map->getVirtualAddress();
                    DbgPrint("Memory mapped at bus address %#x, virtual address %#x,"
                             " length %d\n", (UInt32)map->getPhysicalAddress(),
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
                MemDesc->release();
                if(map != NULL) {
                    vAddr = (void *) map->getVirtualAddress();
                    DbgPrint("Memory mapped at bus address %d, virtual address %#x,"
                             " length %d\n", (UInt32)map->getPhysicalAddress(),
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
    OSBoolean *sPreferMSI = conf ? OSDynamicCast(OSBoolean, conf->getObject("PreferMSI")) : NULL;
    bool PreferMSI = true;
    if (sPreferMSI) PreferMSI = sPreferMSI->isTrue();
    if(!PCIHelperP->CreateDeviceInterrupt(this, provider, PreferMSI, &SASMegaRAID::interruptHandler,
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
    //DbgPrint("super->TerminateController\n");
    
    //BaseClass::stop(provider);
}

void SASMegaRAID::free(void)
{
    mraid_ccbCommand *command;
    
    DbgPrint("IOService->free\n");
    
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
    }
    if(map) map->release();
    if (fInterruptSrc) {
        if (MyWorkLoop)
            MyWorkLoop->removeEventSource(fInterruptSrc);
        if (fInterruptSrc) fInterruptSrc->release();
    }
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
        /*IORWLockUnlock(sc->sc_lock);*/ IORWLockFree(sc->sc_lock);
    }
    
    if (sc->sc_pcq) FreeMem(sc->sc_pcq);
    if (sc->sc_frames) FreeMem(sc->sc_frames);
    if (sc->sc_sense) FreeMem(sc->sc_sense);
    
    if (sc) IODelete(sc, mraid_softc, 1);
    
    BaseClass::free();
}

/* */

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
    bool uiop = (mpd->mpd_iop == MRAID_IOP_XSCALE || mpd->mpd_iop == MRAID_IOP_PPC || mpd->mpd_iop == MRAID_IOP_GEN2
                 || mpd->mpd_iop == MRAID_IOP_SKINNY);
    if(sc->sc_iop->is_set() && !uiop) {
        IOPrint("%s: Unknown IOP %d. The driver will unload.\n", __FUNCTION__, mpd->mpd_iop);
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
    
    DbgPrint("%s\n", __FUNCTION__);
    
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
    sc->sc_max_cmds = eqmin(status & MRAID_STATE_MAXCMD_MASK, 256);
    max_segm_count = (status & MRAID_STATE_MAXSGL_MASK) >> 16;
    /*if(IOPhysSize == 64) {
        OSNumber *byteCount = OSDynamicCast(OSNumber, getProperty(kIOMaximumSegmentByteCountReadKey));
        sc->sc_max_sgl = byteCount? eqmin(max_segm_count, (UInt32) byteCount->unsigned32BitValue() / PAGE_SIZE + 1) :
        max_segm_count;
        sc->sc_sgl_size = sizeof(struct mraid_sg64);
        sc->sc_sgl_flags = MRAID_FRAME_SGL64;
    } else*/ {
        sc->sc_max_sgl = max_segm_count;
        sc->sc_sgl_size = sizeof(struct mraid_sg32);
        //sc->sc_sgl_flags = MRAID_FRAME_SGL32;
    }
    DbgPrint("DMA: %d-bit, max commands: %u, max SGL segment count: %u\n", /*IOPhysSize*/ 32, sc->sc_max_cmds,
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
    /* Rework: handle this case */
    if (MRAID_DVA(sc->sc_frames) & 0x3f) {
        IOPrint("Wrong frame alignment. Addr %#x\n", MRAID_DVA(sc->sc_frames));
        return false;
    }
    
    /* Frame sense memory */
    sc->sc_sense = AllocMem(sc->sc_max_cmds * MRAID_SENSE_SIZE);
    if (!sc->sc_frames) {
        IOPrint("Unable to allocate sense memory\n");
        return false;
    }
    
    if (!Initccb()) {
        IOPrint("Unable to init ccb pool\n");
        return false;
    }
    
    /* Init firmware with all pointers */
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
    if (!mm->bmd)
        goto free;
    
    /* The last two options are excessive */
    mm->cmd = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, 0, IODMACommand::kMapped, size, PAGE_SIZE);
    if (!mm->cmd)
        goto bmd_free;
    
    err = mm->cmd->setMemoryDescriptor(mm->bmd);
    if (err != kIOReturnSuccess)
        goto cmd_free;
    
    while ((err == kIOReturnSuccess) && (offset < mm->bmd->getLength()))
    {
        UInt32 numSeg = 1;
        
        err = mm->cmd->gen32IOVMSegments(&offset, &mm->segment, &numSeg);
        DbgPrint("gen32IOVMSegments(err = %d) paddr %#x, len %d, nsegs %d\n",
              err, mm->segment.fIOVMAddr, mm->segment.fLength, numSeg);
    }
    if (err == kIOReturnSuccess) {
        mm->map = mm->bmd->map();
        /* Memory must be zeroed */
        memset((void *) mm->map->getVirtualAddress(), '\0', size);
        return mm;
    }
    
cmd_free:
    mm->cmd->clearMemoryDescriptor();
    mm->cmd->release();
bmd_free:
    mm->bmd->release();
free:
    IODelete(mm, mraid_mem, 1);
    
    return NULL;
}
void SASMegaRAID::FreeMem(struct mraid_mem *mm)
{
    mm->map->release();
    mm->cmd->clearMemoryDescriptor();
    mm->cmd->release();
    mm->bmd->release();
    IODelete(mm, mraid_mem, 1);
    mm = NULL;
}
bool SASMegaRAID::GenerateSegments(struct mraid_ccbCommand *ccb)
{
    IOReturn err = kIOReturnSuccess;
    UInt64 offset = 0;
    
    /* Bogus restrictions */
    ccb->s.ccb_sglmem.cmd = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, MAXPHYS,
                                                          IODMACommand::kMapped, MAXPHYS);
    if (!ccb->s.ccb_sglmem.cmd)
        return false;
    
    /* XXX: Set kIOMemoryMapperNone or ~kIOMemoryMapperNone */
    err = ccb->s.ccb_sglmem.cmd->setMemoryDescriptor(ccb->s.ccb_sglmem.bmd);
    if (err != kIOReturnSuccess)
        return false;
    
    ccb->s.ccb_sglmem.numSeg = sc->sc_max_sgl;
    while ((err == kIOReturnSuccess) && (offset < ccb->s.ccb_sglmem.bmd->getLength()))
    {
        ccb->s.ccb_sglmem.segments = IONew(IODMACommand::Segment32, sc->sc_max_sgl);
        
        err = ccb->s.ccb_sglmem.cmd->gen32IOVMSegments(&offset, &ccb->s.ccb_sglmem.segments[0], &ccb->s.ccb_sglmem.numSeg);
        /* I doubt this will ever happen */
        if (ccb->s.ccb_sglmem.numSeg > sc->sc_max_sgl)
            return false;
    }
    if (err != kIOReturnSuccess)
        return false;
    else DbgPrint("gen32IOVMSegments(err = %d) nseg %d, perseg %d, totlen %lld\n", err, ccb->s.ccb_sglmem.numSeg,
                ccb->s.ccb_sglmem.segments[0].fLength, ccb->s.ccb_sglmem.bmd->getLength());
    
    return true;
}

bool SASMegaRAID::Initccb()
{
    mraid_ccbCommand *ccb;
    int i, j = 0;
    
    DbgPrint("%s\n", __FUNCTION__);
    
    for (i = 0; i < sc->sc_max_cmds; i++) {
        ccb = mraid_ccbCommand::NewCommand();

        /* Pick i'th frame & i'th sense */
        
        ccb->s.ccb_frame = (union mraid_frame *) (MRAID_KVA(sc->sc_frames) + sc->sc_frames_size * i);
        ccb->s.ccb_pframe = MRAID_DVA(sc->sc_frames) + sc->sc_frames_size * i;
        ccb->s.ccb_pframe_offset = sc->sc_frames_size * i;
        /* XXX: type is too small */
        ccb->s.ccb_frame->mrr_header.mrh_context = i;
        
        ccb->s.ccb_sense = (struct mraid_sense *) (MRAID_KVA(sc->sc_sense) + MRAID_SENSE_SIZE * i);
        ccb->s.ccb_psense = MRAID_DVA(sc->sc_sense) + MRAID_SENSE_SIZE * i;

        /*DbgPrint(MRAID_D_CCB, "ccb(%d) frame: %p (%lx) sense: %p (%lx)\n",
                  cb->s.ccb_frame->mrr_header.mrh_context, ccb->ccb_frame, ccb->ccb_pframe, ccb->ccb_sense, ccb->ccb_psense);*/
        
        Putccb(ccb);
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
void SASMegaRAID::Putccb(mraid_ccbCommand *ccb)
{
    IOSimpleLockLock(sc->sc_ccb_spin);
    ccbCommandPool->returnCommand(ccb);
    IOSimpleLockUnlock(sc->sc_ccb_spin);
}
mraid_ccbCommand *SASMegaRAID::Getccb()
{
    mraid_ccbCommand *ccb;
    
    IOSimpleLockLock(sc->sc_ccb_spin);
    ccb = (mraid_ccbCommand *) ccbCommandPool->getCommand(true);
    IOSimpleLockUnlock(sc->sc_ccb_spin);
    
    return ccb;
}

bool SASMegaRAID::Transition_Firmware()
{
    UInt32 fw_state, cur_state;
    int max_wait;
    
    fw_state = mraid_fw_state() & MRAID_STATE_MASK;
    
    DbgPrint("%s: Firmware state %#x\n", __FUNCTION__, fw_state);
    
    while(fw_state != MRAID_STATE_READY) {
        DbgPrint("Waiting for firmware to become ready\n");
        cur_state = fw_state;
        switch(fw_state) {
            case MRAID_STATE_FAULT:
                IOPrint("Firmware fault\n");
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
                IOPrint("Unknown firmware state\n");
                return FALSE;
        }
        for(int i = 0; i < (max_wait * 10); i++) {
            fw_state = mraid_fw_state() & MRAID_STATE_MASK;
            if(fw_state == cur_state)
                IODelay(100000); /* 100 µsec */
            else break;
        }
        if(fw_state == cur_state) {
            IOPrint("Firmware stuck in state: %#x\n", fw_state);
            return FALSE;
        }
    }
    
    return TRUE;
}

void mraid_empty_done(mraid_ccbCommand *)
{
    /* ;) */
    __asm__ volatile("nop");
}

bool SASMegaRAID::Initialize_Firmware()
{
    bool res = true;
    mraid_ccbCommand* ccb;
    struct mraid_init_frame *init;
    struct mraid_init_qinfo *qinfo;
    
    ccb = Getccb();
    ccb->scrubCommand();
    
    DbgPrint("%s: ccb_num: %d\n", __FUNCTION__, ccb->s.ccb_frame->mrr_header.mrh_context);
    
    init = &ccb->s.ccb_frame->mrr_init;
    qinfo = (struct mraid_init_qinfo *)((UInt8 *) init + MRAID_FRAME_SIZE);
    
    memset(qinfo, 0, sizeof(*qinfo));
    qinfo->miq_rq_entries = htole32(sc->sc_max_cmds + 1);
    qinfo->miq_rq_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_reply_q));
    
	qinfo->miq_pi_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_producer));
	qinfo->miq_ci_addr = htole64(MRAID_DVA(sc->sc_pcq) + offsetof(struct mraid_prod_cons, mpc_consumer));
    
    init->mif_header.mrh_cmd = MRAID_CMD_INIT;
    init->mif_header.mrh_data_len = htole32(sizeof(*qinfo));
    init->mif_qinfo_new_addr = htole64(ccb->s.ccb_pframe + MRAID_FRAME_SIZE);
    
    sc->sc_pcq->cmd->synchronize(kIODirectionOutIn);
    
    ccb->s.ccb_done = mraid_empty_done;
    MRAID_Poll(ccb);
    if (init->mif_header.mrh_cmd_status != MRAID_STAT_OK)
        res = false;
    
    Putccb(ccb);
    
    return res;
}

bool SASMegaRAID::GetInfo()
{
    DbgPrint("%s\n", __FUNCTION__);
    
    if (!Management(MRAID_DCMD_CTRL_GET_INFO, MRAID_DATA_IN | MRAID_CMD_POLL, sizeof(sc->sc_info), &sc->sc_info, NULL))
        return false;

#if defined(DEBUG)
    int i;
	for (i = 0; i < sc->sc_info.mci_image_component_count; i++) {
		IOPrint("Active FW %s Version %s date %s time %s\n",
               sc->sc_info.mci_image_component[i].mic_name,
               sc->sc_info.mci_image_component[i].mic_version,
               sc->sc_info.mci_image_component[i].mic_build_date,
               sc->sc_info.mci_image_component[i].mic_build_time);
	}
#endif
    
    return true;
}

bool SASMegaRAID::Management(UInt32 opc, UInt32 flags, UInt32 len, void *buf, UInt8 *mbox)
{
    mraid_ccbCommand* ccb;
    bool res;
    
    ccb = Getccb();
    ccb->scrubCommand();
    
    res = Do_Management(ccb, opc, flags, len, buf, mbox);
    
    Putccb(ccb);
    
    return res;
}
bool SASMegaRAID::Do_Management(mraid_ccbCommand *ccb, UInt32 opc, UInt32 flags, UInt32 len, void *buf, UInt8 *mbox)
{
    struct mraid_dcmd_frame *dcmd;
    
    IOBufferMemoryDescriptor *bmd;
    IOMemoryMap *map;
    void *addr;
    
    DbgPrint("%s: ccb_num: %d\n", __FUNCTION__, ccb->s.ccb_frame->mrr_header.mrh_context);
    
    bmd = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryPhysicallyContiguous |
                                                           
                                                                     kIOMemoryMapperNone
                                                           
                                                                     /* | kIODirectionNone */
                                                                     , len, 0x00000000FFFFF000ULL);
    if (!bmd)
        return false;
    //bmd->prepare(kIODirectionOutIn);
    map = bmd->map();
    addr = (void *) map->getVirtualAddress();
    
    dcmd = &ccb->s.ccb_frame->mrr_dcmd;
    memset(dcmd->mdf_mbox, 0, MRAID_MBOX_SIZE);
    dcmd->mdf_header.mrh_cmd = MRAID_CMD_DCMD;
    dcmd->mdf_header.mrh_timeout = 0;
    
    dcmd->mdf_opcode = opc;
    dcmd->mdf_header.mrh_data_len = 0;
    
    ccb->s.ccb_direction = flags & ~MRAID_CMD_POLL;
    ccb->s.ccb_frame_size = MRAID_DCMD_FRAME_SIZE;
    
    /* Additional opcodes */
    if (mbox)
        memcpy(dcmd->mdf_mbox, mbox, MRAID_MBOX_SIZE);
    
    if (ccb->s.ccb_direction != MRAID_DATA_NONE) {
        if (ccb->s.ccb_direction == MRAID_DATA_OUT)
            bcopy(buf, addr, len);
        dcmd->mdf_header.mrh_data_len = len;
        
        ccb->s.ccb_sglmem.bmd = bmd;
        ccb->s.ccb_sglmem.len = len;
        ccb->s.ccb_sglmem.map = map;
        
        ccb->s.ccb_sgl = &dcmd->mdf_sgl;
        
        if (!CreateSGL(ccb))
            return false;
    }

    if (flags & MRAID_CMD_POLL) {
        ccb->s.ccb_done = mraid_empty_done;
        MRAID_Poll(ccb);
    } else {
        MRAID_Exec(ccb);
    }
    if (dcmd->mdf_header.mrh_cmd_status != MRAID_STAT_OK)
        return false;

    if (ccb->s.ccb_direction == MRAID_DATA_IN)
        bcopy(addr, buf, len);
    
    FreeSGL(&ccb->s.ccb_sglmem);
    
    return true;
}

bool SASMegaRAID::CreateSGL(struct mraid_ccbCommand *ccb)
{
    struct mraid_frame_header *hdr = &ccb->s.ccb_frame->mrr_header;
    union mraid_sgl *sgl;
    IODMACommand::Segment32 *sgd;
    
    DbgPrint("%s\n", __FUNCTION__);
    
    if (!GenerateSegments(ccb)) {
        IOPrint("Unable to generate segments\n");
        return false;
    }
    
    sgl = ccb->s.ccb_sgl;
    sgd = ccb->s.ccb_sglmem.segments;
    for (int i = 0; i < ccb->s.ccb_sglmem.numSeg; i++) {
        /*if (IOPhysSize == 64) {
            sgl->sg64[i].addr = htole64(sgd[i].fIOVMAddr);
            sgl->sg64[i].len = htole32(sgd[i].fLength);
        } else*/ {
            sgl->sg32[i].addr = htole32(sgd[i].fIOVMAddr);
            sgl->sg32[i].len = htole32(sgd[i].fLength);
        }
        DbgPrint("addr: %#x\n", sgd[i].fIOVMAddr);
    }
    
    if (ccb->s.ccb_direction == MRAID_DATA_IN) {
        hdr->mrh_flags |= MRAID_FRAME_DIR_READ;
        ccb->s.ccb_sglmem.cmd->synchronize(kIODirectionIn);
    } else {
        hdr->mrh_flags |= MRAID_FRAME_DIR_WRITE;
        ccb->s.ccb_sglmem.cmd->synchronize(kIODirectionOut);
    }
    
    //hdr->mrh_flags |= sc->sc_sgl_flags;
    hdr->mrh_sg_count = ccb->s.ccb_sglmem.numSeg;
    ccb->s.ccb_frame_size += sc->sc_sgl_size * ccb->s.ccb_sglmem.numSeg;
    ccb->s.ccb_extra_frames = (ccb->s.ccb_frame_size - 1) / MRAID_FRAME_SIZE;
    
    DbgPrint("frame_size: %d extra_frames: %d\n",
             ccb->s.ccb_frame_size, ccb->s.ccb_extra_frames);
    
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
    DbgPrint("%s: offset %#x data 0x08%x\n", __FUNCTION__, offset, data);
    
    return data;
}
/*bool*/
void SASMegaRAID::MRAID_Write(UInt8 offset, uint32_t data)
{
    DbgPrint("%s: offset %#x data 0x08%x\n", __FUNCTION__, offset, data);
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
    int cycles = 0;
    
    DbgPrint("%s\n", __FUNCTION__);
    
    hdr = &ccb->s.ccb_frame->mrr_header;
    hdr->mrh_cmd_status = 0xff;
    hdr->mrh_flags |= MRAID_FRAME_DONT_POST_IN_REPLY_QUEUE;
    
    mraid_start(ccb);
    
    while (1) {
        IODelay(1000);
        
        sc->sc_frames->cmd->synchronize(kIODirectionOutIn);
        
        if (hdr->mrh_cmd_status != 0xff)
            break;
        
        if (cycles++ > 5000) {
            IOPrint("ccb timeout\n");
            break;
        }
        
        sc->sc_frames->cmd->synchronize(kIODirectionOutIn);
    }

    if (ccb->s.ccb_sglmem.len > 0)
        ccb->s.ccb_sglmem.cmd->synchronize(ccb->s.ccb_direction & MRAID_DATA_IN ?
                                         kIODirectionIn : kIODirectionOut);
    
    ccb->s.ccb_done(ccb);
}

void SASMegaRAID::MRAID_Exec(mraid_ccbCommand *ccb)
{
    
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
    MRAID_Write(MRAID_IQP, (ccb->s.ccb_pframe >> 3) | ccb->s.ccb_extra_frames);
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
    MRAID_Write(MRAID_IQP, 0x1 | ccb->s.ccb_pframe | (ccb->s.ccb_extra_frames << 1));
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
    MRAID_Write(MRAID_IQPL, 0x1 | ccb->s.ccb_pframe | (ccb->s.ccb_extra_frames << 1));
    MRAID_Write(MRAID_IQPH, 0x00000000);
}