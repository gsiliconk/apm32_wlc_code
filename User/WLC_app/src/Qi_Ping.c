#include "Qi_Ping.h"
#include "Qi_Power_Delivery.h"
#include "wlc_timer.h"
#include "wlc_physical.h"
#include "wlc_lib_interal_functions.h"
/* 文件: Qi_Ping.c
 * 描述: Qi协议Ping阶段实现 - 设备检测
 */

/* =========================== Ping模块控制结构 =========================== */

typedef struct {
    ping_state_t state;                /* 当前状态机状态 */
    uint32_t timer_id;                 /* 脉冲时序定时器ID */
    bool active;                       /* 模块激活标志 */
    uint32_t pulse_count;              /* 已传输的脉冲数 */
    uint32_t power_level;              /* 频率功率等级（用户可调） */
    uint32_t sig_strength;             /* 最后接收到的信号强度 */
    uint32_t phase_start_time;         /* 阶段开始时间戳（毫秒） */
    
    /* ===== 已修复：基于周期的时序而非基于状态 ===== */
    uint32_t cycle_start_time;         /* 当前周期开始时间 */
    uint32_t next_pulse_time;          /* 下一个脉冲开始的绝对时间 */
    uint32_t pulse_end_time;           /* 脉冲应该结束的绝对时间 */
    
    /* 统计数据 */
    uint32_t max_timing_error_ms;      /* 检测到的最大时序误差 */
    uint32_t total_timing_errors;      /* 总时序误差数 */
} ping_module_t;

/* 每个设备的静态Ping模块实例 */
static ping_module_t ping_mod[WLC_LIB_DEVICE_CONT];

/* =========================== 静态辅助函数 =========================== */

/**
 * @brief 获取人类可读的Ping状态名称
 * @param state Ping状态枚举
 * @return 状态名称字符串指针
 */
static const char* get_ping_state_name(ping_state_t state)
{
	static const char* state_names[] =
	{
			"Free",
			"Pulse On",
			"Pulse Off",
			"Device Detected"
	};

	if(state < sizeof(state_names) / sizeof(state_names[0]))
	{    /* 枚举值直接做下标，越界返回UNKNOWN */
		return state_names[state];
	}
	return "Unknown";
}

/**
 * @brief 转换到新的Ping状态
 * @param device 设备ID
 * @param new_state 要转换到的新状态
 */
static void ping_set_state(uint8_t device, ping_state_t new_state)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	ping_module_t *pm = &ping_mod[device];

	if(pm->state != new_state)
	{    
		/* 只在状态真正改变时才切换，避免重复触发 */
		uint32_t now = wlc_timer_get_timestamp();
		// wlc_lib_printf("[PING] [%dms] State: %s -> %s\r\n",
		//               now,
		//               get_ping_state_name(pm->state),
		//               get_ping_state_name(new_state));
		pm->state = new_state;
	}
}

/**
 * @brief 检查时序误差
 * @param device 设备ID
 * @param expected_time 期望时间
 * @param actual_time 实际时间
 */
static void check_timing_error(uint8_t device, uint32_t expected_time,
                               uint32_t actual_time)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	ping_module_t *pm = &ping_mod[device];

	int32_t error = (int32_t)actual_time - (int32_t)expected_time;    /* 有符号相减，得到正负误差 */
	uint32_t abs_error = (error < 0) ? -error : error;    /* 取绝对值 */

	/* Log if error > 2ms */
	if(abs_error > 2)
	{
		uint32_t now = wlc_timer_get_timestamp();
		// wlc_lib_printf("[PING] [%dms] ?? TIMING ERROR: Expected %dms, Actual %dms, Error: %+dms\r\n",
		//               now, expected_time, actual_time, error);

		pm->total_timing_errors++;
		if(abs_error > pm->max_timing_error_ms)
		{
			pm->max_timing_error_ms = abs_error;
		}
	}
}

/* =========================== 初始化和控制 =========================== */
/**
 * @brief 初始化指定设备的Ping模块
 * @param device 设备ID（0起始索引）
 */
void qi_ping_init(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
    
	pm->state = PING_STATE_IDLE;
	pm->timer_id = 0;
	pm->active = false;
	pm->pulse_count = 0;
	pm->power_level = PING_POWER_LEVEL;
	pm->sig_strength = 0;
	pm->phase_start_time = 0;
    
	/* 初始化周期时序 */
	pm->cycle_start_time = 0;
	pm->next_pulse_time = 0;
	pm->pulse_end_time = 0;
    
	/* 初始化统计数据 */
	pm->max_timing_error_ms = 0;
	pm->total_timing_errors = 0;
    
	// wlc_lib_printf("[PING] Module initialized for device %d\r\n", device);
}

/**
 * @brief 在启动前设置Ping频率功率等级
 * @param device 设备ID（0起始索引）
 * @param level 频率等级（30-85，值越低频率越低）
 */
void qi_ping_set_power_level(uint8_t device, uint32_t level)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
    
	/* 验证功率等级范围 */
	if(level < 30 || level > 85)
	{
		// wlc_lib_printf("[PING] [Err] Invalid power level: %d (valid range: 30-85)\r\n", level);
		return;
	}
    
	pm->power_level = level;
	// wlc_lib_printf("[PING] Power level set to: %d\r\n", level);
}

