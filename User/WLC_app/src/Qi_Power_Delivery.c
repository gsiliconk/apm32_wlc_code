#include "Qi_Power_Delivery.h"
#include "wlc_power_bridge.h"
#include "wlc_lib_interal_functions.h"
#include "wlc_app.h"
/* 文件名: Qi_Power_Delivery.c
 * 描述: Qi 协议功率传输实现 - PWM 控制
 */

/* 计算百分比的辅助宏 */
#define PERCENT(val, pct)  ((val) * (pct) / 100)

/* Apple 设备专用控制参数结构体 */
typedef struct {
    uint16_t arr;              /* 自动重载值 (决定 PWM 频率) */
    uint16_t duty_steps;       /* 占空比调节步数 */
    uint16_t phase_steps;      /* 移相调节步数 */
} power_apple_control_t;

/* 功率控制结构体，用于跟踪 PWM 参数 */
typedef struct {
    uint16_t arr_max;                  /* 最大 ARR 值 (对应最低频率) */
    uint16_t arr_min;                  /* 最小 ARR 值 (对应最高频率) */
    uint16_t freq_steps;               /* 频率控制步数 */
    uint16_t duty_steps;               /* 占空比控制步数 */
    uint16_t phase_steps;              /* 移相控制步数 */
    uint32_t current_level;            /* 当前功率等级设置 */
    int8_t last_ce_value;              /* 上一次接收到的控制误差值 */
    bool power_on;                     /* 功率传输激活标志 */
    bool in_ping_phase;                /* Ping 阶段激活标志 */
    power_apple_control_t apple_ctrl;  /* Apple 设备控制参数 */
} power_control_t;

/* 每个设备的静态功率控制实例数组 */
static power_control_t pwr_ctrl[WLC_LIB_DEVICE_CONT];

/**
 * @brief 初始化功率传输模块
 * @param device 设备索引 (0-based)
 */
void qi_power_init(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	power_control_t *pc = &pwr_ctrl[device];
	uint32_t pwm_clk = wlc_lib_pwm_get_clock();  /* 获取当前 PWM 定时器时钟频率 */

	if(pwm_clk == 0)
		return;  /* 时钟无效则直接返回 */

	/* 计算频率控制参数—自动重装载寄存器（ARR） */
	pc->arr_max = pwm_clk / QI_FREQ_MIN;   /* 110kHz对应的ARR */
	pc->arr_min = pwm_clk / QI_FREQ_MAX;   /* 205kHz对应的ARR */
	pc->freq_steps = pc->arr_max - pc->arr_min + 1;  /* 频率可调步数 */

	/* 计算占空比控制参数—比较寄存器（CCR） */
	uint16_t duty_max_val = PERCENT(pc->arr_min, QI_DUTY_MAX);  /* 最大占空比对应的比较值 */
	uint16_t duty_min_val = PERCENT(pc->arr_min, QI_DUTY_MIN);  /* 最小占空比对应的比较值 */
	pc->duty_steps = duty_max_val - duty_min_val - 1;           /* 占空比可调步数 */

	/* 计算移相控制参数 */
	pc->phase_steps = PERCENT(pc->arr_min, QI_PHASE_SHIFT_MIN); /* 最小移相对应的步数 */

	/* Apple 设备专用控制参数计算 */
	pc->apple_ctrl.arr = (uint16_t)(pwm_clk / QI_FREQ_APPLE);
	duty_max_val = PERCENT(pc->apple_ctrl.arr, QI_DUTY_MAX);
	duty_min_val = PERCENT(pc->apple_ctrl.arr, QI_DUTY_MIN);
	pc->apple_ctrl.duty_steps = duty_max_val - duty_min_val - 1;
	pc->apple_ctrl.phase_steps = PERCENT(pc->apple_ctrl.arr, QI_PHASE_SHIFT_MIN);

	/* 初始化状态 */
	pc->current_level = 0;
	pc->last_ce_value = 0;
	pc->power_on = false;
	pc->in_ping_phase = false;
}

/**
 * @brief 设置数字 Ping 功率 (175kHz 标准频率)
 * @param device 设备索引
 * @note 用于 Qi 协议 Ping 阶段唤醒接收端
 */
