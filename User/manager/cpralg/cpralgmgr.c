#include "cpralgmgr.h"

#include "CprFeedback_C.h"
#include "../memory/memory.h"
#include "../selfcheck/selfcheck_fault.h"
#include "../sensor/sensor.h"
#include "../../port/pca9535_port.h"
#include "../../port/tm1651_port.h"
#include "../../../rep/service/log/log.h"
#include "rtos.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CPR_ALG_MGR_INTERVAL_MS 50U
#define CPR_ALG_MGR_RTC_BASE_YEAR 2025U
#define CPR_ALG_MGR_PAUSE_RESET_MS 3000U

static const char gCprAlgMgrLogTag[] = "cpralg";
static const char gCprAlgMgrMdataDir[] = "/mdata";
static const uint8_t gCprAlgMgrFreqDisplayMin = 40U;
static const uint8_t gCprAlgMgrFreqDisplayMax = 160U;
static const uint32_t gCprAlgMgrMetricDisplayMs = 2000U;
static const uint32_t gCprAlgMgrIntervalDisplayStartMs = 3000U;
static const uint32_t gCprAlgMgrIntervalDisplayEndMs = 300000U;
static const uint32_t gCprAlgMgrMdataRecordSize = 7U;
static const uint32_t gCprAlgMgrMdataRecordsPerFile = 300U;
static const uint32_t gCprAlgMgrMdataFileMaxSize = 2100U;
static const uint32_t gCprAlgMgrMdataMaxRecords = 30000U;

struct stCprAlgMgrMdataScanContext {
    uint32_t totalRecordCount;
    uint32_t oldestBootTimeStamp;
    uint32_t oldestFileIndex;
    uint32_t oldestRecordCount;
    char oldestPath[VFS_PATH_MAX];
    uint32_t emptyBootTimeStamp;
    uint32_t emptyFileIndex;
    char emptyPath[VFS_PATH_MAX];
    bool hasOldest;
    bool hasEmpty;
};

static bool sCprAlgMgrHasFirstPress = false;
static uint32_t sCprAlgMgrMdataBootTimeStamp = 0U;
static uint32_t sCprAlgMgrMdataFileIndex = 0U;
static uint32_t sCprAlgMgrMdataRecordCount = 0U;
static bool sCprAlgMgrMdataReady = false;
static struct stCprAlgMgrMdataScanContext sCprAlgMgrMdataScanContext;
static char sCprAlgMgrMdataPath[VFS_PATH_MAX];
static uint8_t sCprAlgMgrMdataRecord[7U];

CPR_Data_Typedef s_CPR_Data = {0};
CPR_Manager_Typedef s_CPR_Manager = {0};
static CPR_Recent_Press_History_Typedef sCprAlgMgrRecentPressHistory = {0};
CPR_Alarm_Limit_Typedef s_CPR_Alarm_Limit = {
    .Depth_High_Limit = 60,
    .Depth_Low_Limit = 50,
    .Freq_High_Limit = 120,
    .Freq_Low_Limit = 100,
    .RealseDepth_Low_Limit = 20,
    .Depth_Alarm_Time = 15000,
    .Freq_Alarm_Time = 15000,
    .RealseDepth_Alarm_Time = 15000,
    .Press_Well_Time = 15000,
};

static bool s_CPR_Alg_Initialized = false;

static bool cprAlgMgrEnsureMdataDir(void);
static void cprAlgMgrBuildMdataPath(uint32_t bootTimeStamp, uint32_t fileIndex, char *path, uint32_t pathSize);
static bool cprAlgMgrParseMdataFileName(const char *name, uint32_t *bootTimeStamp, uint32_t *fileIndex);
static bool cprAlgMgrMdataScanVisitor(void *context, const stMemoryEntryInfo *entry);
static bool cprAlgMgrCleanupMdataIfNeeded(void);
static void cprAlgMgrEncodePressRecord(const CPR_Data_Typedef *data, uint8_t record[7U]);
static bool cprAlgMgrPrepareMdataFile(const CPR_Data_Typedef *data);
static bool cprAlgMgrStorePressRecord(const CPR_Data_Typedef *data);

static bool cprAlgMgrEnsureMdataDir(void)
{
    if (memoryExists(gCprAlgMgrMdataDir)) {
        return true;
    }

    if (!memoryMkdir(gCprAlgMgrMdataDir)) {
        LOG_E(gCprAlgMgrLogTag, "mdata mkdir fail path=%s", gCprAlgMgrMdataDir);
        return false;
    }

    return true;
}