/**
 * @brief 启动Ping阶段 - 启用ASK并开始脉冲传输
 * @param device 设备ID（0起始索引）
 */
void qi_ping_start(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	/* 启用ASK解调以接收设备响应 */
	wlc_ask_demod_enable(device, 1);    /* 先开接收，再开功率，防止手机回包时ASK还未就绪 */
    
	/* 设置默认数字Ping（根据Qi v1.3 表11为175kHz） */
	qi_power_set_digital_ping(device);
    
	/* 应用频率等级调整 */
	//qi_power_set_level(device, pm->power_level);
    
	/* 初始化状态机 */
	ping_set_state(device, PING_STATE_PULSE_ON);
	pm->timer_id = wlc_timer_create(PING_PULSE_ON_MS);
	pm->active = true;
	pm->pulse_count = 1;    /* 第一个脉冲已经开始发了，从1计数 */
	pm->phase_start_time = now;
    
	/* 初始化周期时序 - 关键步骤 */
	pm->cycle_start_time = now;
	pm->pulse_end_time = now + PING_PULSE_ON_MS;    /* 绝对时间：68ms后关功率 */
	pm->next_pulse_time = now + PING_CYCLE_MS;    /* 绝对时间：500ms后开下一个脉冲 */
    
	// wlc_lib_printf("[PING] ========== PING Phase Start ==========\r\n");
	// wlc_lib_printf("[PING] [%dms] Pulse 1: ON starting (t1), Level: %d\r\n", 
	//               now, pm->power_level);
	// wlc_lib_printf("[PING] Cycle: ON=%dms, OFF=%dms, Total=%dms\r\n", 
	//               PING_PULSE_ON_MS, PING_PULSE_OFF_MS, PING_CYCLE_MS);
	// wlc_lib_printf("[PING] Max pulses: %d, Phase timeout: %dms\r\n",
	//               PING_MAX_PULSES, PING_TIMEOUT_MS);
}

/**
 * @brief 停止Ping阶段 - 禁用传输和ASK
 * @param device 设备ID（0起始索引）
 */
void qi_ping_stop(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	if(!pm->active)
		return;
    
	/* 如果定时器处于活动状态则销毁 */
	if(pm->timer_id)
	{
		wlc_timer_destroy(pm->timer_id);
		pm->timer_id = 0;
	}
    
	/* 关闭功率传输 */
	qi_power_off(device);
    
	/* 禁用ASK解调 */
	wlc_ask_demod_enable(device, 0);
    
	/* 重置状态机 */
	ping_set_state(device, PING_STATE_IDLE);
	pm->active = false;
    
	// wlc_lib_printf("[PING] ========== PING Phase Stop ==========\r\n");
	// wlc_lib_printf("[PING] [%dms] Total Pulses Sent: %d\r\n", now, pm->pulse_count);
    
	/* 记录时序统计数据 */
	if(pm->total_timing_errors > 0)
	{
		// wlc_lib_printf("[PING] TIMING STATS: Errors=%d, Max Error=%dms\r\n",
		//               pm->total_timing_errors, pm->max_timing_error_ms);
	}
}

/**
 * @brief 停止Ping阶段但保持电源开启以便下一阶段使用
 * @param device 设备ID（0起始索引）
 */
void qi_ping_stop_keep_power(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	ping_module_t *pm = &ping_mod[device];
	uint32_t now = wlc_timer_get_timestamp();

	if(!pm->active)
		return;

	/* 如果定时器处于活动状态则销毁 */
	if(pm->timer_id)
	{
		wlc_timer_destroy(pm->timer_id);
		pm->timer_id = 0;
	}

	/* 重置状态机但保持电源开启 */
	ping_set_state(device, PING_STATE_IDLE);
	pm->active = false;

	// wlc_lib_printf("[PING] ========== PING Stop (Power Kept ON) ==========\r\n");
	// wlc_lib_printf("[PING] [%dms] Total Pulses Sent: %d\r\n", now, pm->pulse_count);
    
	/* 记录时序统计数据 */
	if(pm->total_timing_errors > 0)
	{
		// wlc_lib_printf("[PING] TIMING STATS: Errors=%d, Max Error=%dms\r\n",
		//               pm->total_timing_errors, pm->max_timing_error_ms);
	}
}

/* =========================== 数据包处理 - 基于周期的时序 =========================== */

/**
 * @brief 处理Ping阶段 - 管理脉冲时序和检测设备
 * @param device 设备ID（0起始索引）
 * @param packet 接收到的数据包指针
 * @return 如果收到SIG数据包（检测到设备）返回true，否则返回false
 */
