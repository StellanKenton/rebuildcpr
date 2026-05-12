/***********************************************************************************
* @file     : esp32c5_port.c
* @brief    : Project-side ESP32-C5 binding implementation.
* @details  : Keeps ESP32-C5 transport unbound in the current product while
*             exposing an explicit ops table so the reusable core no longer
*             relies on weak platform hooks.
* @author   : GitHub Copilot
* @date     : 2026-05-12
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "esp32c5_port.h"

#include <stddef.h>

static void esp32c5PortLoadDefaultCfg(eEsp32c5MapType device, stEsp32c5Cfg *cfg)
{
    (void)device;

    if (cfg == NULL) {
        return;
    }

    cfg->linkId = 0U;
    cfg->resetPin = 0U;
    cfg->rxPollChunkSize = ESP32C5_RX_POLL_CHUNK_SIZE;
    cfg->txTimeoutMs = ESP32C5_DEFAULT_TX_TIMEOUT_MS;
    cfg->bootWaitMs = ESP32C5_DEFAULT_BOOT_WAIT_MS;
    cfg->resetPulseMs = ESP32C5_DEFAULT_RESET_PULSE_MS;
    cfg->resetWaitMs = ESP32C5_DEFAULT_RESET_WAIT_MS;
    cfg->readyTimeoutMs = ESP32C5_DEFAULT_READY_TIMEOUT_MS;
    cfg->readyProbeMs = ESP32C5_DEFAULT_READY_PROBE_MS;
    cfg->retryIntervalMs = ESP32C5_DEFAULT_RETRY_INTERVAL_MS;
}

static const stEsp32c5TransportInterface *esp32c5PortGetTransportInterface(const stEsp32c5Cfg *cfg)
{
    (void)cfg;
    return NULL;
}

static const stEsp32c5ControlInterface *esp32c5PortGetControlInterface(eEsp32c5MapType device)
{
    (void)device;
    return NULL;
}

static bool esp32c5PortIsValidCfg(const stEsp32c5Cfg *cfg)
{
    (void)cfg;
    return false;
}

static const stEsp32c5Ops gEsp32c5PortOps = {
    .loadDefaultCfg = esp32c5PortLoadDefaultCfg,
    .getTransportInterface = esp32c5PortGetTransportInterface,
    .getControlInterface = esp32c5PortGetControlInterface,
    .isValidCfg = esp32c5PortIsValidCfg,
};

const stEsp32c5Ops *esp32c5PortGetOps(void)
{
    return &gEsp32c5PortOps;
}

/**************************End of file********************************/