static void cprAlgMgrBuildMdataPath(uint32_t bootTimeStamp, uint32_t fileIndex, char *path, uint32_t pathSize)
{
    if ((path == NULL) || (pathSize == 0U)) {
        return;
    }

    if (fileIndex == 0U) {
        (void)snprintf(path, pathSize, "%s/%08lX.bin",
                       gCprAlgMgrMdataDir,
                       (unsigned long)bootTimeStamp);
    } else {
        (void)snprintf(path, pathSize, "%s/%08lX-%lu.bin",
                       gCprAlgMgrMdataDir,
                       (unsigned long)bootTimeStamp,
                       (unsigned long)fileIndex);
    }
}

static bool cprAlgMgrParseHexDigit(char digit, uint32_t *value)
{
    if (value == NULL) {
        return false;
    }

    if ((digit >= '0') && (digit <= '9')) {
        *value = (uint32_t)(digit - '0');
        return true;
    }

    if ((digit >= 'A') && (digit <= 'F')) {
        *value = (uint32_t)(digit - 'A') + 10U;
        return true;
    }

    if ((digit >= 'a') && (digit <= 'f')) {
        *value = (uint32_t)(digit - 'a') + 10U;
        return true;
    }

    return false;
}

static bool cprAlgMgrParseMdataFileName(const char *name, uint32_t *bootTimeStamp, uint32_t *fileIndex)
{
    uint32_t lBootTimeStamp = 0U;
    uint32_t lFileIndex = 0U;
    uint32_t lDigit;
    uint32_t lIndex;
    const char *lCursor;

    if ((name == NULL) || (bootTimeStamp == NULL) || (fileIndex == NULL)) {
        return false;
    }

    for (lIndex = 0U; lIndex < 8U; lIndex++) {
        if (!cprAlgMgrParseHexDigit(name[lIndex], &lDigit)) {
            return false;
        }
        lBootTimeStamp = (lBootTimeStamp << 4U) | lDigit;
    }

    lCursor = &name[8];
    if (strncmp(lCursor, ".bin", 4U) == 0) {
        if (lCursor[4] != '\0') {
            return false;
        }
        *bootTimeStamp = lBootTimeStamp;
        *fileIndex = 0U;
        return true;
    }

    if (*lCursor != '-') {
        return false;
    }
    lCursor++;

    if ((*lCursor < '0') || (*lCursor > '9')) {
        return false;
    }

    while ((*lCursor >= '0') && (*lCursor <= '9')) {
        uint32_t lNextIndex = (lFileIndex * 10U) + (uint32_t)(*lCursor - '0');

        if (lNextIndex < lFileIndex) {
            return false;
        }
        lFileIndex = lNextIndex;
        lCursor++;
    }

    if ((strncmp(lCursor, ".bin", 4U) != 0) || (lCursor[4] != '\0')) {
        return false;
    }

    *bootTimeStamp = lBootTimeStamp;
    *fileIndex = lFileIndex;
    return true;
}

static bool cprAlgMgrMdataScanVisitor(void *context, const stMemoryEntryInfo *entry)
{
    struct stCprAlgMgrMdataScanContext *lContext = (struct stCprAlgMgrMdataScanContext *)context;
    uint32_t lBootTimeStamp;
    uint32_t lFileIndex;
    uint32_t lRecordCount;
    bool lIsOlder;

    if ((lContext == NULL) || (entry == NULL)) {
        return false;
    }

    if ((entry->type != eMEMORY_ENTRY_TYPE_FILE) ||
        !cprAlgMgrParseMdataFileName(entry->name, &lBootTimeStamp, &lFileIndex)) {
        return true;
    }

    lRecordCount = entry->size / gCprAlgMgrMdataRecordSize;
    lContext->totalRecordCount += lRecordCount;

    if ((entry->size == 0U) && !lContext->hasEmpty) {
        lContext->hasEmpty = true;
        lContext->emptyBootTimeStamp = lBootTimeStamp;
        lContext->emptyFileIndex = lFileIndex;
        cprAlgMgrBuildMdataPath(lBootTimeStamp, lFileIndex, lContext->emptyPath, sizeof(lContext->emptyPath));
    }

    lIsOlder = !lContext->hasOldest ||
               (lBootTimeStamp < lContext->oldestBootTimeStamp) ||
               ((lBootTimeStamp == lContext->oldestBootTimeStamp) &&
                (lFileIndex < lContext->oldestFileIndex));

    if (lIsOlder) {
        lContext->hasOldest = true;
        lContext->oldestBootTimeStamp = lBootTimeStamp;
        lContext->oldestFileIndex = lFileIndex;
        lContext->oldestRecordCount = lRecordCount;
        cprAlgMgrBuildMdataPath(lBootTimeStamp, lFileIndex, lContext->oldestPath, sizeof(lContext->oldestPath));
    }

    return true;
}

