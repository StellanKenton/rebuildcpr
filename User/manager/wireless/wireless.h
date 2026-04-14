/************************************************************************************
* @file     : wireless.h
* @brief    : Project-side wireless manager.
* @details  : Wraps the FC41D module for initialization, periodic processing and
*             status query in the current product.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_WIRELESS_H
#define REBUILDCPR_WIRELESS_H

#include <stdbool.h>

#include "../../../rep/module/fc41d/fc41d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eWirelessState {
    eWIRELESS_STATE_UNINIT = 0,
    eWIRELESS_STATE_READY,
    eWIRELESS_STATE_ACTIVE,
    eWIRELESS_STATE_FAULT,
} eWirelessState;

typedef struct stWirelessStatus {
    eWirelessState state;
    bool initStarted;
    bool aliveCheckStarted;
    stFc41dInfo fc41dInfo;
    stFc41dTxnStatus txnStatus;
} stWirelessStatus;

bool wirelessInit(void);
void wirelessProcess(void);
const stWirelessStatus *wirelessGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
