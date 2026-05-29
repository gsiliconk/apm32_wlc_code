#include "wlc_physical.h"
#include "wlc_lib_interal_functions.h"
#include "wlc_lib.h"
#include "wlc_types.h"
#include "wlc_app.h"
/*
 * 文件: wlc_physical.c
 * 描述: WLC 物理层实现 - ASK 解调
 */
 
/* ====================== 宏定义 - ASK 解调参数 ====================== */
#define MAX_CHANNELS                2           /* 每个设备的最大通道数 */
#define MAX_PACKET_LENGTH           27          /* 最大数据包负载长度 */
#define MIN_PULSE_WIDTH             130         /* 最小脉冲宽度（微秒） */
#define MAX_PULSE_WIDTH             630         /* 最大脉冲宽度（微秒） */
#define PREAMBLE_MIN_COUNT          11          /* 最小前导码位数 */
#define PREAMBLE_MAX_COUNT          25          /* 最大前导码位数 */
#define MAX_ERROR_COUNT             3           /* 重置前的最大错误计数 */
#define STOP_BIT_VALUE              1           /* 停止位值 */
#define START_BIT_VALUE             0           /* 起始位值 */

/* ============================ 枚举类型 ============================ */
/**
 * @brief ASK 解调状态机状态
 */
typedef enum {
    STATE_IDLE = 0,                             /* 空闲状态 - 等待前导码 */
    STATE_PREAMBLE,                             /* 前导码检测状态 */
    STATE_HEADER,                               /* 包头接收状态 */
    STATE_DATA,                                 /* 数据接收状态 */
    STATE_CHECKSUM                              /* 校验和验证状态 */
} ask_state_t;

/**
 * @brief 状态处理结果枚举
 */
typedef enum {
    STATE_RESULT_CONTINUE = 0,                  /* 继续当前状态处理 */
    STATE_RESULT_RESET    = 1                   /* 重置状态机到空闲状态 */
} state_result_t;

/* ====================== ASK 处理器结构体定义 ====================== */
/**
 * @brief ASK 解调处理器结构体
 */
typedef struct {
    ask_state_t	state;                          /* 当前状态机状态 */
    uint8_t			device;                         /* 设备号 */
    uint8_t			channel;                        /* 通道号 */
    uint8_t			preamble_count;                 /* 连续1比特计数 */
    uint8_t			bit_count;                      /* 字节内比特计数（0-10） */
    uint8_t			byte_count;                     /* 数据字节计数 */
    uint8_t			packet_type;                    /* 数据包包头类型 */
    uint8_t			data_length;                    /* 数据负载长度 */
    uint8_t			data[MAX_PACKET_LENGTH];        /* 数据缓冲区 */
    uint8_t			checksum;                       /* 校验和字节 */
    uint8_t			error_count;                    /* 错误计数器 */
    uint8_t			last_bit1_detected;             /* Bit 1 检测标志 */
    uint8_t			parity_bit;                     /* 奇偶校验位 */
} ask_handler_t;

/* =========================== 全局变量定义 =========================== */
/* 所有设备和通道的 ASK 处理器实例 */
static ask_handler_t ask_handlers[WLC_LIB_DEVICE_CONT][MAX_CHANNELS];

/* 每个设备的 ASK 使能标志 */
static uint8_t ask_enable[2] = {false, false};

/* ===================== 静态函数 - 比特解调函数 ===================== */
/**
 * @brief 根据脉冲宽度解码比特
 * @param handler 状态机指针
 * @param pwidth 脉冲宽度（微秒）
 * @param bit_value 存储解码后比特值的指针
 * @return 成功解码返回 1，否则返回 0
 */