static bool cprAlgMgrCleanupMdataIfNeeded(void)
{
    uint32_t lEntryCount;

    do {
        (void)memset(&sCprAlgMgrMdataScanContext, 0, sizeof(sCprAlgMgrMdataScanContext));
        lEntryCount = 0U;

        if (!memoryListDir(gCprAlgMgrMdataDir, cprAlgMgrMdataScanVisitor, &sCprAlgMgrMdataScanContext, &lEntryCount)) {
            LOG_E(gCprAlgMgrLogTag, "mdata list fail path=%s", gCprAlgMgrMdataDir);
            return false;
        }

        if (sCprAlgMgrMdataScanContext.hasEmpty) {
            LOG_I(gCprAlgMgrLogTag, "mdata delete empty path=%s", sCprAlgMgrMdataScanContext.emptyPath);

            if (!memoryDelete(sCprAlgMgrMdataScanContext.emptyPath)) {
                LOG_E(gCprAlgMgrLogTag, "mdata delete fail path=%s", sCprAlgMgrMdataScanContext.emptyPath);
                return false;
            }

            if (sCprAlgMgrMdataReady &&
                (sCprAlgMgrMdataBootTimeStamp == sCprAlgMgrMdataScanContext.emptyBootTimeStamp) &&
                (sCprAlgMgrMdataFileIndex == sCprAlgMgrMdataScanContext.emptyFileIndex)) {
                sCprAlgMgrMdataReady = false;
            }
            continue;
        }

        if ((sCprAlgMgrMdataScanContext.totalRecordCount + 1U) <= gCprAlgMgrMdataMaxRecords) {
            return true;
        }

        if (!sCprAlgMgrMdataScanContext.hasOldest) {
            return false;
        }

        LOG_I(gCprAlgMgrLogTag, "mdata delete oldest path=%s records=%lu",
              sCprAlgMgrMdataScanContext.oldestPath,
              (unsigned long)sCprAlgMgrMdataScanContext.oldestRecordCount);

        if (!memoryDelete(sCprAlgMgrMdataScanContext.oldestPath)) {
            LOG_E(gCprAlgMgrLogTag, "mdata delete fail path=%s", sCprAlgMgrMdataScanContext.oldestPath);
            return false;
        }

        if (sCprAlgMgrMdataReady &&
            (sCprAlgMgrMdataBootTimeStamp == sCprAlgMgrMdataScanContext.oldestBootTimeStamp) &&
            (sCprAlgMgrMdataFileIndex == sCprAlgMgrMdataScanContext.oldestFileIndex)) {
            sCprAlgMgrMdataReady = false;
        }
    } while (true);
}

static void cprAlgMgrEncodePressRecord(const CPR_Data_Typedef *data, uint8_t record[7U])
{
    if ((data == NULL) || (record == NULL)) {
        return;
    }

    record[0] = data->Depth;
    record[1] = data->Freq;
    record[2] = data->RealseDepth;
    record[3] = (uint8_t)(data->TimeStamp & 0xFFU);
    record[4] = (uint8_t)((data->TimeStamp >> 8U) & 0xFFU);
    record[5] = (uint8_t)((data->TimeStamp >> 16U) & 0xFFU);
    record[6] = (uint8_t)((data->TimeStamp >> 24U) & 0xFFU);
}

