#ifndef CCPRFEEDBACK_C_H
#define CCPRFEEDBACK_C_H
#include <stdint.h>


/*******************************************************************
 *  All rights reserved.
 *
 *  文件名称: CprFeedback_C.h
 *  功    能: 基于按压力的CPR反馈参数计算C版本接口

 *  版本: 1.0
 *  作者: maoda
 *  日期: 2025/10/07
 *  说明: 创建
 ******************************************************************/

/*技术要求
设备可以检测按压过程中的深度,并通过指示灯显示
显示范围：0.0~8.0cm
精度：±0.5cm或者±10%取最大值
分辨率：0.1cm
刷新率：≥0.5Hz

设备可以检测按压过程中的深度,并通过数码管显示
显示范围：40~160cpm
精度：±2cpm
分辨率：1cpm
刷新率：≥0.5Hz
*/

#if defined(_WIN32) || defined(_WIN64)
#define DEFINE_EXPORTS_API
#ifdef DEFINE_EXPORTS_API
	#define EXPORTS_API __declspec(dllexport)
#else
	#define EXPORTS_API __declspec(dllimport)
#endif
#else
#ifdef DEFINE_EXPORTS_API
	#define EXPORTS_API __attribute__((visibility("default")))
#else
	#define EXPORTS_API
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct CprFeedbackRst_STR CprFeedbackRst;

/** @brief CPR数据配置 */
//@note:配置参数如下fs=100.0f,gain=8192.0f,fs_correction_factor=1.0f,gain_correction_factor=1.0f
//@note:如果原始数据为200Hz采样率(M700项目),由算法调用者隔一个点抽取将采样率降为100Hz,再行调用本算法
typedef struct S_CPR_ALG_CONFIG
{
	float fs;    //采样率
	float gain;  // 8192 LSB / G
	float fs_correction_factor;   // 采样率校正系数(有时采样率不准确，导致按压频率和深度出现偏差)
	float gain_correction_factor; // 增益校正系数
} CPR_ALG_CONFIG;

/** @brief CPR按压报警配置 */
typedef struct S_CPR_ALG_ALARM_LIMIT
{
	int depth_lower; // 按压深度下限，单位 mm  设置为50mm
	int depth_upper; // 按压深度上限，单位 mm  设置为60mm
	int rate_lower; // 按压频率下限， 单位 cpm 设置为100cpm
	int rate_upper; // 按压频率上限， 单位 cpm 设置为120cpm
} CPR_ALG_ALARM_LIMIT;

/** @brief CPR按压报警类型 */
enum ENUM_CPR_ALARM_TYPE 
{
	CPRALG_ALARM_SHALLOW = 1, /*!<  按压过浅 */
	CPRALG_ALARM_DEEP = 2,    /*!<  按压过深 */
	CPRALG_ALARM_SLOW = 4,    /*!<  按压过慢 */
	CPRALG_ALARM_FAST = 8,    /*!<  按压过快 */
	CPRALG_ALARM_NOISE = 16,  /*!<  信号波动大 */
};

typedef struct CprFeedbackRst_STR CprFeedbackRst;

// @brief 算法结果定义
struct CprFeedbackRst_STR
{
	bool    CprState;                     // 按压状态, 1:有按压 ,0 :无按压
	int16_t CPRDepth;                    // CPR按压深度趋势,单位mm
	int16_t CPRRate;                     // CPR按压频率趋势,单位bpm
	float   CPRRealseRatio;              // CPR回弹率趋势,取值0-1
	bool    SinglePressUpdated;           //单次按压参数更新标志位,1:更新,0:不更新
	int16_t CPRPressDepth_Instantaneous;    // 当前CPR深度,单位 mm
	int16_t CPRReleaseDepth_Instantaneous;  // 当前回弹深度,单位 mm
	bool    AlarmFlag;                      //报警标志位.1:报警，0,报警
	int16_t   Alarm;                        // 按压报警， CPRALG_ALARM_FAST | CPRALG_ALARM_SLOW | CPRALG_ALARM_DEEP | CPRALG_ALARM_SHALLOW
};

/**
* @brief  获取算法版本号
* @return 算法版本字符串，例如"01.00.00.01"
*/
EXPORTS_API const char* CprFeedback_get_version(void);

// 初始化
EXPORTS_API void CprFeedback_init(CPR_ALG_CONFIG cfg);

/**
* @brief  设置报警阈值
* @details
* @note 
*/
EXPORTS_API void CprFeedback_set_alarmlimit(CPR_ALG_ALARM_LIMIT alarmLimit);

// 计算深度：输入加速度ADC值和压力ADC值
EXPORTS_API void CprFeedback_process(int16_t acc_x,int16_t acc_y,int16_t acc_z,int16_t force);

// 获取计算结果
EXPORTS_API void CprFeedback_get_cpr_rst(CprFeedbackRst* pCprFeedbackRst);

#ifdef __cplusplus
}
#endif

#endif // CCPRFEEDBACK_C_H
