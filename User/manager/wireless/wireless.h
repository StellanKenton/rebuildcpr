/************************************************************************************
* @file     : wireless.h
* @brief    : Project-side wireless manager.
* @details  : Wraps the FC41D module for BLE initialization, periodic processing
*             and status query in the current product.
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

#define WIRELESS_LOG_TAG                     "wireless"
#define WIRELESS_FC41D_DEVICE                FC41D_DEV0
#define WIRELESS_RESET_ASSERT_MS             20U
#define WIRELESS_RESET_RELEASE_MS            200U
#define WIRELESS_BOOT_READY_TIMEOUT_MS       3000U
#define WIRELESS_AT_RX_CAPACITY              256U
#define WIRELESS_AT_LINE_BUF_SIZE            128U
#define WIRELESS_AT_CMD_BUF_SIZE             128U
#define WIRELESS_AT_PAYLOAD_BUF_SIZE         128U
#define WIRELESS_BLE_RX_CAPACITY             512U
#define WIRELESS_BLE_FRAME_MAX_LEN           136U
#define WIRELESS_UNUSED_RX_CAPACITY          1U

typedef enum eWirelessState {
    eWIRELESS_STATE_INIT = 0,
    eWIRELESS_STATE_NORMAL,
    eWIRELESS_STATE_ERROR,
} eWirelessState;

bool wirelessInit(void);
void wirelessProcess(void);
const eWirelessState *wirelessGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