static bool cprAlgMgrPrepareMdataFile(const CPR_Data_Typedef *data)
{
    uint32_t lFileSize;
    bool lFileExists;

    if (data == NULL) {
        return false;
    }

    if (!memoryIsReady()) {
        return false;
    }

    if (!cprAlgMgrEnsureMdataDir() || !cprAlgMgrCleanupMdataIfNeeded()) {
        return false;
    }

    if (!sCprAlgMgrMdataReady ||
        (sCprAlgMgrMdataBootTimeStamp != data->BootTimeStamp)) {
        sCprAlgMgrMdataBootTimeStamp = data->BootTimeStamp;
        sCprAlgMgrMdataFileIndex = 0U;
        sCprAlgMgrMdataRecordCount = 0U;
        sCprAlgMgrMdataReady = true;
    }

    do {
        cprAlgMgrBuildMdataPath(sCprAlgMgrMdataBootTimeStamp,
                                sCprAlgMgrMdataFileIndex,
                                sCprAlgMgrMdataPath,
                                sizeof(sCprAlgMgrMdataPath));
        lFileSize = 0U;
        lFileExists = memoryExists(sCprAlgMgrMdataPath);

        if (lFileExists && !memoryGetFileSize(sCprAlgMgrMdataPath, &lFileSize)) {
            LOG_E(gCprAlgMgrLogTag, "mdata size fail path=%s", sCprAlgMgrMdataPath);
            return false;
        }

        if (!lFileExists) {
            sCprAlgMgrMdataRecordCount = 0U;
            return true;
        }

        if ((lFileSize % gCprAlgMgrMdataRecordSize) != 0U) {
            sCprAlgMgrMdataFileIndex++;
            LOG_I(gCprAlgMgrLogTag, "mdata switch file boot=0x%08lX index=%lu",
                  (unsigned long)sCprAlgMgrMdataBootTimeStamp,
                  (unsigned long)sCprAlgMgrMdataFileIndex);
            continue;
        }

        sCprAlgMgrMdataRecordCount = lFileSize / gCprAlgMgrMdataRecordSize;
        if ((sCprAlgMgrMdataRecordCount >= gCprAlgMgrMdataRecordsPerFile) ||
            ((lFileSize + gCprAlgMgrMdataRecordSize) > gCprAlgMgrMdataFileMaxSize)) {
            sCprAlgMgrMdataFileIndex++;
            LOG_I(gCprAlgMgrLogTag, "mdata switch file boot=0x%08lX index=%lu",
                  (unsigned long)sCprAlgMgrMdataBootTimeStamp,
                  (unsigned long)sCprAlgMgrMdataFileIndex);
            continue;
        }

        return true;
    } while (true);
}

static bool cprAlgMgrStorePressRecord(const CPR_Data_Typedef *data)
{
    if (data == NULL) {
        return false;
    }

    if (!cprAlgMgrPrepareMdataFile(data)) {
        return false;
    }

    cprAlgMgrEncodePressRecord(data, sCprAlgMgrMdataRecord);
    cprAlgMgrBuildMdataPath(sCprAlgMgrMdataBootTimeStamp,
                            sCprAlgMgrMdataFileIndex,
                            sCprAlgMgrMdataPath,
                            sizeof(sCprAlgMgrMdataPath));

    if (!memoryAppendFile(sCprAlgMgrMdataPath,
                          sCprAlgMgrMdataRecord,
                          (uint32_t)sizeof(sCprAlgMgrMdataRecord))) {
        uint32_t lFileSize;

        if (memoryGetFileSize(sCprAlgMgrMdataPath, &lFileSize) && (lFileSize == 0U)) {
            (void)memoryDelete(sCprAlgMgrMdataPath);
        }
        LOG_E(gCprAlgMgrLogTag, "mdata append fail path=%s", sCprAlgMgrMdataPath);
        return false;
    }

    sCprAlgMgrMdataRecordCount++;
    return true;
}

static void cprAlgMgrClearRecentPressHistory(void)
{
    uint8_t lIndex;

    sCprAlgMgrRecentPressHistory.Count = 0U;
    sCprAlgMgrRecentPressHistory.NextIndex = 0U;
    for (lIndex = 0U; lIndex < CPR_ALG_MGR_RECENT_PRESS_COUNT; lIndex++) {
        sCprAlgMgrRecentPressHistory.Records[lIndex].Depth = 0U;
        sCprAlgMgrRecentPressHistory.Records[lIndex].Freq = 0U;
        sCprAlgMgrRecentPressHistory.Records[lIndex].TimeStamp = 0U;
        sCprAlgMgrRecentPressHistory.Records[lIndex].Valid = false;
    }
}