static uint8_t wlc_ask_demod_decode_bit(ask_handler_t *handler, uint32_t pwidth, uint8_t *bit_value)
{
	/* 解码 Bit 1（短脉冲） */
	if(pwidth >= MIN_PULSE_WIDTH && pwidth <= 340)
	{
		handler->error_count = 0;
		if(handler->last_bit1_detected)//上一个短脉冲正确
		{
			*bit_value = 1;
			handler->last_bit1_detected = 0;
			return 1;
		}
		else
		{
			handler->last_bit1_detected = 1;
			return 0;//等待下一个短脉冲
		}
	}
	/* 解码 Bit 0（长脉冲） */
	else if(pwidth >= 370 && pwidth <= MAX_PULSE_WIDTH)
	{
		*bit_value = 0;
		handler->error_count = 0;
		handler->last_bit1_detected = 0;
		return 1;
	} 
	/* 无效脉冲宽度 */
	else
	{
		handler->error_count++;
		if(handler->error_count > MAX_ERROR_COUNT)
		{
			handler->error_count = 0;
			handler->state = STATE_IDLE;
		}
		return 0;
	}
}

/* ===================== 静态函数 - 数据解析函数 ===================== */
/**
 * @brief 解析数据位（帧的第0-10位）
 * @param handler 状态机指针
 * @param bit_value 当前比特值
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 */
static state_result_t parse_data_bits(ask_handler_t *handler, uint8_t bit_value)
{
	/* 帧格式：起始位(0) | D0 D1 D2 D3 D4 D5 D6 D7 | 奇偶位 | 停止位(1) */
	if(handler->bit_count == 0)
	{
		/* 起始位必须为0 */
		if(bit_value != START_BIT_VALUE)
		{
			return STATE_RESULT_RESET;
		}
		handler->bit_count++;		/* 进入数据位接收阶段 */
	}
	else if(handler->bit_count >= 1 && handler->bit_count <= 8)
	{
		/* 数据位（8位），LSB 优先 */
		if(handler->state == STATE_HEADER)
		{
			handler->packet_type |= bit_value << (handler->bit_count - 1);	/* 组包头类型字段 */
		} 
		else if (handler->state == STATE_CHECKSUM)
		{
			handler->checksum |= bit_value << (handler->bit_count - 1);		/* 组校验和字段 */
		} 
		else
		{
			handler->data[handler->byte_count] |= bit_value << (handler->bit_count - 1);	/* 组普通数据字段 */
		}
		handler->bit_count++;		/* 继续接收下一位 */
	}
	else if(handler->bit_count == 9)
	{
		/* 奇偶校验位 */
		handler->parity_bit = bit_value;	/* 保存当前字节的奇偶校验位 */
		handler->bit_count++;			/* 进入停止位接收阶段 */
	} 
	else if (handler->bit_count == 10)
	{
		/* 停止位必须为1 */
		if (bit_value != STOP_BIT_VALUE)
		{
			return STATE_RESULT_RESET;
		}
		handler->bit_count = 0;		/* 当前字节接收完成，复位位计数 */
	}
	return STATE_RESULT_CONTINUE;	/* 当前位解析正常，继续状态机流程 */
}

/**
 * @brief 验证接收字节的奇偶校验
 * @param handler 状态机指针
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 */
static state_result_t verify_parity(ask_handler_t *handler)
{
	uint8_t data_byte;      // 要验证的数据字节
	bool parity = true;     // 奇偶校验结果，初始值true表示偶数个1
    
	/* 根据当前状态获取数据字节 */
	if(handler->state == STATE_HEADER)
	{
		// 处于包头状态时，验证包类型字段的奇偶校验
		data_byte = handler->packet_type;
	} 
	else if(handler->state == STATE_DATA)
	{
		// 处于数据状态时，验证当前数据字节的奇偶校验
		data_byte = handler->data[handler->byte_count];
	} 
	else if(handler->state == STATE_CHECKSUM)
	{
		// 处于校验和状态时，验证校验和字节的奇偶校验
		data_byte = handler->checksum;
	} 
	else
	{
		// 其他状态（如STATE_IDLE、STATE_COMPLETE）不需要校验
		return STATE_RESULT_CONTINUE;
	}

	/* 计算奇偶校验 */
	while(data_byte)
	{
		parity = !parity;	// 每检测到一个1，翻转奇偶结果
		data_byte = (data_byte - 1) & data_byte;	// 消除最低位的1
	}

	/* 与接收到的奇偶校验位比较 */
	if(parity != handler->parity_bit)
	{
		// 奇偶校验失败：数据传输可能出错，协议状态机需要复位
		return STATE_RESULT_RESET;
	}
	// 奇偶校验成功，继续处理下一个字节
	return STATE_RESULT_CONTINUE;
}

