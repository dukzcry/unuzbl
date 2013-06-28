require 'hw'
require 'core'

function xgifbMatch(pa, devs, num)
	 if hw.pci_matchbyid(pa, devs, num) == 1 then
	    return 100
	 else
	    return 0
	 end
end
function xgifbAttach(sc, pa)
	 local res
	 local fbsize, mmiosize, iosize

	 hw.pci_aprint_devinfo(pa)

	 res, fbsize = hw.pci_mapreg_map(pa, 0, hw.PCI_MAPREG_TYPE_MEM,
	    hw.BUS_SPACE_MAP_LINEAR, sc.sc_iotp, sc.sc_iohp)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map frame buffer\n")
	    return
	 end
	 res, mmiosize = hw.pci_mapreg_map(pa, 4, hw.PCI_MAPREG_TYPE_MEM,
	   0, sc.mmio_iotp, sc.mmio_iohp)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map mmio area\n")
	    hw.bus_space_unmap(sc.sc_iot, sc.sc_ioh, fbsize)
	    return
	 end
	 res, iosize = hw.pci_mapreg_map(pa, 8, hw.PCI_MAPREG_TYPE_IO,
	    0, sc.iotp, sc.iohp)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map registers\n")
	    hw.bus_space_unmap(sc.mmio_iot, sc.mmio_ioh, mmiosize)
	    hw.bus_space_unmap(sc.sc_iot, sc.sc_ioh, fbsize)
	    return
	 end

	 sc.ioh = sc.ioh + 0x30
end

function onClose()
end