void qi_power_set_digital_ping(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	power_control_t *pc = &pwr_ctrl[device];
	uint32_t pwm_clk = wlc_lib_pwm_get_clock();
	uint16_t arr = pwm_clk / QI_FREQ_PING;  /* 175kHz 对应的 ARR 值 */
    
	/* 根据输入电压状态选择全桥或半桥模式启动，占空比 50% */
	if(qi_app_get_vin_state(device) == QI_POWER_5V)
	{
		wlc_pwm_server_start_full_bridge(device, arr, 0);  /* 全桥，无移相 */
	}
	else
	{
		wlc_pwm_server_start_half_bridge(device, arr, (arr / 2)); /* 半桥，50% 占空比 */
	}
	pc->current_level = 70;   /* 默认功率等级 (经验值) */
	pc->power_on = true;
	pc->in_ping_phase = true; /* 标记 Ping 阶段 */
}

/**
 * @brief 关闭功率传输
 * @param device 设备索引
 */
void qi_power_off(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	power_control_t *pc = &pwr_ctrl[device];

	if(!pc->power_on)
		return;  /* 已关闭则无需操作 */
    
	/* 停止 PWM 输出 */
	wlc_pwm_server_stop_output(device);
	pc->current_level = 0;
	pc->power_on = false;

	/* 仅在非 Ping 阶段输出日志 (抑制 Ping 关闭时的重复消息) */
	if (!pc->in_ping_phase)
	{
		wlc_lib_uart_printf("[PWR] Power OFF\r\n");
	}
}

/**
 * @brief 设置功率等级，使用频率/移相/占空比组合控制
 * @param device 设备索引
 * @param level 功率等级 (0-255，数值越大功率越高)
 * @note 根据接收端类型选择不同的控制策略
 *       Apple 设备: 移相 -> 占空比
 *       其他设备: 频率 -> 移相 -> 占空比
 */
void qi_power_set_level(uint8_t device, uint32_t level)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
	
	uint16_t arr, phase,pulse; // 自动重载值,移相调节步数,占空比（CCR值）
	uint16_t max_level;
	const char *control_mode = "Unknown";
	power_control_t *pc = &pwr_ctrl[device];
    
	/* 等级为 0 表示关闭输出 */
	if(level == 0)
	{
		qi_power_off(device);
		return;
	}
    
	/* 苹果设备走独立分支 */
	if(qi_app_get_rxdevice(device) == QI_DEVICE_APPLE)
	{
		max_level = pc->apple_ctrl.phase_steps + pc->apple_ctrl.duty_steps;
		wlc_lib_uart_printf("[PWR] :QI_DEVICE_APPLE :%d  %d\r\n",level ,max_level);
		if(level > max_level)
			level = max_level;  /* 限制最大等级 */
		arr = pc->apple_ctrl.arr;   /* 固定频率 */

		/* 功率等级分配: 低段为移相控制，高段为占空比控制 */
		if(level <= pc->apple_ctrl.phase_steps)
		{
			phase = (level - 1);
			wlc_pwm_server_start_full_bridge(device, arr, phase); /* 全桥移相 */
		}
		else if(level <= max_level)
		{
			pulse = (arr + 1) / 2 - (level - pc->apple_ctrl.phase_steps);
			wlc_pwm_server_start_half_bridge(device, arr, pulse); /* 半桥占空比 */
		}
	}
	else
	{
		/* 标准 Qi 设备三段式控制 */
		max_level = pc->freq_steps + pc->phase_steps + pc->duty_steps;
		if(level > max_level)
			level = max_level;  /* 限制最大等级 */

		/* 第一段: 频率控制 (110~205kHz) */
		if (level <= pc->freq_steps)
		{
			arr = pc->arr_max + 1 - level;  /* level 越小频率越高 (arr 越小) */
			phase = 0;
			control_mode = "Frequency";
			wlc_pwm_server_start_full_bridge(device, arr, phase);
		}
		/* 第二段: 移相控制 */
		else if(level <= (pc->freq_steps + pc->phase_steps))
		{
			arr = pc->arr_min;               /* 固定为最高频率 */
			phase = level - pc->freq_steps;  /* 移相值随 level 增加 */
			control_mode = "Phase Shift";
			wlc_pwm_server_start_full_bridge(device, arr, phase);
		}
		/* 第三段: 占空比控制 */
		else
		{
			arr = pc->arr_min;               /* 固定最高频率 */
			pulse = (arr + 1) / 2 - (level - pc->freq_steps - pc->phase_steps);
			control_mode = "Duty Cycle";
			wlc_pwm_server_start_half_bridge(device, arr, pulse); /* 半桥占空比 */
		}
	}
    
	/* 更新状态 */
	pc->current_level = level;
	pc->power_on = true;
	pc->in_ping_phase = false;  /* 退出 Ping 阶段 */
}

