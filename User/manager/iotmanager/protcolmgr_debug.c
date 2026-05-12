/************************************************************************************
* @file     : protcolmgr_debug.c
* @brief    : CPR sensor protocol manager RTT debug hooks.
***********************************************************************************/
#include "protcolmgr_debug.h"

#include <stdint.h>
#include <string.h>

#include "cprsensor_protocol.h"
#include "iotmanager.h"
#include "protcolmgr.h"
#include "../audio/audio.h"
#include "../memory/memory.h"
#include "../../../rep/sys/log/console.h"
#include "../../../rep/sys/log/log.h"

#define PROTCOL_MGR_DEBUG_FRAME_BUFFER_SIZE 32U
#define PROTCOL_MGR_DEBUG_LANGUAGE_PATH     "/setting/language"

static eConsoleCommandResult protcolMgrDebugConsoleSelfTestHandler(uint32_t transport, int argc, char *argv[]);
static void protcolMgrDebugFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg);
static bool protcolMgrDebugPackAndPush(eCprsensorProtocolCmd cmd, const uint8_t *payload, uint16_t payloadLen);
static bool protcolMgrDebugRunLanguageSelfTest(uint8_t language, uint8_t *storedValue);
static bool protcolMgrDebugParseU8(const char *text, uint8_t *value);
static bool protcolMgrDebugIsValidLanguage(uint8_t language);

static const stConsoleCommand gProtcolMgrDebugSelfTestConsoleCommand = {
	.commandName = "ptcltest",
	.helpText = "ptcltest language [1-5] - verify 0x35 creates /setting/language",
	.ownerTag = "ptclmgr",
	.handler = protcolMgrDebugConsoleSelfTestHandler,
};

static void protcolMgrDebugFillRawCodec(stCprsensorProtocolCodecCfg *codecCfg)
{
	if (codecCfg == NULL) {
		return;
	}

	codecCfg->crc.polynomial = 0x1021U;
	codecCfg->crc.initValue = 0xFFFFU;
	codecCfg->crc.xorOut = 0U;
	codecCfg->crc.reflectInput = false;
	codecCfg->crc.reflectOutput = false;
	codecCfg->cipher.enabled = false;
	codecCfg->cipher.blockSize = CPRSENSOR_PROTOCOL_AES_ALIGN_SIZE;
	codecCfg->cipher.encrypt = NULL;
	codecCfg->cipher.decrypt = NULL;
	codecCfg->cipher.userData = NULL;
}

static bool protcolMgrDebugPackAndPush(eCprsensorProtocolCmd cmd, const uint8_t *payload, uint16_t payloadLen)
{
	stCprsensorProtocolCodecCfg lCodecCfg;
	uint8_t lFrameBuffer[PROTCOL_MGR_DEBUG_FRAME_BUFFER_SIZE];
	uint16_t lFrameLen = 0U;

	protcolMgrDebugFillRawCodec(&lCodecCfg);
	if (cprsensorProtocolPackFrame(cmd,
				       payload,
				       payloadLen,
				       &lCodecCfg,
				       lFrameBuffer,
				       (uint16_t)sizeof(lFrameBuffer),
				       &lFrameLen) != CPRSENSOR_PROTOCOL_STATUS_OK) {
		return false;
	}

	return protcolMgrPushReceivedData(IOT_MANAGER_LINK_BLE, lFrameBuffer, lFrameLen);
}

static bool protcolMgrDebugRunLanguageSelfTest(uint8_t language, uint8_t *storedValue)
{
	uint8_t lBuffer = 0U;
	uint32_t lActualSize = 0U;

	if ((storedValue == NULL) || !protcolMgrDebugIsValidLanguage(language) || !memoryIsReady()) {
		return false;
	}

	if (memoryExists(PROTCOL_MGR_DEBUG_LANGUAGE_PATH) && !memoryDelete(PROTCOL_MGR_DEBUG_LANGUAGE_PATH)) {
		return false;
	}

	if (!protcolMgrDebugPackAndPush(CPRSENSOR_PROTOCOL_CMD_LANGUAGE, NULL, 0U)) {
		return false;
	}

	if (memoryExists(PROTCOL_MGR_DEBUG_LANGUAGE_PATH)) {
		return false;
	}

	if (!protcolMgrDebugPackAndPush(CPRSENSOR_PROTOCOL_CMD_LANGUAGE, &language, (uint16_t)sizeof(language))) {
		return false;
	}

	if (!memoryReadFile(PROTCOL_MGR_DEBUG_LANGUAGE_PATH, &lBuffer, (uint32_t)sizeof(lBuffer), &lActualSize) ||
	    (lActualSize != sizeof(lBuffer))) {
		return false;
	}

	*storedValue = lBuffer;
	return lBuffer == language;
}

static bool protcolMgrDebugParseU8(const char *text, uint8_t *value)
{
	uint32_t lValue = 0U;
	uint32_t lIndex = 0U;

	if ((text == NULL) || (value == NULL) || (text[0] == '\0')) {
		return false;
	}

	while (text[lIndex] != '\0') {
		if ((text[lIndex] < '0') || (text[lIndex] > '9')) {
			return false;
		}

		lValue = (lValue * 10U) + (uint32_t)(text[lIndex] - '0');
		if (lValue > 255U) {
			return false;
		}
		lIndex++;
	}

	*value = (uint8_t)lValue;
	return true;
}

static bool protcolMgrDebugIsValidLanguage(uint8_t language)
{
	return (language >= (uint8_t)AUDIO_LANGUAGE_ZH) && (language <= (uint8_t)AUDIO_LANGUAGE_IT);
}

static eConsoleCommandResult protcolMgrDebugConsoleSelfTestHandler(uint32_t transport, int argc, char *argv[])
{
	uint8_t lLanguage = (uint8_t)AUDIO_LANGUAGE_EN;
	uint8_t lStoredValue = 0U;

	if ((argc != 2) && (argc != 3)) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	if ((argv[1] == NULL) || (strcmp(argv[1], "language") != 0)) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	if ((argc == 3) && (!protcolMgrDebugParseU8(argv[2], &lLanguage) || !protcolMgrDebugIsValidLanguage(lLanguage))) {
		return CONSOLE_COMMAND_RESULT_INVALID_ARGUMENT;
	}

	if (!protcolMgrDebugRunLanguageSelfTest(lLanguage, &lStoredValue)) {
		if (logConsoleReply(transport,
				    "FAIL language selftest target=%u stored=%u",
				    (unsigned int)lLanguage,
				    (unsigned int)lStoredValue) <= 0) {
			return CONSOLE_COMMAND_RESULT_ERROR;
		}
		return CONSOLE_COMMAND_RESULT_OK;
	}

	if (logConsoleReply(transport,
			    "OK language selftest target=%u stored=%u path=%s",
			    (unsigned int)lLanguage,
			    (unsigned int)lStoredValue,
			    PROTCOL_MGR_DEBUG_LANGUAGE_PATH) <= 0) {
		return CONSOLE_COMMAND_RESULT_ERROR;
	}

	return CONSOLE_COMMAND_RESULT_OK;
}

bool protcolMgrDebugConsoleRegister(void)
{
	return logRegisterConsole(&gProtcolMgrDebugSelfTestConsoleCommand);
}
/**************************End of file********************************/
