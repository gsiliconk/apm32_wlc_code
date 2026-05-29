#include "bsp_eeprom.h"

#define EEPROM_I2C              I2C2
#define EEPROM_I2C_CLK          RCM_APB1_PERIPH_I2C2
#define EEPROM_I2C_GPIO_CLK     RCM_APB2_PERIPH_GPIOB

#define EEPROM_I2C_SCL_PIN      GPIO_PIN_10
#define EEPROM_I2C_SDA_PIN      GPIO_PIN_11
#define EEPROM_I2C_GPIO         GPIOB

#define EEPROM_I2C_SPEED        100000      // 100kHz
#define EEPROM_TIMEOUT_COUNT    2000        // 增加超时时间
#define EEPROM_PAGE_SIZE        8           // AT24C02 页面大小
#define EEPROM_MAX_ADDR         255         // AT24C02 最大地址
#define EEPROM_WRITE_DELAY      0x1FFFF     // 每次写入页面后的延迟时间

/* ========================= 私有函数 ========================= */
/**
 * @brief 延迟
 */
static void eeprom_delay(uint32_t count)
{
    volatile uint32_t i;
    for(i = 0; i < count; i++);
}

/**
 * @brief 向 EEPROM 写入一页数据（内部函数）
 */
static eeprom_status_t eeprom_write_page(uint8_t addr, uint8_t* data, uint8_t len)
{
	uint32_t timeout;
	uint8_t i;
	
	/* 等待直到 I2C 总线空闲 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(I2C_ReadStatusFlag(EEPROM_I2C,I2C_FLAG_BUSBSY))
	{
		if((timeout--) == 0)
		{
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 产生起始条件 */
	I2C_EnableGenerateStart(EEPROM_I2C);
	/* 等待起始条件产生 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}

	/* 发送写操作的设备地址 */
	I2C_Tx7BitAddress(EEPROM_I2C, AT24C02_ADDR_WRITE, I2C_DIRECTION_TX);
	/* 等待地址发送完成 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}

	/* 发送内存地址 */
	I2C_TxData(EEPROM_I2C, addr);
	/* 等待字节传输完成 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}

	/* 写入数据字节 */
	for(i = 0; i < len; i++)
	{
		/* 发送数据字节 */
		I2C_TxData(EEPROM_I2C, data[i]);
		/* 等待字节传输完成 */
		timeout = EEPROM_TIMEOUT_COUNT;
		while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
		{
			if((timeout--) == 0)
			{
				I2C_EnableGenerateStop(EEPROM_I2C);
				return EEPROM_TIMEOUT;
			}
		}
	}

	/* 产生停止条件 */
	I2C_EnableGenerateStop(EEPROM_I2C);

	return EEPROM_SUCCESS;
}

/*!
 * @brief	等待 EEPROM 写周期完成（内部函数）
 */
static eeprom_status_t eeprom_wait_standby(void)
{
	volatile uint16_t status_reg;			// I2C 状态
	uint32_t retry_count = 0;					// 重试计数
	const uint32_t max_retry = 1000;  // 增加重试次数

	do
	{
		/* 产生起始条件 */
		I2C_EnableGenerateStart(EEPROM_I2C);
		
		/* 较长的延时以确保可靠检测 */
		eeprom_delay(0x7FF);

		/* 读取 I2C 状态寄存器 */
		status_reg = I2C_ReadRegister(EEPROM_I2C, I2C_REGISTER_STS1);
        
		/* 发送写操作的设备地址 */
		I2C_Tx7BitAddress(EEPROM_I2C, AT24C02_ADDR_WRITE, I2C_DIRECTION_TX);

		/* 较长的延时 */
		eeprom_delay(0x7FF);

		/* 读取 I2C 状态寄存器 */
		status_reg = I2C_ReadRegister(EEPROM_I2C, I2C_REGISTER_STS1);

		/* 检查是否发生 ACK 失败 */
		if((status_reg & 0x0400) == 0x0400)
		{
			/* 清除 ACK 失败标志 */
			I2C_ClearStatusFlag(EEPROM_I2C, I2C_FLAG_AE);

			/* 产生停止条件 */
			I2C_EnableGenerateStop(EEPROM_I2C);

			/* 重试前的小延时 */
			eeprom_delay(0x3FF);

			retry_count++;
			if (retry_count >= max_retry)
			{
				return EEPROM_TIMEOUT;
			}
		}
		else
		{
			/* EEPROM 已就绪，退出循环 */
			break;
		}
	}
	while (1);

	/* 产生停止条件 */
	I2C_EnableGenerateStop(EEPROM_I2C);

	/* 最终延时以确保 STOP 完成 */
	eeprom_delay(0x3FF);

	return EEPROM_SUCCESS;
}