static void cprAlgMgrPushRecentPressHistory(uint8_t depth, uint8_t freq, uint32_t timeStamp)
{
    CPR_Press_Record_Typedef *lRecord = &sCprAlgMgrRecentPressHistory.Records[sCprAlgMgrRecentPressHistory.NextIndex];

    lRecord->Depth = depth;
    lRecord->Freq = freq;
    lRecord->TimeStamp = timeStamp;
    lRecord->Valid = true;

    sCprAlgMgrRecentPressHistory.NextIndex =
        (uint8_t)((sCprAlgMgrRecentPressHistory.NextIndex + 1U) % CPR_ALG_MGR_RECENT_PRESS_COUNT);
    if (sCprAlgMgrRecentPressHistory.Count < CPR_ALG_MGR_RECENT_PRESS_COUNT) {
        sCprAlgMgrRecentPressHistory.Count++;
    }
}

static bool cprAlgMgrIsLeapYear(uint16_t year)
{
    return (((year % 4U) == 0U) && ((year % 100U) != 0U)) || ((year % 400U) == 0U);
}

static void cprAlgMgrTimestampToDateTime(uint32_t timestamp,
                                         uint16_t *year,
                                         uint8_t *month,
                                         uint8_t *day,
                                         uint8_t *hour,
                                         uint8_t *minute,
                                         uint8_t *second)
{
    static const uint8_t lDaysInMonth[2][12] = {
        {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U},
        {31U, 29U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U},
    };
    uint32_t lTotalDays;
    uint16_t lYear;
    uint8_t lMonth;
    uint8_t lLeap;

    if ((year == NULL) || (month == NULL) || (day == NULL) ||
        (hour == NULL) || (minute == NULL) || (second == NULL)) {
        return;
    }

    *second = (uint8_t)(timestamp % 60U);
    timestamp /= 60U;
    *minute = (uint8_t)(timestamp % 60U);
    timestamp /= 60U;
    *hour = (uint8_t)(timestamp % 24U);
    lTotalDays = timestamp / 24U;

    lYear = CPR_ALG_MGR_RTC_BASE_YEAR;
    while (lTotalDays >= (cprAlgMgrIsLeapYear(lYear) ? 366U : 365U)) {
        lTotalDays -= cprAlgMgrIsLeapYear(lYear) ? 366U : 365U;
        lYear++;
    }

    lLeap = cprAlgMgrIsLeapYear(lYear) ? 1U : 0U;
    for (lMonth = 0U; lMonth < 12U; lMonth++) {
        if (lTotalDays < lDaysInMonth[lLeap][lMonth]) {
            break;
        }
        lTotalDays -= lDaysInMonth[lLeap][lMonth];
    }

    *year = lYear;
    *month = (uint8_t)(lMonth + 1U);
    *day = (uint8_t)(lTotalDays + 1U);
}

static void cprAlgMgrLogBootRtcTime(uint32_t bootTimeStamp)
{
    uint16_t lYear;
    uint8_t lMonth;
    uint8_t lDay;
    uint8_t lHour;
    uint8_t lMinute;
    uint8_t lSecond;

    cprAlgMgrTimestampToDateTime(bootTimeStamp, &lYear, &lMonth, &lDay, &lHour, &lMinute, &lSecond);
    LOG_I(gCprAlgMgrLogTag, "boot rtc timestamp=0x%08lX", (unsigned long)bootTimeStamp);
    LOG_I(gCprAlgMgrLogTag,
          "boot rtc date=%04u-%02u-%02u %02u:%02u:%02u",
          (unsigned int)lYear,
          (unsigned int)lMonth,
          (unsigned int)lDay,
          (unsigned int)lHour,
          (unsigned int)lMinute,
          (unsigned int)lSecond);
}

static uint8_t cprAlgMgrClampToU8(int16_t value)
{
    if (value <= 0) {
        return 0U;
    }

    if (value >= 255) {
        return 255U;
    }

    return (uint8_t)value;
}

static uint8_t cprAlgMgrClampFreqForDisplay(uint8_t freq)
{
    if (freq == 0U) {
        return 0U;
    }

    if (freq < gCprAlgMgrFreqDisplayMin) {
        return gCprAlgMgrFreqDisplayMin;
    }

    if (freq > gCprAlgMgrFreqDisplayMax) {
        return gCprAlgMgrFreqDisplayMax;
    }

    return freq;
}

