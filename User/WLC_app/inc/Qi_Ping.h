#ifndef _QI_PING_H_
#define _QI_PING_H_

#include "wlc_types.h"
#include "wlc_platform_types.h"

/* Ping配置参数 */
#define PING_PULSE_ON_MS		68			/* Ping脉冲开启持续时间（毫秒） */
#define PING_CYCLE_MS				500     /* Ping周期间隔（毫秒） */
#define PING_POWER_LEVEL		50      /* Ping频率等级（30-85范围） */
#define PING_PULSE_OFF_MS		(PING_CYCLE_MS - PING_PULSE_ON_MS)  /* 432ms：脉冲关闭时间 */

#define PING_MAX_PULSES				10     /* 超时前的最大脉冲数 */
#define PING_TIMEOUT_MS				5000   /* Ping阶段总超时时间（毫秒） */

/* Ping状态枚举  */
typedef enum {
    PING_STATE_IDLE = 0,						/* 空闲状态 - 未传输 */
    PING_STATE_PULSE_ON,						/* 脉冲开启状态 - 传输功率 */
    PING_STATE_PULSE_OFF,						/* 脉冲关闭状态 - 等待下一个脉冲 */
    PING_STATE_DEVICE_DETECTED			/* 设备已检测 - 收到信号 */
} ping_state_t;

// 初始化指定设备的Ping模块
void qi_ping_init(uint8_t device);
// 设置Ping频率功率等级（在启动前可调整）
void qi_ping_set_power_level(uint8_t device, uint32_t level);
// 启动Ping阶段 - 传输Ping脉冲以检测设备
void qi_ping_start(uint8_t device);
// 停止Ping阶段 - 终止Ping脉冲传输
void qi_ping_stop(uint8_t device);
// 停止Ping阶段但保持电源开启以便下一阶段使用
void qi_ping_stop_keep_power(uint8_t device);
// 处理Ping阶段 - 处理接收到的数据包和脉冲时序
bool qi_ping_process(uint8_t device, const qi_packet_t *packet);
// 获取当前Ping状态
ping_state_t qi_ping_get_state(uint8_t device);
// 获取最后接收到的信号强度
uint32_t qi_ping_get_signal_strength(uint8_t device);

#endif