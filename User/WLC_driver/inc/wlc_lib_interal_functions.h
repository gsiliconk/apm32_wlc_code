#ifndef _WLC_LIB_INTERAL_FUNCTIONS_H_
#define _WLC_LIB_INTERAL_FUNCTIONS_H_
/*
 * File: wlc_lib_interal_functions.h
 * Description: WLC 库内部函数声明 - 硬件抽象层接口
 */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "wlc_lib.h"

/* ====================== ADC 函数 - 硬件抽象层接口 ====================== */
// 获取 ADC 时钟频率
uint32_t wlc_lib_adc_get_clock(void);
// 切换ADC采集组
void wlc_lib_adc_switch_group(uint8_t device, uint8_t group);
// 获取指定ADC通道的采样值
uint16_t wlc_lib_adc_get_value(uint8_t device, uint8_t model, uint8_t channel);
// 获取连续ADC采集数据
uint8_t wlc_lib_adc_get_continue_data(uint8_t model, uint16_t *addr, uint32_t count);

/* ====================== PWM 函数 - 硬件抽象层接口 ====================== */
// 获取PWM时钟频率
uint32_t wlc_lib_pwm_get_clock(void);
// 使能/禁止PWM输出
void wlc_lib_pwm_enable(uint8_t model, uint8_t en_dis);
// 注册PWM周期中断回调函数
void wlc_lib_pwm_set_callback(uint8_t model, wlc_lib_pwm_callback_t callback);
// 设置PWM频率
void wlc_lib_pwm_set_arr(uint8_t model, uint16_t arr);
// 获取PWM频率
uint16_t wlc_lib_pwm_get_arr(uint8_t model);
// 设置半桥1占空比
void wlc_lib_pwm_set_half_bridge1_pulse(uint8_t model, uint16_t pulse);
// 设置半桥2占空比
void wlc_lib_pwm_set_half_bridge2_pulse(uint8_t model, uint16_t pulse);
// 设置全桥相位偏移
void wlc_lib_pwm_set_phase(uint8_t model, uint16_t phase);

/* ====================== ASK 调制/解调函数 - 硬件抽象层接口 ====================== */
// 使能或禁止ASK解调
void wlc_lib_irq_enable(uint8_t device, uint8_t endis);

/* ====================== Printf 函数 - 硬件抽象层接口 ====================== */
// 输出接口，支持可变参数
int wlc_lib_uart_printf(char *fmt, ...);

#endif