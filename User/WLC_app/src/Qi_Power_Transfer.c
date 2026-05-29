#include "Qi_Power_Transfer.h"
#include "Qi_Power_Delivery.h"
#include "wlc_timer.h"
#include "wlc_lib_interal_functions.h"
#include "Tomato_wlc.h"
/* 文件: Qi_Power_Transfer.c
 * 描述: Qi协议功率传输阶段实现
 */
 
/* ================================ 功率传输阶段状态机 ================================ */
typedef enum {
	PT_STATE_IDLE = 0,						// 空闲 - 未在传输
	PT_STATE_ACTIVE,							// 活跃 - 正在功率传输
	PT_STATE_ERROR								// 错误
}pt_state_t;

typedef enum {
	PT_CALIBRATION = 0,						// 校准阶段（EPP专属）
	PT_POWER_TRANSFER							// 正常功率传输
}pt_phase_t;

/* ================================ 功率传输模块控制结构 ================================ */
typedef struct {
	// === 状态控制 ===
	pt_state_t state;										// 模块状态
	bool active;												// 模块激活标志
	pt_phase_t phase;										// 子阶段
	// === CE 控制误差 ===
	int8_t control_error;								// 当前 CE 值
	bool control_error_packet_flag;			// CE 包到达标志
	// === CE Holdoff 机制 ===
	uint32_t ce_timerout;								// CE 超时计数器
	uint32_t power_control_timeout;			// 功率控制总超时
	uint32_t power_control_time;				// 功率控制当前计时
	uint8_t power_control_holdoff;			// Holdoff 延迟 (来自 PCH 包)
	// === RP 监控 ===
	uint32_t rx_power_packet_timerout;	// RP 包超时计数器
	// === 校准标志 ===
	bool light_load_flag;								// 轻载校准完成标志
	bool heavy_load_flag;								// 重载校准完成标志
	// === FOD ===
	uint8_t ploss_cnt;									// 功率损耗连续超限计数
	// === 响应 ===
	qi_response_t response;							// 待发送的响应类型
	// === 配置参数 ===
	uint16_t reference_power_w;					// 参考功率（瓦特）
	uint16_t window_size_ms;						// 功率控制窗口大小（毫秒）
	uint16_t window_offset_ms;					// 功率控制窗口偏移（毫秒）
	// === 统计 ===
	qi_pt_stats_t stats;								// 统计信息跟踪
	uint32_t last_ce_timestamp;					// 最近CE数据包时间戳
	uint32_t last_rp_timestamp;					// 最近RP数据包时间戳
}pt_module_t;

/* 每个设备的静态功率传输模块实例 */
static pt_module_t pt_mod[WLC_LIB_DEVICE_CONT];

/* ================================ 静态辅助函数 ================================ */
/**
 * @brief 获取可读的EPT原因字符串
 * @param code EPT原因代码
 * @return 指向原因字符串的指针
 */
static const char* get_ept_reason(uint8_t code)
{
	static const char* ept_reasons[] =
	{
		"Unknown/Empty",						/* 0x00 */
		"Charging Complete",				/* 0x01 */
		"Internal Error",						/* 0x02 */
		"Over-temperature",					/* 0x03 */
		"Over-voltage",							/* 0x04 */
		"Over-current",							/* 0x05 */
		"Battery Error",						/* 0x06 */
		"Reserved(0x07)",						/* 0x07 */
		"No Reserved",							/* 0x08 */
		"Reserved(0x09)",						/* 0x09 */
		"Abort Negotiation",				/* 0x0A */
		"Restart",									/* 0x0B */
		"Re-ping",									/* 0x0C */
		"RFID/NFC Card"							/* 0x0D */
	};
    
	if(code < sizeof(ept_reasons) / sizeof(ept_reasons[0]))
	{
		return ept_reasons[code];
	}
	return "Unknown Reason";
}

