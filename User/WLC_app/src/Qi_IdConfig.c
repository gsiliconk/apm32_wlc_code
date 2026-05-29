#include "Qi_IdConfig.h"
#include "Qi_Response.h"
#include "wlc_lib_interal_functions.h"
#include "wlc_timer.h"
#include "Qi_Negotiation.h"
/* 文件: Qi_IdConfig.c
 * 描述: Qi协议ID/Config阶段实现
 */

/* 错误码枚举 */
typedef enum {
    IDCFG_ERR_NONE = 0,               /* 无错误 */
    IDCFG_ERR_INVALID_ID_VERSION,     /* 无效的ID版本 */
    IDCFG_ERR_INVALID_CFG_FORMAT,     /* 无效的配置格式 */
    IDCFG_ERR_INVALID_COUNT,          /* 无效的计数值 */
    IDCFG_ERR_INVALID_PCH_VALUE,      /* 无效的PCH值 */
    IDCFG_ERR_UNEXPECTED_PACKET,      /* 意外的数据包类型 */
    IDCFG_ERR_TIMEOUT                 /* 阶段超时 */
} idcfg_error_code_t;

/* ID/Config模块控制结构 */
typedef struct {
    idcfg_state_t state;               /* 当前状态机状态 */
    bool ext_id_expected;              /* 是否期望扩展ID标志 */
    uint8_t optional_count;            /* 已接收的可选数据包计数 */
    qi_mode_t mode;                    /* 检测到的Qi模式（BPP/EPP） */
    uint8_t error_code;                /* 错误状态时的错误码 */
} idconfig_module_t;

/* 每个设备的静态ID/Config模块实例 */
static idconfig_module_t idcfg_mod[WLC_LIB_DEVICE_CONT];

/* =========================== 静态辅助函数 =========================== */
/**
 * @brief 根据设备类型获取人类可读的设备名称
 * @param device_type 设备类型枚举
 * @return 设备名称字符串指针
 */
static const char* get_device_name(qi_device_type_t device_type)
{
	static const char* device_names[] =
	{
		"Unknown",
		"Apple",
		"Samsung",
		"Huawei",
		"OPPO",
		"Vivo",
		"Xiaomi",
		"Qi Universal",
		"Universal"
	};
    
	if(device_type < sizeof(device_names) / sizeof(device_names[0]))
	{
		return device_names[device_type];
	}
	return "Unknown";
}

/**
 * @brief 获取人类可读的状态名称
 * @param state ID/Config状态
 * @return 状态名称字符串指针
 */
static const char* get_idcfg_state_name(idcfg_state_t state)
{
	static const char* state_names[] =
	{
		"Waiting  ID",
		"Waiting Extension ID",
		"Waiting optional package",
		"Waiting configuration",
		"Complete",
		"Error"
	};
    
	if(state < sizeof(state_names) / sizeof(state_names[0]))
	{
		return state_names[state];
	}
	return "Unknown";
}

/**
 * @brief 获取错误描述字符串
 * @param error_code 错误码
 * @return 错误描述指针
 */
static const char* get_error_description(idcfg_error_code_t error_code)
{
	static const char* error_descs[] =
	{
		"No error",
		"Invalid ID version",
		"Invalid CFG format",
		"Invalid optional packet count",
		"Invalid PCH value",
		"Unexpected packet type",
		"Stage timeout"
	};
    
	if(error_code < sizeof(error_descs) / sizeof(error_descs[0]))
	{
		return error_descs[error_code];
	}
	return "Unknown Error";
}

/**
 * @brief 转换到新的ID/Config状态
 * @param device 设备ID
 * @param new_state 要转换到的新状态
 */
static void idcfg_set_state(uint8_t device, idcfg_state_t new_state)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	idconfig_module_t *mod = &idcfg_mod[device];

	if(mod->state != new_state)
	{
		/* 只在状态真正改变时才切换 */
		// wlc_lib_printf("[IDCFG] State: %s -> %s\r\n",
		//               get_idcfg_state_name(mod->state),
		//               get_idcfg_state_name(new_state));
		mod->state = new_state;
	}
}

