#include "wlc_lib.h"
#include "wlc_timer.h"
#include "wlc_physical.h"
#include "wlc_power_bridge.h"
#include "wlc_app.h"

/* 全局 API 结构体 - 存储硬件功能函数指针 */
static wlc_lib_api_t wlc_lib_api;

/* ====================== 初始化 ====================== */
/**
 * @brief 使用硬件 API 函数初始化 WLC 库
 * @param api 指向包含硬件实现函数的 API 结构体的指针
 */
void wlc_lib_init(wlc_lib_api_t *api)
{
	if(api != NULL)
	{
		/* 复制 API 结构体 */
		wlc_lib_api = *api;

		/* 初始化应用层 */
		wlc_app_init();
	}
}

/**
 * @brief WLC 主处理函数 - 从主循环中调用
 */
void wlc_lib_handler(void)
{
	/* 调用应用任务 */
	wlc_app_task();
}

/**
 * @brief 定时器中断处理函数 - 从 1ms 定时器中断中调用
 */
void wlc_lib_timer_handle_int(void)
{
	/* 更新定时器滴答计数器 */
	wlc_timer_isr_callback();
}

/**
 * @brief ASK 中断处理函数 - 从 ASK 解调中断中调用
 * @param device 设备 ID
 * @param ch 通道 ID
 * @param pwidth 脉冲宽度（微秒）
 */
void wlc_lib_ask_handle_int(uint8_t device, uint8_t ch, uint32_t pwidth)
{
	/* 调用物理层 ASK 解调处理函数 */
	wlc_ask_demod_handler(device, ch, pwidth);
}

/* ====================== ADC 函数 ====================== */

/**
 * @brief 获取 ADC 时钟频率
 * @return ADC 时钟频率，单位：Hz
 */
uint32_t wlc_lib_adc_get_clock(void)
{
	if(wlc_lib_api.wlc_lib_adc_get_clock != NULL)
	{
		return wlc_lib_api.wlc_lib_adc_get_clock();
	}
	return 0;
}

/**
 * @brief 切换 ADC 采样组
 * @param device 设备 ID
 * @param group ADC 模式值
 */
void wlc_lib_adc_switch_group(uint8_t device, uint8_t group)
{
	if(wlc_lib_api.wlc_lib_adc_switch_group != NULL)
	{
		wlc_lib_api.wlc_lib_adc_switch_group(device,group);
	}
}

/**
 * @brief 从指定通道获取 ADC 值
 * @param device 设备 ID
 * @param model ADC 模块
 * @param channel 通道 ID
 * @return ADC 值
 */
uint16_t wlc_lib_adc_get_value(uint8_t device, uint8_t model, uint8_t channel)
{
	if(wlc_lib_api.wlc_lib_adc_get_value != NULL)
	{
		return wlc_lib_api.wlc_lib_adc_get_value(device,model,channel);
	}
	return 0;
}

/**
 * @brief 获取连续 ADC 数据
 * @param model ADC 模块
 * @param addr 数据缓冲区指针
 * @param count 要获取的数据点数
 * @return 实际获取的数据点数
 */
uint8_t wlc_lib_adc_get_continue_data(uint8_t model, uint16_t *addr, uint32_t count)
{
	if(wlc_lib_api.wlc_lib_adc_get_continue_data != NULL)
	{
		return wlc_lib_api.wlc_lib_adc_get_continue_data(model,addr,count);
	}
	return 0;
}

/* ====================== PWM 函数 ====================== */

/**
 * @brief 获取 PWM 时钟频率
 * @return PWM 时钟频率，单位：Hz
 */
uint32_t wlc_lib_pwm_get_clock(void)
{
	if(wlc_lib_api.wlc_lib_pwm_get_clock != NULL)
	{
		return wlc_lib_api.wlc_lib_pwm_get_clock();
	}
	return 0;
}

/**
 * @brief 使能或禁用 PWM 输出
 * @param model PWM 模块 ID
 * @param en_dis 1=使能，0=禁用
 */
void wlc_lib_pwm_enable(uint8_t model, uint8_t en_dis)
{
	if(wlc_lib_api.wlc_lib_pwm_enable != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_enable(model,en_dis);
	}
}

/**
 * @brief 注册 PWM 中断回调函数
 * @param model PWM 模块 ID
 * @param callback PWM 中断回调函数指针
 */
