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

#include "main.h"
#include "../memory/memory.h"
#include "../../port/wt2003hx_port.h"
#include "../cpralg/cpralgmgr.h"
#include "../power/power.h"
#include "../../system/system.h"
#include "../../../rep/sys/log/log.h"
#include "../../../rep/sys/rtos/rtos.h"

#define AUDIO_LOG_TAG "audio"
#define AUDIO_INIT_QUERY_TIMEOUT_MS 1000U
#define AUDIO_INIT_QUERY_RETRY_DELAY_MS 120U
#define AUDIO_METRONOME_HOLD_MS 3000U
#define AUDIO_DIDI_PLAY_MS 250U

static const uint8_t gAudioCprNoticeHistoryMajority = 2U;

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
static uint32_t gAudioPlayingStartTick;
static bool gAudioPlaybackActive;
static eAudioClip gAudioPlayingClip = AUDIO_CLIP_MAX;
static bool gAudioNoticeStopPending;
static eWt2003hxPlayState gAudioDebugLastPlayState = WT2003HX_PLAY_STATE_UNKNOWN;
static uint8_t gAudioDebugLastReplyCmd;
static bool gAudioDebugNoticeWaitLogged;
static bool gAudioDebugStopWaitLogged;
static bool gAudioWaitMemoryLogged;
static bool gAudioWaitVolumeLogged;
static eSystemMode gAudioLastSystemMode;
static bool gAudioSystemModeValid;
static uint8_t gAudioLastBatLevel;
static bool gAudioBatLevelValid;

static bool audioIsValidClip(eAudioClip clip);
static bool audioQueuePush(eAudioClip *queue, uint8_t size, uint8_t *head, uint8_t *used, eAudioClip clip);
static bool audioQueuePeek(eAudioClip *queue, uint8_t size, uint8_t tail, uint8_t used, eAudioClip *clip);
static bool audioQueuePop(eAudioClip *queue, uint8_t size, uint8_t *tail, uint8_t *used, eAudioClip *clip);
static bool audioNoticePeek(eAudioClip *clip);
static bool audioNoticePop(eAudioClip *clip);
static bool audioDidiPeek(eAudioClip *clip);
static bool audioDidiPop(eAudioClip *clip);
static void audioDidiClear(void);
static uint8_t audioMapVolume(uint8_t volumeLevel);
static const char *audioGetLanguageCode(eAudioLanguage language);
static uint8_t audioGetLanguagePrefix(eAudioLanguage language);
static eAudioLanguage audioNormalizeLanguage(uint8_t language);
static uint8_t audioNormalizeVolumeLevel(uint8_t volumeLevel);
static uint8_t audioNormalizeMetronomeFreq(uint8_t freq);
static void audioApplyLanguage(eAudioLanguage language, bool notifyChange);
static void audioApplyVolumeLevel(uint8_t volumeLevel);
static void audioApplyMetronomeFreq(uint8_t freq);
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
static bool audioHasPendingPlayback(void);
static bool audioIsDidiPlayWindowActive(uint32_t nowTick);
static bool audioIsNoticePlayWindowActive(uint32_t nowTick);
static bool audioIsNonDidiPlaybackBusy(void);
static bool audioShouldWaitNoticePlayback(const stWt2003hxInfo *info, uint32_t nowTick);
static bool audioCanPlayDidiNow(void);
static bool audioPlayClip(eAudioClip clip);
static int8_t audioClassifyDepthNotice(uint8_t depth);
static int8_t audioClassifyFreqNotice(uint8_t freq);
static uint8_t audioCountCprNoticeValue(const int8_t *history, uint8_t count, int8_t value);
static void audioResolveCprNotice(const CPR_Recent_Press_History_Typedef *history,
                                  const CPR_Data_Typedef *data,
                                  eAudioClip *depthClip,
                                  eAudioClip *freqClip);
