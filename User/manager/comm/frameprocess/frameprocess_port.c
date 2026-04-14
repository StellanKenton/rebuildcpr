/***********************************************************************************
* @file     : frameprocess_port.c
* @brief    : Frame process project-side transport binding.
* @details  : Binds the bluetooth protocol service to UART4 and the local frame format.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "frameprocess.h"

#include <stddef.h>
#include <string.h>

#include "../../../../rep/module/fc41d/fc41d.h"
#include "../../../../rep/service/rtos/rtos.h"
#include "../../../../rep/service/console/log.h"

#define FRM_PROC_PORT_VERSION               0x01U
#define FRM_PROC_PORT_UART_TX_TIMEOUT_MS    20U
#define FRM_PROC_PORT_ACK_TIMEOUT_MS        1000U
#define FRM_PROC_PORT_ACK_RETRY_MAX         3U
#define FRM_PROC_PORT_RX_RING_CAPACITY      512U
#define FRM_PROC_PORT_URGENT_QUEUE_CAPACITY 256U
#define FRM_PROC_PORT_NORMAL_QUEUE_CAPACITY 512U
#define FRM_PROC_PORT_CRC_INIT              0xFFFFU
#define FRM_PROC_PORT_CRC_POLY              0x1021U

#define FRM_PROC_PORT_LOG_TAG               "FrmProcPort"

typedef struct stFrmProcPortBinding {
    eFc41dMapType fc41dDevice;
    const char *linkName;
} stFrmProcPortBinding;

static const uint8_t gFrmProcPortHead[] = {0xFAU, 0xFCU};

static const stFrmProcPortBinding gFrmProcPortBinding[FRAME_PROC_MAX] = {
    [FRAME_PROC0] = {
        .fc41dDevice = FC41D_DEV0,
        .linkName = "ble",
    },
    [FRAME_PROC1] = {
        .fc41dDevice = FC41D_DEV_MAX,
        .linkName = "wifi",
    },
};

static stRingBuffer gFrmProcPortRxRing[FRAME_PROC_MAX];
static uint8_t gFrmProcPortRxStorage[FRAME_PROC_MAX][FRM_PROC_PORT_RX_RING_CAPACITY];
static uint8_t gFrmProcPortUrgentStorage[FRAME_PROC_MAX][FRM_PROC_PORT_URGENT_QUEUE_CAPACITY];
static uint8_t gFrmProcPortNormalStorage[FRAME_PROC_MAX][FRM_PROC_PORT_NORMAL_QUEUE_CAPACITY];
static uint8_t gFrmProcPortParserBuf[FRAME_PROC_MAX][FRM_PROC_MAX_PKT_LEN];
static uint32_t gFrmProcPortRxBytes[FRAME_PROC_MAX];
static uint8_t gFrmProcPortRxScratch[FRAME_PROC_MAX][32U];

static eFrmProcStatus frmProcPortTxFrame(eFrmProcMapType proc, const uint8_t *frameBuf, uint16_t frameLen);

static const stFrmProcPortBinding *frmProcPortGetBinding(eFrmProcMapType proc)
{
    if ((uint32_t)proc >= (uint32_t)FRAME_PROC_MAX) {
        return NULL;
    }

    return &gFrmProcPortBinding[proc];
}

static bool frmProcPortIsBound(eFrmProcMapType proc)
{
    const stFrmProcPortBinding *lBinding = frmProcPortGetBinding(proc);

    return (lBinding != NULL) && ((uint32_t)lBinding->fc41dDevice < (uint32_t)FC41D_DEV_MAX);
}

static const char *frmProcPortGetLinkName(eFrmProcMapType proc)
{
    const stFrmProcPortBinding *lBinding = frmProcPortGetBinding(proc);

    if ((lBinding == NULL) || (lBinding->linkName == NULL)) {
        return "unknown";
    }

    return lBinding->linkName;
}

static stRingBuffer *frmProcPortGetRxRingBuffer(void *userCtx)
{
    uintptr_t lProc = (uintptr_t)userCtx;

    if (lProc >= (uintptr_t)FRAME_PROC_MAX) {
        return NULL;
    }

    return &gFrmProcPortRxRing[lProc];
}

static uint32_t frmProcPortHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx)
{
    (void)buf;
    (void)userCtx;

    if (availLen < 6U) {
        return 0U;
    }

    return 6U;
}

static uint32_t frmProcPortPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    uint32_t lPayloadLen;

    (void)headLen;
    (void)userCtx;

    if ((buf == NULL) || (availLen < 6U)) {
        return 0U;
    }

    lPayloadLen = ((uint32_t)buf[4] << 8U) | (uint32_t)buf[5];
    return 6U + lPayloadLen + 2U;
}

static uint32_t frmProcPortCalcCrc16(const uint8_t *buf, uint32_t len, void *userCtx)
{
    uint32_t lIndex;
    uint32_t lBit;
    uint16_t lCrc = FRM_PROC_PORT_CRC_INIT;

    (void)userCtx;

    if ((buf == NULL) || (len == 0U)) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < len; lIndex++) {
        lCrc ^= (uint16_t)((uint16_t)buf[lIndex] << 8U);
        for (lBit = 0U; lBit < 8U; lBit++) {
            if ((lCrc & 0x8000U) != 0U) {
                lCrc = (uint16_t)((uint16_t)(lCrc << 1U) ^ FRM_PROC_PORT_CRC_POLY);
            } else {
                lCrc <<= 1U;
            }
        }
    }

    return (uint32_t)lCrc;
}

eFrmProcStatus frmProcLoadPlatformDefaultCfg(eFrmProcMapType proc, stFrmProcCfg *cfg)
{
    if ((cfg == NULL) || !frmProcPortIsBound(proc)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    (void)memset(cfg, 0, sizeof(*cfg));
    cfg->protocol = FRAME_PROTOCOL0;
    cfg->protoCfg.rxHeadPat = gFrmProcPortHead;
    cfg->protoCfg.rxHeadPatLen = sizeof(gFrmProcPortHead);
    cfg->protoCfg.txHeadPat = gFrmProcPortHead;
    cfg->protoCfg.txHeadPatLen = sizeof(gFrmProcPortHead);
    cfg->protoCfg.minHeadLen = 6U;
    cfg->protoCfg.minPktLen = 8U;
    cfg->protoCfg.maxPktLen = FRM_PROC_MAX_PKT_LEN;
    cfg->protoCfg.waitPktToutMs = 50U;
    cfg->protoCfg.crcRangeStartOff = 3;
    cfg->protoCfg.crcRangeEndOff = -3;
    cfg->protoCfg.crcFieldOff = -2;
    cfg->protoCfg.crcFieldLen = 2U;
    cfg->protoCfg.crcFieldEnd = FRM_PSR_CRC_END_BIG;
    cfg->protoCfg.headLenFunc = frmProcPortHeadLen;
    cfg->protoCfg.pktLenFunc = frmProcPortPktLen;
    cfg->protoCfg.crcCalcFunc = frmProcPortCalcCrc16;
    cfg->protoCfg.getTick = repRtosGetTickMs;
    cfg->protoCfg.getRingBuf = frmProcPortGetRxRingBuffer;
    cfg->protoCfg.ringBufUserCtx = (void *)(uintptr_t)proc;
    cfg->protoCfg.userCtx = (void *)(uintptr_t)proc;
    cfg->getTick = repRtosGetTickMs;
    cfg->txFrame = frmProcPortTxFrame;
    cfg->urgentQueue.storage = gFrmProcPortUrgentStorage[proc];
    cfg->urgentQueue.capacity = sizeof(gFrmProcPortUrgentStorage[proc]);
    cfg->normalQueue.storage = gFrmProcPortNormalStorage[proc];
    cfg->normalQueue.capacity = sizeof(gFrmProcPortNormalStorage[proc]);
    cfg->rxFrameBuf = gFrmProcPortParserBuf[proc];
    cfg->rxFrameBufSize = sizeof(gFrmProcPortParserBuf[proc]);
    cfg->ackCfg.timeoutMs = FRM_PROC_PORT_ACK_TIMEOUT_MS;
    cfg->ackCfg.maxRetryCount = FRM_PROC_PORT_ACK_RETRY_MAX;
    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcPlatformInit(eFrmProcMapType proc)
{
    const stFrmProcPortBinding *lBinding = frmProcPortGetBinding(proc);

    if ((lBinding == NULL) || !frmProcPortIsBound(proc) || !fc41dIsReady(lBinding->fc41dDevice)) {
        return FRM_PROC_STATUS_NOT_READY;
    }

    if (ringBufferInit(&gFrmProcPortRxRing[proc], gFrmProcPortRxStorage[proc], sizeof(gFrmProcPortRxStorage[proc])) != RINGBUFFER_OK) {
        return FRM_PROC_STATUS_ERROR;
    }

    gFrmProcPortRxBytes[proc] = 0U;
    (void)fc41dBleClearRx(lBinding->fc41dDevice);
    LOG_I(FRM_PROC_PORT_LOG_TAG,
	      "proc=%u link=%s init ok rxCap=%u",
          (unsigned int)proc,
	      frmProcPortGetLinkName(proc),
          (unsigned int)sizeof(gFrmProcPortRxStorage[proc]));
    return FRM_PROC_STATUS_OK;
}

void frmProcPlatformPollRx(eFrmProcMapType proc)
{
    const stFrmProcPortBinding *lBinding = frmProcPortGetBinding(proc);
    uint32_t lReadCount;
    uint32_t lIndex;
    uint8_t lFirstBytes[4];
    uint32_t lTotalRead = 0U;
    uint32_t lFirstCount = 0U;

    if ((lBinding == NULL) || !frmProcPortIsBound(proc) || !fc41dIsReady(lBinding->fc41dDevice)) {
        return;
    }

    do {
        lReadCount = fc41dBleRead(lBinding->fc41dDevice,
                                  gFrmProcPortRxScratch[proc],
                                  sizeof(gFrmProcPortRxScratch[proc]));
        for (lIndex = 0U; lIndex < lReadCount; lIndex++) {
            if (lFirstCount < sizeof(lFirstBytes)) {
                lFirstBytes[lFirstCount] = gFrmProcPortRxScratch[proc][lIndex];
                lFirstCount++;
            }
        }

        if (lReadCount > 0U) {
            if (ringBufferWrite(&gFrmProcPortRxRing[proc], gFrmProcPortRxScratch[proc], lReadCount) != lReadCount) {
                LOG_W(FRM_PROC_PORT_LOG_TAG, "proc=%u rx ring full", (unsigned int)proc);
                break;
            }
            lTotalRead += lReadCount;
        }
    } while (lReadCount == sizeof(gFrmProcPortRxScratch[proc]));

    if (lTotalRead > 0U) {
        gFrmProcPortRxBytes[proc] += lTotalRead;
        LOG_I(FRM_PROC_PORT_LOG_TAG,
              "proc=%u link=%s rx bytes=%u total=%u first=%02X %02X %02X %02X",
              (unsigned int)proc,
              frmProcPortGetLinkName(proc),
              (unsigned int)lTotalRead,
              (unsigned int)gFrmProcPortRxBytes[proc],
              (unsigned int)(lFirstCount > 0U ? lFirstBytes[0] : 0U),
              (unsigned int)(lFirstCount > 1U ? lFirstBytes[1] : 0U),
              (unsigned int)(lFirstCount > 2U ? lFirstBytes[2] : 0U),
              (unsigned int)(lFirstCount > 3U ? lFirstBytes[3] : 0U));
    }
}

eFrmProcStatus frmProcEnsurePlatformFmt(eFrmProcMapType proc, const stFrmProcCfg *cfg)
{
    if (!frmProcPortIsBound(proc) || (cfg == NULL) || (!frmPsrIsProtoCfgValid(&cfg->protoCfg))) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    return FRM_PROC_STATUS_OK;
}

eFrmProcStatus frmProcBuildPlatformPkt(eFrmProcMapType proc, uint8_t cmd, const uint8_t *payloadBuf, uint16_t payloadLen, uint8_t *pktBuf, uint16_t pktBufSize, uint16_t *pktLen)
{
    uint16_t lTotalLen;
    uint16_t lCrc;

    (void)proc;

    if ((pktBuf == NULL) || (pktLen == NULL) || ((payloadBuf == NULL) && (payloadLen != 0U))) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    lTotalLen = (uint16_t)(6U + payloadLen + 2U);
    if (pktBufSize < lTotalLen) {
        return FRM_PROC_STATUS_NO_SPACE;
    }

    pktBuf[0] = gFrmProcPortHead[0];
    pktBuf[1] = gFrmProcPortHead[1];
    pktBuf[2] = FRM_PROC_PORT_VERSION;
    pktBuf[3] = cmd;
    pktBuf[4] = (uint8_t)((payloadLen >> 8U) & 0xFFU);
    pktBuf[5] = (uint8_t)(payloadLen & 0xFFU);
    if (payloadLen > 0U) {
        (void)memcpy(&pktBuf[6], payloadBuf, payloadLen);
    }

    lCrc = (uint16_t)frmProcPortCalcCrc16(&pktBuf[3], (uint32_t)payloadLen + 3U, NULL);
    pktBuf[6U + payloadLen] = (uint8_t)((lCrc >> 8U) & 0xFFU);
    pktBuf[7U + payloadLen] = (uint8_t)(lCrc & 0xFFU);
    *pktLen = lTotalLen;
    return FRM_PROC_STATUS_OK;
}

static eFrmProcStatus frmProcPortTxFrame(eFrmProcMapType proc, const uint8_t *frameBuf, uint16_t frameLen)
{
    const stFrmProcPortBinding *lBinding = frmProcPortGetBinding(proc);
    eFc41dStatus lStatus;
    uint8_t lCmd = 0U;

    if ((lBinding == NULL) || !frmProcPortIsBound(proc) || (frameBuf == NULL) || (frameLen == 0U)) {
        return FRM_PROC_STATUS_INVALID_PARAM;
    }

    if (frameLen >= 4U) {
        lCmd = frameBuf[3];
    }

    lStatus = fc41dBleSend(lBinding->fc41dDevice, frameBuf, frameLen);
    if (lStatus != FC41D_STATUS_OK) {
        LOG_E(FRM_PROC_PORT_LOG_TAG,
          "proc=%u link=%s tx fail cmd=0x%02X len=%u",
              (unsigned int)proc,
          frmProcPortGetLinkName(proc),
              (unsigned int)lCmd,
              (unsigned int)frameLen);
      return (eFrmProcStatus)lStatus;
    }

    if (lCmd != (uint8_t)FRM_PROC_CMD_CPR_DATA) {
        LOG_I(FRM_PROC_PORT_LOG_TAG,
              "proc=%u link=%s tx ok cmd=0x%02X len=%u",
              (unsigned int)proc,
              frmProcPortGetLinkName(proc),
              (unsigned int)lCmd,
              (unsigned int)frameLen);
    }

    return FRM_PROC_STATUS_OK;
}

/**************************End of file********************************/