/**
 * @brief 设置功率传输状态
 * @param device 设备ID
 * @param new_state 新状态
 */
static void pt_set_state(uint8_t device, pt_state_t new_state)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	pt_module_t *pt = &pt_mod[device];

	/* 检查状态是否真的发生变化 */
	if(pt->state != new_state)
	{
		const char* state_names[] = {"IDLE", "ACTIVE", "ERROR"};
		const char* old_name = (pt->state < 3) ? state_names[pt->state] : "UNKNOWN";
		const char* new_name = (new_state < 3) ? state_names[new_state] : "UNKNOWN";

		wlc_lib_uart_printf("[PT] Status: %s -> %s\r\n", old_name, new_name);
		pt->state = new_state;
	}
}

/**
 * @brief PT响应中断使能回调函数
 * 
 * PT协议响应完成后的中断重启回调函数。
 * 用于在响应发送后、超时结束后,
 * 重新启用接收中断以处理下一个数据包。
 */
static void qi_pt_response_callback(void)
{
	wlc_lib_irq_enable(0, 1);
}

/**
 * @brief 检测功率损失是否触发FOD
 * @param window_size 滑动窗口大小
 * @param offset      滑动窗口偏移量
 * @param rx_power    接收功率(mW)
 * @return true=检测到FOD, false=未检测到FOD
 */
static bool qi_pt_ploss_fod_discern(uint8_t window_size, uint8_t offset, uint32_t rx_power)
{
	bool ret = false;		// 默认返回false（未检测到FOD）
	int16_t ploss_est;	// 估计的功率损失
	int16_t ploss_thr;	// 功率损失阈值
	uint32_t tx_power;	// 发射功率
	pt_module_t *pt = &pt_mod[0];	// 获取PT模块的指针
	uint32_t now = wlc_timer_get_timestamp();	// 获取当前时间戳
	/* 获取校准后的发射功率（使用滑动窗口平均）*/
	tx_power = tomato_calibration_get_aligned_power(now, window_size, offset);
	/* 计算功率损失阈值 */
	ploss_thr = tomato_calibration_calculate_ploss(rx_power);
	/* 计算估计的功率损失（发射功率 - 接收功率） */
	ploss_est = tx_power - rx_power;
	/* 判断功率损失是否超过阈值 */
	if(ploss_est > ploss_thr)
	{
		pt->ploss_cnt++;	// 增加功率损失计数器
		if(pt->ploss_cnt >= 3)
		{
			pt->ploss_cnt = 0;
			ret = true;	// 返回true表示检测到FOD
		}
	}

	wlc_lib_uart_printf("Ploss_est = %d, Ploss_thr = %d pFod counte :%d", ploss_est, ploss_thr, pt->ploss_cnt);
	return ret;
}

/**
 * @brief 解析24位功率数据包并执行相应协议操作
 * 
 * 该函数解析PT（电力传输）协议中的24位功率数据包，根据当前设备阶段和
 * 功率测量类型执行相应的响应操作，包括校准和FOD检测。
 * 
 * @param device      设备编号
 * @param packet      接收到的数据包指针
 * @param max_power   最大功率值（用于功率计算）
 * @param window_size 滑动窗口大小
 * @param offset      滑动窗口偏移量
 * 
 * @return bool 返回true表示检测到FOD，返回false表示未检测到FOD
 * 
 * @note 该函数会修改PT模块的状态、阶段和响应信息
 */
