/***********************************************************************************
* @file     : iotmanager.c
* @brief    : CPR sensor IoT manager for BLE/WiFi links.
* @details  : Keeps the protocol router small for the current FC41D product.
**********************************************************************************/
#include "iotmanager.h"
#include "protcolmgr.h"

#include <string.h>

#include "../wireless/wireless.h"
#include "../../../rep/service/rtos/rtos.h"

typedef struct stIotManagerContext {
    stIotManagerState publicState;
    bool initialized;
} stIotManagerContext;

static stIotManagerContext gIotManagerState;

static const stIotManagerLinkCaps gIotManagerDefaultLinkCaps[IOT_MANAGER_LINK_MAX] = {
    { false, false, false, false },
    { true, false, false, false },
    { false, true, true, false },
    { false, false, false, false },
    { false, false, false, false },
};

static bool iotManagerIsValidLink(eIotManagerLinkId linkId)
{
    return (linkId > IOT_MANAGER_LINK_NONE) && (linkId < IOT_MANAGER_LINK_MAX);
}

static void iotManagerInitRoute(stIotManagerServiceRoute *route,
                                eIotManagerServiceId serviceId,
                                eIotManagerLinkId preferredLink,
                                eIotManagerSendPolicy policy)
{
    if (route == NULL) {
        return;
    }

    (void)memset(route, 0, sizeof(*route));
    route->serviceId = serviceId;
    route->preferredLink = preferredLink;
    route->policy = policy;
    route->state = IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
}

static void iotManagerInitLinkRuntime(stIotManagerLinkRuntime *runtime, eIotManagerLinkId linkId)
{
    if (runtime == NULL) {
        return;
    }

    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->linkId = linkId;
    runtime->caps = gIotManagerDefaultLinkCaps[linkId];
    runtime->state = IOT_MANAGER_LINK_STATE_DISABLED;
    runtime->installed = (linkId == IOT_MANAGER_LINK_BLE) || (linkId == IOT_MANAGER_LINK_WIFI);
    runtime->enabled = runtime->installed;
}

static bool iotManagerLinkCanRunService(const stIotManagerLinkRuntime *runtime, eIotManagerServiceId serviceId)
{
    if ((runtime == NULL) || !runtime->installed || !runtime->enabled) {
        return false;
    }

    switch (serviceId) {
    case IOT_MANAGER_SERVICE_BLE_LOCAL:
        return runtime->caps.supportBleLocal && (runtime->moduleReady || runtime->peerConnected);
    case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
        return runtime->caps.supportMqttAuthHttp && (runtime->netReady || runtime->mqttAuthReady);
    case IOT_MANAGER_SERVICE_MQTT:
        return runtime->caps.supportMqtt && (runtime->netReady || runtime->mqttReady);
    default:
        break;
    }

    return false;
}

static eIotManagerLinkId iotManagerPickAutoLink(eIotManagerServiceId serviceId)
{
    if (serviceId == IOT_MANAGER_SERVICE_BLE_LOCAL) {
        return iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE], serviceId) ?
            IOT_MANAGER_LINK_BLE : IOT_MANAGER_LINK_NONE;
    }

    return iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_WIFI], serviceId) ?
        IOT_MANAGER_LINK_WIFI : IOT_MANAGER_LINK_NONE;
}

static stIotManagerServiceRoute *iotManagerGetRouteByServiceId(eIotManagerServiceId serviceId)
{
    switch (serviceId) {
    case IOT_MANAGER_SERVICE_BLE_LOCAL:
        return &gIotManagerState.publicState.bleLocalRoute;
    case IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP:
        return &gIotManagerState.publicState.mqttAuthRoute;
    case IOT_MANAGER_SERVICE_MQTT:
        return &gIotManagerState.publicState.mqttRoute;
    case IOT_MANAGER_SERVICE_TCP_SERVER:
        return &gIotManagerState.publicState.tcpServerRoute;
    default:
        break;
    }

    return NULL;
}

