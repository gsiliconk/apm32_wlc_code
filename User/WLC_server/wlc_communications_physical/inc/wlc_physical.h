#ifndef __WLC_PHYSICAL_H_
#define __WLC_PHYSICAL_H_

#include "wlc_platform_types.h"

void wlc_ask_demod_enable(uint8_t model, uint8_t endis); // 为指定设备使能/禁用 ASK 解调
void wlc_ask_demod_handler(uint8_t device, uint8_t ch, uint32_t pwidth); // ASK 解调处理函数

#endif