static bool qi_pt_parse_power_24bit(uint8_t device, const qi_packet_t *packet, uint32_t max_power, uint8_t window_size, uint8_t offset)
{
	int16_t tx_power;					// 发射功率（未校准）
	uint32_t rp_raw;					// 接收功率原始值
	uint32_t rpPower;					// 接收功率（计算后，单位：mW）
	bool pfod_flag = false;		// FOD标志
	uint32_t now = wlc_timer_get_timestamp();		// 获取当前时间戳
	pt_module_t *pt = &pt_mod[device];					// 获取指定设备的PT模块指针

	/* 解析24位接收功率数据（从数据包的data[1]和data[2]字段） */
	rp_raw = (packet->data[1] << 8) | packet->data[2];
	/* 将原始值转换为实际功率值（mW） */
	rpPower = ((rp_raw * 1000) / 32768 * max_power);

	/* 根据当前阶段执行不同操作 */
	if(pt->phase == PT_POWER_TRANSFER)	// 正常功率传输阶段
	{
		switch((packet->data[0] & 0x7)) // 提取低3位作为数据包类型
		{
			case QI_RX_POWER_0:	// 实际接收功率
				/* 执行FOD检测 */
				pfod_flag = qi_pt_ploss_fod_discern(window_size, offset, rpPower);
				/* 设置ACK响应 */
				pt->response = QI_RESPONSE_ACK;
				/* 初始化响应定时器，设置回调函数 */
				qi_response_init(device, pt->response, QI_T_RESPONSE_TIMEOUT, qi_pt_response_callback);
			break;
			case QI_RX_POWER_1:	// 轻载校准点
			case QI_RX_POWER_2:	// 重载校准点
				/* 设置ND（无数据）响应 */
				pt->response = QI_RESPONSE_ND;
				/* 初始化响应定时器，设置回调函数 */
				qi_response_init(device, pt->response, QI_T_RESPONSE_TIMEOUT, qi_pt_response_callback);
			break;
			case QI_RX_POWER_4:	// 校准接收功率
				/* 执行FOD检测 */
				pfod_flag = qi_pt_ploss_fod_discern(window_size, offset, rpPower);
			break;
		}
	}
	else	// 校准阶段（非功率传输阶段）
	{
		switch((packet->data[0] & 0x7))
		{
			case QI_RX_POWER_0:	// 实际接收功率
				/* 保存校准点数据 */
				tomato_calibration_points_data(QI_RX_POWER_0, tx_power, rpPower);
				/* 执行FOD检测 */
				pfod_flag = qi_pt_ploss_fod_discern(window_size, offset, rpPower);
				/* 切换到功率传输阶段 */
				pt->phase = PT_POWER_TRANSFER;
				/* 设置ACK响应 */
				pt->response = QI_RESPONSE_ACK;
				/* 初始化响应定时器 */
				qi_response_init(device, pt->response, QI_T_RESPONSE_TIMEOUT, qi_pt_response_callback);
			break;
			case QI_RX_POWER_1:	// 轻载校准点
				/* 使用滑动窗口获取Tx平均功率 */
				tx_power = tomato_calibration_get_aligned_power(now, window_size, offset);

				/* 保存校准点数据并检查是否成功 */
				if(tomato_calibration_points_data(QI_RX_POWER_1, tx_power, rpPower))
				{
					pt->light_load_flag = true;	// 设置轻载校准标志
					pt->response = QI_RESPONSE_ACK;	// 设置ACK响应
					wlc_lib_uart_printf("Light load calibration successful\r\n");
				}
				else
				{
					pt->response = QI_RESPONSE_NAK;	// 设置NAK（否定应答）响应
				}
				/* 初始化响应定时器 */
				qi_response_init(device, pt->response, QI_T_RESPONSE_TIMEOUT, qi_pt_response_callback);
			break;
			case QI_RX_POWER_2:	// 重载校准点
				/* 使用滑动窗口获取Tx平均功率 */
				tx_power = tomato_calibration_get_aligned_power(now, window_size, offset);
				/* 检查轻载校准是否完成，且重载校准是否成功 */
				if((pt->light_load_flag == true) && (tomato_calibration_points_data(QI_RX_POWER_2, tx_power, rpPower) == true))
				{
					pt->heavy_load_flag = true;	// 设置重载校准标志
					pt->response = QI_RESPONSE_ACK;	// 设置ACK响应
					wlc_lib_uart_printf("Heavy load calibration successful\r\n");
				}
				else
				{
					pt->response = QI_RESPONSE_NAK;	// 设置NAK（否定应答）响应
				}
				/* 初始化响应定时器 */
				qi_response_init(device, pt->response, QI_T_RESPONSE_TIMEOUT, qi_pt_response_callback);
			break;
			case QI_RX_POWER_4:	// 校准接收功率
				/* 保存校准点数据 */
				tomato_calibration_points_data(QI_RX_POWER_4, tx_power, rpPower);
				/* 执行FOD检测 */
				pfod_flag = qi_pt_ploss_fod_discern(window_size, offset, rpPower);
				/* 切换到功率传输阶段 */
				pt->phase = PT_POWER_TRANSFER;
			break;
		}
	}
	return pfod_flag;	// 返回FOD检测结果
}