/**
 * @brief 根据控制误差 (CE) 包调整功率
 * @param device 设备索引
 * @param control_error 控制误差值
 * @note 实现自适应功率调节算法，包含:
 *       - 长时间零误差补偿 (根据设备类型设置不同容忍次数)
 *       - 符号变化减速 (防止震荡)
 *       - 误差量化 (根据输入电压划分步长)
 *       - 最终通过 qi_power_set_level 生效
 */
void qi_power_adjust_ce(uint8_t device, int8_t control_error)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	power_control_t *pc = &pwr_ctrl[device];
	static int8_t last_error = 0;           /* 上一次 CE 原始值 */
	static uint8_t zero_count = 0;          /* 连续零 CE 计数器 */
	static uint32_t ce_adjust_count = 0;    /* 调试用调节次数计数器 */

	int8_t ce = control_error;
	bool increase = false;
	int8_t original_ce = ce;
    
	/* 处理控制误差为零的情况 */
	if (ce == 0)
	{
		if(qi_app_get_rxdevice(device) == QI_DEVICE_APPLE)
		{
			/* Apple 设备容错较高，连续 40 次零误差才主动降低功率 */
			if (++zero_count > 40)
			{
				ce = -1;
				zero_count = 0;
			}
		}
		else
		{
			/* 标准设备连续 20 次零误差则降低功率 */
			if(++zero_count > 20)
			{
				ce = -1;
				zero_count = 0;
			}
			else
			{
				return;  /* 保持当前功率 */
			}
		}   
	}
	else
	{
		zero_count = 0;  /* 非零时清除计数器 */
        
		/* 检测误差符号变化 (异或判断符号位) */
		if(last_error != 0 && ((ce ^ last_error) & 0x80))
		{
			/* 符号翻转时减小调节幅度，防止振荡 */
			ce = (ce > 0) ? 1 : -1;
		}
	}
    
	last_error = control_error;  /* 保存本次原始值供下次比较 */
    
	/* 确定调整方向 */
	if(ce < 0)
	{
		increase = true;  /* 负 CE 表示接收端电压偏低，需增加功率 */
		ce = -ce;
	}

	int8_t ce_step = ce;  /* 原始步长 (调试用) */

	/* 根据输入电压状态对误差进行量化 (避免微小波动频繁调节) */
	if(qi_app_get_vin_state(device) == QI_POWER_5V)
	{
		if(ce > 8)
		{
			ce = 6;       /* 大误差快速响应 */
		}
		else if(ce > 4)
		{
			ce = 3;       /* 中等误差 */
		}
		else if(ce >= 1)
		{
			ce = 1;       /* 小步长微调 */
		}
	}
	else
	{
		/* 9V 模式下步长稍大 (因为功率变化更剧烈) */
		if(ce > 12)
		{
			ce = 6;
		}
		else if(ce > 6)
		{
			ce = 3;
		}
		else if(ce >= 1)
		{
			ce = 1;
		}
	}
    
	/* 计算新的功率等级 */
	uint32_t new_level = pc->current_level;
	uint32_t old_level = pc->current_level;
    
	if(increase)
	{
		new_level += ce;  /* 增加功率 */
	}
	else
	{
		new_level = (new_level > ce) ? (new_level - ce) : 1;  /* 减小功率，不低于 1 */
	}
    
	/* 仅当等级确实变化时才调用设置函数 */
	if(new_level != pc->current_level)
	{
		qi_power_set_level(device, new_level);

		ce_adjust_count++;
        
		/* 调试日志 (已注释) */
		wlc_lib_uart_printf("[CE] #%d - Raw: %d, Step: %d, Dir: %s, Level: %d->%d, Quant: %d\r\n",
												ce_adjust_count, original_ce, ce_step,
												increase ? "Inc" : "Dec",
												old_level, new_level, ce);
        
		pc->last_ce_value = original_ce;
	}
}