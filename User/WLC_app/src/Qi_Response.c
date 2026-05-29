#include "Qi_Response.h"
#include "wlc_timer.h"
#include "Tomato_wlc.h"
#include "wlc_power_bridge.h"
/**
	* @file Qi_Response.c
	* @brief Qi无线充电响应处理实现
	* @details 本文件管理使用FSK（频移键控）调制传输Qi协议响应包。
	*          包含初始化、计时器管理、回调执行以及特定数据包生成
	*          （ACK、NAK、ID、Capability等）。
	*/

/* ============================== Qi身份响应包（ID Packet）相关定义 ============================== */
#define QI_RESPONSE_ID_HEAD          0x30	// 包头字节
#define QI_RESPONSE_ID_VERSION       0x20	// 版本字节
#define QI_RESPONSE_ID_BYTE1         0x55	// 第1字节
#define QI_RESPONSE_ID_BYTE2         0x62	// 第2字节
#define QI_RESPONSE_ID_CHECKSUM      (QI_RESPONSE_ID_HEAD^QI_RESPONSE_ID_VERSION^QI_RESPONSE_ID_BYTE1^QI_RESPONSE_ID_BYTE2)

/* ========================== Qi能力响应包（Capability Packet）相关定义 ========================== */
#define QI_RESPONSE_CAP_HEAD         0x31	// 包头字节
#define QI_RESPONSE_CAP_BYTE0_15W    0x1E	// 能力字节0
#define QI_RESPONSE_CAP_BYTE1_15W    0x1E	// 能力字节1
#define QI_RESPONSE_CAP_BYTE2        0x00	// 能力字节2
#define QI_RESPONSE_CAP_CHECKSUM     (QI_RESPONSE_CAP_HEAD^QI_RESPONSE_CAP_BYTE0_15W^QI_RESPONSE_CAP_BYTE1_15W^QI_RESPONSE_CAP_BYTE2)

/* 响应模块结构体数组，每个设备对应一个实例 */
static response_modult_t resp_mod[WLC_LIB_DEVICE_CONT];

/**
* @brief 辅助函数：获取简单响应的原始数据字节
* @param response 响应类型（ACK、NAK、ND）
* @return 对应响应类型的原始数据值
* 
* @details 返回简单响应的固定十六进制值：
*          - ACK → 0xFF
*          - NAK → 0x00
*          - ND  → 0x55
*/
static uint16_t qi_response_get_send_data(qi_response_t response)
{
	switch(response)
	{
		case QI_RESPONSE_ACK:
			return 0xFF;
		break;
		case QI_RESPONSE_NAK:
			return 0x00;
		break;
		case QI_RESPONSE_ND:
			return 0x55;
		break;
		default:
			/* 未知类型，返回0 */
		break;
	}
	return 0;
}

