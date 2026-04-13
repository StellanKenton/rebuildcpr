/************************************************************************************
* @file     : wireless.h
* @brief    : Wireless service entry.
* @details  : Provides project wireless processing across bluetooth and wifi links.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_H
#define REBUILDCPR_WIRELESS_H

#include <stdbool.h>
#include <stdint.h>

#include "frameprocess/frameprocess.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eWirelessLink {
    WIRELESS_LINK_BLUETOOTH = 0,
    WIRELESS_LINK_WIFI,
    WIRELESS_LINK_MAX,
} eWirelessLinkType;

bool wirelessIsReady(eWirelessLinkType link);
void wirelessProcess(void);
void wirelessProcessLink(eWirelessLinkType link);
eFrmProcStatus wirelessPostSelfCheck(eWirelessLinkType link, const stFrmDataTxSelfCheck *data, bool isUrgent);
eFrmProcStatus wirelessPostDisconnect(eWirelessLinkType link, bool isUrgent);
eFrmProcStatus wirelessPostCprData(eWirelessLinkType link, const stFrmDataTxCprData *data, bool isUrgent);
const stFrmDataRxStore *wirelessGetRxStore(eWirelessLinkType link);
void wirelessClearRxFlags(eWirelessLinkType link, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
