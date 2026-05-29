#include "bsp_adc.h"

// 快速ADC采集DMA缓冲区，用于PA4单通道高速连续采样
__IO uint16_t DMA_FastADCBuffer[512] = {0U};

// 慢速ADC采集DMA缓冲区，用于桥输入电压、桥电流、线圈温度三个通道
__IO uint16_t DMA_SlowADCBuffer[3] = {0U};

// ADC模块初始化状态标志，0表示未初始化，1表示已初始化
static uint8_t adc_initialized = 0U;

// 当前ADC采集组，0表示慢速采集组，1表示快速采集组
static uint8_t current_adc_group = 0U; // 0: slow group, 1: fast group

/**
 * @brief 初始化快速采集DMA，使用正常模式并开启传输完成中断
 */
void bsp_adc_fast_dma_init(void)
{
	DMA_Config_T dma;

	// 启用DMA时钟
	RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_DMA1);

	// 用于快速采集的DMA配置
	dma.peripheralBaseAddr = ((uint32_t)ADC1_BASE + 0x4CU);
	dma.memoryBaseAddr = (uint32_t)&DMA_FastADCBuffer;
	dma.dir = DMA_DIR_PERIPHERAL_SRC;
	dma.bufferSize = 512;
	dma.peripheralInc = DMA_PERIPHERAL_INC_DISABLE;
	dma.memoryInc = DMA_MEMORY_INC_ENABLE;
	dma.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_HALFWORD;
	dma.memoryDataSize = DMA_MEMORY_DATA_SIZE_HALFWORD;
	dma.loopMode = DMA_MODE_NORMAL;  // 快速组使用正常模式，便于检测一次采集完成
	dma.priority = DMA_PRIORITY_HIGH;
	dma.M2M = DMA_M2MEN_DISABLE;
    
	// 配置并启用DMA通道
	DMA_Config(DMA1_Channel1, &dma);
    
	// 启用DMA传输完成中断
	DMA_EnableInterrupt(DMA1_Channel1, DMA_INT_TC);
	NVIC_EnableIRQRequest(DMA1_Channel1_IRQn, 2U, 0U);
    
	// 启用DMA
	DMA_Enable(DMA1_Channel1);
}

/**
 * @brief 初始化慢速采集DMA，使用循环模式持续更新3个ADC通道数据
 */
void bsp_adc_slow_dma_init(void)
{
	DMA_Config_T dma;

	// 使能DMA1外设时钟
	RCM_EnableAHBPeriphClock(RCM_AHB_PERIPH_DMA1);

	// 配置慢速ADC采集使用的DMA参数
	dma.peripheralBaseAddr = ((uint32_t)ADC1_BASE + 0x4CU);
	dma.memoryBaseAddr = (uint32_t)&DMA_SlowADCBuffer;
	dma.dir = DMA_DIR_PERIPHERAL_SRC;
	dma.bufferSize = 3U;  // 慢速组包含3个ADC通道
	dma.peripheralInc = DMA_PERIPHERAL_INC_DISABLE;
	dma.memoryInc = DMA_MEMORY_INC_ENABLE;
	dma.peripheralDataSize = DMA_PERIPHERAL_DATA_SIZE_HALFWORD;
	dma.memoryDataSize = DMA_MEMORY_DATA_SIZE_HALFWORD;
	dma.loopMode = DMA_MODE_CIRCULAR;  // 慢速组使用循环模式，持续刷新采样值
	dma.priority = DMA_PRIORITY_HIGH;
	dma.M2M = DMA_M2MEN_DISABLE;
    
	// 配置DMA1通道1
	DMA_Config(DMA1_Channel1, &dma);
    
	// 关闭慢速采集组DMA传输完成中断
	DMA_DisableInterrupt(DMA1_Channel1, DMA_INT_TC);
    
	// 使能DMA1通道1
	DMA_Enable(DMA1_Channel1);
}

