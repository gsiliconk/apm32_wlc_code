#include "Qi_Negotiation.h"
#include "Qi_Power_Delivery.h"
#include "Qi_Power_Transfer.h"
#include "wlc_timer.h"
#include "wlc_physical.h"
#include "wlc_lib_interal_functions.h"
/**
 * @file Qi_Negotiation.c
 * @brief Qi无线充电协商阶段实现
 * @details 负责Qi无线充电协议的协商阶段。处理来自接收端的数据包，
 *          解析协商数据，管理状态转换，并协调功率传输阶段。协商阶
 *          段在实际功率传输前，建立发射端与接收端之间的功率契约。
 */

/* Qi协商数据包类型 */
#define QI_NEG_END_NEFOTIATION          0x00 // 结束协商命令 - 表示协商阶段完成
#define QI_NEG_GUARANTEED_LOAD_POWER    0x01 // 保障负载功率请求 - 接收端请求最小功率保障
#define QI_NEG_RECEIVED_POWER_REPORTING 0x02 // 接收功率报告配置 - 设置功率报告格式
#define QI_NEG_FSK_CONFIGURATION        0x03 // FSK配置 - 配置频移键控参数
#define QI_NEG_REFERENCE_POWER          0x04 // 参考功率 - 设置协商的参考功率等级

/* 协商模块结构体数组，用于管理每个设备的状态 */
static negotiation_modult_t neg_mod[WLC_LIB_DEVICE_CONT];

/* ============================== 静态辅助函数 ============================== */
/**
 * @brief 解析异物检测（FOD）数据包
 * @param device 设备索引
 * @param data 指向FOD数据包数组的指针
 * @param pneg 指向协商模块结构体的指针
 * @details 处理FOD参考值（频率或Q因子）。
 *          对有效FOD数据发送确认，对无效数据发送否认。
 */
static void qi_negtiation_parse_fod_data(uint8_t device, const uint8_t *data, 
																				 negotiation_modult_t *pneg)
{
	if(data[0] != 0x00)
	{
		// 频率参考值
		wlc_lib_uart_printf("Re(freq) = %d\r\n",data[1]);
		pneg->response = QI_RESPONSE_ND;// 没有回复
	}
	else
	{
		// Q因子参考值
		wlc_lib_uart_printf("Qt(refe) = %d\r\n",data[1]);
		pneg->response = QI_RESPONSE_ACK;
	}
}
/**
 * @brief 解析通用请求（GRQ）数据包并确定响应
 * @param device 设备索引
 * @param data 数据包中的请求类型字节
 * @param pneg 指向协商模块结构体的指针
 * @details 处理身份标识（0x30）或能力（0x31）数据包的请求。
 *          对于未知请求类型返回不可用响应。
 */
static void qi_negtiation_parse_gerneralrq_data(uint8_t device, const uint8_t data,
																								negotiation_modult_t *pneg)
{
	switch(data)
	{
		case 0x30: // 请求身份标识数据包
			pneg->response = QI_RESPONSE_ID;
		break;
		case 0x31: // 请求能力数据包
			pneg->response = QI_RESPONSE_CAP;
		break;
		default:	 // 未知请求类型
			pneg->response = QI_RESPONSE_NOT_AVAILABLE;
		break;
	}
}
/**
 * @brief 解析特定请求（SRQ）数据包并配置协商参数
 * @param device 设备索引
 * @param data 指向SRQ数据包数组的指针
 * @param ctx 指向无线充电上下文结构体的指针
 * @param pneg 指向协商模块结构体的指针
 * @details 处理各种协商子命令，包括结束协商、
 *          功率保障、报告配置、FSK设置和参考功率。
 */
