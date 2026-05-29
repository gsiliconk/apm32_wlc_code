#ifndef _WLC_APP_H_
#define _WLC_APP_H_

#include "wlc_types.h"
#include "wlc_platform_types.h"

/* 应用程序上下文结构体 - 保存每个设备的状态信息 */
typedef struct {
    qi_phase_t phase;                  /* 当前协议阶段 */
    qi_phase_t prev_phase;             /* 上一协议阶段 */
    uint32_t timeout_timer;            /* 阶段超时定时器 ID */
    qi_packet_t rx_packet;             /* 接收数据包缓冲区 */
    qi_id_data_t id_data;              /* 解析后的 ID 数据 */
    qi_cfg_data_t cfg_data;            /* 解析后的配置数据 */
    qi_neg_data_t neg_data;            /* 协商数据 */
    qi_power_contract_t contract;      /* 功率传输合约 */
    bool phase_initialized;            /* 阶段初始化标志 */
    qi_power_state_t vin_state;        /* 输入电压状态 (5V/9V) */
} wlc_context_t;


/* 初始化 WLC 应用程序 */
void wlc_app_init(void);
/* WLC 主任务 */
void wlc_app_task(void);
/* 数据包接收回调函数 */
void wlc_app_data_received_callback(uint8_t device, uint8_t channel,
                                   uint8_t packet_type, uint8_t *data,
                                   uint8_t data_length);
/* 获取当前协议阶段 */
qi_phase_t wlc_app_get_phase(uint8_t device);
/* 获取当前功率传输契约 */
void wlc_app_get_contract(uint8_t device, qi_power_contract_t *contract);
/* 获取接收端设备类型 */
qi_device_type_t qi_app_get_rxdevice(uint8_t device);
/* 获取输入电压状态 */
qi_power_state_t qi_app_get_vin_state(uint8_t device);

/* 获取输入电压 (毫伏) */
uint16_t wlc_app_get_vin(uint8_t device);
/* 获取输入电流 (毫伏表示) */
uint16_t wlc_app_get_current(uint8_t device);
/* 获取参考功率 (毫瓦) */
uint32_t wlc_app_get_reference_power(uint8_t device);

#endif