/**
 * @brief 验证数据包校验和
 * @param handler 状态机指针
 * @param device 设备 ID
 * @param ch 通道 ID
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 */
static state_result_t validate_checksum(ask_handler_t *handler, uint8_t device, uint8_t ch)
{
	/* 计算异或校验和
	 * 从头字节到所有数据字节的异或校验
	 * XOR校验原理：
   *   - 相同字节异或结果为0：A ^ A = 0
   *   - 任何字节与0异或不变：A ^ 0 = A
   *   - 异或满足交换律：A ^ B ^ C = A ^ C ^ B
   *   
   * 校验和 = packet_type ^ data[0] ^ data[1] ^ ... ^ data[n-1]
	 */
	uint8_t calculated_checksum = handler->packet_type;	// 初始化为包类型
	for(int i = 0; i < handler->data_length; i++)
	{
		calculated_checksum ^= handler->data[i];	// 依次异或每个数据字节
	}

	uint8_t received_checksum = handler->checksum;	// 获取接收到的校验和
    
	/* 验证校验和 */
	if(calculated_checksum != received_checksum)
	{
//		wlc_lib_uart_printf("[M%d][CH%d] 校验和错误: 计算值=0x%02X, 接收值=0x%02X\n", 
//												device, ch, calculated_checksum, received_checksum);
		return STATE_RESULT_RESET;
	}

	/* 校验和有效 - 调用应用回调函数 */
	wlc_app_data_received_callback(device, ch, handler->packet_type, handler->data, handler->data_length);
	
	// 数据包处理完成，恢复空闲状态，等待下一个数据包
	handler->state = STATE_IDLE;
	return STATE_RESULT_CONTINUE;
}

/* ===================== 静态函数 - 状态处理函数 ===================== */
/**
 * @brief 处理空闲状态 - 等待前导码
 * @param handler 状态机指针
 * @param bit_value 接收到的比特值（0或1）
 * @param device 设备 ID
 * @param ch 通道 ID
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 * @note 空闲状态监听总线，等待前导码(1010...)的开始
 */
static state_result_t handle_idle_state(ask_handler_t *handler, uint8_t bit_value, uint8_t device, uint8_t ch)
{
	/* 检测前导码起始：第一个"1"比特表示数据包开始
   * 时序图：
   * IDLE ___|￣￣￣|___ PREAMBLE
   *        bit=1 检测到
   */
	if(bit_value == 1)
	{
		/* 清空状态机，准备接收新数据包 */
		memset(handler, 0, sizeof(ask_handler_t));
		handler->state = STATE_PREAMBLE;		// 进入前导码检测状态
		handler->preamble_count = 1;				// 已检测到1个前导码比特
		handler->bit_count = 0;							// 比特计数器清零
		handler->checksum = 0;							// 校验和清零
	}
	// 校验和清零
	return STATE_RESULT_CONTINUE;
}

/**
 * @brief 处理前导码状态 - 检测前导码比特序列
 * @param handler 状态机指针
 * @param bit_value 接收到的比特值（0或1）
 * @param device 设备 ID
 * @param ch 通道 ID
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 * @note 前导码格式：101010...
 */
static state_result_t handle_preamble_state(ask_handler_t *handler, uint8_t bit_value, uint8_t device, uint8_t ch)
{
	if(bit_value == 1)
	{
		/* 收到"1"，前导码计数+1 */
		handler->preamble_count++;
		// 检查前导码是否过长（超过最大允许数量）
		if(handler->preamble_count > PREAMBLE_MAX_COUNT)
		{
			// 前导码过长，可能不是有效数据包，复位状态机
			return STATE_RESULT_RESET;
		}
	} 
	else if(bit_value == 0)
	{
		/* 收到"0"，前导码结束，检查是否满足最小要求 */
		if (handler->preamble_count >= PREAMBLE_MIN_COUNT)
		{
			handler->state = STATE_HEADER;	// 进入包头接收状态
			handler->bit_count = 1;					// 开始接收第一个比特
			handler->byte_count = 0;				// 字节计数清零
			memset(handler->data, 0, MAX_PACKET_LENGTH);	// 清空数据缓冲区
		} 
		else
		{
			// 前导码太短，不是有效数据包，复位状态机
			return STATE_RESULT_RESET;
		}
	}
	else
	{
		// 无效的比特值（既不是0也不是1），复位状态机
		return STATE_RESULT_RESET;
	}
	return STATE_RESULT_CONTINUE;
}