/**
 * @brief 配置快速采集ADC，采集PA4对应的ADC通道4
 */
void bsp_adc_fast_config(void)
{
	GPIO_Config_T gpio;
	ADC_Config_T adc;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA);// 使能GPIOA外设时钟

	GPIO_ConfigStructInit(&gpio);// 初始化GPIO配置结构体
	gpio.mode = GPIO_MODE_ANALOG;// 配置GPIO为模拟输入模式
	gpio.pin = GPIO_PIN_4;// 选择PA4引脚，对应ADC通道4
	GPIO_Config(GPIOA, &gpio);// 应用PA4 GPIO配置

	RCM_ConfigADCCLK(RCM_PCLK2_DIV_2);// 配置ADC时钟为PCLK2/2，用于快速采集
    
	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_ADC1);// 使能ADC1外设时钟

	ADC_Reset(ADC1);// 复位ADC1配置

	ADC_ConfigStructInit(&adc);// 初始化ADC配置结构体
	adc.mode = ADC_MODE_INDEPENDENT;// 配置ADC为独立工作模式
	adc.scanConvMode = DISABLE;// 单通道采集，不启用扫描模式
	adc.continuosConvMode = ENABLE;// 启用连续转换模式
	adc.externalTrigConv = ADC_EXT_TRIG_CONV_None;// 不使用外部触发，采用软件触发
	adc.dataAlign = ADC_DATA_ALIGN_RIGHT;// ADC转换结果右对齐
	adc.nbrOfChannel = 1U;// 配置规则转换通道数量为1
	ADC_Config(ADC1, &adc);// 应用ADC配置
    
	ADC_ConfigRegularChannel(ADC1, ADC_CHANNEL_4, 1U, ADC_SAMPLETIME_1CYCLES5);// 配置规则通道4，序列第1位，采样时间1.5个周期

	ADC_EnableDMA(ADC1);// 使能ADC DMA功能
	ADC_Enable(ADC1);// 使能ADC1

	ADC_ResetCalibration(ADC1);// 复位ADC1校准寄存器
	while (ADC_ReadResetCalibrationStatus(ADC1));// 等待ADC1校准复位完成

	ADC_StartCalibration(ADC1);// 启动ADC1校准
	while (ADC_ReadCalibrationStartFlag(ADC1));// 等待ADC1校准完成

	ADC_EnableSoftwareStartConv(ADC1);// 启动ADC1软件转换
}


/**
 * @brief 配置慢速采集ADC，采集PA2、PA5、PA1三个通道
 */
