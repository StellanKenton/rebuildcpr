/***********************************************************************************
* @file     : wireless.c
* @brief    : Project-side FC41D wireless manager.
* @details  : Provides BLE info query and non-blocking periodic service on top of
*             the FC41D project binding.
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "wireless.h"

#include <string.h>

#include "drvuart.h"

#include "../../../rep/service/log/log.h"
#include "../../port/drvuart_port.h"
#include "../../port/fc41dport.h"

typedef enum eWirelessInitQueryState {
    eWIRELESS_INIT_QUERY_WAIT_BLE_READY = 0,
    eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT,
    eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT,
    eWIRELESS_INIT_QUERY_VERSION_SUBMIT,
    eWIRELESS_INIT_QUERY_VERSION_WAIT,
    eWIRELESS_INIT_QUERY_DONE,
} eWirelessInitQueryState;

static eWirelessState gWirelessState = eWIRELESS_STATE_INIT;
static bool gWirelessInitDone = false;
static eWirelessInitQueryState gWirelessInitQueryState = eWIRELESS_INIT_QUERY_WAIT_BLE_READY;
static stFc41dAtResp gWirelessBleAddrResp;
static stFc41dAtResp gWirelessVersionResp;
static bool gWirelessBleAddrLogged = false;
static bool gWirelessVersionLogged = false;

static uint8_t gWirelessBleAddrLineBuf[128U];
static uint8_t gWirelessVersionLineBuf[128U];

#define WIRELESS_INFO_QUERY_TIMEOUT_MS      3000U

static bool wirelessIsBleReadyForInitQuery(eFc41dBleState state);
static bool wirelessShouldIgnoreInfoLine(const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessBleAddrLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static void wirelessVersionLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen);
static eFc41dStatus wirelessStartBleAddrQuery(void);
static eFc41dStatus wirelessStartVersionQuery(void);
static void wirelessFinalizeBleAddrQuery(void);
static void wirelessFinalizeVersionQuery(void);
static void wirelessProcessInitQueries(void);
static void wirelessUpdateState(void);

static bool wirelessIsBleReadyForInitQuery(eFc41dBleState state)
{
    switch (state) {
        case FC41D_BLE_STATE_PERIPHERAL_WAIT_CONNECT:
        case FC41D_BLE_STATE_PERIPHERAL_CONNECTED:
        case FC41D_BLE_STATE_PERIPHERAL_DISCONNECTED:
        case FC41D_BLE_STATE_CENTRAL_WAIT_CONNECT:
        case FC41D_BLE_STATE_CENTRAL_CONNECTED:
        case FC41D_BLE_STATE_CENTRAL_DISCONNECTED:
            return true;

        default:
            return false;
    }
}

static bool wirelessShouldIgnoreInfoLine(const uint8_t *lineBuf, uint16_t lineLen)
{
    if ((lineBuf == NULL) || (lineLen == 0U)) {
        return true;
    }

    if ((lineLen >= 2U) && (lineBuf[0] == 'A') && (lineBuf[1] == 'T')) {
        return true;
    }

    if (((lineLen == 2U) && (memcmp(lineBuf, "OK", 2U) == 0)) ||
        ((lineLen == 5U) && (memcmp(lineBuf, "ERROR", 5U) == 0))) {
        return true;
    }

    return false;
}

static void wirelessBleAddrLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    bool *lLogged = (bool *)userData;

    if (wirelessShouldIgnoreInfoLine(lineBuf, lineLen)) {
        return;
    }

    if (lLogged != NULL) {
        *lLogged = true;
    }

    LOG_I(WIRELESS_LOG_TAG, "ble mac: %.*s", (int)lineLen, (const char *)lineBuf);
}

static void wirelessVersionLineCallback(void *userData, const uint8_t *lineBuf, uint16_t lineLen)
{
    bool *lLogged = (bool *)userData;

    if (wirelessShouldIgnoreInfoLine(lineBuf, lineLen)) {
        return;
    }

    if (lLogged != NULL) {
        *lLogged = true;
    }

    LOG_I(WIRELESS_LOG_TAG, "fc41d version: %.*s", (int)lineLen, (const char *)lineBuf);
}

static eFc41dStatus wirelessStartBleAddrQuery(void)
{
    stFc41dAtOpt lOpt;
    char lCmdBuf[32U];
    eFc41dStatus lStatus;

    (void)memset(&lOpt, 0, sizeof(lOpt));
    (void)memset(&gWirelessBleAddrResp, 0, sizeof(gWirelessBleAddrResp));
    gWirelessBleAddrResp.lineBuf = gWirelessBleAddrLineBuf;
    gWirelessBleAddrResp.lineBufSize = (uint16_t)sizeof(gWirelessBleAddrLineBuf);
    gWirelessBleAddrResp.pfLineCallback = wirelessBleAddrLineCallback;
    gWirelessBleAddrResp.lineCallbackUserData = &gWirelessBleAddrLogged;
    lOpt.totalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.responseToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.finalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;

    lStatus = fc41dAtBuildQueryCmd(lCmdBuf, (uint16_t)sizeof(lCmdBuf), FC41D_AT_CATALOG_CMD_QBLEADDR);
    if (lStatus != FC41D_STATUS_OK) {
        return lStatus;
    }

    lStatus = fc41dExecAtText(WIRELESS_FC41D_DEVICE, lCmdBuf, &lOpt, &gWirelessBleAddrResp);
    if (lStatus == FC41D_STATUS_OK) {
        LOG_I(WIRELESS_LOG_TAG, "ble mac query start");
    }

    return lStatus;
}

static eFc41dStatus wirelessStartVersionQuery(void)
{
    stFc41dAtOpt lOpt;
    eFc41dStatus lStatus;

    (void)memset(&lOpt, 0, sizeof(lOpt));
    (void)memset(&gWirelessVersionResp, 0, sizeof(gWirelessVersionResp));
    gWirelessVersionResp.lineBuf = gWirelessVersionLineBuf;
    gWirelessVersionResp.lineBufSize = (uint16_t)sizeof(gWirelessVersionLineBuf);
    gWirelessVersionResp.pfLineCallback = wirelessVersionLineCallback;
    gWirelessVersionResp.lineCallbackUserData = &gWirelessVersionLogged;
    lOpt.totalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.responseToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;
    lOpt.finalToutMs = WIRELESS_INFO_QUERY_TIMEOUT_MS;

    lStatus = fc41dExecAtCmd(WIRELESS_FC41D_DEVICE, FC41D_AT_CATALOG_CMD_QVERSION, &lOpt, &gWirelessVersionResp);
    if (lStatus == FC41D_STATUS_OK) {
        LOG_I(WIRELESS_LOG_TAG, "fc41d version query start");
    }

    return lStatus;
}

static void wirelessFinalizeBleAddrQuery(void)
{
    if (gWirelessBleAddrResp.result == FC41D_AT_RESULT_OK) {
        if (!gWirelessBleAddrLogged && (gWirelessBleAddrResp.lineLen > 0U)) {
            LOG_I(WIRELESS_LOG_TAG,
                  "ble mac: %.*s",
                  (int)gWirelessBleAddrResp.lineLen,
                  (const char *)gWirelessBleAddrResp.lineBuf);
        }
        return;
    }

    LOG_W(WIRELESS_LOG_TAG,
          "ble mac query fail result=%u lastLine=%.*s",
          (unsigned int)gWirelessBleAddrResp.result,
          (int)gWirelessBleAddrResp.lineLen,
          (const char *)gWirelessBleAddrResp.lineBuf);
}

static void wirelessFinalizeVersionQuery(void)
{
    if (gWirelessVersionResp.result == FC41D_AT_RESULT_OK) {
        if (!gWirelessVersionLogged && (gWirelessVersionResp.lineLen > 0U)) {
            LOG_I(WIRELESS_LOG_TAG,
                  "fc41d version: %.*s",
                  (int)gWirelessVersionResp.lineLen,
                  (const char *)gWirelessVersionResp.lineBuf);
        }
        return;
    }

    LOG_W(WIRELESS_LOG_TAG,
          "fc41d version query fail result=%u lastLine=%.*s",
          (unsigned int)gWirelessVersionResp.result,
          (int)gWirelessVersionResp.lineLen,
          (const char *)gWirelessVersionResp.lineBuf);
}

static void wirelessProcessInitQueries(void)
{
    stFc41dInfo lInfo;
    eFc41dStatus lStatus;

    if (gWirelessInitQueryState == eWIRELESS_INIT_QUERY_DONE) {
        return;
    }

    if (fc41dGetInfo(WIRELESS_FC41D_DEVICE, &lInfo) != FC41D_STATUS_OK) {
        return;
    }

    switch (gWirelessInitQueryState) {
        case eWIRELESS_INIT_QUERY_WAIT_BLE_READY:
            if (wirelessIsBleReadyForInitQuery(lInfo.ble.state)) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_BLE_ADDR_SUBMIT:
            lStatus = wirelessStartBleAddrQuery();
            if (lStatus == FC41D_STATUS_OK) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT;
            } else if (lStatus != FC41D_STATUS_BUSY) {
                LOG_W(WIRELESS_LOG_TAG, "ble mac query submit fail status=%u", (unsigned int)lStatus);
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_BLE_ADDR_WAIT:
            if (!fc41dExecAtIsBusy(WIRELESS_FC41D_DEVICE)) {
                wirelessFinalizeBleAddrQuery();
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_SUBMIT;
            }
            break;

        case eWIRELESS_INIT_QUERY_VERSION_SUBMIT:
            lStatus = wirelessStartVersionQuery();
            if (lStatus == FC41D_STATUS_OK) {
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_VERSION_WAIT;
            } else if (lStatus != FC41D_STATUS_BUSY) {
                LOG_W(WIRELESS_LOG_TAG, "fc41d version query submit fail status=%u", (unsigned int)lStatus);
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_DONE;
            }
            break;

        case eWIRELESS_INIT_QUERY_VERSION_WAIT:
            if (!fc41dExecAtIsBusy(WIRELESS_FC41D_DEVICE)) {
                wirelessFinalizeVersionQuery();
                gWirelessInitQueryState = eWIRELESS_INIT_QUERY_DONE;
            }
            break;

        case eWIRELESS_INIT_QUERY_DONE:
        default:
            break;
    }
}

static void wirelessUpdateState(void)
{
    stFc41dInfo lInfo;

    if (fc41dPortPollBootReady(WIRELESS_FC41D_DEVICE, WIRELESS_BOOT_READY_TIMEOUT_MS) != FC41DPORT_BOOT_READY) {
        if (gWirelessState != eWIRELESS_STATE_ERROR) {
            gWirelessState = eWIRELESS_STATE_INIT;
        }
        return;
    }

    if (fc41dGetInfo(WIRELESS_FC41D_DEVICE, &lInfo) != FC41D_STATUS_OK) {
        if (gWirelessInitDone) {
            gWirelessState = eWIRELESS_STATE_ERROR;
        }
        return;
    }

    if (lInfo.ble.state == FC41D_BLE_STATE_ERROR) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return;
    }

    gWirelessState = lInfo.isReady ? eWIRELESS_STATE_NORMAL : eWIRELESS_STATE_INIT;
}

bool wirelessInit(void)
{
    stFc41dCfg lCfg;

    if (gWirelessInitDone) {
        return gWirelessState != eWIRELESS_STATE_ERROR;
    }

    if (drvUartInit(DRVUART_WIFI) != DRV_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        LOG_E(WIRELESS_LOG_TAG, "wifi uart init fail");
        return false;
    }

    if (fc41dGetDefCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dSetCfg(WIRELESS_FC41D_DEVICE, &lCfg) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    if (fc41dInit(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        return false;
    }

    fc41dPortResetBootWaitState();
    gWirelessInitQueryState = eWIRELESS_INIT_QUERY_WAIT_BLE_READY;
    gWirelessBleAddrLogged = false;
    gWirelessVersionLogged = false;
    (void)memset(&gWirelessBleAddrResp, 0, sizeof(gWirelessBleAddrResp));
    (void)memset(&gWirelessVersionResp, 0, sizeof(gWirelessVersionResp));
    gWirelessInitDone = true;
    wirelessUpdateState();
    LOG_I(WIRELESS_LOG_TAG, "fc41d init ok ble=rumi");
    return true;
}

void wirelessProcess(void)
{
    if (!gWirelessInitDone) {
        return;
    }

    switch (fc41dPortPollBootReady(WIRELESS_FC41D_DEVICE, WIRELESS_BOOT_READY_TIMEOUT_MS)) {
        case FC41DPORT_BOOT_READY:
            break;

        case FC41DPORT_BOOT_TIMEOUT:
            gWirelessState = eWIRELESS_STATE_ERROR;
            return;

        case FC41DPORT_BOOT_WAITING:
        default:
            return;
    }

    if (fc41dProcess(WIRELESS_FC41D_DEVICE) != FC41D_STATUS_OK) {
        gWirelessState = eWIRELESS_STATE_ERROR;
        (void)fc41dRecover(WIRELESS_FC41D_DEVICE);
        return;
    }

    wirelessUpdateState();
    wirelessProcessInitQueries();
}

const eWirelessState *wirelessGetStatus(void)
{
    return &gWirelessState;
}

/**************************End of file********************************/
