#include "bsp_pwm.h"

/* IO配置：TMR1_CH2、TMR3_CH3 */
#define PWM_HB1_GPIO GPIOB
#define PWM_HB1_PIN  GPIO_PIN_0
#define PWM_HB2_GPIO GPIOA
#define PWM_HB2_PIN  GPIO_PIN_9

/* A11线圈配置 */

#define PWM_COIL_A11_FREQ               (136)			// A11线圈主PWM周期参数
#define PWM_COIL_SLAVE_A11_FREQ         (0x800)		// A11线圈从PWM周期参数
#define PWM_COIL_A11_STOP_VALUE         (0)				// A11线圈PWM停止输出值
#define PWM_COIL_A11_PHASE              ((PWM_COIL_A11_FREQ / 2) - 2)// A11线圈两路半桥相位偏移参数

// PWM中断回调函数
static void (*g_pwm_callback)(void) = NULL;

void bsp_pwm_init(void)
{
	GPIO_Config_T			gpio;
	TMR_BaseConfig_T	tmr;
	TMR_OCConfig_T		oc;

	// 使能GPIO和定时器时钟
	RCM_EnableAPB2PeriphClock((RCM_APB2_PERIPH_T)(RCM_APB2_PERIPH_GPIOA | RCM_APB2_PERIPH_TMR1 | RCM_APB2_PERIPH_GPIOB));
	RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_TMR3);

	// 配置半桥2 PWM输出引脚：TMR1_CH2
	gpio.pin = PWM_HB2_PIN;
	gpio.mode = GPIO_MODE_AF_PP;
	gpio.speed = GPIO_SPEED_50MHz;
	GPIO_Config(PWM_HB2_GPIO, &gpio);

	// 配置半桥1 PWM输出引脚：TMR3_CH3
	gpio.pin = PWM_HB1_PIN;
	GPIO_Config(PWM_HB1_GPIO, &gpio);

	// 配置TMR3为主定时器
	tmr.clockDivision = TMR_CLOCK_DIV_1;
	tmr.countMode = TMR_COUNTER_MODE_UP;
	tmr.division = 2;
	tmr.repetitionCounter = 0;
	tmr.period = PWM_COIL_A11_FREQ;
	TMR_ConfigTimeBase(TMR3, &tmr);

	// 配置TMR1为从定时器
	tmr.period = PWM_COIL_SLAVE_A11_FREQ;
	TMR_ConfigTimeBase(TMR1, &tmr);

	// 配置PWM输出模式
	oc.mode = TMR_OC_MODE_PWM2;
	oc.pulse = PWM_COIL_A11_STOP_VALUE;
	oc.polarity = TMR_OC_POLARITY_LOW;
	oc.nPolarity = TMR_OC_NPOLARITY_HIGH;

	oc.nIdleState = TMR_OC_NIDLE_STATE_RESET;
	oc.idleState = TMR_OC_IDLE_STATE_RESET;

	oc.outputNState = TMR_OC_NSTATE_DISABLE;
	oc.outputState = TMR_OC_STATE_ENABLE;

	// TMR1_CH2：半桥2 PWM输出
	TMR_ConfigOC2(TMR1, &oc);
	TMR_ConfigOC2Preload(TMR1, TMR_OC_PRELOAD_ENABLE);

	// TMR3_CH3：半桥1 PWM输出
	TMR_ConfigOC3(TMR3, &oc);
	TMR_ConfigOC3Preload(TMR3, TMR_OC_PRELOAD_ENABLE);

	// TMR3_CH1用于产生主从定时器同步触发相位
	oc.pulse = PWM_COIL_A11_PHASE;
	TMR_ConfigOC1(TMR3, &oc);
	TMR_ConfigOC1Preload(TMR3, TMR_OC_PRELOAD_ENABLE);

	// TMR3通过OC1REF输出触发信号
	TMR_SelectOutputTrigger(TMR3, TMR_TRGO_SOURCE_OC1REF);
	TMR_EnableMasterSlaveMode(TMR3);

	// TMR1接收TMR3触发信号，复位计数器实现相位同步
	TMR_SelectInputTrigger(TMR1, TMR_TRIGGER_SOURCE_ITR2);
	TMR_SelectSlaveMode(TMR1, TMR_SLAVE_MODE_RESET);

	// 使能自动重装载
	TMR_EnableAutoReload(TMR1);
	TMR_EnableAutoReload(TMR3);

	// 使能TMR3更新中断
	TMR_EnableInterrupt(TMR3, TMR_INT_UPDATE);
	NVIC_EnableIRQRequest(TMR3_IRQn, 2, 0);

	// 启动PWM输出和定时器
	TMR_EnablePWMOutputs(TMR1);
	TMR_EnablePWMOutputs(TMR3);
	TMR_Enable(TMR1);
	TMR_Enable(TMR3);

	// 半桥驱动使能引脚
	gpio.pin = GPIO_PIN_8;
	gpio.mode = GPIO_MODE_OUT_PP;
	gpio.speed = GPIO_SPEED_50MHz;
	GPIO_Config(GPIOA, &gpio);

	// 使能半桥驱动
	GPIO_WriteBitValue(GPIOA, GPIO_PIN_8, 1);
}