void bsp_adc_slow_config(void)
{
	GPIO_Config_T gpio;
	ADC_Config_T adc;

	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOA);// 使能GPIOA外设时钟

	GPIO_ConfigStructInit(&gpio);// 初始化GPIO配置结构体
	gpio.mode = GPIO_MODE_ANALOG;// 配置GPIO为模拟输入模式

	gpio.pin = GPIO_PIN_2;// 选择PA2引脚，对应ADC通道2，用于采集桥输入电压
	GPIO_Config(GPIOA, &gpio);// 应用PA2 GPIO配置

	gpio.pin = GPIO_PIN_5;// 选择PA5引脚，对应ADC通道5，用于采集桥电流
	GPIO_Config(GPIOA, &gpio);// 应用PA5 GPIO配置

	gpio.pin = GPIO_PIN_1;// 选择PA1引脚，对应ADC通道1，用于采集线圈温度
	GPIO_Config(GPIOA, &gpio);// 应用PA1 GPIO配置

	RCM_ConfigADCCLK(RCM_PCLK2_DIV_8);// 配置ADC时钟为PCLK2/8，用于慢速采集
    
	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_ADC1);// 使能ADC1外设时钟

	ADC_Reset(ADC1);// 复位ADC1配置

	ADC_ConfigStructInit(&adc);// 初始化ADC配置结构体
	adc.mode = ADC_MODE_INDEPENDENT;// 配置ADC为独立工作模式
	adc.scanConvMode = ENABLE;// 多通道采集，启用扫描模式
	adc.continuosConvMode = ENABLE;// 启用连续转换模式
	adc.externalTrigConv = ADC_EXT_TRIG_CONV_None;// 不使用外部触发，采用软件触发
	adc.dataAlign = ADC_DATA_ALIGN_RIGHT;// ADC转换结果右对齐
	adc.nbrOfChannel = 3U;// 配置规则转换通道数量为3
	ADC_Config(ADC1, &adc);// 应用ADC配置
    
	ADC_ConfigRegularChannel(ADC1, ADC_CHANNEL_2, 1U, ADC_SAMPLETIME_55CYCLES5);// 配置序列第1位：PA2/ADC通道2，采集桥输入电压
	ADC_ConfigRegularChannel(ADC1, ADC_CHANNEL_5, 2U, ADC_SAMPLETIME_55CYCLES5);// 配置序列第2位：PA5/ADC通道5，采集桥电流
	ADC_ConfigRegularChannel(ADC1, ADC_CHANNEL_1, 3U, ADC_SAMPLETIME_55CYCLES5);// 配置序列第3位：PA1/ADC通道1，采集线圈温度

	ADC_EnableDMA(ADC1);// 使能ADC DMA功能

	ADC_Enable(ADC1);// 使能ADC1

	ADC_ResetCalibration(ADC1);// 复位ADC1校准寄存器
	while (ADC_ReadResetCalibrationStatus(ADC1));// 等待ADC1校准复位完成

	ADC_StartCalibration(ADC1);// 启动ADC1校准
	while (ADC_ReadCalibrationStartFlag(ADC1));// 等待ADC1校准完成

	ADC_EnableSoftwareStartConv(ADC1);// 启动ADC1软件转换
}

/**
 * @brief 重新启动快速采集DMA，重新设置DMA传输数量
 */
void bsp_adc_fast_dma_restart(void)
{
	DMA_Disable(DMA1_Channel1);
	DMA_ConfigDataNumber(DMA1_Channel1, 512);
	DMA_Enable(DMA1_Channel1);
}

/**
 * @brief ADC初始化函数，默认初始化为慢速采集组
 */
void bsp_adc_init(void)
{
	if(adc_initialized != 0U)
	{
		return; // ADC已经初始化，直接返回
	}
    
	// 默认初始化慢速采集组
	bsp_adc_slow_dma_init();
	bsp_adc_slow_config();

	// 当前采集组设置为慢速组
	current_adc_group = 0U;
    
	// 标记ADC已初始化
	adc_initialized = 1U;
}

/**
 * @brief ADC去初始化函数，关闭ADC和DMA
 */
void bsp_adc_deinit(void)
{
	if(adc_initialized == 0U)
	{
		return; // ADC未初始化，直接返回
	}
    
	// 禁用ADC和DMA
	ADC_Disable(ADC1);
	DMA_Disable(DMA1_Channel1);
    
	// 清除初始化状态和采集组状态
	adc_initialized = 0U;
	current_adc_group = 0U;
}

/**
 * @brief 获取ADC当前工作时钟频率
 * @return ADC时钟频率，单位Hz
 */
uint32_t bsp_adc_get_clock(void)
{
	if(current_adc_group == 1U)
	{
		// 快速组ADC时钟为PCLK2/2
		return (72000000 / 2);
	}
	else
	{
		// 慢速组ADC时钟为PCLK2/8
		return (72000000 / 8);
	}
}

/**
 * @brief 启用ADC模块
 * @param model ADC模型标识，当前实现暂未使用
 * @return 0: 成功, 1: 失败
 */
uint8_t bsp_adc_enable(uint8_t model)
{
	// 当前实现中model参数未使用
	if(adc_initialized == 0U)
	{
		bsp_adc_init();
		return (adc_initialized != 0U) ? 0U : 1U;
	}

	return 0U;
}