/* ================================ 数据包处理器(静态函数) ================================ */
/**
 * @brief 处理控制误差数据包（Qi v1.3 第8.5节）
 * @param device 设备ID
 * @param control_error 控制误差值
 */
static void handle_ce_packet(uint8_t device, int8_t control_error)
{
	pt_module_t *pt = &pt_mod[device];
	
	if(!pt->active) return;
	
	/* 根据控制误差调整功率 */
	qi_power_adjust_ce(device, control_error);
	
	/* 更新统计信息 */
	pt->stats.ce_count++;
	pt->last_ce_timestamp = wlc_timer_get_timestamp();
}

/**
 * @brief 处理接收功率8位数据包（Qi v1.3 第8.14节）
 * @param device 设备ID
 * @param rp8_percent 接收功率百分比（0-100）
 */
static void handle_rp8_packet(uint8_t device, uint8_t rp8_percent)
{
	pt_module_t *pt = &pt_mod[device];
	
	if(!pt->active) return;
	
	/* 限制在100% */
	uint8_t percent = (rp8_percent > 100) ? 100 : rp8_percent;
	
	/* 计算接收功率（毫瓦） */
	uint32_t rx_power_mw = (percent * pt->reference_power_w * 1000) / 100;
    
	/* 更新统计信息 */
	pt->stats.rp8_count++;
	pt->stats.current_rx_power_mw = rx_power_mw;
	pt->last_rp_timestamp = wlc_timer_get_timestamp();
    
	/* 跟踪峰值功率 */
	if (rx_power_mw > pt->stats.peak_rx_power_mw)
	{
		pt->stats.peak_rx_power_mw = rx_power_mw;
	}
    
	/* 仅记录显著变化（>= 10%）或前3个数据包 */
	if (abs((int)percent - (int)pt->stats.last_rp8_percent) >= 10 || pt->stats.rp8_count <= 3)
	{
		// wlc_lib_uart_printf("[RP8] 接收功率: %d%% -> %d mW (参考: %dW), 计数: %d\r\n", 
		//               percent, rx_power_mw, pt->reference_power_w,
		//               pt->stats.rp8_count);
		pt->stats.last_rp8_percent = percent;
	}
}

/**
 * @brief 处理接收功率16位数据包（Qi v1.3 第8.15节）
 * @param device 设备ID
 * @param rp_raw 接收功率原始值（单位：0.1W）
 */
