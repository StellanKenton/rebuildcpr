/***********************************************************************************
* @file     : audio.c
* @brief    : Project-side audio manager implementation.
* @details  : Runs the CPR audio prompt business state machine on top of WT2003HX.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
**********************************************************************************/
#include "audio.h"

#include <string.h>

#include "../memory/memory.h"
#include "../../port/wt2003hx_port.h"
#include "../cpralg/cpralgmgr.h"
#include "../power/power.h"
#include "../../system/system.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

#define AUDIO_LOG_TAG "audio"
#define AUDIO_INIT_QUERY_TIMEOUT_MS 1000U
#define AUDIO_INIT_QUERY_RETRY_DELAY_MS 120U
#define AUDIO_SETTING_SYNC_MS 200U
#define AUDIO_METRONOME_HOLD_MS 3000U

static const char gAudioSettingLanguagePath[] = "/setting/language";
static const char gAudioSettingVolumePath[] = "/setting/volume";
static const char gAudioSettingMetronomePath[] = "/setting/metronome";

static stAudioStatus gAudioStatus = {
    .state = AUDIO_STATE_UNINIT,
    .language = AUDIO_DEFAULT_LANGUAGE,
    .volumeLevel = AUDIO_DEFAULT_VOLUME_LEVEL,
    .metronomeFreq = AUDIO_DEFAULT_METRONOME_FREQ,
    .musicNum = 0U,
    .moduleReady = false,
    .commResponded = false,
    .musicNumValid = false,
};
static eAudioClip gAudioNoticeQueue[AUDIO_NOTICE_QUEUE_SIZE];
static eAudioClip gAudioDidiQueue[AUDIO_DIDI_QUEUE_SIZE];
static uint8_t gAudioNoticeHead;
static uint8_t gAudioNoticeTail;
static uint8_t gAudioNoticeUsed;
static uint8_t gAudioDidiHead;
static uint8_t gAudioDidiTail;
static uint8_t gAudioDidiUsed;
static uint32_t gAudioLastNoticeTick;
static uint32_t gAudioLastMetronomeTick;
static uint32_t gAudioBatteryDeadStartTick;
static uint32_t gAudioNextStateQueryTick;
static uint32_t gAudioNextPlayTick;
static uint32_t gAudioNextSettingSyncTick;
static eSystemMode gAudioLastSystemMode;
static bool gAudioSystemModeValid;
static uint8_t gAudioLastBatLevel;
static bool gAudioBatLevelValid;

static bool audioIsValidClip(eAudioClip clip);
static bool audioQueuePush(eAudioClip *queue, uint8_t size, uint8_t *head, uint8_t *used, eAudioClip clip);
static bool audioQueuePop(eAudioClip *queue, uint8_t size, uint8_t *tail, uint8_t *used, eAudioClip *clip);
static bool audioNoticePop(eAudioClip *clip);
static bool audioDidiPop(eAudioClip *clip);
static uint8_t audioMapVolume(uint8_t volumeLevel);
static const char *audioGetLanguageCode(eAudioLanguage language);
static uint8_t audioGetLanguagePrefix(eAudioLanguage language);
static eAudioLanguage audioNormalizeLanguage(uint8_t language);
static uint8_t audioNormalizeVolumeLevel(uint8_t volumeLevel);
static uint8_t audioNormalizeMetronomeFreq(uint8_t freq);
static bool audioParseSettingValue(const uint8_t *buffer, uint32_t size, uint8_t *value);
static bool audioLoadSettingValue(const char *path, uint8_t *value);
static void audioLoadSettings(void);
static uint32_t audioGetTickMs(void);
static bool audioSendAndWait(uint8_t cmd, eDrvStatus (*sendFunc)(void), uint32_t timeoutMs, bool logTimeout);
static bool audioInitQueryVersion(void);
static bool audioInitQueryMusicNum(void);
static void audioLogVersionFromInfo(void);
static void audioUpdateCommStatusFromInfo(void);
static void audioUpdateMusicNumFromInfo(void);
static eDrvStatus audioQueryVersion(void);
static eDrvStatus audioQueryMusicNum(void);
static eDrvStatus audioSetSingleMode(void);
static eDrvStatus audioSetDacMode(void);
static bool audioBuildFileName(eAudioClip clip, uint8_t *name);
static bool audioPlayClip(eAudioClip clip);
static void audioUpdateSettings(void);
static void audioServiceSystemMode(void);
static void audioServiceMetronome(void);
static void audioServiceCprNotice(void);
static void audioServiceLowBattery(void);
static void audioServicePlayback(void);
static void audioQueryStatePeriodically(void);