void wlc_lib_pwm_set_callback(uint8_t model, wlc_lib_pwm_callback_t callback)
{
	if(wlc_lib_api.wlc_lib_pwm_set_callback != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_set_callback(model,callback);
	}
}

/**
 * @brief 设置 PWM 频率（ARR 寄存器值）
 * @param model PWM 模块 ID
 * @param arr 频率值（ARR 寄存器值）
 */
void wlc_lib_pwm_set_arr(uint8_t model, uint16_t arr)
{
	if(wlc_lib_api.wlc_lib_pwm_set_arr != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_set_arr(model,arr);
	}
}

/**
 * @brief 获取 PWM 频率（ARR 寄存器值）
 * @param model PWM 模块 ID
 * @return 当前 ARR 寄存器值
 */
uint16_t wlc_lib_pwm_get_arr(uint8_t model)
{
	if(wlc_lib_api.wlc_lib_pwm_get_arr != NULL)
	{
		return wlc_lib_api.wlc_lib_pwm_get_arr(model);
	}
	return 0;
}

/**
 * @brief 设置半桥 1 占空比
 * @param model PWM 模块 ID
 * @param pulse 占空比值
 */
void wlc_lib_pwm_set_half_bridge1_pulse(uint8_t model, uint16_t pulse)
{
	if(wlc_lib_api.wlc_lib_pwm_set_half_bridge1_pulse != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_set_half_bridge1_pulse(model,pulse);
	}
}

/**
 * @brief 设置半桥 2 占空比
 * @param model PWM 模块 ID
 * @param pulse 占空比值
 */
void wlc_lib_pwm_set_half_bridge2_pulse(uint8_t model, uint16_t pulse)
{
	if(wlc_lib_api.wlc_lib_pwm_set_half_bridge2_pulse != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_set_half_bridge2_pulse(model,pulse);
	}
}

/**
 * @brief 设置全桥相移
 * @param model PWM 模块 ID
 * @param phase 相移值
 */
void wlc_lib_pwm_set_phase(uint8_t model, uint16_t phase)
{
	if(wlc_lib_api.wlc_lib_pwm_set_phase != NULL)
	{
		wlc_lib_api.wlc_lib_pwm_set_phase(model,phase);
	}
}

/* ====================== ASK 函数 ====================== */

/**
 * @brief 使能或禁用 ASK 解调
 * @param device 设备 ID
 * @param endis 1=使能，0=禁用
 */
void wlc_lib_irq_enable(uint8_t device, uint8_t endis)
{
	if(wlc_lib_api.wlc_lib_irq_enable != NULL)
	{
		wlc_lib_api.wlc_lib_irq_enable(device,endis);
	}
}

/* ====================== 打印函数 ====================== */

/**
 * @brief 带可变参数的 printf 实现
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 打印的字符数
 */
int wlc_lib_printf(char *fmt, ...)
{
	if(wlc_lib_api.wlc_lib_uart_printf != NULL)
	{
		va_list args;
		va_start(args, fmt);
		int result = wlc_lib_api.wlc_lib_uart_printf(fmt, args);
		va_end(args);
		return result;
	}
	return 0;
}

/**
 * @brief UART 打印函数实现
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 打印的字符数
 */
int wlc_lib_uart_printf(char *fmt, ...)
{
	if(wlc_lib_api.wlc_lib_uart_printf != NULL)
	{
		va_list args;
		va_start(args, fmt);
		int result = wlc_lib_api.wlc_lib_uart_printf(fmt, args);
		va_end(args);
		return result;
	}
	return 0;
}

/* ====================== FSK 频率封装函数 ====================== */
/**
 * @brief 获取 PWM 频率（ARR 值）
 * @param model PWM 模块 ID
 * @return 当前 PWM 频率（ARR 值）
 */
uint16_t wlc_lib_pwm_get_freq(uint8_t model)
{
	return wlc_lib_pwm_get_arr(model);
}

/**
 * @brief 设置 PWM 频率（ARR 值）
 * @param model PWM 模块 ID
 * @param freq 频率值（将设置为 ARR 值）
 */
void wlc_lib_pwm_set_freq(uint8_t model, uint32_t freq)
{
	wlc_lib_pwm_set_arr(model, freq);
}