static void handle_rp_packet(uint8_t device, uint16_t rp_raw)
{
	pt_module_t *pt = &pt_mod[device];

	if(!pt->active) return;

	/* 计算接收功率（毫瓦）（rp_raw * 0.1W = rp_raw * 100mW） */
	uint32_t rx_power_mw = rp_raw * 100;

	/* 更新统计信息 */
	pt->stats.rp_count++;
	pt->stats.current_rx_power_mw = rx_power_mw;
	pt->last_rp_timestamp = wlc_timer_get_timestamp();

	/* 跟踪峰值功率 */
	if(rx_power_mw > pt->stats.peak_rx_power_mw)
	{
		pt->stats.peak_rx_power_mw = rx_power_mw;
	}

	/* 仅记录显著变化（>= 500mW/50原始值）或前3个数据包 */
	if(abs((int)rp_raw - (int)pt->stats.last_rp_raw) >= 50 || pt->stats.rp_count <= 3)
	{
		// wlc_lib_uart_printf("[RP] 接收功率: %d mW (原始: 0x%04X = %d.%dW), 计数: %d\r\n", 
		//               rx_power_mw, rp_raw, rp_raw / 10, (rp_raw % 10),
		//               pt->stats.rp_count);
		pt->stats.last_rp_raw = rp_raw;
	}
}

/**
 * @brief 处理充电状态数据包（Qi v1.3 第8.3节）
 * @param device 设备ID
 * @param battery_percent 电池百分比（0-100）
 */
static void handle_chs_packet(uint8_t device, uint8_t battery_percent)
{
	pt_module_t *pt = &pt_mod[device];
    
	if(!pt->active) return;
    
	pt->stats.chs_count++;
    
	if(battery_percent <= 100)
	{
		// wlc_lib_uart_printf("[CHS] 电池: %d%%, 计数: %d\r\n", 
		//               battery_percent, pt->stats.chs_count);
	}
	else
	{
		// wlc_lib_uart_printf("[CHS] 电池: 不适用 (0x%02X), 计数: %d\r\n", 
		//               battery_percent, pt->stats.chs_count);
	}
}

/**
 * @brief 处理功率控制保持数据包（Qi v1.3 第8.12节）
 * @param device 设备ID
 * @param holdoff_ms 保持时间（毫秒）
 */
static void handle_pch_packet(uint8_t device, uint8_t holdoff_ms)
{
	pt_module_t *pt = &pt_mod[device];
    
	if(!pt->active) return;
    
	pt->stats.pch_count++;
    
	// wlc_lib_uart_printf("[PCH] 保持时间: %d 毫秒, 计数: %d\r\n", 
	//               holdoff_ms, pt->stats.pch_count);
}

/**
 * @brief 处理异物检测数据包（Qi v1.3 第8.9节）
 * @param device 设备ID
 * @param fod_status FOD状态（0=清除，1=检测到）
 */
static void handle_fod_packet(uint8_t device, uint8_t fod_status)
{
	pt_module_t *pt = &pt_mod[device];
    
	if(!pt->active) return;
    
	pt->stats.fod_count++;
    
	// wlc_lib_uart_printf("[FOD] 异物: %s, 计数: %d\r\n", 
	//               fod_status ? "检测到" : "清除",
	//               pt->stats.fod_count);
}

/**
 * @brief 停止功率传输阶段（内部）
 * @param device 设备ID
 * @param ept_reason EPT原因代码
 */
