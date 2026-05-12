/***********************************************************************************
* @file     : frameparser_port.c
* @brief    : Project-side frameparser binding implementation.
* @details  : Provides default protocol configuration and tick hooks for parser
*             instances used in the current product.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "frameparser_port.h"

#include <string.h>

#include "stm32f1xx_hal.h"

#include "rtos.h"
#include "wt2003hx.h"

static uint32_t frameparserPortGetTickMsImpl(void);
static void frameparserPortLoadDefaultProtoCfgImpl(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg);
static uint32_t frameparserPortWt2003hxHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx);
static uint32_t frameparserPortWt2003hxPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx);
static uint32_t frameparserPortWt2003hxChecksum(const uint8_t *buf, uint32_t len, void *userCtx);

static const uint8_t gFrameparserPortWt2003hxFrameHead[] = {WT2003HX_FRAME_HEAD};

static const stFrmPsrOps gFrameparserPortOps = {
    .getTickMs = frameparserPortGetTickMsImpl,
    .loadDefaultProtoCfg = frameparserPortLoadDefaultProtoCfgImpl,
};

static uint32_t frameparserPortGetTickMsImpl(void)
{
    if (repRtosIsSchedulerRunning()) {
        return repRtosGetTickMs();
    }

    return HAL_GetTick();
}

static void frameparserPortLoadDefaultProtoCfgImpl(uint32_t protocolId, stFrmPsrProtoCfg *protoCfg)
{
    if (protoCfg == NULL) {
        return;
    }

    (void)memset(protoCfg, 0, sizeof(*protoCfg));
    if (protocolId != WT2003HX_PROTOCOL_ID) {
        return;
    }

    protoCfg->headPatList[0] = gFrameparserPortWt2003hxFrameHead;
    protoCfg->headPatCount = 1U;
    protoCfg->headPatLen = 1U;
    protoCfg->minHeadLen = 3U;
    protoCfg->minPktLen = WT2003HX_FRAME_MIN_LEN;
    protoCfg->maxPktLen = WT2003HX_FRAME_MAX_LEN;
    protoCfg->waitPktToutMs = 100U;
    protoCfg->crcRangeStartOff = 1;
    protoCfg->crcRangeEndOff = -3;
    protoCfg->crcFieldOff = -2;
    protoCfg->crcFieldLen = 1U;
    protoCfg->cmdindex = 2U;
    protoCfg->cmdLen = 1U;
    protoCfg->packlenindex = 1U;
    protoCfg->packlenLen = 1U;
    protoCfg->crcFieldEnd = FRM_PSR_CRC_END_LITTLE;
    protoCfg->headLenFunc = frameparserPortWt2003hxHeadLen;
    protoCfg->pktLenFunc = frameparserPortWt2003hxPktLen;
    protoCfg->crcCalcFunc = frameparserPortWt2003hxChecksum;
}

static uint32_t frameparserPortWt2003hxHeadLen(const uint8_t *buf, uint32_t availLen, void *userCtx)
{
    (void)buf;
    (void)userCtx;

    return (availLen >= 3U) ? 3U : 0U;
}

static uint32_t frameparserPortWt2003hxPktLen(const uint8_t *buf, uint32_t headLen, uint32_t availLen, void *userCtx)
{
    uint32_t lPktLen;

    (void)headLen;
    (void)userCtx;

    if ((buf == NULL) || (availLen < 2U)) {
        return 0U;
    }

    lPktLen = (uint32_t)buf[1] + 2U;
    if ((lPktLen < WT2003HX_FRAME_MIN_LEN) || (lPktLen > WT2003HX_FRAME_MAX_LEN)) {
        return 0U;
    }

    if ((availLen >= lPktLen) && (buf[lPktLen - 1U] != WT2003HX_FRAME_TAIL)) {
        return 0U;
    }

    return lPktLen;
}

static uint32_t frameparserPortWt2003hxChecksum(const uint8_t *buf, uint32_t len, void *userCtx)
{
    uint32_t lIndex;
    uint8_t lChecksum = 0U;

    (void)userCtx;

    if (buf == NULL) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < len; lIndex++) {
        lChecksum = (uint8_t)(lChecksum + buf[lIndex]);
    }

    return lChecksum;
}

const stFrmPsrOps *frmPsrPortGetOps(void)
{
    return &gFrameparserPortOps;
}

/**************************End of file********************************/