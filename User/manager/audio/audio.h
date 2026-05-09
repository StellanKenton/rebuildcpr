/************************************************************************************
* @file     : audio.h
* @brief    : Project-side audio manager declarations.
* @details  : Converts CPR and product events into WT2003HX audio playback requests.
* @author   : 
* @date     : 2026-04-30
* @version  : V1.0.0
* @copyright: Copyright (c) 2050
***********************************************************************************/
#ifndef REBUILDCPR_AUDIO_H
#define REBUILDCPR_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_NOTICE_QUEUE_SIZE             6U
#define AUDIO_DIDI_QUEUE_SIZE               6U
#define AUDIO_DEFAULT_LANGUAGE              AUDIO_LANGUAGE_ZH
#define AUDIO_DEFAULT_VOLUME_LEVEL          3U
#define AUDIO_DEFAULT_METRONOME_FREQ        100U
#define AUDIO_NOTICE_WINDOW_MS              15000U
#define AUDIO_LOW_BATTERY_NOTICE_MS         60000U
#define AUDIO_SHUTDOWN_DELAY_MS             5000U
#define AUDIO_STATE_QUERY_IDLE_MS           200U
#define AUDIO_STATE_QUERY_ACTIVE_MS         20U
#define AUDIO_PLAY_COOLDOWN_MS              20U

typedef enum eAudioLanguage {
    AUDIO_LANGUAGE_INVALID = 0,
    AUDIO_LANGUAGE_ZH = 1,
    AUDIO_LANGUAGE_EN = 2,
    AUDIO_LANGUAGE_DE = 3,
    AUDIO_LANGUAGE_FR = 4,
    AUDIO_LANGUAGE_IT = 5,
} eAudioLanguage;

typedef enum eAudioClip {
    AUDIO_CLIP_START_CPR = 0,
    AUDIO_CLIP_DIDI,
    AUDIO_CLIP_PRESS_DEEP,
    AUDIO_CLIP_PRESS_SHALLOW,
    AUDIO_CLIP_PRESS_SLOW,
    AUDIO_CLIP_PRESS_FAST,
    AUDIO_CLIP_LOW_BATTERY,
    AUDIO_CLIP_BATTERY_DEAD,
    AUDIO_CLIP_CHANGE_LANGUAGE,
    AUDIO_CLIP_PRESS_WELL,
    AUDIO_CLIP_MAX,
} eAudioClip;

typedef enum eAudioState {
    AUDIO_STATE_UNINIT = 0,
    AUDIO_STATE_READY,
    AUDIO_STATE_FAULT,
} eAudioState;

typedef struct stAudioStatus {
    eAudioState state;
    eAudioLanguage language;
    uint8_t volumeLevel;
    uint8_t metronomeFreq;
    uint16_t musicNum;
    bool moduleReady;
    bool commResponded;
    bool musicNumValid;
} stAudioStatus;

bool audioInit(void);
void audioProcess(void);
void audioApplyLanguageSetting(uint8_t language, bool notifyChange);
void audioApplyVolumeSetting(uint8_t volumeLevel);
void audioApplyMetronomeSetting(uint8_t metronomeFreq);
bool audioEnqueueNotice(eAudioClip clip);
bool audioEnqueueDidi(void);
const stAudioStatus *audioGetStatus(void);

#ifdef __cplusplus
}
#endif

#endif  // REBUILDCPR_AUDIO_H
/**************************End of file********************************/
