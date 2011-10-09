#include "SASMegaRAID.h"
#include "Registers.h"

bool SASMegaRAID::init()
{
    BaseClass::init();
	/*if(!BaseClass::init())
		return false;*/
	_megaraid_was_initf_called = TRUE;
	
	fPCIDevice = NULL;
    MemDesc = NULL; map = NULL;
    MyWorkLoop = NULL; fInterruptSrc = NULL; fMSIEnabled = FALSE;
    sc->sc_lock = NULL; sc->sc_ccb_spin = NULL;
    sc->sc_max_cmds = NULL;
    
	/* Create an instance of PCI class from Helper Library */
	PCIHelperP = new PCIHelper<SASMegaRAID>;

	return true;
}
void SASMegaRAID::free()
{
    /* Helper Library is not inherited from OSObject */
    /*PCIHelperP->release();*/
    delete PCIHelperP;
	PCIHelperP = NULL;
    _megaraid_was_initf_called = FALSE;
    
	BaseClass::free();
}
bool SASMegaRAID::free_start()
{
    if(sc->sc_ccb_spin) {
        IOSimpleLockUnlock(sc->sc_ccb_spin); IOSimpleLockFree(sc->sc_ccb_spin);
    }
    if(sc->sc_lock) {
        IOLockUnlock(sc->sc_lock); IOLockFree(sc->sc_lock);
    }
    if(fInterruptSrc && MyWorkLoop)
        MyWorkLoop->removeEventSource(fInterruptSrc);
    if(fInterruptSrc) NRelease(fInterruptSrc);
    if(ccbCommandPool)
        NRelease(ccbCommandPool);
    
    if(fPCIDevice) {
        fPCIDevice->close(this);
        NRelease(fPCIDevice);
    }
    if(MemDesc) NRelease(MemDesc);
    if(map) NRelease(map);
    
    return false;
}
void SASMegaRAID::TerminateController()
{
    this->free();
}

bool SASMegaRAID::createWorkLoop(void)
{
    MyWorkLoop = IOWorkLoop::workLoop();
    
    return (MyWorkLoop != 0);
}
IOWorkLoop *SASMegaRAID::getWorkLoop(void) const
{
    return MyWorkLoop;
}
void SASMegaRAID::Done(struct mraid_ccb *ccb)
{

}

