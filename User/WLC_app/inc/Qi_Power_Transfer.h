#ifndef _QI_POWER_TRANSFER_H_
#define _QI_POWER_TRANSFER_H_
/* 文件: Qi_Power_Transfer.h
 * 描述: Qi协议功率传输阶段模块头文件
 */
#include "wlc_app.h"
#include "wlc_types.h"
#include "Qi_Response.h"
#include "wlc_platform_types.h"

/* 功率传输统计信息结构 */
typedef struct {
	uint32_t ce_count;                 /* 控制误差数据包计数 */
	uint32_t rp8_count;                /* 接收功率8位数据包计数 */
	uint32_t rp_count;                 /* 接收功率16位数据包计数 */
	uint32_t last_rp8_percent;         /* 最近RP8百分比 */
	uint16_t last_rp_raw;              /* 最近RP原始值 */
	uint32_t current_rx_power_mw;      /* 当前接收功率（毫瓦） */
	uint32_t peak_rx_power_mw;         /* 记录的峰值接收功率 */
	uint32_t chs_count;                /* 充电状态数据包计数 */
	uint32_t pch_count;                /* 功率控制保持数据包计数 */
	uint32_t fod_count;                /* 异物检测数据包计数 */
}qi_pt_stats_t;


/* 功率传输阶段初始化函数 */
void qi_pt_init(uint8_t device, qi_phase_t phase, uint16_t reference_power_w, 
                uint16_t window_size_ms, uint16_t window_offset_ms);
/* 开始功率传输阶段 */
void qi_pt_start(uint8_t device);
/* 功率传输阶段处理函数 */
bool qi_pt_process(uint8_t device, wlc_context_t *ctx);
/* 获取功率传输统计信息 */
void qi_pt_get_stats(uint8_t device, qi_pt_stats_t *stats);
/* 重置功率传输统计信息 */
void qi_pt_reset_stats(uint8_t device);
/* 获取当前接收功率（毫瓦） */
uint32_t qi_pt_get_rx_power_mw(uint8_t device);
/* 检查功率传输是否活跃 */
bool qi_pt_is_active(uint8_t device);

#endif