static void pt_stop_internal(uint8_t device, uint8_t ept_reason)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;

	pt_module_t *pt = &pt_mod[device];

	if(!pt->active) return;

	/* 关闭电源 */
	qi_power_off(device);
    
	/* 记录EPT信息 */
	// wlc_lib_uart_printf("[EPT] ========== 结束功率传输 ==========\r\n");
	// wlc_lib_uart_printf("[EPT] 原因: %s (代码: 0x%02X)\r\n", 
	//               get_ept_reason(ept_reason), ept_reason);

	/* 记录统计信息 */
	// wlc_lib_uart_printf("[PT_STATS] ========== 传输统计信息 ==========\r\n");
	// wlc_lib_uart_printf("[PT_STATS] CE 数据包: %d\r\n", pt->stats.ce_count);
	// wlc_lib_uart_printf("[PT_STATS] RP8 数据包: %d\r\n", pt->stats.rp8_count);
	// wlc_lib_uart_printf("[PT_STATS] RP 数据包: %d\r\n", pt->stats.rp_count);
	// wlc_lib_uart_printf("[PT_STATS] CHS 数据包: %d\r\n", pt->stats.chs_count);
	// wlc_lib_uart_printf("[PT_STATS] PCH 数据包: %d\r\n", pt->stats.pch_count);
	// wlc_lib_uart_printf("[PT_STATS] FOD 数据包: %d\r\n", pt->stats.fod_count);
    
	if(pt->stats.current_rx_power_mw > 0)
	{
		// wlc_lib_uart_printf("[PT_STATS] 最终接收功率: %d mW (%.2f W)\r\n", 
		//               pt->stats.current_rx_power_mw,
		//               (float)pt->stats.current_rx_power_mw / 1000.0);
	}
    
	if(pt->stats.peak_rx_power_mw > 0)
	{
		// wlc_lib_uart_printf("[PT_STATS] 峰值接收功率: %d mW (%.2f W)\r\n", 
		//               pt->stats.peak_rx_power_mw,
		//               (float)pt->stats.peak_rx_power_mw / 1000.0);
	}
    
	pt->active = false;
	pt_set_state(device, PT_STATE_IDLE);
}

/* ================================ 初始化和开始传输函数 ================================ */
/**
 * @brief 初始化功率传输模块
 * @param device 设备ID（从0开始的索引）
 * @param phase Qi协议阶段
 * @param reference_power_w 参考功率（瓦特）
 * @param window_size_ms 窗口大小（毫秒）
 * @param window_offset_ms 窗口偏移（毫秒）
 * @note 符合 Qi v1.3 第7节 - 功率传输阶段
 */
void qi_pt_init(uint8_t device, qi_phase_t phase, uint16_t reference_power_w,
                uint16_t window_size_ms, uint16_t window_offset_ms)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;

	pt_module_t *pt = &pt_mod[device];

	/* 清零状态标志 */
	pt->state = PT_STATE_IDLE;
	pt->active = false;
	pt->ploss_cnt = 0;
	/* 保存配置参数 */
	pt->reference_power_w = reference_power_w;
	pt->window_size_ms = window_size_ms;
	pt->window_offset_ms = window_offset_ms;
	pt->last_ce_timestamp = 0;
	pt->last_rp_timestamp = 0;
	pt->power_control_holdoff = QI_PCH_MIN;	// 默认最小保持时间
	/* 根据协议阶段配置不同的超时参数 */
	if(phase == QI_PHASE_CALIBRATION) // 校准阶段
	{
		pt->phase = PT_CALIBRATION;
		pt->rx_power_packet_timerout = QI_T_RP_CAIL_TIMEOUT;	// 校准阶段RP超时
		pt->ce_timerout = QI_T_CE_TIMEOUT_MAX;
		pt->light_load_flag = false;	// 轻载校准未完成
		pt->heavy_load_flag = false;	// 重载校准未完成
		tomato_calibration_init(0);	// 初始化Qi功率校准模块
	}
	else
	{
		pt->phase = PT_POWER_TRANSFER;
		pt->rx_power_packet_timerout = QI_T_RP_TIMEOUT_TYP;	// 传输阶段标准RP超时
		pt->ce_timerout = QI_T_CE_TIMEOUT_MAX;
	}
    
	/* 初始化统计信息 */
	memset(&pt->stats, 0, sizeof(qi_pt_stats_t));
}

/**
 * @brief 开始功率传输阶段
 * @param device 设备ID（从0开始的索引）
 */
void qi_pt_start(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;
    
	pt_module_t *pt = &pt_mod[device];
    
	pt->active = true;
	pt_set_state(device, PT_STATE_ACTIVE);	// 切换到激活状态
    
	// wlc_lib_printf("[PT] ========== 功率传输阶段开始 ==========\r\n");
	// wlc_lib_printf("[PT] 参考功率: %dW\r\n", pt->reference_power_w);
	// wlc_lib_printf("[PT] 窗口: %dms 偏移, %dms 大小\r\n", 
	//               pt->window_offset_ms, pt->window_size_ms);
}