static void qi_negtiation_parse_specific_request_data(uint8_t device, const uint8_t *data, 
																											wlc_context_t *ctx, negotiation_modult_t *pneg)
{
	uint8_t srq_type = data[0];

	switch(srq_type)
	{
		case QI_NEG_END_NEFOTIATION:	// 验证变更计数匹配后再结束协商
			if(pneg->change_count == data[1])
			{
				pneg->next_phase = true;
				pneg->response = QI_RESPONSE_ACK;
				wlc_lib_uart_printf("End Negotiation Success\r\n");
			}
			else
			{
				pneg->next_phase = false;
				pneg->change_count = 0;
				wlc_lib_uart_printf("End Negotiation Error\r\n");
			}
		break;
		case QI_NEG_GUARANTEED_LOAD_POWER:	// 接收端请求保障最小功率
			pneg->change_count++;
			ctx->neg_data.rx_guaranteed_power = data[1] & 0x3F;
			wlc_lib_uart_printf("Request Load Power = %d W\r\n", ctx->neg_data.rx_guaranteed_power / 2);
			pneg->response = QI_RESPONSE_ACK;
		break;
		case QI_NEG_RECEIVED_POWER_REPORTING:	// 配置功率报告格式（24位或其它）
			if(data[1] == QI_PKT_RP)
			{
				pneg->change_count++;
				ctx->neg_data.rx_power_bits = QI_RX_POWER_BITS_24;
				pneg->response = QI_RESPONSE_ACK;
				wlc_lib_uart_printf("Receive Power Report = 24 Pos\r\n");
			}
			else
			{
				pneg->response = QI_RESPONSE_ND;
			}
		break;
		case QI_NEG_FSK_CONFIGURATION:	// 配置FSK调制深度参数
			qi_response_set_fsk_params(device, (data[1] & 0x07));
			pneg->change_count++;
			pneg->response = QI_RESPONSE_ACK;
		break;
		case QI_NEG_REFERENCE_POWER:	// 设置协商的新参考功率等级
			pneg->change_count++;
			ctx->cfg_data.max_power_w = ((data[1] & 0x3F) / 2);
			pneg->response = QI_RESPONSE_ACK;
			wlc_lib_uart_printf("New Reference Power: %d W\r\n",ctx->cfg_data.max_power_w);
		break;
		default:
			// 未知SRQ类型
			pneg->response = QI_RESPONSE_ND;
		break;
	}
}
/**
 * @brief 解析无线功率标识（WPID）数据包
 * @param device 设备索引
 * @param data 指向WPID数据包数组的指针
 * @param ctx 指向无线充电上下文结构体的指针
 * @param pneg 指向协商模块结构体的指针
 * @details 从高和低WPID数据包中提取并存储接收端的
 *          无线功率标识（24位值）。始终对有效WPID数据发送确认。
 */
static void qi_negtiation_parse_wpid_data(uint8_t device, const uint8_t *data, wlc_context_t *ctx, negotiation_modult_t *pneg)
{
	// 将3个字节组合成24位WPID值
	ctx->neg_data.rx_wpid = ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
	wlc_lib_uart_printf("PTx WPID = %x\r\n", ctx->neg_data.rx_wpid);
	pneg->response = QI_RESPONSE_ACK;
}

/**
 * @brief 响应传输完成后执行的回调函数
 * @details 如果协商未完成则重置协商定时器。
 *          传输完成时由qi_response_init调用。
 */
static void qi_negtiation_callback(void)
{
	negotiation_modult_t *pneg = &neg_mod[0];

	if(pneg->next_phase == false)
	{
		// 继续协商 - 为下一个数据包重置定时器
		pneg->timerId = wlc_timer_create(QI_T_NEGOTIATE_MIN);
	}
}

/* ================================= 全局函数 ================================= */
/**
 * @brief 初始化指定设备的Qi协商模块
 * @param device 设备索引（必须小于WLC_LIB_DEVICE_CONT）
 * @details 设置初始协商状态，重置计数器，并创建
 *          协商超时定时器。进入协商阶段时调用。
 */