static void audioServiceSystemMode(void);
static void audioServiceMetronome(void);
static void audioServiceCprNotice(void);
static void audioServiceLowBattery(void);
static void audioServicePlayback(void);
static void audioQueryStatePeriodically(void);
static void audioDebugToggleDidiPin(void);

bool audioInit(void)
{
    uint8_t lVolume;
    uint32_t lStartTick;

    if (gAudioStatus.state == AUDIO_STATE_READY) {
        return true;
    }

    if (!memoryIsReady()) {
        if (!gAudioWaitMemoryLogged) {
            LOG_I(AUDIO_LOG_TAG, "wait memory ready before audio init");
            gAudioWaitMemoryLogged = true;
        }
        return false;
    }

    if (!memoryExists(gAudioSettingVolumePath)) {
        if (!gAudioWaitVolumeLogged) {
            LOG_W(AUDIO_LOG_TAG,
                  "missing %s, use default audio settings",
                  gAudioSettingVolumePath);
            gAudioWaitVolumeLogged = true;
        }
    }

    gAudioStatus.language = AUDIO_DEFAULT_LANGUAGE;
    gAudioStatus.volumeLevel = AUDIO_DEFAULT_VOLUME_LEVEL;
    gAudioStatus.metronomeFreq = AUDIO_DEFAULT_METRONOME_FREQ;
    audioLoadSettings();
    gAudioWaitMemoryLogged = false;
    gAudioWaitVolumeLogged = false;

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

    (void)audioInitQueryVersion();
    (void)audioInitQueryMusicNum();
    (void)audioSetSingleMode();
    (void)audioSetDacMode();
    lVolume = audioMapVolume(gAudioStatus.volumeLevel);
    (void)wt2003hxPortSetVolume(lVolume);
    gAudioLastSystemMode = systemGetMode();
    gAudioSystemModeValid = true;
    gAudioBatLevelValid = false;
    gAudioLastMetronomeTick = 0U;
    gAudioPlayingStartTick = 0U;
    gAudioPlaybackActive = false;
    gAudioPlayingClip = AUDIO_CLIP_MAX;
    gAudioNoticeStopPending = false;
    gAudioDebugLastPlayState = WT2003HX_PLAY_STATE_UNKNOWN;
    gAudioDebugLastReplyCmd = 0U;
    gAudioDebugNoticeWaitLogged = false;
    gAudioDebugStopWaitLogged = false;
    gAudioLastNoticeTick = 0U;

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

    audioDidiClear();
    return audioQueuePush(gAudioNoticeQueue, AUDIO_NOTICE_QUEUE_SIZE, &gAudioNoticeHead, &gAudioNoticeUsed, clip);
}

bool audioEnqueueDidi(void)
{
    if (!audioCanPlayDidiNow()) {
        return false;
    }

    if (gAudioDidiUsed > 0U) {
        return true;
    }

    return audioQueuePush(gAudioDidiQueue, AUDIO_DIDI_QUEUE_SIZE, &gAudioDidiHead, &gAudioDidiUsed, AUDIO_CLIP_DIDI);
}

const stAudioStatus *audioGetStatus(void)
{
    return &gAudioStatus;
}

void audioApplyLanguageSetting(uint8_t language, bool notifyChange)
{
    audioApplyLanguage((eAudioLanguage)language, notifyChange);
}

void audioApplyVolumeSetting(uint8_t volumeLevel)
{
    audioApplyVolumeLevel(volumeLevel);
}

