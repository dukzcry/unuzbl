-- Written by Artem Falcon <lomka@gero.in>

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
	 local res, limit
	 local fbsize, mmiosize, iosize

	 hw.pci_aprint_devinfo(pa)

	 res, fbsize = hw.pci_mapreg_map(pa, 0, hw.PCI_MAPREG_TYPE_MEM,
	    hw.BUS_SPACE_MAP_LINEAR, sc.sc_iot, sc.sc_iohp)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map frame buffer\n")
	    return
	 end
	 res, mmiosize = hw.pci_mapreg_map(pa, 4, hw.PCI_MAPREG_TYPE_MEM,
	   0, sc.mmio_iot, sc.mmio_ioh)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map mmio area\n")
	    hw.bus_space_unmap(sc.sc_iot, sc.sc_iohp, fbsize)
	    return
	 end
	 res, iosize = hw.pci_mapreg_map(pa, 8, hw.PCI_MAPREG_TYPE_IO,
	    0, sc.iot, sc.iohp)
	 if res ~= 0 then
	    core.aprint_error(sc.dv_xname .. ": can't map registers\n")
	    hw.bus_space_unmap(sc.mmio_iot, sc.mmio_ioh, mmiosize)
	    hw.bus_space_unmap(sc.sc_iot, sc.sc_iohp, fbsize)
	    return
	 end

	 -- since pci_mapreg_submap is local, try a hack
	 limit = 16 * 1024 * 1024
	 if fbsize > limit then
	    res = fbsize - limit
	    sc.sc_ioh = sc.sc_ioh + limit + 1
	    hw.bus_space_unmap(sc.sc_iot, sc.sc_iohp, res)
	    sc.sc_ioh = sc.sc_ioh - (limit + 1)
	 end
	 sc.ioh = sc.ioh + 0x30

	 -- core.print(sc.dv_xname .. ': ' .. fbsize .. ' ' .. mmiosize .. ' ' .. iosize .. '\n')
end

function onClose()
end