bool audioInit(void)
{
    uint8_t lVolume;
    uint32_t lStartTick;

    if (gAudioStatus.state == AUDIO_STATE_READY) {
        return true;
    }

    lStartTick = audioGetTickMs();
    if (wt2003hxPortInit() != DRV_STATUS_OK) {
        gAudioStatus.state = AUDIO_STATE_FAULT;
        LOG_E(AUDIO_LOG_TAG, "module init failed");
        return false;
    }

    gAudioStatus.moduleReady = true;
    gAudioStatus.commResponded = false;
    gAudioStatus.musicNum = 0U;
    gAudioStatus.musicNumValid = false;
    gAudioStatus.language = AUDIO_DEFAULT_LANGUAGE;
    gAudioStatus.volumeLevel = AUDIO_DEFAULT_VOLUME_LEVEL;
    gAudioStatus.metronomeFreq = AUDIO_DEFAULT_METRONOME_FREQ;

    (void)audioInitQueryVersion();
    (void)audioInitQueryMusicNum();
    (void)audioSetSingleMode();
    (void)audioSetDacMode();
    lVolume = audioMapVolume(gAudioStatus.volumeLevel);
    (void)wt2003hxPortSetVolume(lVolume);
    gAudioNextSettingSyncTick = audioGetTickMs();
    gAudioLastSystemMode = systemGetMode();
    gAudioSystemModeValid = true;
    gAudioBatLevelValid = false;
    gAudioLastMetronomeTick = 0U;

    gAudioStatus.state = AUDIO_STATE_READY;
    LOG_I(AUDIO_LOG_TAG,
          "audio ready lang=%s volume=%u metronome=%u elapsed=%lu",
          audioGetLanguageCode(gAudioStatus.language),
          (unsigned int)gAudioStatus.volumeLevel,
          (unsigned int)gAudioStatus.metronomeFreq,
          (unsigned long)(audioGetTickMs() - lStartTick));
    return true;
}

void audioProcess(void)
{
    if (gAudioStatus.state != AUDIO_STATE_READY) {
        (void)audioInit();
        return;
    }

    (void)wt2003hxPortProcess();
    audioUpdateCommStatusFromInfo();
    audioUpdateMusicNumFromInfo();
    audioUpdateSettings();
    audioServiceSystemMode();
    audioQueryStatePeriodically();
    audioServiceMetronome();
    audioServiceCprNotice();
    audioServiceLowBattery();
    audioServicePlayback();
}

bool audioEnqueueNotice(eAudioClip clip)
{
    if (!audioIsValidClip(clip) || (clip == AUDIO_CLIP_DIDI)) {
        return false;
    }

    return audioQueuePush(gAudioNoticeQueue, AUDIO_NOTICE_QUEUE_SIZE, &gAudioNoticeHead, &gAudioNoticeUsed, clip);
}

bool audioEnqueueDidi(void)
{
    return audioQueuePush(gAudioDidiQueue, AUDIO_DIDI_QUEUE_SIZE, &gAudioDidiHead, &gAudioDidiUsed, AUDIO_CLIP_DIDI);
}

const stAudioStatus *audioGetStatus(void)
{
    return &gAudioStatus;
}

static bool audioIsValidClip(eAudioClip clip)
{
    return ((uint32_t)clip < (uint32_t)AUDIO_CLIP_MAX);
}

static bool audioQueuePush(eAudioClip *queue, uint8_t size, uint8_t *head, uint8_t *used, eAudioClip clip)
{
    if ((queue == NULL) || (head == NULL) || (used == NULL) || (*used >= size)) {
        return false;
    }

    queue[*head] = clip;
    *head = (uint8_t)((*head + 1U) % size);
    *used = (uint8_t)(*used + 1U);
    return true;
}

static bool audioQueuePop(eAudioClip *queue, uint8_t size, uint8_t *tail, uint8_t *used, eAudioClip *clip)
{
    if ((queue == NULL) || (tail == NULL) || (used == NULL) || (clip == NULL) || (*used == 0U)) {
        return false;
    }

    *clip = queue[*tail];
    *tail = (uint8_t)((*tail + 1U) % size);
    *used = (uint8_t)(*used - 1U);
    return true;
}

