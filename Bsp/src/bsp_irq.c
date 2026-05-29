#include "bsp_irq.h"

#define ASK_DEMODULATION_GPIO		GPIOB
#define ASK_DEMODULATION_PIN		GPIO_PIN_7

/**
 * @brief  初始化 GPIO PB7 为浮空输入，并配置外部中断线7，
 *         使能下降沿和上升沿触发，为无线充电信号检测做好准备。
 *         注意：此处未使能 NVIC 中断，需调用 io_irq_enable() 开启。
 */
void bsp_irq_init(void)
{
	GPIO_Config_T	gpio;
	EINT_Config_T	enit;
	
	/* 使能 GPIOB 和复用功能（AFIO）的 APB2 时钟 */
	RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_GPIOB | RCM_APB2_PERIPH_AFIO);
	
	/* 配置 PB7 为浮空输入模式（无内部上下拉，由外部电路决定电平） */
	gpio.mode = GPIO_MODE_IN_FLOATING;
	gpio.pin = ASK_DEMODULATION_PIN;
	GPIO_Config(ASK_DEMODULATION_GPIO,&gpio);
	
	/* 将 PB7 连接到外部中断线 7 */
	GPIO_ConfigEINTLine(GPIO_PORT_SOURCE_B, GPIO_PIN_SOURCE_7);
	
	/* 配置外部中断线 7 为中断模式，双边沿触发，并使能该线 */
	enit.line = EINT_LINE_7;					// 中断线 7
	enit.mode = EINT_MODE_INTERRUPT;	// 中断模式（非事件）
	enit.trigger = EINT_TRIGGER_RISING_FALLING;	// 上升沿和下降沿均触发
	enit.lineCmd = ENABLE;						// 使能中断
	EINT_Config(&enit);
}

/**
 * @brief  去初始化函数，当前无实际操作（预留）。
 */
void bsp_irq_deinit(void)
{
    ;
}

/**
 * @brief  根据 model 和 endis 参数，使能或禁止 NVIC 中的外部中断。
 * @param  model : 模型/通道选择，目前仅处理 model == 0 的情况（对应 EINT9_5 中断）
 * @param  endis : 非零表示使能中断，零表示禁止中断
 *         当前代码仅对 model == 0 进行操作，对应 EINT9_5_IRQn。
 */
void bsp_irq_enable(uint8_t model, uint8_t endis)
{
	if (endis)
	{
		/* 使能中断 */
		if (model == 0)
		{
			/* 设置优先级为1（高优先级），子优先级为0 */
			NVIC_EnableIRQRequest(EINT9_5_IRQn, 0x01, 0x00);
		}
	}
	else
	{
		/* 禁止中断 */
		if (model == 0)
		{
			NVIC_DisableIRQRequest(EINT9_5_IRQn);
		}
	}
}

/**
 * @brief  使用 SysTick 滴答定时器测量两次调用之间的时间差（单位：微秒）
 * @param  model : 模块标识（用于多个设备/通道的独立记录）
 * @param  ch    : 通道号
 * @return 两次调用之间的时间间隔（微秒），基于 72MHz 系统时钟计算
 * @note   采用静态数组 last_time[][] 保存上次 SysTick 的 VAL 值，
 *         处理 SysTick 递减计数器的翻转问题，并转换为微秒。
 */
static uint32_t Capturetime_callevelwidth(uint8_t model, uint8_t ch)
{
	/* 静态变量，记录各模块各通道上一次 SysTick->VAL 的值（初始为0） */
	static uint32_t last_time[WLC_LIB_DEVICE_DAT][1] = {0};

	/* 读取当前 SysTick 计数值 */
	uint32_t tick = (uint32_t)SysTick->VAL;

	/* 计算时间差（tick 是递减的）：处理计数器下溢翻转 */
	uint32_t delta = (last_time[model][ch] < tick)
										? (last_time[model][ch] - tick + (uint32_t)SysTick->LOAD + 1)
										: (last_time[model][ch] - tick);

	/* 更新上次计数值 */
	last_time[model][ch] = tick;

	/* 系统时钟 72MHz，每 1/72000000 秒 = 1/72 微秒 */
	uint32_t sys_clock = 72000000;

	/* 转换为微秒，四舍五入： (delta * 100 / (sys_clock/1000000) + 50) / 100  */
	return (delta * 100 / (sys_clock / 1000000) + 50) / 100;
}

// 外部中断回调函数指针，中断中调用时需判空
static void (*g_irq_callback)(uint8_t device, uint8_t ch, uint32_t pwidth) = NULL;

/**
 * @brief 注册外部中断回调
 * @param cb 回调函数指针，参数：设备号，通道号，脉冲宽度(时间)
 */
void bsp_irq_set_callback(void (*cb)(uint8_t device, uint8_t ch, uint32_t pwidth))
{
	g_irq_callback = cb;
}

/**
 * @brief 外部中断 9_5 的中断服务函数（对应 PB7 上的双边沿触发）
 *        当检测到 PB7 电平跳变时，清除标志位，并调用无线充电库的中断处理函数，
 *        同时传入测得的脉冲宽度（两次中断之间的时间间隔）。
 */
void EINT9_5_IRQHandler(void)
{
	/* 检查并确认是 EINT_LINE_7 产生的中断 */
	if (EINT_ReadIntFlag(EINT_LINE_7))
	{
		/* 清除该线中断标志 */
		EINT_ClearIntFlag(EINT_LINE_7);
		/* 调用无线充电库的处理函数，传入 model=0, ch=0 以及测得的时间间隔 */
		if (g_irq_callback)
			g_irq_callback(0, 0, Capturetime_callevelwidth(0, 0));
	}
}