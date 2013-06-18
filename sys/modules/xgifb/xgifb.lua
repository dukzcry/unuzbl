require 'hw'
require 'core'

function xgifbMatch(pa, devs, num)
	 if hw.pci_matchbyid(pa, devs, num) == 1 then
	    return 100
	 else
	    return 0
	 end
end
function xgifbAttach(pa)
	 hw.pci_aprint_devinfo(pa);
end

function onClose()
end