/* ================================ 主处理函数 ================================ */
/**
 * @brief 处理功率传输数据包 - 主数据包分发器
 * @param device 设备ID（从0开始的索引）
 * @param packet 接收数据包指针
 * @return 如果接收到EPT（结束功率传输）则返回true，否则返回false
 * @note 符合 Qi v1.3 第7.1节 - 功率传输状态图
 */
bool qi_pt_process(uint8_t device, wlc_context_t *ctx)
{
	if(device >= WLC_LIB_DEVICE_CONT || !ctx)
		return false;

	uint32_t rpPower;	// 用于计算接收功率（毫瓦）
	bool demod_continue_flag = true;	// 解调继续标志
	pt_module_t *pt = &pt_mod[device];
    
	if(!pt->active) return false;
    
	bool ept_received = false;	// EPT接收标志（true：需要结束功率传输）

	/* ============= 主处理逻辑 ============= */
	if(qi_response_active())
	{
		/* 发射端正在发送响应 */ 
	}
	else if(pt->control_error_packet_flag == true)
	{
		demod_continue_flag = false;	// CE处理期间暂停解调
		pt->power_control_time++;			// 递增延迟计时器
		/* 检查是否超过最大超时时间 */
		if(pt->power_control_time >= pt->power_control_timeout)
		{
			/* 超时：重置CE处理状态，恢复正常解调 */
			pt->power_control_time = 0;
			pt->control_error_packet_flag = false;
			demod_continue_flag = true;
		}
		/* 检查是否到达保持时间（holdoff），执行功率调节 */
		else if(pt->power_control_time == pt->power_control_holdoff)
		{
			/* 到达holdoff时间：执行CE处理，调节发射功率 */
			handle_ce_packet(device, pt->control_error);
		}
	}
	/* 收到有效数据包，先关闭解调，防止处理期间被打断 */
	else if(ctx->rx_packet.valid == true)
	{
		wlc_lib_irq_enable(device, 0);
		/* 根据头部类型处理数据包 */
		switch(ctx->rx_packet.header)
		{
			case QI_PKT_CE:	// 控制误差数据包
				demod_continue_flag = false;	// 暂停解调，进入CE延迟响应
				pt->control_error_packet_flag = true;	// 标记CE处理中
				pt->control_error = (int8_t)ctx->rx_packet.data[0];	// 保存CE值
				/* 计算CE处理超时 = holdoff时间 + 活跃时间 - 1 */
				pt->power_control_timeout = pt->power_control_holdoff + QI_ACTIVE_TIME -1;
				/* 重置CE包接收超时计时器（收到新CE包，重新计时） */
				pt->ce_timerout = QI_T_CE_TIMEOUT_MAX;
			break;
			case QI_PKT_RP8:	// 接收功率8位数据包（BPP）
				handle_rp8_packet(device, ctx->rx_packet.data[0]);
				pt->rx_power_packet_timerout = QI_T_RP_TIMEOUT_TYP;
			break;
			case QI_PKT_RP:	// 接收功率16位数据包（扩展协议）
				if(ctx->rx_packet.length >= 2)
				{
					demod_continue_flag = false;	// 暂停解调处理
					/* 解析16位原始功率值 */
					uint32_t rp_raw = (ctx->rx_packet.data[1] << 8) | ctx->rx_packet.data[2];
					// handle_rp_packet(device, rp_raw);
					rpPower = ((rp_raw * 1000) / 32768 * ctx->cfg_data.max_power_w);	// 计算实际接收功率
					/* 解析24位RP包，执行FOD检测逻辑 */
					ept_received = qi_pt_parse_power_24bit(
						device, 
						&ctx->rx_packet, 
						ctx->cfg_data.max_power_w,	// 最大功率（瓦）
						ctx->cfg_data.window_size,	// 校准窗口大小
						ctx->cfg_data.window_offset	// 校准窗口偏移
					);
					wlc_lib_uart_printf("RP = %d , RpPower = %d \r\n",rp_raw, rpPower);
				}
				pt->rx_power_packet_timerout = QI_T_RP_TIMEOUT_TYP;
			break;
			case QI_PKT_CHS:	// 充电状态数据包
				handle_chs_packet(device, ctx->rx_packet.data[0]);
			break;
			case QI_PKT_PCH:	// 功率控制保持数据包
				handle_pch_packet(device, ctx->rx_packet.data[0]);
			break;
			case QI_PKT_EPT:	// 结束功率传输数据包
				{
					uint8_t code = ctx->rx_packet.data[0];	// 保存EPT原因码
					pt_stop_internal(device, code);					// 内部停止处理
					ept_received = true;										// 标记EPT已收到
				}
			break;
			case QI_PKT_GRQ:	// 通用请求数据包
				// wlc_lib_uart_printf("[GRQ] 通用请求\r\n");
			break;
			case QI_PKT_SRQ:	// 特定请求数据包
				// wlc_lib_uart_printf("[SRQ] 特定请求 (类型: 0x%02X)\r\n", 
				//               			packet->data[0]);
			break;
			case QI_PKT_FOD:	// 异物检测数据包
				handle_fod_packet(device, ctx->rx_packet.data[0]);
			break;
			default:
				// wlc_lib_uart_printf("[PT] 未知数据包类型: 0x%02X\r\n", packet->header);
			break;
		}
	}
	else	// 无数据包时的超时检测
	{
		/* RP（接收功率）超时检测 */
		if(pt->rx_power_packet_timerout)
		{
			pt->rx_power_packet_timerout--;
			if(pt->rx_power_packet_timerout == 0)
			{
				/* RP超时：触发EPT结束充电 */
				ept_received = true;
			}
		}
		/* CE（控制误差）超时检测 */
		if(pt->ce_timerout)
		{
			pt->ce_timerout--;
			if(pt->ce_timerout == 0)
			{
				/* CE超时：触发EPT结束充电，并恢复解调 */
				ept_received = true;
				demod_continue_flag = true;
				pt->control_error_packet_flag = false;
			}
		}
	}
    
	if(demod_continue_flag == true)
	{
		wlc_lib_irq_enable(device, 1);	// 开启解调
	}
	return ept_received;	// true=结束充电，false=继续充电
}