UInt32 SASMegaRAID::MRAID_Read(IOByteCount offset)
{
    UInt32 data;
    /*if(MemDesc->readBytes(offset, &data, 4) != 4) {
        DbgPrint("%s[%p]::Read(): Unsuccessfull.\n", getName(), this);
        return(0);
    }*/
    data = OSReadLittleInt32((void *) MemDesc, offset);
    DnbgPrint(MRAID_D_RW, "%s[%p]::Read(): offset 0x%x data 0x08%x.\n", getName(), this, (UInt32) offset, data);
    
    return data;
}
/*bool*/
void SASMegaRAID::MRAID_Write(IOByteCount offset, IOByteCount data)
{
    DnbgPrint(MRAID_D_RW, "%s[%p]::Read(): offset 0x%x data 0x08%x.\n", getName(), this, (UInt32) offset, (UInt32) data);
    OSWriteLittleInt32((void *) MemDesc, offset, data);
    
    /*if(MemDesc->writeBytes(offset, &data, 4) != 4) {
        DbgPrint("%s[%p]::Write(): Unsuccessfull.\n", getName(), this);
        return FALSE;
    }
    
    return TRUE;*/
}
bool SASMegaRAID::mraid_xscale_intr()
{
    UInt32 Status;
    
    Status = OSReadLittleInt32(MemDesc, MRAID_OSTS);
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

IOInterruptEventSource *SASMegaRAID::CreateDeviceInterrupt(IOInterruptEventSource::Action action,
    IOFilterInterruptEventSource::Filter filter, IOService *provider)
{
    /* Interrupts are our sole proprietary */
    return NULL;
}
bool SASMegaRAID::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *sender)
{
    /* Check by which device the interrupt line is occupied (mine or other) */
    
    if(mraid_my_intr() != TRUE)
        return false;
    
    SignalInterrupt();
    return false;
}
void SASMegaRAID::interruptHandler(OSObject *owner, void *src, IOService *nub, int count)
{
    //TO-DO: Learn how it works
    //Looks like Producer/Consumer model mentioned in "PCI System Architecture" book
    struct mraid_prod_cons *pcq;
    //struct mraid_ccb *ccb;
    UInt32 Producer, Consumer, ctx;
    
    pcq = MRAIDMEM_KVA(sc->sc_pcq);
    Producer = pcq->mpc_producer;
    Consumer = pcq->mpc_consumer;
    
    DnbgPrint(MRAID_D_INTR, "%s[%p]::interruptHandler(): sc %#x pcq %#x.\n", 
              getName(), this, (UInt32) sc, (UInt32) pcq);
    
    while(Consumer != Producer) {
        DnbgPrint(MRAID_D_INTR, "%s[%p]::interruptHandler(): Prodh %#x Consh %#x.\n",
                  getName(), this, Producer, Consumer);
        
        ctx = pcq->mpc_reply_q[Consumer];
        pcq->mpc_reply_q[Consumer] = MRAID_INVALID_CTX;
        if(ctx == MRAID_INVALID_CTX)
            IOPrint("Invalid context, Prod: %d Cons: %d.\n", Producer, Consumer);
        else {
            /* XXX: Remove from queue and call scsi_done */
            //ccb = &sc->sc_ccb[ctx];
            DnbgPrint(MRAID_D_INTR, "%s[%p]::interruptHandler(): context %#x.\n", getName(), this, ctx);
            /* XXX: bus_dmamap_sync */
            /* this->Done(ccb);*/
        }
        Consumer++;
        if(Consumer == (sc->sc_max_cmds + 1))
            Consumer = 0;
    }
    
    pcq->mpc_consumer = Consumer;
    return;
}

