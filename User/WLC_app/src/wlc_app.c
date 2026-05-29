#include "wlc_app.h"
#include "Qi_Ping.h"
#include "Qi_IdConfig.h"
#include "Qi_Power_Delivery.h"
#include "Qi_Power_Transfer.h"
#include "wlc_timer.h"
#include "wlc_physical.h"
#include "wlc_lib_interal_functions.h"
#include "Qi_Response.h"
#include "Qi_Negotiation.h"

#define SELECTION_TIMEOUT_MS     100    /* 选择阶段超时 - 缩短为 100ms */
#define PING_TIMEOUT_MS          5000   /* Ping 阶段超时 (t_detect 设备检测时间) */
#define IDCONFIG_TIMEOUT_MS      1000   /* ID/配置阶段超时 */
#define NEGOTIATION_TIMEOUT_MS   1500   /* 协商阶段超时 */
#define POWER_XFER_TIMEOUT_MS    10000  /* 功率传输阶段超时 */
#define POWER_XFER_RX_TIMEOUT_MS 2000   /* 功率传输阶段接收数据包超时 */

/* 输入电压检测阈值 (mV) */
#define POWER_VIN_5V_MIN         4700   /* 5V 输入电压下限 */
#define POWER_VIN_5V_MAX         5300   /* 5V 输入电压上限 */
#define POWER_VIN_9V_MIN         8700   /* 9V 输入电压下限 */
#define POWER_VIN_9V_MAX         9300   /* 9V 输入电压上限 */

/* 每个设备的静态应用程序上下文实例 */
static wlc_context_t wlc_ctx[WLC_LIB_DEVICE_CONT];

/*  阶段处理函数指针类型定义 */
typedef void (*phase_handler_t)(uint8_t device);   /* 阶段处理函数类型 */
typedef void (*phase_cleanup_t)(uint8_t device);  /* 阶段清理函数类型 (未使用) */

static uint16_t wlc_app_get_calcadc_filter(uint8_t device, uint8_t model, uint8_t channel);	// 前向声明

/* ================================ 阶段管理函数(静态函数) ================================ */
/**
 * @brief 转换到新阶段并设置超时
 * @param device 设备索引
 * @param new_phase 要转换到的新阶段
 * @param timeout_ms 阶段超时时间 (毫秒)
 */
static void set_phase(uint8_t device, qi_phase_t new_phase, uint32_t timeout_ms)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;

	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();

	/* 销毁之前的定时器 */
	if(ctx->timeout_timer)
	{
		wlc_timer_destroy(ctx->timeout_timer);
		ctx->timeout_timer = 0;
	}
    
	/* 更新阶段状态 */
	ctx->prev_phase = ctx->phase;
	ctx->phase = new_phase;
	ctx->phase_initialized = false;  /* 新阶段需要重新初始化 */

	/* 创建新的超时定时器 */
	if(timeout_ms > 0)
	{
		ctx->timeout_timer = wlc_timer_create(timeout_ms);
	}
	
	// wlc_lib_uart_printf("[APP] [%dms] Phase transition: timeout=%dms\r\n", now, timeout_ms);
}

/**
 * @brief 返回选择阶段 - 清理并重置
 * @param device 设备索引
 * @note 停止所有活动模块，清除上下文状态
 */
static void goto_selection(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return;

	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();

	/* 停止所有活动模块 */
	qi_ping_stop(device);
	qi_power_off(device);
	// qi_response_stop();

	/* 禁用 ASK 解调 */
	wlc_ask_demod_enable(device, 0);

	/* 清除上下文数据 */
	ctx->rx_packet.valid = false;
	memset(&ctx->id_data, 0, sizeof(qi_id_data_t));
	memset(&ctx->cfg_data, 0, sizeof(qi_cfg_data_t));
	memset(&ctx->contract, 0, sizeof(qi_power_contract_t));

	/* 转换到选择阶段，使用最小超时时间 */
	set_phase(device, QI_PHASE_SELECTION, SELECTION_TIMEOUT_MS);

	// wlc_lib_uart_printf("[APP] [%dms] ========== Returned to Selection Phase ==========\r\n", now);
}

/* ============================= 阶段处理函数(静态函数) - 解耦实现 ============================= */
/**
 * @brief 处理选择 Selection 阶段
 * @param device 设备索引
 * @note 首次调用时立即转换到 Ping 阶段，避免延迟
 */