static bool audioNoticePop(eAudioClip *clip)
{
    return audioQueuePop(gAudioNoticeQueue, AUDIO_NOTICE_QUEUE_SIZE, &gAudioNoticeTail, &gAudioNoticeUsed, clip);
}

static bool audioDidiPop(eAudioClip *clip)
{
    return audioQueuePop(gAudioDidiQueue, AUDIO_DIDI_QUEUE_SIZE, &gAudioDidiTail, &gAudioDidiUsed, clip);
}

static uint8_t audioMapVolume(uint8_t volumeLevel)
{
    switch (volumeLevel) {
        case 0U:
            return 0U;
        case 1U:
            return 20U;
        case 2U:
            return 26U;
        case 3U:
        default:
            return 31U;
    }
}

static const char *audioGetLanguageCode(eAudioLanguage language)
{
    static const char *lLanguageCode[] = {"ZH", "ZH", "EN", "DE", "FR", "IT"};

    if ((uint8_t)language >= (uint8_t)(sizeof(lLanguageCode) / sizeof(lLanguageCode[0]))) {
        return lLanguageCode[(uint8_t)AUDIO_DEFAULT_LANGUAGE];
    }

    return lLanguageCode[(uint8_t)language];
}

static uint8_t audioGetLanguagePrefix(eAudioLanguage language)
{
    static const uint8_t lLanguagePrefix[] = {'Z', 'Z', 'E', 'D', 'F', 'I'};

    if ((uint8_t)language >= (uint8_t)(sizeof(lLanguagePrefix) / sizeof(lLanguagePrefix[0]))) {
        return lLanguagePrefix[(uint8_t)AUDIO_DEFAULT_LANGUAGE];
    }

    return lLanguagePrefix[(uint8_t)language];
}

static eAudioLanguage audioNormalizeLanguage(uint8_t language)
{
    if ((language >= (uint8_t)AUDIO_LANGUAGE_ZH) && (language <= (uint8_t)AUDIO_LANGUAGE_IT)) {
        return (eAudioLanguage)language;
    }

    return AUDIO_DEFAULT_LANGUAGE;
}

static uint8_t audioNormalizeVolumeLevel(uint8_t volumeLevel)
{
    return (volumeLevel <= 3U) ? volumeLevel : AUDIO_DEFAULT_VOLUME_LEVEL;
}

static uint8_t audioNormalizeMetronomeFreq(uint8_t freq)
{
    return (freq == 0U) ? AUDIO_DEFAULT_METRONOME_FREQ : freq;
}

static bool audioParseSettingValue(const uint8_t *buffer, uint32_t size, uint8_t *value)
{
    uint32_t lIndex;
    uint32_t lParsedValue = 0U;
    bool lHasDigit = false;

    if ((buffer == NULL) || (value == NULL) || (size == 0U)) {
        return false;
    }

    if (size == 1U) {
        *value = buffer[0];
        return true;
    }

    for (lIndex = 0U; lIndex < size; lIndex++) {
        uint8_t lChar = buffer[lIndex];

        if ((lChar == ' ') || (lChar == '\r') || (lChar == '\n') || (lChar == '\t')) {
            continue;
        }

        if ((lChar < (uint8_t)'0') || (lChar > (uint8_t)'9')) {
            return false;
        }

        lHasDigit = true;
        lParsedValue = (lParsedValue * 10U) + (uint32_t)(lChar - (uint8_t)'0');
        if (lParsedValue > 255U) {
            return false;
        }
    }

    if (!lHasDigit) {
        return false;
    }

    *value = (uint8_t)lParsedValue;
    return true;
}

static bool audioLoadSettingValue(const char *path, uint8_t *value)
{
    uint8_t lBuffer[8];
    uint32_t lActualSize = 0U;
    uint8_t lRawValue;

    if ((path == NULL) || (value == NULL) || !memoryIsReady() || !memoryExists(path)) {
        return false;
    }

    if (!memoryReadFile(path, lBuffer, (uint32_t)sizeof(lBuffer), &lActualSize) ||
        !audioParseSettingValue(lBuffer, lActualSize, &lRawValue)) {
        return false;
    }

    *value = lRawValue;
    return true;
}

