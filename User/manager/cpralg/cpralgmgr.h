#ifndef REBUILDCPR_CPRALGMGR_H
#define REBUILDCPR_CPRALGMGR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    uint8_t Depth;
    uint8_t Freq;
    uint8_t RealseDepth;  
    uint32_t TimeStamp;
    uint32_t BootTimeStamp;
}CPR_Data_Typedef;

typedef struct 
{
    bool IsPressing;
    bool DataReady;
    uint32_t IntervalTime;
}CPR_Manager_Typedef;

typedef struct 
{
    uint8_t Depth_High_Limit;
    uint8_t Depth_Low_Limit;
    uint8_t Freq_High_Limit;
    uint8_t Freq_Low_Limit;
    uint8_t RealseDepth_Low_Limit;
    uint16_t Depth_Alarm_Time;
    uint16_t Freq_Alarm_Time;
    uint16_t RealseDepth_Alarm_Time;
    uint16_t Press_Well_Time;

}CPR_Alarm_Limit_Typedef;

extern CPR_Data_Typedef s_CPR_Data;
extern CPR_Manager_Typedef s_CPR_Manager;
extern CPR_Alarm_Limit_Typedef s_CPR_Alarm_Limit;

bool cprAlgMgrInit(void);
void cprAlgMgrProcess(void);
void cprAlgMgrDisplayProcess(void);
void cprAlgMgrGetData(CPR_Data_Typedef *data);
void cprAlgMgrGetManager(CPR_Manager_Typedef *manager);
uint32_t cprAlgMgrGetRtcTime(void);
bool cprAlgMgrSetRtcTime(uint32_t timestamp);

#ifdef __cplusplus
}
#endif

#endif