static void handle_selection_phase(uint8_t device)
{
	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();
	
	/* 首次调用时初始化阶段 */
	if(!ctx->phase_initialized)
	{
		// wlc_lib_uart_printf("[APP] [%dms] ========== Selection Phase ==========\r\n", now);
		ctx->phase_initialized = true;
		
		/* ===== 立即转换到 Ping 阶段 ===== */
		// wlc_lib_uart_printf("[APP] [%dms] Selection phase - transitioning to Ping phase immediately\r\n", now);
		set_phase(device, QI_PHASE_PING, PING_TIMEOUT_MS);
		return;
	}

	/* 后备机制: 超时后转到 Ping 阶段 */
	if(ctx->timeout_timer && wlc_timer_is_expired(ctx->timeout_timer))
	{
		// wlc_lib_uart_printf("[APP] [%dms] Selection timeout - transitioning to Ping phase\r\n", now);
		set_phase(device, QI_PHASE_PING, PING_TIMEOUT_MS);
	}
}

/**
 * @brief 处理 Ping 阶段
 * @param device 设备索引
 * @note 发送 Ping 脉冲并检测接收端设备
 */
static void handle_ping_phase(uint8_t device)
{
	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	/* 首次调用时初始化阶段 */
	if(!ctx->phase_initialized)
	{
		// wlc_lib_uart_printf("[APP] [%dms] ========== Ping Phase ==========\r\n", now);

		/* 设置 Ping 功率等级 */
		qi_ping_set_power_level(device, PING_POWER_LEVEL);
        
		/* 启动 Ping */
		qi_ping_start(device);
        
		ctx->phase_initialized = true;
//		wlc_lib_uart_printf("Ping start\r\n");  // 加这行
	}
    
	/* 处理 Ping 并检查设备检测 */
	if(qi_ping_process(device, &ctx->rx_packet))
	{
		// wlc_lib_uart_printf("[APP] [%dms] Device detected - transitioning to ID/Config phase\r\n", now);
//		wlc_lib_uart_printf("Device detected!\r\n");  // 加这行
		/* 停止 Ping 但保持功率开启 */
		qi_ping_stop_keep_power(device);

		/* 转换到 ID/配置阶段 */
		set_phase(device, QI_PHASE_ID_CONFIG, IDCONFIG_TIMEOUT_MS);
		qi_idconfig_start(device);
	}
    
	/* 清除数据包有效标志 */
	ctx->rx_packet.valid = false;
}

/**
 * @brief 处理 ID/配置阶段
 * @param device 设备索引
 * @note 接收并解析设备标识信息
 */
static void handle_idconfig_phase(uint8_t device)
{
	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	/* 首次调用时初始化阶段 */
	if(!ctx->phase_initialized)
	{
		ctx->phase_initialized = true;
	}
    
	/* 处理 ID/配置数据包 */
	if(qi_response_active())
	{
		/* 响应处理中，等待完成 */
		
	}
	else if(qi_idconfig_process(device, &ctx->rx_packet, 
															&ctx->id_data, &ctx->cfg_data))
	{
		/* ID/配置完成 - 构建功率传输契约 */
		qi_mode_t mode = qi_idconfig_get_mode(device);
		
		/* 填充功率传输契约参数 */
		ctx->contract.reference_power_w = ctx->cfg_data.max_power_w;        /* 参考功率 (瓦) */
		ctx->contract.window_size_ms = ctx->cfg_data.window_size * 4;      /* 窗口大小 (4ms 倍数) */
		ctx->contract.window_offset_ms = ctx->cfg_data.window_offset * 4;  /* 窗口偏移 (4ms 倍数) */
		ctx->contract.control_holdoff_ms = QI_PCH_DEFAULT;                 /* 控制保持时间 */
		ctx->contract.mode = mode;                                          /* 工作模式 (BPP/EPP) */
		
		/* 初始化功率传输模块 */
		// qi_pt_init(device, ctx->cfg_data.max_power_w,
		//           ctx->contract.window_size_ms,
		//           ctx->contract.window_offset_ms);
		// qi_pt_start(device);
        
		/* 根据模式转换到相应阶段 */
		if (mode == QI_MODE_BPP)
		{
			set_phase(device, QI_PHASE_POWER_TRANSFER, POWER_XFER_TIMEOUT_MS);
			wlc_lib_uart_printf("Qi: BPP mode\r\n");  /* 基线功率配置模式 */
		}
		else
		{
			set_phase(device, QI_PHASE_NEGOTIATION, NEGOTIATION_TIMEOUT_MS);
			wlc_lib_uart_printf("Qi: EPP mode\r\n");  /* 扩展功率配置模式 */
		}
	}
    
	/* 清除数据包有效标志 */
	ctx->rx_packet.valid = false;
}