static void audioLoadSettings(void)
{
    uint8_t lValue;

    if (!memoryIsReady()) {
        return;
    }

    if (audioLoadSettingValue(gAudioSettingLanguagePath, &lValue)) {
        gAudioStatus.language = audioNormalizeLanguage(lValue);
    }

    if (audioLoadSettingValue(gAudioSettingVolumePath, &lValue)) {
        gAudioStatus.volumeLevel = audioNormalizeVolumeLevel(lValue);
    }

    if (audioLoadSettingValue(gAudioSettingMetronomePath, &lValue)) {
        gAudioStatus.metronomeFreq = audioNormalizeMetronomeFreq(lValue);
    }
}

static uint32_t audioGetTickMs(void)
{
    return repRtosGetTickMs();
}

static bool audioSendAndWait(uint8_t cmd, eDrvStatus (*sendFunc)(void), uint32_t timeoutMs, bool logTimeout)
{
    uint32_t lStartTick;
    stWt2003hxInfo lInfo;

    if (sendFunc == NULL) {
        return false;
    }

    lStartTick = audioGetTickMs();
    if (sendFunc() != DRV_STATUS_OK) {
        return false;
    }

    while ((uint32_t)(audioGetTickMs() - lStartTick) < timeoutMs) {
        (void)wt2003hxPortProcess();
        if (wt2003hxPortGetInfo(&lInfo) && (lInfo.lastReplyCmd == cmd) && ((uint32_t)(lInfo.lastReplyTick - lStartTick) < timeoutMs)) {
            return true;
        }
        (void)repRtosDelayMs(20U);
    }

    if (logTimeout) {
        LOG_W(AUDIO_LOG_TAG,
              "init query timeout cmd=0x%02X elapsed=%lu",
              (unsigned int)cmd,
              (unsigned long)(audioGetTickMs() - lStartTick));
    }
    return false;
}

static bool audioInitQueryMusicNum(void)
{
    if (audioSendAndWait(WT2003HX_CMD_CHECK_MUSIC_NUM, audioQueryMusicNum, AUDIO_INIT_QUERY_TIMEOUT_MS, false)) {
        audioUpdateMusicNumFromInfo();
        return true;
    }

    (void)repRtosDelayMs(AUDIO_INIT_QUERY_RETRY_DELAY_MS);
    if (audioSendAndWait(WT2003HX_CMD_CHECK_MUSIC_NUM, audioQueryMusicNum, AUDIO_INIT_QUERY_TIMEOUT_MS, false)) {
        audioUpdateMusicNumFromInfo();
        LOG_I(AUDIO_LOG_TAG,
              "init query cmd=0x%02X recovered after retry",
              (unsigned int)WT2003HX_CMD_CHECK_MUSIC_NUM);
        return true;
    }

    LOG_I(AUDIO_LOG_TAG,
          "music num query pending, continue init");
    return false;
}

static bool audioInitQueryVersion(void)
{
    if (audioSendAndWait(WT2003HX_CMD_CHECK_VERSION, audioQueryVersion, AUDIO_INIT_QUERY_TIMEOUT_MS, true)) {
        audioLogVersionFromInfo();
        return true;
    }

    return false;
}

static void audioLogVersionFromInfo(void)
{
    char lVersionText[WT2003HX_VERSION_MAX_LEN + 1U];
    stWt2003hxInfo lInfo;

    if (!wt2003hxPortGetInfo(&lInfo) ||
        (lInfo.lastReplyCmd != WT2003HX_CMD_CHECK_VERSION) ||
        (lInfo.versionLen == 0U)) {
        return;
    }

    (void)memset(lVersionText, 0, sizeof(lVersionText));
    (void)memcpy(lVersionText, lInfo.version, lInfo.versionLen);
    LOG_I(AUDIO_LOG_TAG,
          "version len=%u text=%s",
          (unsigned int)lInfo.versionLen,
          lVersionText);
}

static void audioUpdateCommStatusFromInfo(void)
{
    stWt2003hxInfo lInfo;

    if (!wt2003hxPortGetInfo(&lInfo)) {
        return;
    }

    if ((lInfo.lastReplyCmd != 0U) || (lInfo.lastReplyTick != 0U)) {
        gAudioStatus.commResponded = true;
    }
}

