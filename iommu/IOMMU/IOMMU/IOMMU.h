#define super IOMapper

#define drvid "[IOMMU] "
#define IOPrint(arg...) IOLog(drvid arg)
//#if DEBUG
#define DbgPrint(arg...) IOPrint(arg)
/*#else
#define DbgPrint(arg...) ;
#endif*/

#define UnitsNum                        8

#define ACPI_DMAR_TYPE_HARDWARE_UNIT    0

typedef struct acpi_table_header
{
    // stub only
    uint8_t                   Reserved[0x24];
} ACPI_TABLE_HEADER;
typedef struct acpi_table_dmar
{
    ACPI_TABLE_HEADER         Header;
    uint8_t                   Width;
    uint8_t                   Flags;
    uint8_t                   Reserved[10];
} ACPI_TABLE_DMAR;
typedef struct acpi_dmar_header
{
    uint16_t                  Type;
    uint16_t                  Length;
} ACPI_DMAR_HEADER;
typedef struct acpi_dmar_hardware_unit
{
    ACPI_DMAR_HEADER          Header;
    uint8_t                   Flags;
    uint8_t                   Reserved;
    uint16_t                  Segment;
    uint64_t                  Address;
} ACPI_DMAR_HARDWARE_UNIT;

struct dmar_unit {
    IOMemoryMap *map;
    void* regs;
};

class IOMMU: public IOMapper
{
    OSDeclareDefaultStructors(IOMMU);
    
    // switch to dynamic queue
    dmar_unit* units[UnitsNum];
    
    dmar_unit* dmarInit(ACPI_DMAR_HARDWARE_UNIT*);
    UInt32 dmarRead4(dmar_unit *unit, int off);
    UInt64 dmarRead8(dmar_unit *unit, int off);
    
    /* parent methods */
protected:
    virtual bool initHardware(IOService *provider);
public:
    virtual bool start(IOService *provider);
    virtual void free();
    
    virtual ppnum_t iovmAlloc(IOItemCount pages);
    virtual void iovmFree(ppnum_t addr, IOItemCount pages);
    
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            ppnum_t *pageList, IOItemCount pageCount);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            upl_page_info_t *pageList, IOItemCount pageCount);
    
    virtual addr64_t mapAddr(IOPhysicalAddress addr);
    /*virtual ppnum_t iovmMapMemory(
                                  OSObject                    * memory,
                                  ppnum_t                       offsetPage,
                                  ppnum_t                       pageCount,
                                  uint32_t                      options,
                                  upl_page_info_t             * pageList,
                                  const IODMAMapSpecification * mapSpecification);*/
    /* */
};