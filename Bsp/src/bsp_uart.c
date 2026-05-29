#include "bsp_uart.h"

#define UART_TX_BUFFER_SIZE 1024        					// UART发送缓冲区大小
static uint8_t tx_buffer[UART_TX_BUFFER_SIZE];  	// UART发送数据缓存
static ring_buffer_t tx_ring_buffer;    					// 环形缓冲区管理结构体
static volatile bool dma_transmitting = false;  	// DMA是否正在发送
static volatile uint16_t last_dma_buffer_size = 0;// 上次DMA发送的数据长度

void bsp_uart_init(void)
{
	GPIO_Config_T gpio;
	USART_Config_T usart;
	DMA_Config_T dma;

	// 使能相关时钟
	RCM_EnableAPB2PeriphClock((RCM_APB2_PERIPH_T)(RCM_APB2_PERIPH_GPIOB | RCM_APB2_PERIPH_USART1 | RCM_APB2_PERIPH_AFIO)); // 添加AF时钟
	RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_DMA1);
	
	// USART1 重映射: PA9/PA10 → PB6/PB7
	GPIO_ConfigPinRemap(GPIO_REMAP_USART1);  // 使能USART1重映射
	
	/* ── PB6 = TX, AF推挽 ── */
	gpio.pin   = GPIO_PIN_6;
	gpio.mode  = GPIO_MODE_AF_PP;
	gpio.speed = GPIO_SPEED_50MHz;
	GPIO_Config(GPIOB, &gpio);
	
	/* ── USART: 115200, 8N1, 只发 ── */
	USART_ConfigStructInit(&usart);
	usart.baudRate     = 115200u;
	usart.wordLength   = USART_WORD_LEN_8B;
	usart.stopBits     = USART_STOP_BIT_1;
	usart.parity       = USART_PARITY_NONE;
	usart.mode         = USART_MODE_TX;
	usart.hardwareFlow = USART_HARDWARE_FLOW_NONE;
	USART_Config(USART1, &usart);
	
	// 初始化发送缓冲区
	ring_buffer_init(&tx_ring_buffer, tx_buffer, UART_TX_BUFFER_SIZE);
	
	/* ── DMA: 内存→USART, 单次模式, 字节 ── */
	dma.bufferSize         = 0;
	dma.dir                = DMA_DIR_PERIPHERAL_DST;
	dma.peripheralInc      = DMA_PERIPHERAL_INC_DISABLE;
	dma.memoryInc          = DMA_MEMORY_INC_ENABLE;
	dma.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_BYTE;
	dma.memoryDataSize     = DMA_MEMORY_DATA_SIZE_BYTE;
	dma.loopMode           = DMA_MODE_NORMAL;
	dma.priority           = DMA_PRIORITY_HIGH;
	dma.M2M                = DMA_M2MEN_DISABLE;
	dma.peripheralBaseAddr = (uint32_t)&(USART1->DATA);
	dma.memoryBaseAddr     = 0;  /* 发送时动态指定 */
	DMA_Config(DMA1_Channel4, &dma);
	
	// 使能 USART
	USART_Enable(USART1);
	
	// 使能 USART DMA 发送
	USART_EnableDMA(USART1, USART_DMA_TX);
    
	// 配置并使能 DMA 中断
	DMA_EnableInterrupt(DMA1_Channel4, DMA_INT_TC);
	NVIC_EnableIRQRequest(DMA1_Channel4_IRQn, 4, 0);
    
	// 确保DMA通道初始状态正确
	DMA_Disable(DMA1_Channel4);
	dma_transmitting = false;
}