static void audioUpdateMusicNumFromInfo(void)
{
    stWt2003hxInfo lInfo;

    if (!wt2003hxPortGetInfo(&lInfo) || (lInfo.lastReplyCmd != WT2003HX_CMD_CHECK_MUSIC_NUM)) {
        return;
    }

    if ((!gAudioStatus.musicNumValid) || (gAudioStatus.musicNum != lInfo.musicNum)) {
        gAudioStatus.musicNum = lInfo.musicNum;
        gAudioStatus.musicNumValid = true;
        LOG_I(AUDIO_LOG_TAG, "music num=%u", (unsigned int)gAudioStatus.musicNum);
    }
}

static eDrvStatus audioQueryVersion(void)
{
    return wt2003hxPortQuery(WT2003HX_CMD_CHECK_VERSION);
}

static eDrvStatus audioQueryMusicNum(void)
{
    return wt2003hxPortQuery(WT2003HX_CMD_CHECK_MUSIC_NUM);
}

static eDrvStatus audioSetSingleMode(void)
{
    return wt2003hxPortSetPlayMode(WT2003HX_PLAY_MODE_SINGLE);
}

static eDrvStatus audioSetDacMode(void)
{
    return wt2003hxPortSetOutputMode(WT2003HX_OUTPUT_MODE_DAC);
}

static bool audioBuildFileName(eAudioClip clip, uint8_t *name)
{
    if ((name == NULL) || !audioIsValidClip(clip) || (clip > AUDIO_CLIP_PRESS_WELL)) {
        return false;
    }

    name[0] = audioGetLanguagePrefix(gAudioStatus.language);
    name[1] = (uint8_t)('0' + (uint8_t)clip);
    return true;
}

static bool audioPlayClip(eAudioClip clip)
{
    uint8_t lName[2];

    if (!audioBuildFileName(clip, lName)) {
        return false;
    }

    if (wt2003hxPortPlayName(lName, (uint8_t)sizeof(lName)) != DRV_STATUS_OK) {
        return false;
    }

    gAudioNextPlayTick = audioGetTickMs() + AUDIO_PLAY_COOLDOWN_MS;
    return true;
}

static void audioUpdateSettings(void)
{
    eAudioLanguage lLanguage = gAudioStatus.language;
    uint8_t lVolume = gAudioStatus.volumeLevel;
    uint32_t lNowTick = audioGetTickMs();

    if ((int32_t)(lNowTick - gAudioNextSettingSyncTick) < 0) {
        return;
    }

    gAudioNextSettingSyncTick = lNowTick + AUDIO_SETTING_SYNC_MS;

    audioLoadSettings();

    if (lLanguage != gAudioStatus.language) {
        (void)audioEnqueueNotice(AUDIO_CLIP_CHANGE_LANGUAGE);
    }

    if (lVolume != gAudioStatus.volumeLevel) {
        (void)wt2003hxPortSetVolume(audioMapVolume(gAudioStatus.volumeLevel));
    }
}

static void audioServiceSystemMode(void)
{
    eSystemMode lCurrentMode = systemGetMode();

    if (!gAudioSystemModeValid) {
        gAudioLastSystemMode = lCurrentMode;
        gAudioSystemModeValid = true;
        return;
    }

    if ((gAudioLastSystemMode == eSYSTEM_STANDBY_MODE) && (lCurrentMode == eSYSTEM_NORMAL_MODE)) {
        (void)audioEnqueueNotice(AUDIO_CLIP_START_CPR);
    }

    gAudioLastSystemMode = lCurrentMode;
}

static void audioServiceMetronome(void)
{
    CPR_Manager_Typedef lManager;
    uint32_t lPeriodMs;
    uint32_t lNowTick = audioGetTickMs();

    cprAlgMgrGetManager(&lManager);
    if (!lManager.IsPressing && (lManager.IntervalTime >= AUDIO_METRONOME_HOLD_MS)) {
        gAudioLastMetronomeTick = 0U;
        return;
    }

    lPeriodMs = 60000UL / (uint32_t)gAudioStatus.metronomeFreq;
    if (gAudioLastMetronomeTick == 0U) {
        gAudioLastMetronomeTick = lNowTick;
        return;
    }

    if ((uint32_t)(lNowTick - gAudioLastMetronomeTick) >= lPeriodMs) {
        do {
            gAudioLastMetronomeTick += lPeriodMs;
        } while ((uint32_t)(lNowTick - gAudioLastMetronomeTick) >= lPeriodMs);
        (void)audioEnqueueDidi();
    }
}