/**
 * @brief 设置错误状态及错误码
 * @param device 设备ID
 * @param error_code 错误码
 */
static void idcfg_set_error(uint8_t device, idcfg_error_code_t error_code)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	idconfig_module_t *mod = &idcfg_mod[device];
    
	mod->error_code = error_code;
	idcfg_set_state(device, IDCFG_STATE_ERROR);
    
	//wlc_lib_printf("[IDCFG] [Err] %s\r\n", get_error_description(error_code));
}

/**
 * @brief 解析厂商代码为设备类型
 * @param manuf_code 厂商代码
 * @return 设备类型枚举
 */
static qi_device_type_t parse_device_type(uint16_t manuf_code)
{
	switch(manuf_code)
	{
		case QI_MANUF_APPLE:       return QI_DEVICE_APPLE;
		case QI_MANUF_SAMSUNG:     return QI_DEVICE_SAMSUNG;
		case QI_MANUF_HUAWEI:      return QI_DEVICE_HUAWEI;
		case QI_MANUF_OPPO:        return QI_DEVICE_OPPO;
		case QI_MANUF_VIVO:        return QI_DEVICE_VIVO;
		case QI_MANUF_XIAOMI:      return QI_DEVICE_XIAOMI;
		case QI_MANUF_QI_GENERIC:  return QI_DEVICE_QI_GENERIC;
		default:                   return QI_DEVICE_COMMON;    /* 未知厂商归类为通用 */
	}
}

/**
 * @brief ID/Config完成后的响应回调函数
 * @note 当ID/Config阶段成功完成后被调用，初始化协商模块
 */
static void qi_idconfig_response_callback(void)
{
	qi_negotiation_init(0);    /* ID/Config完成后进入协商阶段 */
}

/* =========================== 初始化和控制 =========================== */
/**
 * @brief 初始化ID/Config模块
 * @param device 设备ID（0起始索引）
 */
void qi_idconfig_init(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;

	idconfig_module_t *mod = &idcfg_mod[device];

	mod->state = IDCFG_STATE_WAIT_ID;    /* 从等待ID开始 */
	mod->ext_id_expected = false;
	mod->optional_count = 0;
	mod->mode = QI_MODE_BPP;    /* 默认BPP模式，后续根据CFG包更新 */
	mod->error_code = IDCFG_ERR_NONE;

	//wlc_lib_printf("[IDCFG] Module initialized for device %d\r\n", device);
}

/**
 * @brief 启动ID/Config阶段
 * @param device 设备ID（0起始索引）
 * @note 符合Qi v1.3第5节 - 配置阶段
 */
void qi_idconfig_start(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return;
    
	qi_idconfig_init(device);    /* 先初始化再开始 */

	//wlc_lib_printf("[IDCFG] ========== ID/Config Phase Start ==========\r\n");
}

/* =========================== 数据包处理 =========================== */
/**
 * @brief 处理ID/Config数据包 - 状态机实现
 * @param device 设备ID（0起始索引）
 * @param packet 接收到的数据包指针
 * @param id ID数据结构指针，用于存储解析结果
 * @param cfg 配置数据结构指针，用于存储解析结果
 * @return 如果配置完成返回true，否则返回false
 * @note 符合Qi v1.3第5.1节 - 配置阶段状态图
 */
