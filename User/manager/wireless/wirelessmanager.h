/************************************************************************************
* @file     : wirelessmanager.h
* @brief    : Wireless manager service entry.
* @details  : Wraps the frameprocess instance used by the project bluetooth link.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESSMANAGER_H
#define REBUILDCPR_WIRELESSMANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "frameprocess/frameprocess.h"

#ifdef __cplusplus
extern "C" {
#endif

bool wirelessManagerIsReady(void);
void wirelessManagerProcess(void);
eFrmProcStatus wirelessManagerPostSelfCheck(const stFrmDataTxSelfCheck *data, bool isUrgent);
eFrmProcStatus wirelessManagerPostDisconnect(bool isUrgent);
eFrmProcStatus wirelessManagerPostCprData(const stFrmDataTxCprData *data, bool isUrgent);
const stFrmDataRxStore *wirelessManagerGetRxStore(void);
void wirelessManagerClearRxFlags(uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
