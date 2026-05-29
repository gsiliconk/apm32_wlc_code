#ifndef _WLC_LIB_H_
#define _WLC_LIB_H_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>


/* ===================== 函数指针类型定义 - ADC函数 ===================== */
// 获取ADC时钟频率
typedef uint32_t (*wlc_lib_adc_get_clock_t)(void);
// 切换ADC采集组
typedef void (*wlc_lib_adc_switch_group_t)(uint8_t device, uint8_t group);
// 获取指定ADC通道的采样值
typedef uint16_t (*wlc_lib_adc_get_value_t)(uint8_t device, uint8_t model, uint8_t channel);
// 获取连续ADC采集数据
typedef uint8_t (*wlc_lib_adc_get_continue_data_t)(uint8_t model, uint16_t *addr, uint32_t count);

/* ===================== 函数指针类型定义 - PWM函数 ===================== */
// 定义回调类型
typedef void (*wlc_lib_pwm_callback_t)(void);

// 获取PWM时钟频率
typedef uint32_t (*wlc_lib_pwm_get_clock_t)(void);
// 使能/禁止PWM输出
typedef void (*wlc_lib_pwm_enable_t)(uint8_t model, uint8_t en_dis);
// 注册PWM周期中断回调函数
typedef void (*wlc_lib_pwm_set_callback_t)(uint8_t model, wlc_lib_pwm_callback_t callback);
// 设置PWM频率
typedef void (*wlc_lib_pwm_set_arr_t)(uint8_t model, uint16_t arr);
// 获取PWM频率
typedef uint16_t (*wlc_lib_pwm_get_arr_t)(uint8_t model);
// 设置半桥1占空比
typedef void (*wlc_lib_pwm_set_half_bridge1_pulse_t)(uint8_t model, uint16_t pulse);
// 设置半桥2占空比
typedef void (*wlc_lib_pwm_set_half_bridge2_pulse_t)(uint8_t model, uint16_t pulse);
// 设置全桥相位偏移
typedef void (*wlc_lib_pwm_set_phase_t)(uint8_t model, uint16_t phase);

/* ===================== 函数指针类型定义 - ASK函数 ===================== */
// 使能/禁止ASK调制
typedef void (*wlc_lib_irq_enable_t)(uint8_t device, uint8_t endis);

/* ===================== 函数指针类型定义 - Uart函数 ===================== */
// 可变参数printf实现
typedef int (*wlc_lib_uart_printf_t)(char *fmt, va_list args);

/* ========================== API结构体定义 ========================== */
/**
 * @brief WLC库API结构体 - 包含所有硬件接口函数
 * 该结构体持有指向具体硬件实现的函数指针
 */
typedef struct wlc_lib_api
{
	/* ADC相关函数指针 */
	wlc_lib_adc_get_clock_t						wlc_lib_adc_get_clock;
	wlc_lib_adc_switch_group_t				wlc_lib_adc_switch_group;
	wlc_lib_adc_get_value_t						wlc_lib_adc_get_value;
	wlc_lib_adc_get_continue_data_t		wlc_lib_adc_get_continue_data;
	
	/* PWM相关函数指针 */
	wlc_lib_pwm_get_clock_t								wlc_lib_pwm_get_clock;
	wlc_lib_pwm_enable_t									wlc_lib_pwm_enable;
	wlc_lib_pwm_set_callback_t						wlc_lib_pwm_set_callback;
	wlc_lib_pwm_set_arr_t									wlc_lib_pwm_set_arr;
	wlc_lib_pwm_get_arr_t									wlc_lib_pwm_get_arr;
	wlc_lib_pwm_set_half_bridge1_pulse_t	wlc_lib_pwm_set_half_bridge1_pulse;
	wlc_lib_pwm_set_half_bridge2_pulse_t	wlc_lib_pwm_set_half_bridge2_pulse;
	wlc_lib_pwm_set_phase_t								wlc_lib_pwm_set_phase;
	
	/* ASK相关函数指针 */
	wlc_lib_irq_enable_t				wlc_lib_irq_enable;
	
	/* Printf函数指针 */
	wlc_lib_uart_printf_t				wlc_lib_uart_printf;
	
} wlc_lib_api_t;

/**
 * @brief 使用硬件API初始化WLC库
 * @param api 指向包含硬件函数的WLC API结构体的指针
 */
void wlc_lib_init(wlc_lib_api_t *api);

/**
 * @brief WLC库主处理函数 - 在主循环中调用
 */
void wlc_lib_handler(void);

/**
 * @brief 定时器中断处理函数 - 从1ms定时器ISR中调用
 */
void wlc_lib_timer_handle_int(void);

/**
 * @brief ASK解调中断处理函数 - 从ASK中断中调用
 * @param device 设备ID
 * @param ch 通道ID
 * @param pwidth 脉冲宽度，单位微秒
 */
void wlc_lib_ask_handle_int(uint8_t device, uint8_t ch, uint32_t pwidth);


/* ====================== 适配Tomato_wlc的FSK调制调用函数 ====================== */
uint16_t wlc_lib_pwm_get_freq(uint8_t model);
void wlc_lib_pwm_set_freq(uint8_t model, uint32_t freq);
#endif