bool qi_idconfig_process(uint8_t device, const qi_packet_t *packet,qi_id_data_t *id, qi_cfg_data_t *cfg)
{
	if(device >= WLC_LIB_DEVICE_CONT || !packet || !packet->valid)
	{
		return false;    /* 快速路径：参数无效直接返回 */
	}
	uint32_t now = wlc_timer_get_timestamp();
	idconfig_module_t *mod = &idcfg_mod[device];
    
	/* 错误状态 - 不再处理 */
	if(mod->state == IDCFG_STATE_ERROR)
	{
		return false;
	}

	switch(mod->state)
	{
		case IDCFG_STATE_WAIT_ID:
			/* ============ 状态2：等待ID数据包 ============ */
			if(packet->header != QI_PKT_ID)
			{
				/* 必须是ID包，不是则报错 */
				idcfg_set_error(device, IDCFG_ERR_UNEXPECTED_PACKET);
				return false;
			}
        
			/* 验证数据包长度 */
			if(packet->length < 6)
			{
				idcfg_set_error(device, IDCFG_ERR_INVALID_ID_VERSION);
				return false;
			}
        
			/* 根据Qi v1.3第8.11节解析ID数据包 */
			id->major_version = (packet->data[0] >> 4) & 0x0F;   /* 高4位为主版本号 */
			id->minor_version = packet->data[0] & 0x0F;          /* 低4位为次版本号 */
        
			/* 验证版本（必须是1.x） */

			id->manufacturer_code = (packet->data[1] << 8) | packet->data[2];    /* 厂商代码：2字节 */
			id->ext_bit = (packet->data[3] >> 7) & 0x01;    /* 扩展位：1位，1=有扩展ID */
			id->basic_id = ((packet->data[3] & 0x7F) << 16) | (packet->data[4] << 8) | packet->data[5];    /* 基本ID：3字节 */
			id->device_type = parse_device_type(id->manufacturer_code);    /* 根据厂商代码判断设备类型 */
        
			/* 根据扩展位决定下一状态 */
			if(id->ext_bit)
			{
				idcfg_set_state(device, IDCFG_STATE_WAIT_XID);    /* 有扩展ID，等待扩展ID包 */
				mod->ext_id_expected = true;
			}
			else
			{
				idcfg_set_state(device, IDCFG_STATE_WAIT_OPTIONAL);    /* 无扩展ID，直接等待可选包 */
			}
		break;
    case IDCFG_STATE_WAIT_XID:
			/* ============ 状态3：等待扩展ID数据包 ============ */
			if(packet->header != QI_PKT_XID)
			{
				/* 扩展ID是可选的 - 可以继续处理可选数据包 */
				idcfg_set_state(device, IDCFG_STATE_WAIT_OPTIONAL);
				/* 重新处理此数据包作为可选包 */
				return qi_idconfig_process(device, packet, id, cfg);
			}
			idcfg_set_state(device, IDCFG_STATE_WAIT_OPTIONAL);    /* 扩展ID处理完成 */
		break;
    case IDCFG_STATE_WAIT_OPTIONAL:
			/* ============ 状态4：等待可选数据包 ============ */
			if(packet->header == QI_PKT_PCH)
			{
				/* 功率控制保持数据包（Power Control Hold-off） */
				uint8_t holdoff = packet->data[0];    /* PCH值：接收器的保持时间请求 */

				/* 验证PCH值（Qi v1.3第8.12节：5-100ms） */
				if(holdoff < 5 || holdoff > 100)
				{
					idcfg_set_error(device, IDCFG_ERR_INVALID_PCH_VALUE);    /* PCH值越界 */
					return false;
				}

				mod->optional_count++;    /* 可选包计数+1 */
			}
			else if(packet->header == QI_PKT_CFG)
			{
				/* 收到配置数据包 */
				idcfg_set_state(device, IDCFG_STATE_WAIT_CFG);    /* 转入配置解析状态 */
				/* 重新处理作为CFG包 */
				return qi_idconfig_process(device, packet, id, cfg);
			}
			else if((packet->header >= 0x18 && packet->header <= 0x29))
			{
				/* 专有或保留数据包 - 计数后忽略 */
				mod->optional_count++;    /* proprietary/reserved包也计入可选计数 */
			}
			else
			{
				/* 意外的数据包类型 */
				idcfg_set_error(device, IDCFG_ERR_UNEXPECTED_PACKET);
				return false;
			}
		break;
    case IDCFG_STATE_WAIT_CFG:
			/* ============ 状态5：解析配置数据包 ============ */
			if(packet->header != QI_PKT_CFG)
			{
				idcfg_set_error(device, IDCFG_ERR_UNEXPECTED_PACKET);
				return false;
			}
			/* 验证数据包长度 */
			if(packet->length < 5)
			{
				idcfg_set_error(device, IDCFG_ERR_INVALID_CFG_FORMAT);
				return false;
			}
        
			/* 根据Qi v1.3第8.4节解析CFG数据包 */
			cfg->reference_power = packet->data[0] & 0x3F;    /* 参考功率：6位，单位0.5W */
			cfg->count = packet->data[2] & 0x07;             /* 可选包数量：3位 */
			/* 窗口大小：以4ms为单位（Qi v1.3第8.4节） */
			cfg->window_size = (packet->data[3] >> 3) * 4;   /* 窗口大小（ms） */
			cfg->window_offset = (packet->data[3] & 0x07) * 4;    /* 窗口偏移（ms） */
			cfg->neg_bit = (packet->data[4] >> 7) & 0x01;   /* 协商位：1=EPP，0=BPP */
			cfg->max_power_w = cfg->reference_power / 2;     /* 最大功率（W）=参考功率/2 */
			cfg->depth = (packet->data[4] & 0x70) >> 4;      /* 谐振深度：用于EPP FSK调制参数 */

			/* 验证可选包数量 */
			if (cfg->count != mod->optional_count)
			{
				/* 接收到的可选包数必须与CFG报文中声明的一致 */
				idcfg_set_error(device, IDCFG_ERR_INVALID_COUNT);
				return false;
			}
        
			/* 验证窗口大小（Qi v1.3第8.4节：>=2） */
			if (cfg->window_size < 2)
			{
				idcfg_set_error(device, IDCFG_ERR_INVALID_CFG_FORMAT);
				return false;
			}
			
			/* 根据Neg位设置模式并标记完成 */
			mod->mode = cfg->neg_bit ? QI_MODE_EPP : QI_MODE_BPP;    /* Neg=1表示需要功率协商 */
			if(mod->mode == QI_MODE_EPP)
			{
				/* EPP模式需要初始化FSK参数和响应模块 */
				qi_response_set_fsk_params(device, cfg->depth);
				qi_response_init(device, QI_RESPONSE_ACK, QI_T_RESPONSE_TIMEOUT, qi_idconfig_response_callback);
			}
			idcfg_set_state(device, IDCFG_STATE_COMPLETE);    /* 阶段完成 */
			return true;    /* 成功完成ID/Config阶段 */
    default:
		break;
	}
	return false;    /* 仍在处理中，未完成 */
}

/* =========================== 查询函数 =========================== */

/**
 * @brief 获取当前Qi模式
 * @param device 设备ID（0起始索引）
 * @return 当前模式（BPP或EPP）
 */
qi_mode_t qi_idconfig_get_mode(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return QI_MODE_BPP;
	return idcfg_mod[device].mode;
}

/**
 * @brief 获取当前ID/Config阶段状态
 * @param device 设备ID（0起始索引）
 * @return 当前状态
 */
idcfg_state_t qi_idconfig_get_state(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return IDCFG_STATE_WAIT_ID;
	return idcfg_mod[device].state;
}

/**
 * @brief 检查阶段是否完成
 * @param device 设备ID（0起始索引）
 * @return 如果完成返回true，否则返回false
 */
bool qi_idconfig_is_complete(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return false;
	return idcfg_mod[device].state == IDCFG_STATE_COMPLETE;
}

/**
 * @brief 检查是否发生错误
 * @param device 设备ID（0起始索引）
 * @return 如果有错误返回true，否则返回false
 */
bool qi_idconfig_has_error(uint8_t device)
{
	if(device >= WLC_LIB_DEVICE_CONT)
		return false;
	return idcfg_mod[device].state == IDCFG_STATE_ERROR;
}
