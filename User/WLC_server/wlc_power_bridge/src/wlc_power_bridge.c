#include "wlc_power_bridge.h"
#include "wlc_lib_interal_functions.h"
/*
 * 文件: wlc_power_bridge.c
 * 描述: WLC功率桥实现 - PWM桥控制
 */

/* ======================= PWM服务器控制结构 ======================= */
/**
 * @brief PWM服务器控制参数结构体
 */
typedef struct 
{
    bool active;                           /* PWM输出激活标志 */
    uint16_t arr;                          /* 自动重载寄存器值 */
    uint16_t half_bridge1_pulse;           /* 半桥1脉冲宽度 */
    uint16_t half_bridge2_pulse;           /* 半桥2脉冲宽度 */
    uint16_t pwm_phase;                    /* PWM信号之间的相位偏移 */
} wlc_pwm_server_control_params_t;

/* PWM服务器控制参数（静态变量） */
static wlc_pwm_server_control_params_t wlc_pwm_server_data;

/**
 * @brief PWM中断回调函数
 * @note 交替更新半桥1和半桥2的控制参数，实现双极性调制
 * 
 * 工作原理：
 *   - 奇数次中断：更新频率和半桥1参数
 *   - 偶数次中断：更新半桥2参数并禁用输出
 *   
 * 时序图：
 *   中断1 ──────────────────────────────→ 中断2 ────────────────────────→ 中断3
 *         │                                 │                                       │
 *         ▼                                 ▼                                       ▼
 *   更新H1参数  ───────────────────→   更新H2参数  ──────────────→   更新H1参数
 *   更新freq                             更新phase                                  更新freq
 *   使能输出                             禁用输出                                   使能输出
 */
static void wlc_pwm_server_irq_callback(void)
{
	/* 检查PWM输出是否激活 */
	if (wlc_pwm_server_data.active)
	{
		/* 激活状态：更新频率和半桥1参数 */
		wlc_pwm_server_data.active = false;
		wlc_lib_pwm_set_arr(0, wlc_pwm_server_data.arr);
		wlc_lib_pwm_set_half_bridge1_pulse(0, wlc_pwm_server_data.half_bridge1_pulse);
		wlc_lib_pwm_set_phase(0, wlc_pwm_server_data.pwm_phase);
	}
	else
	{
		/* 非激活状态：更新半桥2参数并禁用输出 */
		wlc_lib_pwm_set_half_bridge2_pulse(0, wlc_pwm_server_data.half_bridge2_pulse);
		wlc_lib_pwm_enable(0, 0); // 关闭pwm中断
	}
}

/**
 * @brief 停止PWM输出并重置为默认状态
 * @param device 设备ID
 */
void wlc_pwm_server_stop_output(uint8_t device)
{
	/* 标记为非激活状态 */
	wlc_pwm_server_data.active = false;
    
	/* 禁用PWM输出 */
	wlc_lib_pwm_enable(device, 0);
    
	/* 清除所有PWM设置 */
	wlc_lib_pwm_set_half_bridge1_pulse(device, 0);
	wlc_lib_pwm_set_half_bridge2_pulse(device, 0);
	wlc_lib_pwm_set_arr(device, WLC_PWM_A11_DEFAULT_FREQ);
	wlc_lib_pwm_set_phase(device, WLC_PWM_A11_DEFAULT_PHASE);
}

/**
 * @brief 启动半桥PWM输出
 * @param device 设备ID
 * @param arr 自动重载寄存器值（决定PWM频率）
 * @param pulse 半桥1的脉冲宽度
 */
void wlc_pwm_server_start_half_bridge(uint8_t device, uint16_t arr, uint16_t pulse)
{
	/* 更新控制参数 */
	wlc_pwm_server_data.active = true;
	wlc_pwm_server_data.arr = arr;
	wlc_pwm_server_data.half_bridge1_pulse = pulse;
	wlc_pwm_server_data.half_bridge2_pulse = 0;           /* 半桥2禁用 */
	wlc_pwm_server_data.pwm_phase = (arr + 1) / 2;        /* 相位偏移为半周期 */

	wlc_lib_pwm_set_callback(device, wlc_pwm_server_irq_callback);
    
	/* 使能PWM输出 */
	wlc_lib_pwm_enable(device, 1);
}

/**
 * @brief 启动全桥PWM输出
 * @param device 设备ID
 * @param arr 自动重载寄存器值（决定PWM频率）
 * @param phase 全桥工作时的相位偏移值
 */
void wlc_pwm_server_start_full_bridge(uint8_t device, uint16_t arr, uint16_t phase)
{
	/* 更新控制参数 */
	wlc_pwm_server_data.active = true;
	wlc_pwm_server_data.arr = arr;

	/* 设置两个桥臂均为50%占空比 */
	wlc_pwm_server_data.half_bridge1_pulse = (arr + 1) / 2;
	wlc_pwm_server_data.half_bridge2_pulse = arr + 1 - wlc_pwm_server_data.half_bridge1_pulse;

	/* 计算带偏移量的相位偏移 */
	wlc_pwm_server_data.pwm_phase = wlc_pwm_server_data.half_bridge1_pulse - 2 + phase;

	wlc_lib_pwm_set_callback(device, wlc_pwm_server_irq_callback);

	/* 使能PWM输出 */
	wlc_lib_pwm_enable(device, 1);
}

/**
 * @brief 获取自动重载寄存器值
 * @param device 设备ID
 * @return 自动重载寄存器值（ARR）
 */
uint16_t wlc_pwm_server_get_arr(uint8_t device)
{
	return wlc_pwm_server_data.arr;
}