/**
 * @brief 处理协商 (Negotiation) 阶段
 * @param device 设备索引
 * @note 扩展协议协商 (本版本未完整实现)
 */
static void handle_negotiation_phase(uint8_t device)
{
	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	/* 首次调用时初始化阶段 */
	if(!ctx->phase_initialized)
	{
		ctx->phase_initialized = true;
	}

	/* 处理协商过程 */
	if(qi_negtiation_process(device, ctx, &ctx->rx_packet))
	{
		/* 协商正常完成 */
		if(ctx->neg_data.end_negotiation_flag == true)
		{
			ctx->neg_data.end_negotiation_flag = false;  /* 清除标志 */
			set_phase(device, QI_PHASE_POWER_TRANSFER, POWER_XFER_TIMEOUT_MS);
		}
	}
	else
	{
		/* 协商失败或超时，返回选择阶段 */
		goto_selection(device);
	}

	/* 清除数据包有效标志 */
	ctx->rx_packet.valid = false;
}

/**
 * @brief 处理功率传输 (Power Transfer) 阶段
 * @param device 设备索引
 * @note 管理功率传输和控制
 */
static void handle_power_transfer_phase(uint8_t device)
{
	wlc_context_t *ctx = &wlc_ctx[device];
	uint32_t now = wlc_timer_get_timestamp();
    
	/* 首次调用时初始化阶段 */
	if(!ctx->phase_initialized)
	{
		// wlc_lib_uart_printf("[APP] [%dms] ========== Power Transfer Phase ==========\r\n", now);
		ctx->phase_initialized = true;
	}
    
	/* 处理功率传输数据包 */
	if(qi_pt_process(device, ctx))
	{
		/* 收到 EPT (End Power Transfer) 包 - 返回选择阶段 */
		wlc_lib_uart_printf("[APP] [%dms] EPT received - returning to Selection phase\r\n", now);
		goto_selection(device);
	}
	else
	{
		/* 正常处理，刷新超时定时器 */
		set_phase(device, QI_PHASE_POWER_TRANSFER, POWER_XFER_TIMEOUT_MS);
	}

	/* 清除数据包有效标志 */
	ctx->rx_packet.valid = false;
}

/* ============================= 阶段处理函数分发表 - 解耦实现 ============================= */
static const phase_handler_t phase_handlers[] =
{
	handle_selection_phase,            // QI_PHASE_SELECTION
	handle_ping_phase,                 // QI_PHASE_PING
	handle_idconfig_phase,             // QI_PHASE_ID_CONFIG
	handle_negotiation_phase,          // QI_PHASE_NEGOTIATION
	handle_power_transfer_phase        // QI_PHASE_POWER_TRANSFER
};

/* =================================== 查询函数 =================================== */
/**
 * @brief 获取当前协议阶段
 * @param device 设备索引
 * @return 当前阶段枚举值
 */
qi_phase_t wlc_app_get_phase(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)	return QI_PHASE_SELECTION;
	return wlc_ctx[device].phase;
}

/**
 * @brief 获取当前功率功率合约
 * @param device 设备索引
 * @param contract 契约结构体指针
 */
void wlc_app_get_contract(uint8_t device, qi_power_contract_t *contract)
{
	if(device >= WLC_LIB_DEVICE_CONT || !contract) return;
	memcpy(contract, &wlc_ctx[device].contract, sizeof(qi_power_contract_t));
}

/**
 * @brief 获取参考功率 (毫瓦)
 * @param device 设备索引
 * @return 参考功率值 (mW)
 */
uint32_t wlc_app_get_reference_power(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return 0;
	return (uint32_t)(wlc_ctx[device].cfg_data.max_power_w * 1000);  /* 瓦转毫瓦 */
}

/**
 * @brief 获取当前阶段名称 (用于调试)
 * @param device 设备索引
 * @return 阶段名称字符串
 */
const char* wlc_app_get_phase_name(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return "INVALID";
    
	static const char* phase_names[] =
	{
		"SELECTION",      /* 选择阶段 */
		"PING",           /* Ping 阶段 */
		"ID_CONFIG",      /* ID/配置阶段 */
		"NEGOTIATION",    /* 协商阶段 */
		"POWER_TRANSFER"  /* 功率传输阶段 */
	};
    
	qi_phase_t phase = wlc_ctx[device].phase;
	if(phase < sizeof(phase_names) / sizeof(phase_names[0]))
	{
		return phase_names[phase];
	}
	return "UNKNOWN";
}