bool qi_ping_process(uint8_t device, const qi_packet_t *packet)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return false;
    
	ping_module_t *pm = &ping_mod[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	if(!pm->active)
		return false;
    
	/* ====== 数据包接收处理 ====== */
	if(packet && packet->valid)
	{
		if(packet->header == QI_PKT_SIG)
		{
			pm->sig_strength = packet->data[0];    /* 存信号强度值，IdConfig阶段会用到 */
			// wlc_lib_printf("[SIG] [%dms] Signal Strength Received: %d (0x%02X)\r\n", 
			//               now, pm->sig_strength, pm->sig_strength);
			ping_set_state(device, PING_STATE_DEVICE_DETECTED);
			return true;
		}
		else if(packet->header == QI_PKT_EPT)
		{
			uint8_t ept_code = packet->data[0];
			// wlc_lib_printf("[PING] [%dms] [Err] EPT received during ping (code: 0x%02X)\r\n", 
			//               now, ept_code);
			return false;
		}
		else
		{
			// wlc_lib_printf("[PING] [%dms] [Err] Illegal packet type during ping: 0x%02X\r\n", 
			//               now, packet->header);
			return false;
		}
	}
	/* ====== 基于周期的时序状态机 ====== */
	switch(pm->state)
	{
		case PING_STATE_PULSE_ON:
			/* 检查脉冲开启持续时间是否已到期 */
			if(now >= pm->pulse_end_time)
			{
				/* 到了关功率时刻 */
				/* 立即关闭功率 */
				qi_power_off(device);

				// wlc_lib_printf("[PING] [%dms] Pulse %d: ON completed (%dms)\r\n", 
				//               now, pm->pulse_count, PING_PULSE_ON_MS);

				/* 转换到关闭状态 */
				ping_set_state(device, PING_STATE_PULSE_OFF);
			}
		break;
		case PING_STATE_PULSE_OFF:
			/* 检查是否到了下一个脉冲的时间 */
			if(now >= pm->next_pulse_time)
			{
				/* 到了开下一个脉冲的时刻 */
				/* 开始下一个脉冲 */
				pm->pulse_count++;

				/* 检查Ping超时 */
				uint32_t elapsed_ms = now - pm->phase_start_time;

				if(pm->pulse_count > PING_MAX_PULSES)
				{
					// wlc_lib_printf("[PING] [%dms] [Err] Max pulses exceeded: %d > %d\r\n", 
					//               now, pm->pulse_count, PING_MAX_PULSES);
					return false;
				}
				
				if(elapsed_ms > PING_TIMEOUT_MS)
				{
					// wlc_lib_printf("[PING] [%dms] [Err] Ping phase timeout: %dms > %dms\r\n", 
					//               now, elapsed_ms, PING_TIMEOUT_MS);
					return false;
				}
                
				/* 立即开始下一个脉冲 */
				qi_power_set_digital_ping(device);
				//qi_power_set_level(device, pm->power_level);
				
				// wlc_lib_printf("[PING] [%dms] Pulse %d: ON starting | Cycle: 500ms | Elapsed: %dms\r\n", 
				//               now, pm->pulse_count, elapsed_ms);
                
				/* 返回开启状态 */
				ping_set_state(device, PING_STATE_PULSE_ON);
                
				/* 为下一个脉冲更新周期时序 - 关键步骤 */
				pm->cycle_start_time = now;
				pm->pulse_end_time = now + PING_PULSE_ON_MS;
				pm->next_pulse_time = now + PING_CYCLE_MS;    /* 从实际触发时刻推算，防止误差累积 */
                
				/* 销毁旧定时器并创建新定时器 */
				if(pm->timer_id)
				{
					wlc_timer_destroy(pm->timer_id);
				}
				pm->timer_id = wlc_timer_create(PING_PULSE_ON_MS);
			}
		break;
		case PING_STATE_DEVICE_DETECTED:
			/* 设备已检测到，等待阶段转换 */
		break;
		default:
			ping_set_state(device, PING_STATE_IDLE);
		break;
	}
	return false;
}

/**
 * @brief 获取当前Ping状态
 * @param device 设备ID（0起始索引）
 * @return 当前Ping状态
 */
ping_state_t qi_ping_get_state(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return PING_STATE_IDLE;
	return ping_mod[device].state;
}

/**
 * @brief 获取最后接收到的信号强度
 * @param device 设备ID（0起始索引）
 * @return 最后SIG数据包的强度值
 */
uint32_t qi_ping_get_signal_strength(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return 0;
	return ping_mod[device].sig_strength;
}

/**
 * @brief 获取Ping时序统计数据
 * @param device 设备ID（0起始索引）
 * @param max_error 用于存储最大时序误差的指针
 * @param total_errors 用于存储总误差数的指针
 */
void qi_ping_get_timing_stats(uint8_t device, uint32_t *max_error, 
                              uint32_t *total_errors)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
    
	if(max_error) *max_error = pm->max_timing_error_ms;
	if(total_errors) *total_errors = pm->total_timing_errors;
}

/**
 * @brief 重置Ping时序统计数据
 * @param device 设备ID（0起始索引）
 */
void qi_ping_reset_timing_stats(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	ping_module_t *pm = &ping_mod[device];
	pm->max_timing_error_ms = 0;
	pm->total_timing_errors = 0;
    
	// wlc_lib_printf("[PING] Timing statistics reset for device %d\r\n", device);
}