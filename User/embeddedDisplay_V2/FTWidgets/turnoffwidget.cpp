#include "turnoffwidget.h"
#include "riverdieve.h"

void TurnOffWidget::turnOff(Gpu_Hal_Context_t *host)
{
    if (host == nullptr)
        return;

    Gpu_CoCmd_Dlstart(host);

    Gpu_CoCmd_Gradient(host, 464,73, 0x707070, 464,283, 0x000000);
    App_WrCoCmd_Buffer(host, COLOR_RGB(255,255,255));

    Gpu_CoCmd_Text(host, 
                   240, 
                   34, 
                   29, 
                   static_cast<uint16_t>(OPT_CENTER), 
                   "Control unit will be turned off or rebooted.");

    Gpu_CoCmd_Text(host, 
                   240, 
                   62, 
                   29, 
                   static_cast<uint16_t>(OPT_CENTER), 
                   "Please wait until operation is done.");
    
    Gpu_CoCmd_Spinner(host, 240,164, 0, 1);
    App_WrCoCmd_Buffer(host, DISPLAY());
    Gpu_CoCmd_Swap(host);
    App_Flush_Co_Buffer(host);
    Gpu_Hal_WaitCmdfifo_empty(host);
}