/**
 * @brief 获取输入电压 (毫伏)
 * @param device 设备索引
 * @return 输入电压值 (mV)
 * @note 使用 ADC 采样并经过滤波处理
 */
uint16_t wlc_app_get_vin(uint8_t device)
{
	uint16_t adc_value;
	uint16_t vin;

	/* 获取滤波后的 ADC 值 */
	adc_value = wlc_app_get_calcadc_filter(device, 0, 0);
    
	/* ADC 值转换为电压: 参考电压 3.3V, 12位 ADC, 分压电阻比 5.32 */
	vin = (uint16_t)((adc_value * 3300 / 4095) * 5.32);

	return vin;
}

/**
 * @brief 获取输入电流 (毫伏，间接表示电流)
 * @param device 设备索引
 * @return 电流采样值 (mV)
 * @note 返回的是电流采样电阻上的电压值，需根据电阻值换算
 */
uint16_t wlc_app_get_current(uint8_t device)
{ 
	uint16_t adc_value;
	uint16_t current;
    
	/* 获取滤波后的 ADC 值 */
	adc_value = wlc_app_get_calcadc_filter(device, 0, 1);
    
	/* ADC 值转换为电压 (3.3V 参考, 12位 ADC) */
	current = (uint16_t)(adc_value * 3300 / 4095);

	return current;
}

/**
 * @brief 获取并滤波 ADC 值 (去除最大值和最小值后平均)
 * @param device 设备索引
 * @param model ADC 型号
 * @param channel ADC 通道
 * @return 滤波后的 ADC 值
 */
static uint16_t wlc_app_get_calcadc_filter(uint8_t device, uint8_t model, uint8_t channel) {
    uint32_t t_sum = 0x0;
    uint16_t t_min = 0xffff;
    uint16_t t_max = 0x0;
    uint16_t t_adc;
    uint8_t i;

    /* 采样 6 次 */
    for (i = 0; i < 6; i++) {
        t_adc = wlc_lib_adc_get_value(device, model, channel);
        t_sum += t_adc;
        if (t_adc > t_max) t_max = t_adc;
        if (t_adc < t_min) t_min = t_adc;
    }
    
    /* 去除最大值和最小值后求平均 */
    t_sum -= (t_max + t_min);
    
    return (t_sum >> 2);  /* 除以 4 (6-2=4 次采样平均) */
}

/* =================================== 初始化函数 =================================== */
/**
 * @brief 初始化 WLC 应用程序 - 设置所有模块
 */
void wlc_app_init(void)
{
	uint32_t now = wlc_timer_get_timestamp();
    
	for(uint8_t i = 0; i < WLC_LIB_DEVICE_CONT; i++)
	{
		/* 初始化所有协议模块 */
		qi_ping_init(i);       /* Ping 模块 */
		qi_idconfig_init(i);   /* ID/配置模块 */
		qi_power_init(i);      /* 功率传输模块 */

		/* 初始化上下文 */
		wlc_ctx[i].phase = QI_PHASE_SELECTION;
		wlc_ctx[i].prev_phase = QI_PHASE_SELECTION;
		wlc_ctx[i].timeout_timer = 0;
		wlc_ctx[i].rx_packet.valid = false;
		wlc_ctx[i].phase_initialized = false;

		/* 检测输入电压状态 */
		uint16_t vin = wlc_app_get_vin(i);
		if((vin >= POWER_VIN_5V_MIN) && (vin <= POWER_VIN_5V_MAX))
		{
			wlc_ctx[i].vin_state = QI_POWER_5V;   /* 5V 输入 */
		}
		else if((vin >= POWER_VIN_9V_MIN) && (vin <= POWER_VIN_9V_MAX))
		{
			wlc_ctx[i].vin_state = QI_POWER_9V;   /* 9V 输入 */
		}
		else
		{
			wlc_ctx[i].vin_state = QI_POWER_5V;   /* 默认 5V */
		}
        
		/* 清除数据结构 */
		memset(&wlc_ctx[i].id_data, 0, sizeof(qi_id_data_t));
		memset(&wlc_ctx[i].cfg_data, 0, sizeof(qi_cfg_data_t));
		memset(&wlc_ctx[i].contract, 0, sizeof(qi_power_contract_t));
	}
    
	/* 从选择阶段开始 */
	set_phase(0, QI_PHASE_SELECTION, SELECTION_TIMEOUT_MS);
    
	// wlc_lib_uart_printf("[APP] [%dms] ========== WLC Application Initialized ==========\r\n", now);
}