static void audioServiceCprNotice(void)
{
    CPR_Data_Typedef lData;
    CPR_Manager_Typedef lManager;
    uint32_t lNowTick = audioGetTickMs();

    if ((uint32_t)(lNowTick - gAudioLastNoticeTick) < AUDIO_NOTICE_WINDOW_MS) {
        return;
    }

    cprAlgMgrGetData(&lData);
    cprAlgMgrGetManager(&lManager);
    if (!lManager.DataReady) {
        return;
    }

    gAudioLastNoticeTick = lNowTick;
    if (lData.Depth > s_CPR_Alarm_Limit.Depth_High_Limit) {
        (void)audioEnqueueNotice(AUDIO_CLIP_PRESS_DEEP);
    } else if (lData.Depth < s_CPR_Alarm_Limit.Depth_Low_Limit) {
        (void)audioEnqueueNotice(AUDIO_CLIP_PRESS_SHALLOW);
    } else if (lData.Freq > s_CPR_Alarm_Limit.Freq_High_Limit) {
        (void)audioEnqueueNotice(AUDIO_CLIP_PRESS_FAST);
    } else if (lData.Freq < s_CPR_Alarm_Limit.Freq_Low_Limit) {
        (void)audioEnqueueNotice(AUDIO_CLIP_PRESS_SLOW);
    } else {
        (void)audioEnqueueNotice(AUDIO_CLIP_PRESS_WELL);
    }
}

static void audioServiceLowBattery(void)
{
    const PowerManager *lPower = powerGetManager();
    uint32_t lNowTick = audioGetTickMs();
    bool lIsLowBattery;

    if (lPower == NULL) {
        return;
    }

    if (!gAudioBatLevelValid) {
        gAudioLastBatLevel = lPower->BatLevel;
        gAudioBatLevelValid = true;
    }

    lIsLowBattery = (lPower->BatLevel <= POWER_BATTERY_LOW_LEVEL_MAX);
    if ((gAudioLastBatLevel > POWER_BATTERY_LOW_LEVEL_MAX) && lIsLowBattery) {
        (void)audioEnqueueNotice(AUDIO_CLIP_LOW_BATTERY);
    }

    if ((gAudioLastBatLevel == 1U) && (lPower->BatLevel == 0U)) {
        (void)audioEnqueueNotice(AUDIO_CLIP_BATTERY_DEAD);
    }

    if ((lPower->BatLevel == 0U) && (lPower->voltage.dcMv <= POWER_VOLTAGE_TO_10MV(100U))) {
        if (gAudioBatteryDeadStartTick == 0U) {
            gAudioBatteryDeadStartTick = lNowTick;
        } else if ((uint32_t)(lNowTick - gAudioBatteryDeadStartTick) >= AUDIO_SHUTDOWN_DELAY_MS) {
            (void)powerRequestShutDown();
        }
    } else {
        gAudioBatteryDeadStartTick = 0U;
    }

    gAudioLastBatLevel = lPower->BatLevel;
}

static void audioServicePlayback(void)
{
    stWt2003hxInfo lInfo;
    eAudioClip lClip;
    uint32_t lNowTick = audioGetTickMs();

    if ((int32_t)(lNowTick - gAudioNextPlayTick) < 0) {
        return;
    }

    if (!wt2003hxPortGetInfo(&lInfo) || (lInfo.playState == WT2003HX_PLAY_STATE_PLAY)) {
        return;
    }

    if (audioNoticePop(&lClip)) {
        (void)audioPlayClip(lClip);
        (void)audioDidiPop(&lClip);
        return;
    }

    if (audioDidiPop(&lClip)) {
        (void)audioPlayClip(lClip);
    }
}

static void audioQueryStatePeriodically(void)
{
    uint32_t lNowTick = audioGetTickMs();

    if ((int32_t)(lNowTick - gAudioNextStateQueryTick) < 0) {
        return;
    }

    gAudioNextStateQueryTick = lNowTick + AUDIO_STATE_QUERY_MS;
    if (!gAudioStatus.musicNumValid) {
        (void)audioQueryMusicNum();
        return;
    }

    (void)wt2003hxPortQuery(WT2003HX_CMD_CHECK_STATE);
}

/**************************End of file********************************/