static uint8_t cprAlgMgrGetDepthLedNum(uint8_t depth)
{
    uint8_t lLedNum;

    if (depth == 0U) {
        return 0U;
    }

    lLedNum = (uint8_t)((depth + 9U) / 10U);
    if (lLedNum > PCA9535_PORT_LED_MAX) {
        lLedNum = PCA9535_PORT_LED_MAX;
    }

    return lLedNum;
}

static bool cprAlgMgrIsPressWell(uint8_t depth, uint8_t freq)
{
    return (depth >= s_CPR_Alarm_Limit.Depth_Low_Limit) &&
           (depth < s_CPR_Alarm_Limit.Depth_High_Limit) &&
           (freq >= s_CPR_Alarm_Limit.Freq_Low_Limit) &&
           (freq < s_CPR_Alarm_Limit.Freq_High_Limit);
}

static void cprAlgMgrUpdateInterval(void)
{
    if ((UINT32_MAX - s_CPR_Manager.IntervalTime) >= CPR_ALG_MGR_INTERVAL_MS) {
        s_CPR_Manager.IntervalTime += CPR_ALG_MGR_INTERVAL_MS;
    } else {
        s_CPR_Manager.IntervalTime = UINT32_MAX;
    }
}

static void cprAlgMgrShowMetrics(uint8_t depth, uint8_t freq)
{
    uint8_t lDisplayFreq = cprAlgMgrClampFreqForDisplay(freq);
    uint8_t lDisplayDepth = depth;

    (void)tm1651PortShowNumber3(lDisplayFreq);
    (void)pca9535PortLedLightNum(cprAlgMgrGetDepthLedNum(lDisplayDepth));
}

static void cprAlgMgrLedProcess(void)
{
    uint8_t lDepth;
    uint8_t lFreq;
    uint32_t lIntervalTime;
    bool lPressWell;

    repRtosEnterCritical();
    lDepth = s_CPR_Data.Depth;
    lFreq = s_CPR_Data.Freq;
    lIntervalTime = s_CPR_Manager.IntervalTime;
    repRtosExitCritical();

    lPressWell = cprAlgMgrIsPressWell(lDepth, lFreq);

    if (!sCprAlgMgrHasFirstPress) {
        (void)tm1651PortShowNone();
        (void)pca9535PortLedLightNum(0U);
        (void)pca9535PortLedPressShow(false, false, false);
        return;
    }

    if (lIntervalTime < gCprAlgMgrMetricDisplayMs) {
        cprAlgMgrShowMetrics(lDepth, lFreq);
        (void)pca9535PortLedPressShow(!lPressWell, lPressWell, false);
        return;
    }

    (void)pca9535PortLedLightNum(0U);

    if (lIntervalTime < gCprAlgMgrIntervalDisplayStartMs) {
        (void)tm1651PortShowNone();
        (void)pca9535PortLedPressShow(!lPressWell, lPressWell, false);
        return;
    }

    if (lIntervalTime <= gCprAlgMgrIntervalDisplayEndMs) {
        (void)tm1651PortShowNumber3((uint16_t)(lIntervalTime / 1000U));
        (void)pca9535PortLedPressShow(false, false, true);
        return;
    }

    (void)tm1651PortShowNone();
    (void)pca9535PortLedPressShow(false, false, false);
}

bool cprAlgMgrInit(void)
{
    CPR_ALG_CONFIG lAlgConfig;
    CPR_ALG_ALARM_LIMIT lAlarmLimit;
    uint32_t lBootTimeStamp;

    if (s_CPR_Alg_Initialized) {
        return true;
    }

    lAlgConfig.fs = 100.0f;
    lAlgConfig.gain = 8192.0f;
    lAlgConfig.fs_correction_factor = 1.0f;
    lAlgConfig.gain_correction_factor = 1.0f;

    lAlarmLimit.depth_lower = (int)s_CPR_Alarm_Limit.Depth_Low_Limit;
    lAlarmLimit.depth_upper = (int)s_CPR_Alarm_Limit.Depth_High_Limit;
    lAlarmLimit.rate_lower = (int)s_CPR_Alarm_Limit.Freq_Low_Limit;
    lAlarmLimit.rate_upper = (int)s_CPR_Alarm_Limit.Freq_High_Limit;

    CprFeedback_init(lAlgConfig);
    CprFeedback_set_alarmlimit(lAlarmLimit);

    if (!selfCheckFaultGetBootRtcTime(&lBootTimeStamp)) {
        lBootTimeStamp = cprAlgMgrGetRtcTime();
    }

    repRtosEnterCritical();
    s_CPR_Data.Depth = 0U;
    s_CPR_Data.Freq = 0U;
    s_CPR_Data.RealseDepth = 0U;
    s_CPR_Data.TimeStamp = 0U;
    s_CPR_Data.BootTimeStamp = lBootTimeStamp;
    s_CPR_Manager.IsPressing = false;
    s_CPR_Manager.DataReady = false;
    s_CPR_Manager.IntervalTime = 0U;
    cprAlgMgrClearRecentPressHistory();
    repRtosExitCritical();

    sCprAlgMgrHasFirstPress = false;
    cprAlgMgrLogBootRtcTime(lBootTimeStamp);

    s_CPR_Alg_Initialized = true;
    return true;
}