/**
 * @brief 获取接收端设备类型
 * @param device 设备索引
 * @return 设备类型枚举
 */
qi_device_type_t qi_app_get_rxdevice(uint8_t device)
{
	if (device >= WLC_LIB_DEVICE_CONT) return QI_DEVICE_UNKNOWN;
	return wlc_ctx[device].id_data.device_type;
}

/**
 * @brief 获取输入电压状态
 * @param device 设备索引
 * @return 电压状态 (5V/9V)
 */
qi_power_state_t qi_app_get_vin_state(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT) return QI_POWER_5V;
	return wlc_ctx[device].vin_state;
}

/* =============================== 主任务(每 1ms 调用一次) =============================== */
/**
 * @brief WLC 主任务 - 每 1ms 从系统定时器调用
 * @note 使用解耦阶段处理器的简化状态机
 */
void wlc_app_task(void)
{
	for(uint8_t dev = 0; dev < WLC_LIB_DEVICE_CONT; dev++)
	{
		wlc_context_t *ctx = &wlc_ctx[dev];
		uint32_t now = wlc_timer_get_timestamp();
        
		/* ====== 阶段超时处理 ====== */
		if(ctx->timeout_timer && wlc_timer_is_expired(ctx->timeout_timer))
		{
			switch (ctx->phase)
			{
				case QI_PHASE_SELECTION:	// 选择阶段超时
					/*  正常，将转换到 Ping */
				break;
				case QI_PHASE_PING:	// Ping 阶段超时
					/* 未检测到设备 */
					// wlc_lib_printf("[APP] [%dms] [Err] Ping phase timeout\r\n", now);
					goto_selection(dev);
				continue;
				case QI_PHASE_ID_CONFIG:	// ID/配置阶段超时
					// wlc_lib_printf("[APP] [%dms] [Err] ID/Config phase timeout\r\n", now);
					goto_selection(dev);
				continue;
				case QI_PHASE_NEGOTIATION:	// 协商阶段超时
					// wlc_lib_printf("[APP] [%dms] [Err] Negotiation phase timeout\r\n", now);
					goto_selection(dev);
				continue;
				case QI_PHASE_POWER_TRANSFER:	// 功率传输阶段超时
					// wlc_lib_printf("[APP] [%dms] [Err] Power transfer phase timeout\r\n", now);
					goto_selection(dev);
				continue;
				default:	// 未知阶段 - 返回选择阶段
					goto_selection(dev);
				continue;
			}
		}
        
		/* ====== 通过分发表进行阶段处理 ====== */
		if(ctx->phase < sizeof(phase_handlers) / sizeof(phase_handlers[0]))
		{
			phase_handlers[ctx->phase](dev);
		}
		else
		{
			wlc_lib_uart_printf("[APP] [%dms] [Err] Invalid phase: %d\r\n", now, ctx->phase);
			goto_selection(dev);
		}
	}
}

/* =============================== 数据接收回调函数 =============================== */
/**
 * @brief 数据包接收回调函数
 * @param device 设备索引
 * @param channel 通道编号
 * @param packet_type 数据包类型/头字节
 * @param data 数据负载指针
 * @param data_length 负载长度 (字节)
 */
void wlc_app_data_received_callback(uint8_t device, uint8_t channel,
                                   uint8_t packet_type, uint8_t *data,
                                   uint8_t data_length)
{
	if(device >= WLC_LIB_DEVICE_CONT || !data) return;

	qi_packet_t *pkt = &wlc_ctx[device].rx_packet;
	uint32_t now = wlc_timer_get_timestamp();

	/* 存储数据包信息 */
	pkt->device = device;
	pkt->channel = channel;
	pkt->header = packet_type;
	pkt->length = (data_length <= 27) ? data_length : 27;  /* Qi 包最大 27 字节 */

	/* 复制负载数据 */
	if(pkt->length > 0)
	{
		memcpy(pkt->data, data, pkt->length);
	}

	/* 标记数据包有效 */
	pkt->valid = true;

	/* 打印接收到的数据包日志 */
	wlc_lib_uart_printf("[RX] [%dMs]  %02X ", now, packet_type);
	for (uint8_t i = 0; i < data_length && i < 16; i++)
	{
		wlc_lib_uart_printf(" %02X", data[i]);
	}
	wlc_lib_uart_printf("\r\n");
}