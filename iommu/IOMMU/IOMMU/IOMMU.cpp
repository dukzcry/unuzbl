// Written by Artem Falcon <lomka@gero.in>

#include <IOKit/IOMapper.h>
#include <IOKit/IOLib.h>
//#include <IOKit/IOPlatformExpert.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include "IOMMU.h"

OSDefineMetaClassAndStructors(IOMMU, IOMapper);
IOMMU *IOMMUInstance;

dmar_unit* IOMMU::dmarInit(ACPI_DMAR_HARDWARE_UNIT *dmarUnit)
{
    IODeviceMemory *MemDesc;
    dmar_unit *unit = IONew(dmar_unit, 1);
    
    bzero(unit, sizeof(unit));
    if (!(MemDesc = IODeviceMemory::withRange(dmarUnit->Address, PAGE_SIZE))) {
        DbgPrint("Memory mapping failed\n");
        goto fail;
    }
    unit->map = MemDesc->map();
    MemDesc->release();
    if (unit->map)
        unit->regs = (void *) unit->map->getVirtualAddress();
    else {
        IOPrint("Can't map IO space\n");
        goto fail;
    }
    
    return unit;

/*fail2:
    unit->map->release();*/
fail:
    IODelete(unit, dmar_unit, 1);
    IOPrint("DMARU %llx destroyed!\n", dmarUnit->Address);
    return NULL;
}

UInt32 IOMMU::dmarRead4(dmar_unit *unit, int off)
{
    return OSReadLittleInt32(unit->regs, off);
}
UInt64 IOMMU::dmarRead8(dmar_unit *unit, int off)
{
    // check
    return OSReadLittleInt64(unit->regs, off);
}

bool IOMMU::initHardware(IOService *provider)
{
    //IOPlatformDevice *nub = OSDynamicCast(IOPlatformDevice, provider);
    IOACPIPlatformDevice *acpiDevice;
    const OSData *dmarData;
    ACPI_TABLE_DMAR *dmar;
    ACPI_DMAR_HEADER *dmarEnd, *haw;
    int i;
    
    DbgPrint("%s\n", __FUNCTION__);
    
    //IOResources
    if (!(acpiDevice = (typeof(acpiDevice)) provider->metaCast("IOACPIPlatformDevice"))) {
        DbgPrint("Can't get IOACPIPlatformDevice. Unloading\n");
        return false;
    }
    if (!(dmarData = acpiDevice->getACPITableData("DMAR", 0))) {
        IOPrint("Can't get DMAR table. Unloading\n");
        return false;
    }
    
    dmar = (typeof(dmar)) dmarData->getBytesNoCopy();
    //dmarSpaceEnd as alloc space border
    
    haw = (typeof(haw)) (dmar + 1);
    dmarEnd = (typeof(dmarEnd)) (((uintptr_t) dmar) + dmarData->getLength());
    for (i = 0; haw < dmarEnd; haw = (typeof(haw)) (((uintptr_t) haw) + haw->Length)) {
        if (haw->Length <= 0) {
            IOPrint("Corrupted DMAR table, Length %d\n", haw->Length);
            break;
        }
        else if (haw->Type == ACPI_DMAR_TYPE_HARDWARE_UNIT &&
                 (units[i] = dmarInit((ACPI_DMAR_HARDWARE_UNIT *) haw)))
            i++;
    }
    if (!i) {
        IOPrint("No units were found. Unloading\n");
        return false;
    }
    
    //super::start();
    //registerService(); // and mapper
    return false;
}

bool IOMMU::start(IOService *provider)
{
    // FIXME
    if (!IOMMUInstance)
        IOMMUInstance = this;
    else
        return false;
    
    DbgPrint("%s\n", __FUNCTION__);
    
    return initHardware(provider);
}

void IOMMU::free()
{
    int i;
    
    //DbgPrint("%s\n", __FUNCTION__);
    
        for (i = 0; i < UnitsNum; i++)
            if (units[i]) {
                units[i]->map->release();
                IODelete(units[i], dmar_unit, 1);
            }
    
	super::free();
}

ppnum_t IOMMU::iovmAlloc(IOItemCount pages)
{
    ppnum_t ret = 0;
    
    //DbgPrint("%s\n", __FUNCTION__);
    
    // aligns and bounds on pages
    
    return ret;
}
void IOMMU::iovmFree(ppnum_t addr, IOItemCount pages)
{
    //DbgPrint("%s\n", __FUNCTION__);
    
    // aligns and bounds on addr and pages
}
void IOMMU::iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page)
{
    //DbgPrint("%s\n", __FUNCTION__);
    
    // addr += offset; // check them
}
void IOMMU::iovmInsert(ppnum_t addr, IOItemCount offset,
                     ppnum_t *pageList, IOItemCount pageCount)
{
    //DbgPrint("%s\n", __FUNCTION__);
    
    // addr += offset;
    // bounds on addr and pageCount
}
void IOMMU::iovmInsert(ppnum_t addr, IOItemCount offset,
					 upl_page_info_t *pageList, IOItemCount pageCount)
{
    //DbgPrint("%s\n", __FUNCTION__);
    
    // same as above but for phys addrs
}

addr64_t IOMMU::mapAddr(IOPhysicalAddress addr)
{
    //DbgPrint("%s\n", __FUNCTION__);
    
    // bypasses?
    
    //return addr;
    return 0; // bounds on addr
}
