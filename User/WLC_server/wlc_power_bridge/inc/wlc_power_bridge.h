#ifndef _WLC_POWER_BRIDGE_H_
#define _WLC_POWER_BRIDGE_H_

#include <stdint.h>
#include <stdbool.h>

/* ========================= 默认PWM配置值 ========================= */
/* PWM FSK响应类型 - 用于FSK调制的应答信号 */
#define WLC_PWM_FSK_ACK           (0xFF)  /* FSK确认应答 */
#define WLC_PWM_FSK_NAK           (0x00)  /* FSK否定应答 */
#define WLC_PWM_FSK_ND            (0x55)  /* FSK无数据 */

#define WLC_PWM_A11_DEFAULT_FREQ  (136)   /* 默认PWM频率值 */

/* 默认PWM相位偏移值（为周期的一半减2） */
#define WLC_PWM_A11_DEFAULT_PHASE  ((WLC_PWM_A11_DEFAULT_FREQ / 2) - 2)

/* ======================= PWM FSK响应类型枚举 ======================= */

/**
 * @brief PWM FSK响应类型
 * @note 用于无线充电协议中的FSK调制应答
 */
typedef enum
{
    wlc_pwm_fsk_response_ack,  /* 确认应答 - 表示接收成功 */
    wlc_pwm_fsk_response_nak,  /* 否定应答 - 表示接收失败 */
    wlc_pwm_fsk_response_nd    /* 无数据 - 表示无数据传输 */
} wlc_pwm_fsk_response_t;

/* ========================== 功率桥控制函数 ========================== */
// 停止PWM输出并重置为默认状态
void wlc_pwm_server_stop_output(uint8_t device);

// 启动半桥PWM输出
void wlc_pwm_server_start_half_bridge(uint8_t device, uint16_t arr, uint16_t pulse);

// 启动全桥PWM输出
void wlc_pwm_server_start_full_bridge(uint8_t device, uint16_t arr, uint16_t phase);

// 获取自动重载寄存器值
uint16_t wlc_pwm_server_get_arr(uint8_t device);

#endif