void audioApplyMetronomeSetting(uint8_t metronomeFreq)
{
    audioApplyMetronomeFreq(metronomeFreq);
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

static bool audioQueuePeek(eAudioClip *queue, uint8_t size, uint8_t tail, uint8_t used, eAudioClip *clip)
{
    if ((queue == NULL) || (clip == NULL) || (used == 0U) || (tail >= size)) {
        return false;
    }

    *clip = queue[tail];
    return true;
}

static bool audioNoticePeek(eAudioClip *clip)
{
    return audioQueuePeek(gAudioNoticeQueue, AUDIO_NOTICE_QUEUE_SIZE, gAudioNoticeTail, gAudioNoticeUsed, clip);
}

static bool audioNoticePop(eAudioClip *clip)
{
    return audioQueuePop(gAudioNoticeQueue, AUDIO_NOTICE_QUEUE_SIZE, &gAudioNoticeTail, &gAudioNoticeUsed, clip);
}

static bool audioDidiPeek(eAudioClip *clip)
{
    return audioQueuePeek(gAudioDidiQueue, AUDIO_DIDI_QUEUE_SIZE, gAudioDidiTail, gAudioDidiUsed, clip);
}

static bool audioDidiPop(eAudioClip *clip)
{
    return audioQueuePop(gAudioDidiQueue, AUDIO_DIDI_QUEUE_SIZE, &gAudioDidiTail, &gAudioDidiUsed, clip);
}

static void audioDidiClear(void)
{
    gAudioDidiHead = 0U;
    gAudioDidiTail = 0U;
    gAudioDidiUsed = 0U;
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

static void audioApplyLanguage(eAudioLanguage language, bool notifyChange)
{
    eAudioLanguage lNormalizedLanguage = audioNormalizeLanguage((uint8_t)language);

    if (gAudioStatus.language == lNormalizedLanguage) {
        if (notifyChange && (gAudioStatus.state == AUDIO_STATE_READY)) {
            (void)audioEnqueueNotice(AUDIO_CLIP_CHANGE_LANGUAGE);
        }
        return;
    }

    gAudioStatus.language = lNormalizedLanguage;
    if (notifyChange && (gAudioStatus.state == AUDIO_STATE_READY)) {
        (void)audioEnqueueNotice(AUDIO_CLIP_CHANGE_LANGUAGE);
    }
}

static void audioApplyVolumeLevel(uint8_t volumeLevel)
{
    uint8_t lNormalizedVolumeLevel = audioNormalizeVolumeLevel(volumeLevel);

    if (gAudioStatus.volumeLevel == lNormalizedVolumeLevel) {
        return;
    }

    gAudioStatus.volumeLevel = lNormalizedVolumeLevel;
    if (gAudioStatus.moduleReady) {
        (void)wt2003hxPortSetVolume(audioMapVolume(gAudioStatus.volumeLevel));
    }
}

static void audioApplyMetronomeFreq(uint8_t freq)
{
    uint8_t lNormalizedMetronomeFreq = audioNormalizeMetronomeFreq(freq);

    if (gAudioStatus.metronomeFreq == lNormalizedMetronomeFreq) {
        return;
    }

    gAudioStatus.metronomeFreq = lNormalizedMetronomeFreq;
    gAudioLastMetronomeTick = 0U;
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

    if (audioLoadSettingValue(gAudioSettingLanguagePath, &lValue)) {
        audioApplyLanguage((eAudioLanguage)lValue, false);
    }

    if (audioLoadSettingValue(gAudioSettingVolumePath, &lValue)) {
        audioApplyVolumeLevel(lValue);
    }

    if (audioLoadSettingValue(gAudioSettingMetronomePath, &lValue)) {
        audioApplyMetronomeFreq(lValue);
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

static bool audioHasPendingPlayback(void)
{
    return gAudioPlaybackActive || (gAudioNoticeUsed > 0U) || (gAudioDidiUsed > 0U);
}

static bool audioIsDidiPlayWindowActive(uint32_t nowTick)
{
    return (gAudioPlayingClip == AUDIO_CLIP_DIDI) &&
           ((uint32_t)(nowTick - gAudioPlayingStartTick) < AUDIO_DIDI_PLAY_MS);
}

static bool audioIsNoticePlayWindowActive(uint32_t nowTick)
{
    return (gAudioPlayingClip != AUDIO_CLIP_DIDI) &&
           (gAudioPlayingClip != AUDIO_CLIP_MAX) &&
           ((uint32_t)(nowTick - gAudioPlayingStartTick) < AUDIO_NOTICE_PLAY_HOLD_MS);
}

static bool audioIsNonDidiPlaybackBusy(void)
{
    stWt2003hxInfo lInfo;
    uint32_t lNowTick = audioGetTickMs();

    if (!wt2003hxPortGetInfo(&lInfo)) {
        return false;
    }

    if (audioIsDidiPlayWindowActive(lNowTick)) {
        return false;
    }

    if (!gAudioPlaybackActive && (lInfo.playState != WT2003HX_PLAY_STATE_PLAY)) {
        return false;
    }

    return (gAudioPlayingClip != AUDIO_CLIP_DIDI);
}

static bool audioShouldWaitNoticePlayback(const stWt2003hxInfo *info, uint32_t nowTick)
{
    if (info == NULL) {
        return false;
    }

    if ((gAudioPlayingClip == AUDIO_CLIP_MAX) || (gAudioPlayingClip == AUDIO_CLIP_DIDI)) {
        return false;
    }

    if (audioIsNoticePlayWindowActive(nowTick)) {
        if (!gAudioDebugNoticeWaitLogged) {
            LOG_I(AUDIO_LOG_TAG,
                  "notice hold clip=%u active=%u state=%u cmd=0x%02X",
                  (unsigned int)gAudioPlayingClip,
                  (unsigned int)gAudioPlaybackActive,
                  (unsigned int)info->playState,
                  (unsigned int)info->lastReplyCmd);
            gAudioDebugNoticeWaitLogged = true;
        }
        return true;
    }

    if ((info->lastReplyCmd != WT2003HX_CMD_CHECK_STATE) ||
        ((gAudioPlayingStartTick != 0U) && ((uint32_t)(info->lastReplyTick - gAudioPlayingStartTick) >= 0x80000000UL))) {
        if (!gAudioDebugNoticeWaitLogged) {
            LOG_I(AUDIO_LOG_TAG,
                  "notice query state clip=%u state=%u cmd=0x%02X tick=%lu start=%lu",
                  (unsigned int)gAudioPlayingClip,
                  (unsigned int)info->playState,
                  (unsigned int)info->lastReplyCmd,
                  (unsigned long)info->lastReplyTick,
                  (unsigned long)gAudioPlayingStartTick);
            gAudioDebugNoticeWaitLogged = true;
        }
        (void)wt2003hxPortQuery(WT2003HX_CMD_CHECK_STATE);
        gAudioNextPlayTick = nowTick + AUDIO_STATE_QUERY_ACTIVE_MS;
        return true;
    }

    if (info->playState != WT2003HX_PLAY_STATE_STOP) {
        if (!gAudioDebugNoticeWaitLogged) {
            LOG_I(AUDIO_LOG_TAG,
                  "notice wait stop clip=%u state=%u cmd=0x%02X",
                  (unsigned int)gAudioPlayingClip,
                  (unsigned int)info->playState,
                  (unsigned int)info->lastReplyCmd);
            gAudioDebugNoticeWaitLogged = true;
        }
        return true;
    }

    gAudioDebugNoticeWaitLogged = false;
    return (info->playState != WT2003HX_PLAY_STATE_STOP);
}

static bool audioCanPlayDidiNow(void)
{
    return (gAudioNoticeUsed == 0U) && !audioIsNonDidiPlaybackBusy();
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
    uint32_t lNowTick;
    stWt2003hxInfo lInfo;

    if (!audioBuildFileName(clip, lName)) {
        LOG_W(AUDIO_LOG_TAG, "invalid clip=%u for play", (unsigned int)clip);
        return false;
    }

    if (wt2003hxPortPlayName(lName, (uint8_t)sizeof(lName)) != DRV_STATUS_OK) {
        if (wt2003hxPortGetInfo(&lInfo)) {
            LOG_W(AUDIO_LOG_TAG,
                  "play failed clip=%u state=%u cmd=0x%02X pendingStop=%u active=%u notice=%u didi=%u",
                  (unsigned int)clip,
                  (unsigned int)lInfo.playState,
                  (unsigned int)lInfo.lastReplyCmd,
                  (unsigned int)gAudioNoticeStopPending,
                  (unsigned int)gAudioPlaybackActive,
                  (unsigned int)gAudioNoticeUsed,
                  (unsigned int)gAudioDidiUsed);
        } else {
            LOG_W(AUDIO_LOG_TAG,
                  "play failed clip=%u pendingStop=%u active=%u notice=%u didi=%u",
                  (unsigned int)clip,
                  (unsigned int)gAudioNoticeStopPending,
                  (unsigned int)gAudioPlaybackActive,
                  (unsigned int)gAudioNoticeUsed,
                  (unsigned int)gAudioDidiUsed);
        }
        return false;
    }

    lNowTick = audioGetTickMs();
    gAudioPlaybackActive = true;
    gAudioPlayingClip = clip;
    gAudioDebugNoticeWaitLogged = false;
    gAudioDebugStopWaitLogged = false;
    gAudioPlayingStartTick = lNowTick;
    gAudioNextStateQueryTick = (clip == AUDIO_CLIP_DIDI) ? (lNowTick + AUDIO_DIDI_PLAY_MS) : lNowTick;
    gAudioNextPlayTick = lNowTick + AUDIO_PLAY_COOLDOWN_MS;
    LOG_I(AUDIO_LOG_TAG,
          "play clip=%u name=%c%c notice=%u didi=%u",
          (unsigned int)clip,
          (char)lName[0],
          (char)lName[1],
          (unsigned int)gAudioNoticeUsed,
          (unsigned int)gAudioDidiUsed);
    return true;
}

static int8_t audioClassifyDepthNotice(uint8_t depth)
{
    if (depth > s_CPR_Alarm_Limit.Depth_High_Limit) {
        return 1;
    }

    if (depth < s_CPR_Alarm_Limit.Depth_Low_Limit) {
        return -1;
    }

    return 0;
}

static int8_t audioClassifyFreqNotice(uint8_t freq)
{
    if (freq > s_CPR_Alarm_Limit.Freq_High_Limit) {
        return 1;
    }

    if (freq < s_CPR_Alarm_Limit.Freq_Low_Limit) {
        return -1;
    }

    return 0;
}

static uint8_t audioCountCprNoticeValue(const int8_t *history, uint8_t count, int8_t value)
{
    uint8_t lIndex;
    uint8_t lMatchCount = 0U;

    if (history == NULL) {
        return 0U;
    }

    for (lIndex = 0U; lIndex < count; lIndex++) {
        if (history[lIndex] == value) {
            lMatchCount++;
        }
    }

    return lMatchCount;
}

static void audioResolveCprNotice(const CPR_Recent_Press_History_Typedef *history,
                                  const CPR_Data_Typedef *data,
                                  eAudioClip *depthClip,
                                  eAudioClip *freqClip)
{
    int8_t lDepthHistory[CPR_ALG_MGR_RECENT_PRESS_COUNT];
    int8_t lFreqHistory[CPR_ALG_MGR_RECENT_PRESS_COUNT];
    uint8_t lIndex;
    uint8_t lValidCount = 0U;
    int8_t lDepthState;
    int8_t lFreqState;

    if ((history == NULL) || (data == NULL) || (depthClip == NULL) || (freqClip == NULL)) {
        return;
    }

    *depthClip = AUDIO_CLIP_MAX;
    *freqClip = AUDIO_CLIP_MAX;

    for (lIndex = 0U; lIndex < history->Count; lIndex++) {
        if (!history->Records[lIndex].Valid) {
            continue;
        }

        lDepthHistory[lValidCount] = audioClassifyDepthNotice(history->Records[lIndex].Depth);
        lFreqHistory[lValidCount] = audioClassifyFreqNotice(history->Records[lIndex].Freq);
        lValidCount++;
    }

    if (lValidCount == 0U) {
        return;
    }

    lDepthState = audioClassifyDepthNotice(data->Depth);
    lFreqState = audioClassifyFreqNotice(data->Freq);

    if ((lValidCount >= gAudioCprNoticeHistoryMajority) &&
        (audioCountCprNoticeValue(lDepthHistory,
                                  lValidCount,
                                  1) >= gAudioCprNoticeHistoryMajority)) {
        *depthClip = AUDIO_CLIP_PRESS_DEEP;
    } else if ((lValidCount >= gAudioCprNoticeHistoryMajority) &&
               (audioCountCprNoticeValue(lDepthHistory,
                                         lValidCount,
                                         -1) >= gAudioCprNoticeHistoryMajority)) {
        *depthClip = AUDIO_CLIP_PRESS_SHALLOW;
    }

    if ((lValidCount >= gAudioCprNoticeHistoryMajority) &&
        (audioCountCprNoticeValue(lFreqHistory,
                                  lValidCount,
                                  1) >= gAudioCprNoticeHistoryMajority)) {
        *freqClip = AUDIO_CLIP_PRESS_FAST;
    } else if ((lValidCount >= gAudioCprNoticeHistoryMajority) &&
               (audioCountCprNoticeValue(lFreqHistory,
                                         lValidCount,
                                         -1) >= gAudioCprNoticeHistoryMajority)) {
        *freqClip = AUDIO_CLIP_PRESS_SLOW;
    }

    if ((*depthClip == AUDIO_CLIP_MAX) && (*freqClip == AUDIO_CLIP_MAX) &&
        (lDepthState == 0) && (lFreqState == 0)) {
        *depthClip = AUDIO_CLIP_PRESS_WELL;
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

static void audioDebugToggleDidiPin(void)
{
    HAL_GPIO_TogglePin(DEBUG_DIDI_GPIO_Port, DEBUG_DIDI_Pin);
}

static void audioServiceMetronome(void)
{
    CPR_Data_Typedef lData;
    CPR_Manager_Typedef lManager;
    uint32_t lPeriodMs;
    uint32_t lNowTick = audioGetTickMs();

    if (systemGetMode() != eSYSTEM_NORMAL_MODE) {
        gAudioLastMetronomeTick = 0U;
        audioDidiClear();
        return;
    }

    cprAlgMgrGetManager(&lManager);
    cprAlgMgrGetData(&lData);
    if (!lManager.IsPressing && (lData.TimeStamp == 0U)) {
        gAudioLastMetronomeTick = 0U;
        audioDidiClear();
        return;
    }

    if (!lManager.IsPressing && (lManager.IntervalTime >= AUDIO_METRONOME_HOLD_MS)) {
        gAudioLastMetronomeTick = 0U;
        audioDidiClear();
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

        audioDebugToggleDidiPin();
        if (!audioEnqueueDidi()) {
            audioDidiClear();
        }
    }
}

static void audioServiceCprNotice(void)
{
    CPR_Data_Typedef lData;
    CPR_Manager_Typedef lManager;
    CPR_Recent_Press_History_Typedef lHistory;
    eAudioClip lDepthClip;
    eAudioClip lFreqClip;
    uint32_t lNowTick = audioGetTickMs();

    cprAlgMgrGetData(&lData);
    cprAlgMgrGetManager(&lManager);
    cprAlgMgrGetRecentPressHistory(&lHistory);

    if ((lData.TimeStamp == 0U) || (!lManager.IsPressing && (lManager.IntervalTime >= AUDIO_METRONOME_HOLD_MS))) {
        gAudioLastNoticeTick = lNowTick;
        return;
    }

    if ((uint32_t)(lNowTick - gAudioLastNoticeTick) < AUDIO_NOTICE_WINDOW_MS) {
        return;
    }

    gAudioLastNoticeTick = lNowTick;

    audioResolveCprNotice(&lHistory, &lData, &lDepthClip, &lFreqClip);
    if (lDepthClip != AUDIO_CLIP_MAX) {
        (void)audioEnqueueNotice(lDepthClip);
    }
    if (lFreqClip != AUDIO_CLIP_MAX) {
        (void)audioEnqueueNotice(lFreqClip);
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
    bool lModuleBusy;
    bool lNonDidiBusy;

    if ((int32_t)(lNowTick - gAudioNextPlayTick) < 0) {
        return;
    }

    if (!wt2003hxPortGetInfo(&lInfo)) {
        return;
    }

    if ((gAudioPlayingClip == AUDIO_CLIP_DIDI) && !audioIsDidiPlayWindowActive(lNowTick)) {
        gAudioPlaybackActive = false;
    }

    lModuleBusy = gAudioPlaybackActive ||
                  ((lInfo.playState == WT2003HX_PLAY_STATE_PLAY) &&
                   ((gAudioPlayingClip != AUDIO_CLIP_DIDI) || audioIsDidiPlayWindowActive(lNowTick)));
    lNonDidiBusy = lModuleBusy && (gAudioPlayingClip != AUDIO_CLIP_DIDI);

    if (audioNoticePeek(&lClip)) {
        if ((gAudioPlayingClip == AUDIO_CLIP_DIDI) || (lInfo.playState == WT2003HX_PLAY_STATE_PLAY)) {
            if ((lInfo.lastReplyCmd != WT2003HX_CMD_CHECK_STATE) ||
                (lInfo.playState == WT2003HX_PLAY_STATE_PLAY) ||
                (lInfo.playState == WT2003HX_PLAY_STATE_UNKNOWN)) {
                if (!gAudioDebugStopWaitLogged) {
                    LOG_I(AUDIO_LOG_TAG,
                          "notice wait current audio clip=%u current=%u state=%u cmd=0x%02X",
                          (unsigned int)lClip,
                          (unsigned int)gAudioPlayingClip,
                          (unsigned int)lInfo.playState,
                          (unsigned int)lInfo.lastReplyCmd);
                    gAudioDebugStopWaitLogged = true;
                }
                (void)wt2003hxPortQuery(WT2003HX_CMD_CHECK_STATE);
                gAudioNextPlayTick = lNowTick + AUDIO_STATE_QUERY_ACTIVE_MS;
                return;
            }

            gAudioDebugStopWaitLogged = false;
            if (gAudioPlayingClip == AUDIO_CLIP_DIDI) {
                LOG_I(AUDIO_LOG_TAG,
                      "notice after didi clip=%u state=%u cmd=0x%02X gap=%u",
                      (unsigned int)lClip,
                      (unsigned int)lInfo.playState,
                      (unsigned int)lInfo.lastReplyCmd,
                      (unsigned int)AUDIO_NOTICE_CHAIN_GAP_MS);
                gAudioPlayingClip = AUDIO_CLIP_MAX;
                gAudioNextPlayTick = lNowTick + AUDIO_NOTICE_CHAIN_GAP_MS;
                return;
            }
        }

        if (lModuleBusy || audioShouldWaitNoticePlayback(&lInfo, lNowTick)) {
            return;
        }

        if (!audioPlayClip(lClip)) {
            gAudioNextPlayTick = lNowTick + AUDIO_STATE_QUERY_ACTIVE_MS;
            return;
        }

        (void)audioNoticePop(&lClip);
        audioDidiClear();
        gAudioLastMetronomeTick = lNowTick;
        return;
    }

    gAudioNoticeStopPending = false;

    if (audioDidiPeek(&lClip)) {
        if (lNonDidiBusy) {
            audioDidiClear();
            return;
        }

        if (!audioPlayClip(lClip)) {
            audioDidiClear();
            gAudioNextPlayTick = lNowTick + AUDIO_STATE_QUERY_ACTIVE_MS;
            return;
        }

        (void)audioDidiPop(&lClip);
    }
}

static void audioQueryStatePeriodically(void)
{
    stWt2003hxInfo lInfo;
    uint32_t lQueryIntervalMs = audioHasPendingPlayback() ? AUDIO_STATE_QUERY_ACTIVE_MS : AUDIO_STATE_QUERY_IDLE_MS;
    uint32_t lNowTick = audioGetTickMs();

    if (gAudioPlaybackActive && (gAudioPlayingClip == AUDIO_CLIP_DIDI)) {
        if (audioIsDidiPlayWindowActive(lNowTick)) {
            return;
        }

        gAudioPlaybackActive = false;
        lQueryIntervalMs = audioHasPendingPlayback() ? AUDIO_STATE_QUERY_ACTIVE_MS : AUDIO_STATE_QUERY_IDLE_MS;
    }

    if (gAudioPlaybackActive && audioIsNoticePlayWindowActive(lNowTick)) {
        return;
    }

    if (wt2003hxPortGetInfo(&lInfo) &&
        gAudioPlaybackActive &&
        (lInfo.lastReplyCmd == WT2003HX_CMD_CHECK_STATE) &&
        (lInfo.playState != WT2003HX_PLAY_STATE_PLAY) &&
        (lInfo.playState != WT2003HX_PLAY_STATE_UNKNOWN)) {
        bool lHasPendingNotice = (gAudioNoticeUsed > 0U);

        gAudioPlaybackActive = false;
        LOG_I(AUDIO_LOG_TAG,
              "playback released clip=%u state=%u cmd=0x%02X",
              (unsigned int)gAudioPlayingClip,
              (unsigned int)lInfo.playState,
              (unsigned int)lInfo.lastReplyCmd);
        if (gAudioPlayingClip != AUDIO_CLIP_DIDI) {
            gAudioPlayingClip = AUDIO_CLIP_MAX;
            if (lHasPendingNotice && ((int32_t)(gAudioNextPlayTick - (lNowTick + AUDIO_NOTICE_CHAIN_GAP_MS)) < 0)) {
                gAudioNextPlayTick = lNowTick + AUDIO_NOTICE_CHAIN_GAP_MS;
                LOG_I(AUDIO_LOG_TAG,
                      "notice chain gap=%u pending=%u",
                      (unsigned int)AUDIO_NOTICE_CHAIN_GAP_MS,
                      (unsigned int)gAudioNoticeUsed);
            }
        }
        /*
         * Keep the last commanded DIDI clip only. WT2003HX state replies can
         * briefly report STOP before the physical beat is fully idle; clearing
         * the clip here makes that residual DIDI playback look like an unknown
         * non-DIDI busy state and drops every other 120 BPM beat.
         */
        lQueryIntervalMs = audioHasPendingPlayback() ? AUDIO_STATE_QUERY_ACTIVE_MS : AUDIO_STATE_QUERY_IDLE_MS;
    }

    if ((int32_t)(lNowTick - gAudioNextStateQueryTick) < 0) {
        return;
    }

    if (wt2003hxPortGetInfo(&lInfo) &&
        ((lInfo.playState != gAudioDebugLastPlayState) || (lInfo.lastReplyCmd != gAudioDebugLastReplyCmd))) {
        LOG_I(AUDIO_LOG_TAG,
              "state update clip=%u active=%u pendingStop=%u state=%u cmd=0x%02X notice=%u didi=%u",
              (unsigned int)gAudioPlayingClip,
              (unsigned int)gAudioPlaybackActive,
              (unsigned int)gAudioNoticeStopPending,
              (unsigned int)lInfo.playState,
              (unsigned int)lInfo.lastReplyCmd,
              (unsigned int)gAudioNoticeUsed,
              (unsigned int)gAudioDidiUsed);
        gAudioDebugLastPlayState = lInfo.playState;
        gAudioDebugLastReplyCmd = lInfo.lastReplyCmd;
    }

    gAudioNextStateQueryTick = lNowTick + lQueryIntervalMs;
    if (!gAudioStatus.musicNumValid) {
        (void)audioQueryMusicNum();
        return;
    }

    (void)wt2003hxPortQuery(WT2003HX_CMD_CHECK_STATE);
}

/**************************End of file********************************/