static void iotManagerRefreshRoute(stIotManagerServiceRoute *route)
{
    eIotManagerLinkId candidate = IOT_MANAGER_LINK_NONE;

    if (route == NULL) {
        return;
    }

    if ((route->preferredLink != IOT_MANAGER_LINK_NONE) &&
        iotManagerLinkCanRunService(&gIotManagerState.publicState.links[route->preferredLink], route->serviceId)) {
        candidate = route->preferredLink;
    } else if (route->policy == IOT_MANAGER_SEND_POLICY_AUTO) {
        candidate = iotManagerPickAutoLink(route->serviceId);
    }

    route->activeLink = candidate;
    route->state = (candidate != IOT_MANAGER_LINK_NONE) ? IOT_MANAGER_SERVICE_STATE_READY : IOT_MANAGER_SERVICE_STATE_WAIT_LINK;
}

static void iotManagerRefreshState(void)
{
    uint32_t index;

    for (index = 0U; index < (uint32_t)IOT_MANAGER_LINK_MAX; index++) {
        gIotManagerState.publicState.links[index].selected = false;
    }

    iotManagerRefreshRoute(&gIotManagerState.publicState.bleLocalRoute);
    iotManagerRefreshRoute(&gIotManagerState.publicState.mqttAuthRoute);
    iotManagerRefreshRoute(&gIotManagerState.publicState.mqttRoute);
    iotManagerRefreshRoute(&gIotManagerState.publicState.tcpServerRoute);

    if (iotManagerIsValidLink(gIotManagerState.publicState.bleLocalRoute.activeLink)) {
        gIotManagerState.publicState.links[gIotManagerState.publicState.bleLocalRoute.activeLink].selected = true;
    }
    if (iotManagerIsValidLink(gIotManagerState.publicState.mqttRoute.activeLink)) {
        gIotManagerState.publicState.links[gIotManagerState.publicState.mqttRoute.activeLink].selected = true;
    }

    gIotManagerState.publicState.localBleReady =
        iotManagerLinkCanRunService(&gIotManagerState.publicState.links[IOT_MANAGER_LINK_BLE], IOT_MANAGER_SERVICE_BLE_LOCAL);
    gIotManagerState.publicState.cloudAnyReady =
        iotManagerPickAutoLink(IOT_MANAGER_SERVICE_MQTT) != IOT_MANAGER_LINK_NONE;
    gIotManagerState.publicState.mqttAuthDone =
        gIotManagerState.publicState.links[IOT_MANAGER_LINK_WIFI].mqttAuthReady;
}

void iotManagerEnsureStateInitialized(void)
{
    uint32_t index;

    if (gIotManagerState.initialized) {
        return;
    }

    (void)memset(&gIotManagerState.publicState, 0, sizeof(gIotManagerState.publicState));
    for (index = 0U; index < (uint32_t)IOT_MANAGER_LINK_MAX; index++) {
        iotManagerInitLinkRuntime(&gIotManagerState.publicState.links[index], (eIotManagerLinkId)index);
    }
    gIotManagerState.publicState.links[IOT_MANAGER_LINK_NONE].state = IOT_MANAGER_LINK_STATE_ABSENT;
    iotManagerInitRoute(&gIotManagerState.publicState.bleLocalRoute,
                        IOT_MANAGER_SERVICE_BLE_LOCAL,
                        IOT_MANAGER_LINK_BLE,
                        IOT_MANAGER_SEND_POLICY_FIXED);
    iotManagerInitRoute(&gIotManagerState.publicState.mqttAuthRoute,
                        IOT_MANAGER_SERVICE_MQTT_AUTH_HTTP,
                        IOT_MANAGER_LINK_WIFI,
                        IOT_MANAGER_SEND_POLICY_AUTO);
    iotManagerInitRoute(&gIotManagerState.publicState.mqttRoute,
                        IOT_MANAGER_SERVICE_MQTT,
                        IOT_MANAGER_LINK_WIFI,
                        IOT_MANAGER_SEND_POLICY_AUTO);
    iotManagerInitRoute(&gIotManagerState.publicState.tcpServerRoute,
                        IOT_MANAGER_SERVICE_TCP_SERVER,
                        IOT_MANAGER_LINK_NONE,
                        IOT_MANAGER_SEND_POLICY_FIXED);
    gIotManagerState.initialized = true;
    iotManagerRefreshState();
}