/* ==================================== Qi响应处理核心函数 ==================================== */
/**
* @brief 根据响应类型发送特定的Qi响应包
* @param device 设备索引
* @param response 响应类型（ACK、NAK、ND、ID、CAP等）
* 
* @details 函数根据响应类型构建数据包，然后调用底层FSK发送函数：
*          - ACK/NAK/ND：单字节响应，使用简化的FSK字节响应
*          - ID/CAP：5字节完整数据包，包含包头+数据+校验和
*          - NOT_AVAILABLE：3字节全零响应
* 
* @par 响应包格式：
* @code
* 简单响应 (1字节):
*   ┌──────┐
*   │   数据    │
*   │ 0xFF/00/55│
*   └──────┘
* 
* ID响应 (5字节):
*   ┌──┬──┬──┬──┬─────┐
*   │HEAD│VER │B1  │B2  │CHECKSUM│
*   │0x30│0x20│0x55│0x62│ 0x37   │
*   └──┴──┴──┴──┴─────┘
* 
* CAP响应 (5字节):
*   ┌──┬──┬──┬──┬─────┐
*   │HEAD│CAP0│CAP1│CAP2│CHECKSUM│
*   │0x31│0x1E│0x1E│0x00│  0x?   │
*   └──┴──┴──┴──┴─────┘
* @endcode
*/
void qi_response(uint8_t device, qi_response_t response)
{
	response_modult_t *presp = NULL;

	/* 参数校验 */
	Wlc_Assert_Param(device < WLC_LIB_DEVICE_CONT);

	presp = &resp_mod[device];
	presp->resp_len = 1;  /* 默认响应长度为1字节 */

	switch (response)
	{
		/*----------------------------------------------------------------------
		 * 简单响应：单字节FSK调制
		 * 用于快速确认/否定应答
		 *----------------------------------------------------------------------*/
		case QI_RESPONSE_ACK:
			presp->resp_data[0] = 0xFF;
			/* ACK：发送0xFF，表示确认应答 */
			tomato_wlc_fsk_byte_response(device, wlc_pwm_fsk_response_ack);
		break;
		case QI_RESPONSE_NAK:
			presp->resp_data[0] = 0x00;
			/* NAK：发送0x00，表示否定应答 */
			tomato_wlc_fsk_byte_response(device, wlc_pwm_fsk_response_nak);
		break;
		case QI_RESPONSE_ND:
			presp->resp_data[0] = 0x55;
			/* ND：无数据响应，发送0x55，表示无数据传输 */
			tomato_wlc_fsk_byte_response(device, wlc_pwm_fsk_response_nd);
		break;
           
		/*----------------------------------------------------------------------
		 * 身份响应（ID Packet）：5字节完整数据包
		 * 用于向发送端报告接收端身份标识
		 *----------------------------------------------------------------------*/
		case QI_RESPONSE_ID:
			/* 构建身份响应包（5字节） */
			presp->resp_data[0] = QI_RESPONSE_ID_HEAD;      /* 包头 0x30 */
			presp->resp_data[1] = QI_RESPONSE_ID_VERSION;   /* 版本 0x20 */
			presp->resp_data[2] = QI_RESPONSE_ID_BYTE1;     /* 字节1 0x55 */
			presp->resp_data[3] = QI_RESPONSE_ID_BYTE2;     /* 字节2 0x62 */
			presp->resp_data[4] = QI_RESPONSE_ID_CHECKSUM;  /* 校验和 */
			presp->resp_len = 5;
           
			/* 发送完整消息（多字节FSK调制） */
			tomato_wlc_fsk_send_msg(device, presp->resp_data, presp->resp_len);
		break;
           
		/*----------------------------------------------------------------------
		 * 能力响应（CAP Packet）：5字节完整数据包
		 * 用于向发送端报告接收端功率能力（如15W）
		 *----------------------------------------------------------------------*/
		case QI_RESPONSE_CAP:
			/* 构建能力响应包（5字节） */
			presp->resp_data[0] = QI_RESPONSE_CAP_HEAD;        /* 包头 0x31 */
			presp->resp_data[1] = QI_RESPONSE_CAP_BYTE0_15W;    /* 15W能力字节0 */
			presp->resp_data[2] = QI_RESPONSE_CAP_BYTE1_15W;    /* 15W能力字节1 */
			presp->resp_data[3] = QI_RESPONSE_CAP_BYTE2;        /* 能力字节2 */
			presp->resp_data[4] = QI_RESPONSE_CAP_CHECKSUM;     /* 校验和 */
			presp->resp_len = 5;
           
			/* 发送完整消息 */
			tomato_wlc_fsk_send_msg(device, presp->resp_data, presp->resp_len);
		break;
           
		/*----------------------------------------------------------------------
		 * 不可用响应：3字节全零
		 * 用于表示请求的能力或资源不可用
		 *----------------------------------------------------------------------*/
		case QI_RESPONSE_NOT_AVAILABLE:
			/* 发送3字节0x00表示不可用 */
			presp->resp_data[0] = 0x00;
			presp->resp_data[1] = 0x00;
			presp->resp_data[2] = 0x00;
			presp->resp_len = 3;
			tomato_wlc_fsk_send_msg(device, presp->resp_data, presp->resp_len);
		break;
		default:
			/* 未知响应类型，不执行任何操作 */
		break;
	}
}