// 启动DMA传输的通用函数
static void start_dma_transmission(void)
{
	// 检查缓冲区是否为空
	if (ring_buffer_empty(&tx_ring_buffer))
	{
		dma_transmitting = false;
		return;
	}
	// 获取线性块大小
	uint16_t size = ring_buffer_linear_block_size(&tx_ring_buffer);
	// 检查是否有数据需要传输
	if (size > 0)
	{
		// 禁用DMA通道后再重新配置
		DMA_Disable(DMA1_Channel4);
        
		DMA_Config_T dma;
		dma.bufferSize = size;
		dma.dir = DMA_DIR_PERIPHERAL_DST;
		dma.peripheralInc = DMA_PERIPHERAL_INC_DISABLE;
		dma.memoryInc = DMA_MEMORY_INC_ENABLE;
		dma.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_BYTE;
		dma.memoryDataSize = DMA_MEMORY_DATA_SIZE_BYTE;
		dma.loopMode = DMA_MODE_NORMAL;
		dma.priority = DMA_PRIORITY_HIGH;
		dma.M2M = DMA_M2MEN_DISABLE;
		dma.memoryBaseAddr = (uint32_t)&tx_buffer[tx_ring_buffer.head];
		dma.peripheralBaseAddr = (uint32_t)&(USART1->DATA);
        
		DMA_Config(DMA1_Channel4, &dma);
		last_dma_buffer_size = size;
        
		// 重新使能DMA中断（确保中断使能）
		DMA_EnableInterrupt(DMA1_Channel4, DMA_INT_TC);
        
		DMA_Enable(DMA1_Channel4);
		dma_transmitting = true;
	}
	else
	{
		dma_transmitting = false;
	}
}

// DMA中断处理程序
void DMA1_Channel4_IRQHandler(void)
{
	if (DMA_ReadIntFlag(DMA1_INT_FLAG_TC4))
	{
		// 重要：顺序不能错
		// 1. 先保存传输大小
		uint16_t transmitted_size = last_dma_buffer_size;
        
		// 2. 更新环形缓冲区的head指针
		if (dma_transmitting && transmitted_size > 0)
		{
			tx_ring_buffer.head = (tx_ring_buffer.head + transmitted_size) % tx_ring_buffer.size;
		}
        
		// 3. 清除传输标志
		dma_transmitting = false;
		last_dma_buffer_size = 0;
        
		// 4. 清除传输完成中断标志（必须在最后）
		DMA_ClearIntFlag(DMA1_INT_FLAG_TC4);
        
		// 5. 立即检查是否还有数据需要发送
		if (!ring_buffer_empty(&tx_ring_buffer))
		{
			start_dma_transmission();
		}
	}
}

// 批量发送数据函数
void uart_send_byte(uint8_t *data, uint16_t len)
{
	if (data == NULL || len == 0)
	{
		return;
	}
    
	// 批量写入数据到环形缓冲区
	for (uint16_t i = 0; i < len; i++)
	{
		// 等待缓冲区有空间（添加超时保护）
		uint16_t timeout = 0;
		while (ring_buffer_full(&tx_ring_buffer))
		{
			uint8_t temp;
			if (!ring_buffer_get(&tx_ring_buffer, &temp))
			{
				timeout++;
				if (timeout > 10000)
				{
					break;
				}
			}
		}
        
		// 写入数据
		ring_buffer_put(&tx_ring_buffer, data[i]);
	}
    
	// 只有在DMA空闲时才尝试启动传输
	if (!dma_transmitting)
	{
		start_dma_transmission();
	}
}

// 单字节发送函数
void uart_send_single_byte(uint8_t data)
{
	// 等待缓冲区有空间（添加超时保护）
	uint16_t timeout = 0;
	while (ring_buffer_full(&tx_ring_buffer))
	{
		uint8_t temp;
		if (!ring_buffer_get(&tx_ring_buffer, &temp))
		{
			timeout++;
			if (timeout > 10000)
			{
				break;
			}
		}
	}
    
	// 写入数据
	ring_buffer_put(&tx_ring_buffer, data);

	// 只有在DMA空闲时才尝试启动传输
	if (!dma_transmitting)
	{
		start_dma_transmission();
	}
}

// UART printf 格式化输出函数
int bsp_uart_printf(char *fmt, va_list args)
{
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    uart_send_byte((uint8_t *)buf, (uint16_t)len);
    return len;
}