void cprAlgMgrProcess(void)
{
    stSensorSample lSample;
    CprFeedbackRst lCprRst;
    bool lDataReady = false;
    CPR_Data_Typedef lPressData;
    bool lShouldStorePress;

    if (!s_CPR_Alg_Initialized && !cprAlgMgrInit()) {
        return;
    }

    while (sensorReadSample(&lSample, 0U)) {
        uint32_t lPressTimeStamp;

        lShouldStorePress = false;

        CprFeedback_process(lSample.x, lSample.y, lSample.z, (int16_t)lSample.force);
        CprFeedback_get_cpr_rst(&lCprRst);

        repRtosEnterCritical();
        s_CPR_Manager.IsPressing = lCprRst.CprState;

        if (lCprRst.SinglePressUpdated) {
            s_CPR_Data.Depth = cprAlgMgrClampToU8(lCprRst.CPRDepth);
            s_CPR_Data.Freq = cprAlgMgrClampToU8(lCprRst.CPRRate);
            s_CPR_Data.RealseDepth = cprAlgMgrClampToU8(lCprRst.CPRReleaseDepth_Instantaneous);
            lPressTimeStamp = repRtosGetTickMs();
            s_CPR_Data.TimeStamp = lPressTimeStamp;
            s_CPR_Manager.IntervalTime = 0U;
            cprAlgMgrPushRecentPressHistory(s_CPR_Data.Depth, s_CPR_Data.Freq, lPressTimeStamp);
            sCprAlgMgrHasFirstPress = true;
            lPressData = s_CPR_Data;
            lShouldStorePress = true;
            lDataReady = true;
        }

        s_CPR_Manager.DataReady = lDataReady;
        repRtosExitCritical();

        if (lShouldStorePress) {
            (void)cprAlgMgrStorePressRecord(&lPressData);
        }
    }

    repRtosEnterCritical();
    s_CPR_Manager.DataReady = lDataReady;
    if (!lDataReady) {
        cprAlgMgrUpdateInterval();
        if (!s_CPR_Manager.IsPressing && (s_CPR_Manager.IntervalTime >= CPR_ALG_MGR_PAUSE_RESET_MS)) {
            cprAlgMgrClearRecentPressHistory();
        }
    }
    repRtosExitCritical();
}

void cprAlgMgrDisplayProcess(void)
{
    if (!s_CPR_Alg_Initialized && !cprAlgMgrInit()) {
        return;
    }

    cprAlgMgrLedProcess();
}

void cprAlgMgrGetData(CPR_Data_Typedef *data)
{
    if (data == NULL) {
        return;
    }

    repRtosEnterCritical();
    *data = s_CPR_Data;
    repRtosExitCritical();
}

void cprAlgMgrGetManager(CPR_Manager_Typedef *manager)
{
    if (manager == NULL) {
        return;
    }

    repRtosEnterCritical();
    *manager = s_CPR_Manager;
    repRtosExitCritical();
}

void cprAlgMgrGetRecentPressHistory(CPR_Recent_Press_History_Typedef *history)
{
    if (history == NULL) {
        return;
    }

    repRtosEnterCritical();
    *history = sCprAlgMgrRecentPressHistory;
    repRtosExitCritical();
}

uint32_t cprAlgMgrGetRtcTime(void)
{
    uint32_t lTimestamp;

    if (!selfCheckFaultGetRtcTime(&lTimestamp)) {
        lTimestamp = 0U;
    }

    return lTimestamp;
}

bool cprAlgMgrSetRtcTime(uint32_t timestamp)
{
    return selfCheckFaultSetRtcTime(timestamp);
}