/**
 * @brief 处理包头状态 - 接收并解析数据包头
 * @param handler 状态机指针
 * @param bit_value 接收到的比特值（0或1）
 * @param device 设备 ID
 * @param ch 通道 ID
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 */
static state_result_t handle_header_state(ask_handler_t *handler, uint8_t bit_value, uint8_t device, uint8_t ch)
{
	/* 解析比特数据 */
	if(parse_data_bits(handler, bit_value) == STATE_RESULT_RESET)
	{
		return STATE_RESULT_RESET;
	}
  
	/* 一个字节接收完成 */
	if(handler->bit_count == 0 && handler->packet_type != 0)
	{
		// 验证包头的奇偶校验位
		if(verify_parity(handler) == STATE_RESULT_RESET)
		{
			return STATE_RESULT_RESET;
		}
    
		/* 根据包类型计算数据长度 */
		handler->state = STATE_DATA;	// 进入数据接收状态
		handler->bit_count = 0;				// 重置比特计数器
		handler->byte_count = 0;			// 重置字节计数器

		/* 根据包头类型计算数据长度 
   	 * 包类型范围         数据长度公式
   	 * 0x00-0x1F (31)     1 + (type - 0x00) / 32 = 1 字节
   	 * 0x20-0x7F (96)     2 + (type - 0x20) / 16 = 2~7 字节
   	 * 0x80-0xDF (96)     8 + (type - 0x80) / 8  = 8~19 字节
   	 * 0xE0-0xFF (32)     20 + (type - 0xE0) / 4 = 20~27 字节
		 */
		if(handler->packet_type <= 0x1F)
		{
			handler->data_length = 1 + (handler->packet_type - 0x00) / 32;
		} 
		else if(handler->packet_type <= 0x7F)
		{
			handler->data_length = 2 + (handler->packet_type - 0x20) / 16;
		} 
		else if(handler->packet_type <= 0xDF)
		{
			handler->data_length = 8 + (handler->packet_type - 0x80) / 8;
		} 
		else
		{
			handler->data_length = 20 + (handler->packet_type - 0xE0) / 4;
		}
		/* 检查数据长度是否在有效范围内 */
		if(handler->data_length > 0 && handler->data_length < MAX_PACKET_LENGTH)
		{
			// 清空数据缓冲区，准备接收数据
			memset(handler->data, 0, MAX_PACKET_LENGTH);
		}
	}
	return STATE_RESULT_CONTINUE;
}

/**
 * @brief 处理数据状态 - 接收数据包数据
 * @param handler 状态机指针
 * @param bit_value 接收到的比特值（0或1）
 * @param device 设备 ID
 * @param ch 通道 ID
 * @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
 */
static state_result_t handle_data_state(ask_handler_t *handler, uint8_t bit_value, uint8_t device, uint8_t ch)
{
	/* 解析比特数据 */
	if(parse_data_bits(handler, bit_value) == STATE_RESULT_RESET)
	{
		return STATE_RESULT_RESET;
	}
  
	/* 一个数据字节接收完成 */
	if(handler->bit_count == 0)
	{
		// 验证当前数据字节的奇偶校验位
		if(verify_parity(handler) == STATE_RESULT_RESET)
		{
			return STATE_RESULT_RESET;
		}

		handler->byte_count++; // 数据字节计数+1

		/* 检查是否已接收完所有数据字节 */
		if(handler->byte_count == handler->data_length)
		{
			handler->state = STATE_CHECKSUM;	// 进入校验和接收状态
			handler->bit_count = 0;						// 重置比特计数器
			handler->checksum = 0;						// 清空校验和暂存区
		}
	}
	return STATE_RESULT_CONTINUE;
}