const struct mraid_pci_device* SASMegaRAID::MatchDevice()
{
	using mraid_structs::mraid_pci_devices;

	const struct mraid_pci_device *mpd;
	UInt16 VendorId, DeviceId;
	
	if (!fPCIDevice) {
		DbgPrint("%s[%p]::fPCIDevice field is not initialized.\n", getName(), this);
		return(NULL);
	}	
	
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

//bool SASMegaRAID::start(IOService *provider)
bool SASMegaRAID::InitializeController(void)
{
    const struct mraid_pci_subtype *st;
	UInt8 regbar;
    UInt32 type, barval, reg;
    const char *subtype = NULL;
    char subid[32];
	
    
	DbgPrint("%s[%p]::start(IOService *provider).\n", getName(), this);
    /* "Custom HBA drivers do not implement the I/O kit init() method */
	if(_megaraid_was_initf_called == FALSE)
        this->init();
	
    fPCIDevice = OSDynamicCast(IOPCIDevice, getProvider());
	if (!fPCIDevice) {
		DbgPrint("Failed to cast provider.\n");
		return false;
	}
	/*if(!BaseClass::start(fPCIDevice)) {
		DbgPrint("Can't start IOService class.\n");
		return false;
	}*/
    fPCIDevice->retain();
	fPCIDevice->open(this);
	
	mpd = MatchDevice();
    if(!mpd) {
        IOPrint("Failure in %s[%p]::MatchDevice().\n", getName(), this);
        return free_start();
    }
	
	/* Choosing BAR register */
	if (mpd->mpd_iop == MRAID_IOP_GEN2)
		regbar = MRAID_BAR_GEN2;
    else
		regbar = MRAID_BAR;
    
    /* Figuring out mapping scheme */
    type = PCIHelperP->MappingType(this, regbar, &barval);
    switch(type) {
        case PCI_MAPREG_TYPE_IO:
            fPCIDevice->setIOEnable(true);
        break;
        case PCI_MAPREG_MEM_TYPE_32BIT_1M:
        case PCI_MAPREG_MEM_TYPE_32BIT:
            
            fPCIDevice->setMemoryEnable(true);
            
            MemDesc->initWithPhysicalAddress((IOPhysicalAddress32) barval, MRAID_PCI_MEMSIZE, kIODirectionOutIn);
            if(MemDesc != NULL) {
                map = MemDesc->map();
                if(map != NULL) {
                    DbgPrint("Memory mapped at bus address ..., virtual address %x,"
                             " length %d.\n", /*(UInt32)map->getPhysicalAddress(),*/
                             (UInt32)map->getVirtualAddress(),
                             (UInt32)map->getLength());
                }
                else {
                    IOPrint("Can't map controller PCI space.\n");
                    return free_start();
                }
            }
        break;
        case PCI_MAPREG_MEM_TYPE_64BIT:
            /*IOPrint("Only PCI-E cards are supported.\n");
            return free_start();*/
            
            fPCIDevice->setMemoryEnable(true);
            
            /* Rework: Mapping with 64-bit address. */
            MemDesc->initWithPhysicalAddress((IOPhysicalAddress64) barval >> 32, MRAID_PCI_MEMSIZE, kIODirectionOutIn);
            if(MemDesc != NULL) {
                map = MemDesc->map();
                if(map != NULL) {
                    DbgPrint("Memory mapped at bus address ..., virtual address %x,"
                             " length %d.\n", /*(UInt32)map->getPhysicalAddress(),*/
                             (UInt32)map->getVirtualAddress(),
                             (UInt32)map->getLength());
                }
                else {
                    IOPrint("Can't map controller PCI space.\n");
                    return free_start();
                }
            }
        break;
        default:
            DbgPrint("Can't find out mapping scheme.\n");
            return free_start();
    }
    MyWorkLoop = (IOWorkLoop *) getWorkLoop();
    if(!PCIHelperP->CreateDeviceInterrupt(this, getProvider(), MRAID_PREFER_MSI, &SASMegaRAID::interruptHandler, 
        &SASMegaRAID::interruptFilter))
            return free_start();
        
    reg = fPCIDevice->configRead32(kIOPCIConfigSubSystemVendorID);
    
    if(mpd->mpd_subtype != NULL) {
        st = mpd->mpd_subtype;
        while(st->st_id != 0x0) {
            if(st->st_id == reg) {
                subtype = st->st_string;
                break;
            }
            st++;
        }
    }
    
    if(subtype == NULL) {
        snprintf(subid, sizeof(subid), "0x%08x", reg);
        subtype = subid;
    }

    IOPrint("Controller ID string: %s.\n", subtype);
    
    if(Attach()) {
        IOPrint("Can't attach device.\n");
        return free_start();
    }
    
	fPCIDevice->close(this);
	fPCIDevice->release();
	
	return true;
}
IOService* SASMegaRAID::probe(IOService *provider, SInt32 *score)
{
    bool call_status = _megaraid_was_probef_called;
    bool uiop = (mpd->mpd_iop != MRAID_IOP_XSCALE && mpd->mpd_iop != MRAID_IOP_PPC && mpd->mpd_iop != MRAID_IOP_GEN2);
    _megaraid_was_probef_called = true;
    if(call_status == false && sc->sc_iop->is_set())
        if(uiop)
            MyPanic("%s[%p]::probe(): Unknown IOP %d", getName(), this, mpd->mpd_iop);
    if(uiop) {
        IOPrint("%s[%p]::probe(): Unknown IOP %d. The driver will unload.\n", getName(), this, mpd->mpd_iop);
        *score = 0;
        return(NULL);
    }
        
    *score = 11000;
    return(this);
}
bool SASMegaRAID::Transition_Firmware()
{
    UInt32 fw_state, cur_state;
    int max_wait;
    
    fw_state = mraid_fw_state() & MRAID_STATE_MASK;
    
    DnbgPrint(MRAID_D_CMD, "%s[%p]::Transition_Firmware(): Firmware state %#x.\n", getName(), this, fw_state);
    
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
SCSIServiceResponse SASMegaRAID::ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest)
{
    UInt8                      cdbLength       = GetCommandDescriptorBlockSize(parallelRequest);
    SCSICommandDescriptorBlock cdbData;
    
    if(cdbLength > sizeof(cdbData)) {
        DbgPrint("ProcessParallelTask(): ABORT - cdbLength > sizeof(cdbData) - %d vs. %d\n", cdbLength, int(sizeof(cdbData)));
        goto failure_exit;
    }
    if(!GetCommandDescriptorBlock(parallelRequest, &cdbData)) {
        DbgPrint("ProcessParallelTask(): ABORT - !GetCommandDescriptorBlock(parallelRequest, &cdbData).\n");
        goto failure_exit;
    }
    
    DnbgPrint(MRAID_D_CMD, "%s[%p]::ProcessParallelTask(): opcode - %#x.\n", getName(), this, cdbData[0]);
    
failure_exit:
    ;
}

//
#if 0
SCSIServiceResponse SASMegaRAID::ProcessParallelTask(SCSIParallelTaskIdentifier parallelRequest)
{
    UInt8                      transferDir     = GetDataTransferDirection(parallelRequest);
    IOMemoryDescriptor*        transferMemDesc = GetDataBuffer(parallelRequest);
    UInt64                     transferLength  = GetRequestedDataTransferCount(parallelRequest);

    SCSITargetIdentifier       targetID        = GetTargetIdentifier(parallelRequest);
    
    mraid_iokitframe_header pf;
    mraid_ccbCommand *ccb = IONew(mraid_ccbCommand, sizeof(mraid_ccbCommand));
    if(!ccb)
        goto failure_exit;
    /* I create object, so dunno regarding init() */
    ccb->initCommand();
    pf.mrh_cmd                  = MRAID_CMD_LD_SCSI_IO;
    pf.mrh_data_len             = 0;
    
    DnbgPrint(MRAID_D_CMD, "%s[%p]::ProcessParallelTask(): Target ID - %d.\n", getName(), this, int(targetID << 32));
    
    if(!transferMemDesc && (transferDir != kSCSIDataTransfer_NoDataTransfer)) {
        DbgPrint("ProcessParallelTask(): ABORT - !transferMemDesc && (transferDir != kSCSIDataTransfer_NoDataTransfer) - %p and %d.\n", 
                 transferMemDesc, transferDir);
        goto failure_exit;
    }
    if(transferMemDesc && (transferDir != kSCSIDataTransfer_NoDataTransfer))
        pf.mrh_data_len = transferLength;
    
    //ccb->setDone(mraid_scsi_xs_done);
    ccb->setFrameSize(MRAID_PASS_FRAME_SIZE);
    //ccb->setsgl(...);
    if(transferDir == kSCSIDataTransfer_FromInitiatorToTarget || transferDir == kSCSIDataTransfer_FromTargetToInitiator) 
        ccb->setDirection(transferDir == kSCSIDataTransfer_FromInitiatorToTarget ? MRAID_DATA_IN : MRAID_DATA_OUT);
    else
        ccb->setDirection(MRAID_DATA_NONE);

    ccbCommandPool->returnCommand(ccb);
failure_exit:
    if(ccb) IOFree(ccb, sizeof(ccb));
    //CompleteTaskOnWorkloopThread...
    ;
}
#endif
//

SCSIDeviceIdentifier SASMegaRAID::ReportHighestSupportedDeviceID(void)
{
    return SCSIDeviceIdentifier(MRAID_MAX_LD);
}
IOTypes::IOUInt32 SASMegaRAID::ReportMaximumTaskCount(void)
{
    if(sc->sc_max_cmds == NULL) {
        IOPrint("%s[%p]::ReportMaximumTaskCount(): Returned value isn't greater zero. "
                "The driver will unload.\n", getName(), this);
        return 0;
    }
    
    return sc->sc_max_cmds;
}
void SASMegaRAID::ReportHBAConstraints(OSDictionary *constraints)
{
    OSNumber *byteCount = NULL;
    UInt64 align_mask = 0xFFFFFFFFFFFFFFFFULL;
    
    byteCount = OSDynamicCast(OSNumber, getProperty(kIOMaximumSegmentCountReadKey));
    if(byteCount != NULL)
        byteCount->setValue(sc->sc_max_sgl);
    else byteCount = OSNumber::withNumber(sc->sc_max_sgl, 32);
    setProperty(kIOMaximumSegmentCountReadKey, byteCount); byteCount = NULL;
    
    byteCount = OSDynamicCast(OSNumber, getProperty(kIOMaximumSegmentCountWriteKey));
    if(byteCount != NULL)
        byteCount->setValue(sc->sc_max_sgl);
    else byteCount = OSNumber::withNumber(sc->sc_max_sgl, 32);
    setProperty(kIOMaximumSegmentCountWriteKey, byteCount); byteCount = NULL;
    
    /* Not really required by hw */
    byteCount = OSDynamicCast(OSNumber, getProperty(kIOMinimumHBADataAlignmentMaskKey));
    if(byteCount != NULL)
        byteCount->setValue(align_mask);
    else byteCount = OSNumber::withNumber(align_mask, 64);
    setProperty(kIOMinimumHBADataAlignmentMaskKey, byteCount); byteCount = NULL;
}
bool SASMegaRAID::DoesHBAPerformDeviceManagement(void)
{
    return true;
}
void SASMegaRAID::AllocMem(dmawidth width)
{
    /* Let's use IOMemoryCursor for 32-bit DMA operations, it's slightly 
     * unrecommended though, since it uses the small common pool */
    if(width == DMA32BIT) {
        
    }
        
}
bool SASMegaRAID::Attach()
{
    UInt32 status, max_segm_count; SInt32 holder;
    
    sc->sc_iop->init(mpd->mpd_iop);
    if(!_megaraid_was_probef_called)
        this->probe(getProvider(), &holder); /* Just to match format */
    
    DnbgPrint(MRAID_D_MISC, "%s[%p]::Attach().\n", getName(), this);
    
    if(Transition_Firmware() != TRUE)
        return FALSE;
    
    ccbCommandPool = IOCommandPool::withWorkLoop(MyWorkLoop);
    if(ccbCommandPool == NULL) {
        DbgPrint("Can't init ccb pool.\n");
        return false;
    }
    /* Mac OS X doesn't support IPLs, so instead of using "block I/O" level, let's try to use IOLock mutex lock. */
    /* XXX: Figure out the situation for multiple MSI/MSI-X */
    sc->sc_ccb_spin = IOSimpleLockAlloc(); sc->sc_lock = IOLockAlloc();
    
    status = mraid_fw_state();
    sc->sc_max_cmds = status & MRAID_STATE_MAXCMD_MASK;
    max_segm_count = (status & MRAID_STATE_MAXSGL_MASK) >> 16;
    if(IOPhysSize == 64) {
        OSNumber *byteCount = OSDynamicCast(OSNumber, getProperty(kIOMaximumSegmentByteCountReadKey));
        sc->sc_max_sgl = eqmin(max_segm_count, byteCount->unsigned32BitValue() / PAGE_SIZE);
        sc->sc_sgl_size = sizeof(struct mraid_sg64);
        sc->sc_sgl_flags = MRAID_FRAME_SGL64;
    } else {
        sc->sc_max_sgl = max_segm_count;
        sc->sc_sgl_size = sizeof(struct mraid_sg32);
        sc->sc_sgl_flags = MRAID_FRAME_SGL32;
    }
    DnbgPrint(MRAID_D_MISC, "64-bit DMA: %d, max commands: %u, max SGL segment count: %u.\n", IOPhysSize, sc->sc_max_cmds, 
              sc->sc_max_sgl);
    
    
}

/*IOTypes::IOUInt32 SASMegaRAID::ReportHBASpecificTaskDataSize(void)
 {
 return (UInt32) sizeof(struct mraid_ccb); 
 }*/