/**
* @brief 配置FSK调制深度参数
* @param device 设备索引
* @param value 参数值，选择调制深度配置文件 (0x0 - 0x7)
* 
* @details 将输入值映射到特定的FSK深度常量：
*          - 0x0~0x3: 正向深度配置（P0-P3）
*          - 0x4~0x7: 负向深度配置（N0-N3）
* 
* @note 不同的调制深度会影响FSK信号的抗干扰能力和传输距离
*       需要根据实际硬件特性选择合适的深度值
*/
void qi_response_set_fsk_params(uint8_t device, uint8_t value)
{
	switch(value)
	{
		case 0x0:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_P0);
		break;
		case 0x1:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_P1);
		break;
		case 0x2:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_P2);
		break;
		case 0x3:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_P3);
		break;
		case 0x4:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_N0);
		break;
		case 0x5:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_N1);
		break;
		case 0x6:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_N2);
		break;
		case 0x7:
			tomato_wlc_fsk_set_depth(device, QI_FSK_DEPTH_N3);
		break;
		default:
			/* 无效参数，不做任何修改 */
		break;
	}
}

/**
* @brief 初始化Qi响应模块（针对特定设备）
* @param device 设备索引（必须小于WLC_LIB_DEVICE_CONT）
* @param response 要发送的Qi响应类型（如ACK、ID、CAP等）
* @param timeout 超时值（毫秒）。若为0，立即发送，不使用定时器。
* @param callback 传输完成时的回调函数指针
* @note 此函数是响应发送的入口点，通常在接收到ASK数据包后调用
*/
void qi_response_init(uint8_t device, qi_response_t response, uint32_t timeout, Response_Callback_t callback)
{
	response_modult_t *presp = NULL;

	/* 参数断言，确保设备索引有效 */
	Wlc_Assert_Param(device < WLC_LIB_DEVICE_CONT);

	presp = &resp_mod[device];

	/* 存储响应类型、回调函数，设置初始状态 */
	presp->response = response;
	presp->callback = callback;
	presp->active = true;      /* 标记为激活状态 */
	presp->start = false;      /* 尚未开始发送 */

	if(timeout)
	{
		/* 指定了超时值：创建定时器，用于超时重发 */
		presp->timerId = wlc_timer_create(timeout);
	}
	else
	{
		/* 无超时：立即标记为已启动并发送响应 */
		presp->start = true;
		qi_response(device, presp->response);
	}
}

/**
* @brief 停止当前的Qi响应过程
* @details 执行以下操作：
*          1. 将响应模块标记为非激活状态
*          2. 清除所有挂起的FSK标志
* 
* @note 通常在通信异常或需要中断当前响应时调用
*/
void qi_response_stop(void)
{
	response_modult_t *presp = &resp_mod[0];
	
	/* 标记为非激活状态 */
	presp->active = false;
	
	/* 清除FSK硬件标志 */
	tomato_wlc_clear_fsk_flag(0);
}

/**
* @brief 轮询函数：检查活跃响应状态并处理完成事件
* @return true - 响应模块处于激活状态
* @return false - 响应模块已停用
* 
* @details 函数工作流程：
*          1. 获取当前时间戳
*          2. 检查响应模块是否激活
*          3. 若已开始：检查FSK传输是否完成
*             - 完成：清除标志，执行回调，销毁定时器
*          4. 若未开始：检查定时器是否到期
*             - 到期：标记为已启动，重试发送响应
*/
bool qi_response_active(void)
{
	response_modult_t *presp = &resp_mod[0];
	uint32_t now = wlc_timer_get_timestamp();

	if(presp->active)
	{
		if(presp->start)
		{
			/* 正在等待传输完成：检查FSK传输标志 */
			if(tomato_wlc_get_fsk_flag(0))
			{
				/* 硬件FSK传输已完成 */

				/* 清除硬件标志，准备下一次传输 */
				tomato_wlc_clear_fsk_flag(0);

				/* 标记为非激活状态 */
				presp->active = false;

				/* 执行回调函数（若已注册） */
				if(presp->callback != NULL)
				{
					/* 打印调试信息：时间戳 + 响应数据 */
					wlc_lib_uart_printf("[Tx]  [%dMs]  ", now);
					for(uint8_t i = 0; i < presp->resp_len; i++)
					{
						wlc_lib_uart_printf("%x  ", presp->resp_data[i]);
					}
					wlc_lib_uart_printf("\r\n");
                   
					/* 调用用户回调函数 */
					presp->callback();
				}
               
				/* 清理定时器资源 */
				wlc_timer_destroy(presp->timerId);
			}
		}
		else
		{
			/* 尚未开始传输：检查定时器是否到期 */
			if(wlc_timer_is_expired(presp->timerId))
			{
				/* 定时器到期，标记为已启动并重试发送 */
				presp->start = true;
				qi_response(0, presp->response);
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}