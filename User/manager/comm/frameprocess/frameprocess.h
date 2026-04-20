/************************************************************************************
* @file     : frameprocess.h
* @brief    : Project-side USB communication manager.
* @details  : Provides the system-facing communication entry points on top of the
*             reusable drvusb CDC transport.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_FRAMEPROCESS_H
#define REBUILDCPR_FRAMEPROCESS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRM_PROC_LOG_TAG                "frmProc"
#define FRM_PROC_MAX                    1U
#define FRM_PROC_RX_BUFFER_SIZE         512U
#define FRM_PROC_IO_CHUNK_SIZE          64U

typedef enum eFrmProcMap {
    FRAME_PROC0 = 0,
    FRAME_PROC_COUNT = FRM_PROC_MAX,
} eFrmProcMap;

typedef enum eFrmProcState {
    eFRM_PROC_STATE_UNINIT = 0,
    eFRM_PROC_STATE_READY,
    eFRM_PROC_STATE_RUNNING,
    eFRM_PROC_STATE_ERROR,
} eFrmProcState;

typedef enum eFrmProcStatus {
    FRM_PROC_STATUS_OK = 0,
    FRM_PROC_STATUS_EMPTY,
    FRM_PROC_STATUS_INVALID_PARAM,
    FRM_PROC_STATUS_NOT_READY,
    FRM_PROC_STATUS_BUSY,
    FRM_PROC_STATUS_TIMEOUT,
    FRM_PROC_STATUS_ERROR,
} eFrmProcStatus;

typedef struct stFrmProcInfo {
    eFrmProcState state;
    bool isConnected;
    bool isConfigured;
    uint32_t rxBytes;
    uint32_t txBytes;
    uint32_t dropBytes;
} stFrmProcInfo;

eFrmProcStatus frmProcInit(uint8_t process);
void frmProcProcess(uint8_t process);
eFrmProcStatus frmProcSend(uint8_t process, const uint8_t *buffer, uint16_t length);
eFrmProcStatus frmProcRead(uint8_t process, uint8_t *buffer, uint16_t length, uint16_t *actualLength);
uint16_t frmProcGetRxLength(uint8_t process);
const stFrmProcInfo *frmProcGetStatus(uint8_t process);

#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
