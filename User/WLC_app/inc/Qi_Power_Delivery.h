#ifndef _QI_POWER_DELIVERY_H_
#define _QI_POWER_DELIVERY_H_
/* 文件名: Qi_Power_Delivery.h
 * 描述: Qi 协议功率传输模块头文件 - 功率控制
 */

#include "wlc_types.h"
#include "wlc_platform_types.h"

// 功率传输初始化函数
void qi_power_init(uint8_t device);
// 设置数字 Ping 功率
void qi_power_set_digital_ping(uint8_t device);
// 关闭功率传输
void qi_power_off(uint8_t device);
// 通过频率/移相/占空比组合设置功率等级
void qi_power_set_level(uint8_t device, uint32_t level);
// 功率传输控制误差处理函数
void qi_power_adjust_ce(uint8_t device, int8_t control_error);


#endif