/**
 * @brief 禁用ADC模块
 * @param model ADC模型标识，当前实现暂未使用
 * @return 0: 成功, 1: 失败
 */
uint8_t bsp_adc_disable(uint8_t model)
{
	// 当前实现中model参数未使用
	if (adc_initialized != 0U)
	{
		bsp_adc_deinit();
	}

	return 0U;
}

/**
 * @brief 获取指定ADC通道的采样值
 * @param device 设备标识，当前实现暂未使用
 * @param model ADC模型标识，0表示慢速组，1表示快速组
 * @param channel ADC通道，慢速组: 0桥输入电压, 1桥电流, 2线圈温度；快速组: 0为PA4
 * @return ADC转换值
 */
uint16_t bsp_adc_get_value(uint8_t device, uint8_t model, uint8_t channel)
{
	if (adc_initialized == 0U)
	{
		return 0U;
	}
    
	if (current_adc_group == 1U)
	{
		// 快速采集组：model 1，channel 0，对应PA4
		if (channel == 0U && model == 1U)
		{
			return DMA_FastADCBuffer[0];
		}
	}
	else
	{
		// 慢速采集组：model 0，channel 0~2，对应三个慢速采样通道
		if (channel < 3U && model == 0U)
		{
			return DMA_SlowADCBuffer[channel];
		}
	}
    
	return 0U;
}

/**
 * @brief 切换ADC采集组
 * @param device 设备标识，当前实现暂未使用
 * @param group 采集组，0表示慢速采集组，1表示快速采集组
 */
void bsp_adc_switch_group(uint8_t device, uint8_t group)
{
	// 切换前先关闭ADC和DMA，避免配置过程中产生异常转换
	ADC_Disable(ADC1);
	DMA_Disable(DMA1_Channel1);
    
	// 根据目标采集组重新配置DMA和ADC
	if (group == 1U)
	{
		// 切换到快速采集组
		bsp_adc_fast_dma_init();
		bsp_adc_fast_config();
		bsp_adc_fast_dma_restart();
	}
	else
	{
		// 切换到慢速采集组
		bsp_adc_slow_dma_init();
		bsp_adc_slow_config();
	}
    
	// 更新当前采集组状态
	current_adc_group = group;
}

/**
 * @brief 获取连续ADC采集数据
 * @param model ADC模型标识，0表示慢速组，1表示快速组
 * @param addr 数据存储地址
 * @param count 期望读取的数据点数
 * @return 0: 成功, 1: 失败
 */
uint8_t bsp_adc_get_continue_data(uint8_t model, uint16_t *addr, uint32_t count)
{
	if (adc_initialized == 0U || addr == NULL)
	{
		return 1U;
	}
    
	if (current_adc_group == 1U)
	{
		// 快速采集组：最多读取512个采样点
		if (model == 1U && count <= 512)
		{
			for (uint32_t i = 0U; i < count; i++)
			{
				addr[i] = DMA_FastADCBuffer[i];
			}
			return 0U;
		}
	}
	else
	{
		// 慢速采集组：最多读取3个通道采样值
		if (model == 0U && count <= 3U)
		{
			for (uint32_t i = 0U; i < count; i++)
			{
				addr[i] = DMA_SlowADCBuffer[i];
			}
			return 0U;
		}
	}
    
	return 1U;
}

/**
 * @brief DMA1 Channel1中断服务程序，用于处理快速ADC DMA传输完成事件
 */
void DMA1_Channel1_IRQHandler(void)
{
	if (DMA_ReadIntFlag(DMA1_INT_FLAG_TC1) == SET)
	{
		// 清除DMA传输完成中断标志
		DMA_ClearIntFlag(DMA1_INT_FLAG_TC1);
        
		// 快速采集完成后自动切换回慢速采集组
		if (current_adc_group == 1U) 
		{
			bsp_adc_switch_group(0, 0);
		}
	}
}