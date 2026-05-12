/***********************************************************************************
* @file     : ec800m_port.c
* @brief    : Project-side EC800M binding implementation.
* @details  : Keeps EC800M transport unbound in the current product while
*             exposing an explicit ops table so the reusable core no longer
*             relies on weak platform hooks.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "ec800m_port.h"

#include <stddef.h>

static void ec800mPortLoadDefaultCfg(eEc800mMapType device, stEc800mCfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->pwrkeyPin = 0U;
    cfg->resetPin = 0U;
    cfg->rxPollChunkSize = EC800M_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = EC800M_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = EC800M_DEFAULT_BOOT_WAIT_MS;
    cfg->pwrkeyPulseMs = EC800M_DEFAULT_PWRKEY_PULSE_MS;
    cfg->resetPulseMs = EC800M_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = EC800M_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = EC800M_DEFAULT_READY_TIMEOUT_MS;
    cfg->retryIntervalMs = EC800M_DEFAULT_RETRY_INTERVAL_MS;
}

static const stEc800mTransportInterface *ec800mPortGetTransportInterface(const stEc800mCfg *cfg)
{
    (void)cfg;
    return NULL;
}

static const stEc800mControlInterface *ec800mPortGetControlInterface(eEc800mMapType device)
{
    (void)device;
    return NULL;
}

static bool ec800mPortIsValidCfg(const stEc800mCfg *cfg)
{
    (void)cfg;
    return false;
}

static const stEc800mOps gEc800mPortOps = {
    .loadDefaultCfg = ec800mPortLoadDefaultCfg,
    .getTransportInterface = ec800mPortGetTransportInterface,
    .getControlInterface = ec800mPortGetControlInterface,
    .isValidCfg = ec800mPortIsValidCfg,
};

const stEc800mOps *ec800mPortGetOps(void)
{
    return &gEc800mPortOps;
}

/**************************End of file********************************/