/**
* @brief 处理校验和状态 - 接收并验证校验和
* @param handler 状态机指针
* @param bit_value 接收到的比特值（0或1）
* @param device 设备 ID
* @param ch 通道 ID
* @return STATE_RESULT_CONTINUE 或 STATE_RESULT_RESET
*/
static state_result_t handle_checksum_state(ask_handler_t *handler, uint8_t bit_value, uint8_t device, uint8_t ch)
{
	/* 解析校验和字节的比特数据 */
	if(parse_data_bits(handler, bit_value) == STATE_RESULT_RESET)
	{
		return STATE_RESULT_RESET;
	}
  
	/* 校验和字节接收完成 */
	if(handler->bit_count == 0)
	{
		// 验证校验和字节的奇偶校验位
		if (verify_parity(handler) == STATE_RESULT_RESET)
		{
			return STATE_RESULT_RESET;
		}
		
		// 验证整个数据包的校验和
		return validate_checksum(handler, device, ch);
	}
	return STATE_RESULT_CONTINUE;
}

/* ==================== 全局函数 - ASK 解调使能/禁用 ==================== */
/**
 * @brief 为指定设备使能/禁用 ASK 解调
 * @param device 设备 ID
 * @param endis 使能（1）或禁用（0）
 */
void wlc_ask_demod_enable(uint8_t device, uint8_t endis)
{  
	/* 参数边界检查 */
	if(device >= WLC_LIB_DEVICE_CONT)
	{  
		return;  
	}  
      
	/* 初始化所有通道的状态机 */
	for(int c = 0; c < MAX_CHANNELS; c++)
	{  
		memset(&ask_handlers[device][c], 0, sizeof(ask_handler_t));  
		ask_handlers[device][c].state = STATE_IDLE;  
	}  
      
	/* 设置使能标志 */
	ask_enable[device] = endis;
    
	/* 调用硬件使能函数 */
	wlc_lib_irq_enable(device, endis);
    
//	wlc_lib_uart_printf("ASK 解调 %s 设备 %d\r\n", endis ? "已使能" : "已禁用", device);
}

/* =================== 全局函数 - 主 ASK 解调处理函数 =================== */
/**
 * @brief ASK 解调主函数 - 处理脉冲并解码比特
 * @param device 设备 ID
 * @param ch 通道 ID
 * @param pwidth 脉冲宽度（微秒）
 */
void wlc_ask_demod_handler(uint8_t device, uint8_t ch, uint32_t pwidth)
{
	/* 检查 ASK 是否使能 */
	if(ask_enable[device] != true)
		return;
    
//	// 加这一行，打印脉冲宽度
//      wlc_lib_uart_printf("pw=%d\r\n", pwidth);
	
	/* 验证脉冲宽度范围 */
	if(pwidth < MIN_PULSE_WIDTH || pwidth > MAX_PULSE_WIDTH)
		return;
    
	/* 验证参数 */
	if (ch >= MAX_CHANNELS || device >= WLC_LIB_DEVICE_CONT)
		return;

	/* 获取当前通道的状态机实例 */
	ask_handler_t* handler = &ask_handlers[device][ch];

	/* 解码脉冲宽度为比特值 */
	uint8_t bit_value = 2; // // 初始化为无效值（2不是有效比特）
	uint8_t decoded = wlc_ask_demod_decode_bit(handler, pwidth, &bit_value);

	/* 检查是否解码到有效比特 */
	if(!decoded || bit_value > 1)
		return;

	/* 处理状态机 */
	state_result_t result = STATE_RESULT_CONTINUE; // 默认结果为继续
	switch(handler->state)
	{
		case STATE_IDLE: // 空闲状态
			result = handle_idle_state(handler, bit_value, device, ch);
		break;
		case STATE_PREAMBLE: // 前导码检测状态
			result = handle_preamble_state(handler, bit_value, device, ch);
		break;
		case STATE_HEADER: // 包头接收状态
			result = handle_header_state(handler, bit_value, device, ch);
		break;
		case STATE_DATA: // 数据接收状态
			result = handle_data_state(handler, bit_value, device, ch);
		break;
		case STATE_CHECKSUM: // 校验和接收状态
			result = handle_checksum_state(handler, bit_value, device, ch);
		break;
		default: // 应重置状态机
			result = STATE_RESULT_RESET;
		break;
	}
    
	/* 如果需要重置状态机 */
	if(result == STATE_RESULT_RESET)
		handler->state = STATE_IDLE;
}
