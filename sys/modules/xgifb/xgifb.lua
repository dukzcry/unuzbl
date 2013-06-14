require 'hw'
require 'core'

function xgifbMatch(pa, devs, num)
	 --core.print("matching\n")
	 if hw.pci_matchbyid(pa, devs, num) == 1 then
	    return 100
	 else
	    return 0
	 end
end
function xgifbAttach(parent, sc, pa)
	 --core.print("attaching\n")
end

function onClose()
end