/* ========================= 对外接口函数 ========================= */

/**
 * @brief 初始化EEPROM I2C接口
 */
void bsp_eeprom_init(void)
{
	GPIO_Config_T gpio;
	I2C_Config_T i2c;
	
	RCM_EnableAPB1PeriphClock(EEPROM_I2C_CLK);
	RCM_EnableAPB2PeriphClock(EEPROM_I2C_GPIO_CLK);
	eeprom_delay(50000);
	
	gpio.pin = EEPROM_I2C_SCL_PIN | EEPROM_I2C_SDA_PIN;
	gpio.mode = GPIO_MODE_IN_FLOATING; // 浮空
	gpio.speed = GPIO_SPEED_50MHz;
	GPIO_Config(EEPROM_I2C_GPIO, &gpio);
	eeprom_delay(50000);
	
	gpio.mode = GPIO_MODE_AF_OD; // 开漏
	GPIO_Config(EEPROM_I2C_GPIO, &gpio);
	eeprom_delay(50000);
	
	I2C_Disable(EEPROM_I2C); // 禁用i2c
	eeprom_delay(50000);
    
	I2C_Reset(EEPROM_I2C); // 重置i2c
	eeprom_delay(50000);
	
	i2c.mode = I2C_MODE_I2C;
	i2c.dutyCycle = I2C_DUTYCYCLE_2;
	i2c.ownAddress1 = 0x00;
	i2c.ack = I2C_ACK_ENABLE;
	i2c.ackAddress = I2C_ACK_ADDRESS_7BIT;
	i2c.clockSpeed = EEPROM_I2C_SPEED;
	I2C_Config(EEPROM_I2C, &i2c);
	eeprom_delay(50000);
	
	I2C_Enable(EEPROM_I2C); // 使能i2c
	eeprom_delay(50000);
}

/**
 * @brief 获取eeprom的起始地址
 */
uint32_t bsp_eeprom_get_start_addr(void)
{
    return 0;
}

/**
 * @brief 向EEPROM写入数据，自动处理页边界
 */
eeprom_status_t bsp_eeprom_write_data(uint32_t addr, uint8_t* data, uint32_t len)
{
	uint32_t data_index = 0;    // 数据指针
	uint8_t current_page_offset;// 当前页面偏移量
	uint8_t bytes_to_write;			// 要写入的字节
	eeprom_status_t status;			// 状态
	
	/* 检查参数 */
	if(data == NULL || len == 0)
	{
		return EEPROM_ERROR;
	}
	
	/* 检查地址是否溢出 */
	if((addr + len) > (EEPROM_MAX_ADDR + 1))
	{
		return EEPROM_ADDR_OVERFLOW;
	}
	
	/* 逐页写入数据 */
	while(len > 0)
	{
		/* 计算当前页内的偏移量 */
		current_page_offset = (uint8_t)(addr % EEPROM_PAGE_SIZE);
		/* 计算当前页内可以写入的字节数 */
		bytes_to_write = EEPROM_PAGE_SIZE - current_page_offset;
		
		/* 不要超过剩余数据长度 */
		if (bytes_to_write > len)
		{
			bytes_to_write = (uint8_t)len;
		}
		
		/* 写入当前页 */
		status = eeprom_write_page((uint8_t)addr,&data[data_index],bytes_to_write);
		if(status != EEPROM_SUCCESS)
		{
			return status;
		}
		/* 页写入后的关键延时 —— 确保写周期开始 */
		eeprom_delay(EEPROM_WRITE_DELAY);
		
		/* 等待写周期完成 */
		status = eeprom_wait_standby();
		if(status != EEPROM_SUCCESS)
		{
			return status;
		}
		
		/* 在下一次页写入之前添加额外的安全延时 */
		eeprom_delay(EEPROM_WRITE_DELAY / 2);

		/* 更新下一次迭代的计数器 */
		addr += bytes_to_write;
		data_index += bytes_to_write;
		len -= bytes_to_write;
	}
	return EEPROM_SUCCESS;
}