// 使能或关闭指定模型的PWM周期中断
void bsp_pwm_enable(uint8_t model, uint8_t en_dis)
{
	if (en_dis)
	{
		if (model == 0)
		{
			// 开启PWM周期中断
			TMR_EnableInterrupt(TMR3, TMR_INT_UPDATE);
		}
	}
	else
	{
		if (model == 0)
		{
			// 关闭PWM周期中断
			TMR_DisableInterrupt(TMR3, TMR_INT_UPDATE);
		}
	}
}

// 设置半桥1的PWM脉冲宽度/占空比比较值
void bsp_pwm_set_half_bridge1_pulse(uint8_t model, uint16_t pulse)
{
	// 设置半桥1占空比：TMR3_CH3
	TMR_ConfigCompare3(TMR3, pulse);
}

// 设置半桥2的PWM脉冲宽度/占空比比较值
void bsp_pwm_set_half_bridge2_pulse(uint8_t model, uint16_t pulse)
{
	// 设置半桥2占空比：TMR1_CH2
	TMR_ConfigCompare2(TMR1, pulse);
}

// 设置两路半桥PWM之间的同步相位
void bsp_pwm_set_phase(uint8_t model, uint16_t phase)
{
	// 设置两路半桥之间的同步相位
	TMR_ConfigCompare1(TMR3, phase);
}

// 设置PWM周期自动重装载值
void bsp_pwm_set_arr(uint8_t model, uint16_t arr)
{
	// 设置PWM周期
	// TMR_ConfigAutoreload(TMR3, arr);
	TMR3->AUTORLD = arr;
}

// 获取当前PWM周期自动重装载值
uint16_t bsp_pwm_get_arr(uint8_t model)
{
	// 获取当前PWM周期
	return TMR3->AUTORLD;
}

// 获取PWM定时器输入时钟频率
uint32_t bsp_pwm_get_clock(void)
{
	// 返回PWM定时器输入时钟
	return 24000000;
}

// 注册PWM周期中断回调函数
void bsp_pwm_set_callback(uint8_t model, void (*cb)(void))
{
	// 注册PWM周期中断回调
	g_pwm_callback = cb;
}

// TMR3中断服务函数，处理PWM周期更新中断
void TMR3_IRQHandler(void)
{
	// 清除TMR3更新中断标志
	TMR_ClearIntFlag(TMR3, TMR_FLAG_UPDATE);
	
	if(g_pwm_callback)
		g_pwm_callback();
}

// TMR1更新中断服务函数，处理PWM周期更新中断
void TMR1_UP_IRQHandler(void)
{
	// 清除TMR1更新中断标志
	TMR_ClearIntFlag(TMR1, TMR_FLAG_UPDATE);

	if(g_pwm_callback)
		g_pwm_callback();
}