/* ================================ 统计与查询函数 ================================ */
/**
 * @brief 获取功率传输统计信息
 * @param device 设备ID（从0开始的索引）
 * @param stats 指向要填充的统计结构的指针
 */
void qi_pt_get_stats(uint8_t device, qi_pt_stats_t *stats)
{
	if(device >= WLC_LIB_DEVICE_CONT || !stats) return;
    
	memcpy(stats, &pt_mod[device].stats, sizeof(qi_pt_stats_t));
}

/**
 * @brief 重置功率传输统计信息
 * @param device 设备ID（从0开始的索引）
 */
void qi_pt_reset_stats(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;
    
	memset(&pt_mod[device].stats, 0, sizeof(qi_pt_stats_t));
    
	// wlc_lib_printf("[PT] 设备 %d 的统计信息已重置\r\n", device);
}

/**
 * @brief 获取当前接收功率（毫瓦）
 * @param device 设备ID（从0开始的索引）
 * @return 当前接收功率（毫瓦）
 */
uint32_t qi_pt_get_rx_power_mw(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return 0;
    
	return pt_mod[device].stats.current_rx_power_mw;
}

/**
 * @brief 检查功率传输是否活跃
 * @param device 设备ID（从0开始的索引）
 * @return 如果活跃则返回true，否则返回false
 */
bool qi_pt_is_active(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return false;
    
	return pt_mod[device].active;
}