/**
 * @brief	从 EEPROM 读取数据，自动处理页边界
 */
eeprom_status_t bsp_eeprom_read_data(uint32_t addr, uint8_t* data, uint32_t len)
{
	uint32_t timeout;
	
	/* 检查参数 */
	if (data == NULL || len == 0)
	{
		return EEPROM_ERROR;
	}

	/* 检查地址是否溢出 */
	if ((addr + len) > (EEPROM_MAX_ADDR + 1))
	{
		return EEPROM_ADDR_OVERFLOW;
	}
	
	/* 等待直到 I2C 总线空闲 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(I2C_ReadStatusFlag(EEPROM_I2C,I2C_FLAG_BUSBSY))
	{
		if((timeout--) == 0)
		{
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 产生起始条件 */
	I2C_EnableGenerateStart(EEPROM_I2C);
	/* 等待起始条件产生 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 发送写操作的设备地址 */
	I2C_Tx7BitAddress(EEPROM_I2C, AT24C02_ADDR_WRITE, I2C_DIRECTION_TX);
	/* 等待地址发送完成 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 发送内存地址 */
	I2C_TxData(EEPROM_I2C, addr);
	/* 等待字节传输完成 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while(!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
	{
		if((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 产生重复起始条件 */
	I2C_EnableGenerateStart(EEPROM_I2C);
	/* 等待起始条件产生 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while (!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if ((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 发送读操作的设备地址 */
	I2C_Tx7BitAddress(EEPROM_I2C, AT24C02_ADDR_READ, I2C_DIRECTION_RX);
	/* 等待地址发送完成 */
	timeout = EEPROM_TIMEOUT_COUNT;
	while (!I2C_ReadEventStatus(EEPROM_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
		if ((timeout--) == 0)
		{
			I2C_EnableGenerateStop(EEPROM_I2C);
			return EEPROM_TIMEOUT;
		}
	}
	
	/* 读取数据 */
	while (len > 0)
	{
		if (len == 1)
		{
			/* 最后一个字节前禁用 ACK */
			I2C_DisableAcknowledge(EEPROM_I2C);

			/* 产生停止条件 */
			I2C_EnableGenerateStop(EEPROM_I2C);
		}

		/* 等待接收数据就绪 */
		timeout = EEPROM_TIMEOUT_COUNT;
		while (!I2C_ReadStatusFlag(EEPROM_I2C, I2C_FLAG_RXBNE))
		{
			if ((timeout--) == 0)
			{
				/* 返回前重新使能 ACK */
				I2C_EnableAcknowledge(EEPROM_I2C);
				return EEPROM_TIMEOUT;
			}
		}

		/* 读取数据 */
		*data = I2C_RxData(EEPROM_I2C);
		data++;
		len--;
	}
	
	/* 为下一次传输使能 ACK */
	I2C_EnableAcknowledge(EEPROM_I2C);

	return EEPROM_SUCCESS;
}