bool iotManagerSendByLink(eIotManagerLinkId linkId, const uint8_t *buffer, uint16_t length)
{
    if (!iotManagerIsValidLink(linkId) || (buffer == NULL) || (length == 0U)) {
        return false;
    }

    if (linkId == IOT_MANAGER_LINK_BLE) {
        return wirelessSendBleData(buffer, length);
    }
    if (linkId == IOT_MANAGER_LINK_WIFI) {
        return wirelessSendWifiData(buffer, length);
    }

    return false;
}

bool iotManagerSend(eIotManagerServiceId serviceId, const uint8_t *buffer, uint16_t length)
{
    stIotManagerServiceRoute *route;
    eIotManagerLinkId linkId;

    iotManagerEnsureStateInitialized();
    route = iotManagerGetRouteByServiceId(serviceId);
    if (route == NULL) {
        return false;
    }

    repRtosEnterCritical();
    iotManagerRefreshState();
    linkId = route->activeLink;
    repRtosExitCritical();
    return iotManagerSendByLink(linkId, buffer, length);
}

bool iotManagerSendByInterface(eIotManagerInterface interfaceType, const uint8_t *buffer, uint16_t length)
{
    if (interfaceType == IOT_MANAGER_INTERFACE_WIRELESS) {
        return iotManagerSendByLink(IOT_MANAGER_LINK_BLE, buffer, length);
    }
    return false;
}

bool iotManagerUpdateLinkState(eIotManagerLinkId linkId, const stIotManagerLinkRuntime *runtime)
{
    if (!iotManagerIsValidLink(linkId) || (runtime == NULL)) {
        return false;
    }

    iotManagerEnsureStateInitialized();
    repRtosEnterCritical();
    gIotManagerState.publicState.links[linkId] = *runtime;
    gIotManagerState.publicState.links[linkId].linkId = linkId;
    if ((gIotManagerState.publicState.links[linkId].caps.supportBleLocal == false) &&
        (gIotManagerState.publicState.links[linkId].caps.supportMqtt == false) &&
        (gIotManagerState.publicState.links[linkId].caps.supportMqttAuthHttp == false)) {
        gIotManagerState.publicState.links[linkId].caps = gIotManagerDefaultLinkCaps[linkId];
    }
    iotManagerRefreshState();
    repRtosExitCritical();
    return true;
}

bool iotManagerSelectRoute(eIotManagerServiceId serviceId, eIotManagerLinkId linkId)
{
    stIotManagerServiceRoute *route;

    iotManagerEnsureStateInitialized();
    route = iotManagerGetRouteByServiceId(serviceId);
    if ((route == NULL) || ((linkId != IOT_MANAGER_LINK_NONE) && !iotManagerIsValidLink(linkId))) {
        return false;
    }

    repRtosEnterCritical();
    route->preferredLink = linkId;
    route->policy = IOT_MANAGER_SEND_POLICY_FIXED;
    iotManagerRefreshState();
    repRtosExitCritical();
    return true;
}

bool iotManagerSetActiveInterface(eIotManagerInterface interfaceType)
{
    (void)interfaceType;
    return true;
}

bool iotManagerSetTargetInterface(eIotManagerInterface interfaceType)
{
    (void)interfaceType;
    return true;
}

bool iotManagerSetInterfaceReady(eIotManagerInterface interfaceType, bool ready)
{
    (void)interfaceType;
    (void)ready;
    return true;
}

bool iotManagerSetInterfaceStatus(eIotManagerInterface interfaceType, eIotManagerNetStatus status)
{
    (void)interfaceType;
    (void)status;
    return true;
}

const stIotManagerState *iotManagerGetState(void)
{
    iotManagerEnsureStateInitialized();
    return &gIotManagerState.publicState;
}

void iotManagerProcess(void)
{
    protcolMgrProcess();
}

/**************************End of file********************************/