void qi_negotiation_init(uint8_t device)
{
	negotiation_modult_t *pneg = NULL;
	Wlc_Assert_Param(device < WLC_LIB_DEVICE_CONT);

	pneg = &neg_mod[device];

	// 初始化协商状态标志位和计数器
	pneg->next_phase = false;
	pneg->change_count = 0;
	pneg->phase = QI_STATE_NEGGOTIATION;
	// 创建协商超时处理定时器
	pneg->timerId = wlc_timer_create(QI_T_NEGOTIATE_MIN);
}

/**
 * @brief 协商阶段处理收到的Qi数据包
 * @param device 设备索引
 * @param ctx 指向无线充电上下文结构体的指针
 * @param packet 指向接收到的Qi数据包结构的指针
 * @return true表示处理成功，false表示应退出协商
 * @details 协商阶段的主状态机。处理不同数据包
 *          类型（GRQ、FOD、SRQ、WPID等），解析数据，确定响应，
 *          并管理到功率传输的阶段转换。
 */
bool qi_negtiation_process(uint8_t device, wlc_context_t *ctx, const qi_packet_t *packet)
{
	bool need_response = false;	// 响应标志
	negotiation_modult_t *pneg = NULL;	// 指向本设备的 Negotiation 模块结构体的指针

	Wlc_Assert_Param(device < WLC_LIB_DEVICE_CONT);	// 参数校验宏
	Wlc_Assert_Param(packet != NULL);	// 参数校验宏

	pneg = &neg_mod[device];

	// 检查响应模块是否当前处于活跃状态（等待传输中）
	if(qi_response_active())
	{
		/* 等待中 - 响应时不处理新数据包 */
	}
	else if(pneg->next_phase == true)
	{
		// 协商完成，转换到校准/功率传输阶段
		ctx->neg_data.end_negotiation_flag = true;
		qi_pt_init(device, QI_PHASE_CALIBRATION, ctx->cfg_data.max_power_w,
							 ctx->contract.window_size_ms,
							 ctx->contract.window_offset_ms);
		qi_pt_start(device);
		// wlc_lib_printf("进入校准阶段\r\n");
	}
	else if(pneg->timerId && wlc_timer_is_expired(pneg->timerId))
	{
		// 协商超时 - 清理并退出
		wlc_timer_destroy(pneg->timerId);
		return false;
	}
	else if(packet->valid == true)
	{
		// 收到有效数据包 - 确定是否需要响应
		need_response = true;
		switch(packet->header)
		{
			case QI_PKT_GRQ:
				// 通用请求 - 返回ID或能力信息
				qi_negtiation_parse_gerneralrq_data(device, packet->data[0], pneg);
			break;
			case QI_PKT_FOD:
				// 异物检测数据
				qi_negtiation_parse_fod_data(device, packet->data, pneg);
			break;
			case QI_PKT_SRQ:
				// 特定请求 - 详细协商参数
				qi_negtiation_parse_specific_request_data(device, packet->data, ctx, pneg);
			break;
			case QI_PKT_WPID_HI:
			case QI_PKT_WPID_LO:
				// 无线功率标识数据包
				qi_negtiation_parse_wpid_data(device, packet->data, ctx, pneg);
			break;
			case QI_PKT_SIG:
			case QI_PKT_CHS:
			case QI_PKT_RP8:
			case QI_PKT_RP:
				// 功率传输阶段数据包 - 协商期间忽略
				need_response = false;
				if(pneg->phase == QI_STATE_NEGGOTIATION)
				{
					return false;
				}
				else
				{
					/* 在重新协商阶段，直接切换到功率传输阶段 */
				}
			break;
			case QI_PKT_EPT:
			case QI_PKT_PCH:
			case QI_PKT_NEGO:
			case QI_PKT_ID:
			case QI_PKT_XID:
				// 结束功率传输或协商期间无效的数据包
				need_response = false;
				return false;
			break;
			default:
				// 未知数据包类型 - 发送无数据响应
				pneg->response = QI_RESPONSE_ND;
			break;
		}
	}

	// 需要响应时发送响应
	if(need_response)
	{
		qi_response_init(device, pneg->response, QI_T_RESPONSE_TIMEOUT, qi_negtiation_callback);
	}
	return true;
}