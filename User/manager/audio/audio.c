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

#include "../../port/wt2003hx_port.h"
#include "../cpralg/cpralgmgr.h"
#include "../iotmanager/protcolmgr.h"
#include "../power/power.h"
#include "../../../rep/service/log/log.h"
#include "../../../rep/service/rtos/rtos.h"

#define AUDIO_LOG_TAG "audio"
#define AUDIO_INIT_QUERY_TIMEOUT_MS 250U

static stAudioStatus gAudioStatus = {
    .state = AUDIO_STATE_UNINIT,
    .language = AUDIO_DEFAULT_LANGUAGE,
    .volumeLevel = AUDIO_DEFAULT_VOLUME_LEVEL,
    .metronomeFreq = AUDIO_DEFAULT_METRONOME_FREQ,
    .moduleReady = false,
};
static eAudioClip gAudioNoticeQueue[AUDIO_NOTICE_QUEUE_SIZE];
static eAudioClip gAudioDidiQueue[AUDIO_DIDI_QUEUE_SIZE];
static uint8_t gAudioNoticeHead;
static uint8_t gAudioNoticeTail;
static uint8_t gAudioNoticeUsed;
static uint8_t gAudioDidiHead;
static uint8_t gAudioDidiTail;
static uint8_t gAudioDidiUsed;
static bool gAudioStartPlayed;
static uint32_t gAudioLastNoticeTick;
static uint32_t gAudioLastMetronomeTick;
static uint32_t gAudioLastLowBatteryTick;
static uint32_t gAudioBatteryDeadStartTick;
static uint32_t gAudioNextStateQueryTick;
static uint32_t gAudioNextPlayTick;

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
static uint32_t audioGetTickMs(void);
static bool audioSendAndWait(uint8_t cmd, eDrvStatus (*sendFunc)(void), uint32_t timeoutMs);
static eDrvStatus audioQueryMusicNum(void);
static eDrvStatus audioSetSingleMode(void);
static eDrvStatus audioSetDacMode(void);
static bool audioBuildFileName(eAudioClip clip, uint8_t *name);
static bool audioPlayClip(eAudioClip clip);
static void audioUpdateSettings(void);
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
    gAudioStatus.language = audioNormalizeLanguage(protcolMgrGetLanguageSetting());
    gAudioStatus.volumeLevel = audioNormalizeVolumeLevel(protcolMgrGetVolumeSetting());
    gAudioStatus.metronomeFreq = audioNormalizeMetronomeFreq(protcolMgrGetMetronomeFreq());

    (void)audioSendAndWait(WT2003HX_CMD_CHECK_MUSIC_NUM, audioQueryMusicNum, AUDIO_INIT_QUERY_TIMEOUT_MS);
    (void)audioSetSingleMode();
    (void)audioSetDacMode();
    lVolume = audioMapVolume(gAudioStatus.volumeLevel);
    (void)wt2003hxPortSetVolume(lVolume);

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
    audioUpdateSettings();
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

static uint32_t audioGetTickMs(void)
{
    return repRtosGetTickMs();
}

static bool audioSendAndWait(uint8_t cmd, eDrvStatus (*sendFunc)(void), uint32_t timeoutMs)
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

    LOG_W(AUDIO_LOG_TAG,
          "init query timeout cmd=0x%02X elapsed=%lu",
          (unsigned int)cmd,
          (unsigned long)(audioGetTickMs() - lStartTick));
    return false;
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
    eAudioLanguage lLanguage = audioNormalizeLanguage(protcolMgrGetLanguageSetting());
    uint8_t lVolume = audioNormalizeVolumeLevel(protcolMgrGetVolumeSetting());
    uint8_t lMetronome = audioNormalizeMetronomeFreq(protcolMgrGetMetronomeFreq());

    if (lLanguage != gAudioStatus.language) {
        gAudioStatus.language = lLanguage;
        (void)audioEnqueueNotice(AUDIO_CLIP_CHANGE_LANGUAGE);
    }

    if (lVolume != gAudioStatus.volumeLevel) {
        gAudioStatus.volumeLevel = lVolume;
        (void)wt2003hxPortSetVolume(audioMapVolume(lVolume));
    }

    gAudioStatus.metronomeFreq = lMetronome;
}

static void audioServiceMetronome(void)
{
    CPR_Manager_Typedef lManager;
    uint32_t lPeriodMs;
    uint32_t lNowTick = audioGetTickMs();

    cprAlgMgrGetManager(&lManager);
    if (!lManager.IsPressing) {
        return;
    }

    lPeriodMs = 60000UL / (uint32_t)gAudioStatus.metronomeFreq;
    if ((uint32_t)(lNowTick - gAudioLastMetronomeTick) >= lPeriodMs) {
        gAudioLastMetronomeTick = lNowTick;
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

    if (lPower == NULL) {
        return;
    }

    if ((lPower->BatLevel <= POWER_BATTERY_LOW_LEVEL_MAX) &&
        ((gAudioLastLowBatteryTick == 0U) || ((uint32_t)(lNowTick - gAudioLastLowBatteryTick) >= AUDIO_LOW_BATTERY_NOTICE_MS))) {
        gAudioLastLowBatteryTick = lNowTick;
        (void)audioEnqueueNotice(AUDIO_CLIP_LOW_BATTERY);
    }

    if ((lPower->BatLevel == 0U) && (lPower->voltage.dcMv <= 100U)) {
        if (gAudioBatteryDeadStartTick == 0U) {
            gAudioBatteryDeadStartTick = lNowTick;
            (void)audioEnqueueNotice(AUDIO_CLIP_BATTERY_DEAD);
        } else if ((uint32_t)(lNowTick - gAudioBatteryDeadStartTick) >= AUDIO_SHUTDOWN_DELAY_MS) {
            (void)powerRequestShutDown();
        }
    } else {
        gAudioBatteryDeadStartTick = 0U;
    }
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

    if (!gAudioStartPlayed) {
        gAudioStartPlayed = audioPlayClip(AUDIO_CLIP_START_CPR);
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
    (void)wt2003hxPortQuery(WT2003HX_CMD_CHECK_STATE);
}

/**************************End of file********************************/
