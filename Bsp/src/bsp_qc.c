#include "bsp_qc.h"

#define DRI_D_POSITIVE_GPIO     GPIOA
#define DRI_D_POSITIVE_PIN      GPIO_PIN_12
#define DRI_D_NEGATIVE_GPIO     GPIOA
#define DRI_D_NEGATIVE_PIN      GPIO_PIN_11
#define AFC_CTR_GPIO            GPIOA
#define AFC_CTR_PIN             GPIO_PIN_10

/* ========================= 私有函数 ========================= */

/* GPIO 配置结构体，速度固定 50MHz */
static GPIO_Config_T gpio = 
{
    .speed = GPIO_SPEED_50MHz
};

/**
 * @brief 延时（循环计数）
 */
static void qc_delay(uint32_t count)
{
    volatile uint32_t i;
    for(i = 0; i < count; i++);
}

/**
 * @brief 通用 GPIO 引脚初始化函数
 */
static void Qc_Gpio_Init(GPIO_T* Port, uint16_t Pin, GPIO_MODE_T Mode)
{
    gpio.mode = Mode;
    gpio.pin = Pin;
    GPIO_Config(Port, &gpio);
}

/* ========================= 对外接口函数 ========================= */

/**
 * @brief QC 引脚初始化，进入 5V 默认状态
 *        状态：D+ = 0.6V， D- = 0V
 */
void bsp_Qc_Init(void)
{
	RCM_EnableAPB2PeriphClock((RCM_APB2_PERIPH_T)(RCM_APB2_PERIPH_GPIOA));
	
	/* D- 浮空输入 -> 0V */
	Qc_Gpio_Init(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN, GPIO_MODE_IN_FLOATING);
	
	/* D+ 输出 0 -> 0.6V（通过后续分压网络实现） */
	GPIO_ResetBit(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN);
	Qc_Gpio_Init(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN, GPIO_MODE_OUT_PP);
	
	/* AFC 控制引脚浮空 */
	Qc_Gpio_Init(AFC_CTR_GPIO, AFC_CTR_PIN, GPIO_MODE_IN_FLOATING);
	
	/* 等待电压稳定 */
	for (uint32_t tick; tick < 65535; tick++);
}

/**
 * @brief 请求 9V 输出
 *        协议状态：D+ = 3.3V, D- = 0.6V
 */
void bsp_Qc_Inveigle_9V(void)
{
	/* D- 输出 0 -> 0.6V */
	GPIO_SetBit(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN);
	Qc_Gpio_Init(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN, GPIO_MODE_OUT_PP);
	
	/* D+ 输出 1 -> 3.3V */
	GPIO_SetBit(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN);
	Qc_Gpio_Init(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN, GPIO_MODE_OUT_PP);

	/* AFC 控制引脚浮空 */
	Qc_Gpio_Init(AFC_CTR_GPIO, AFC_CTR_PIN, GPIO_MODE_IN_FLOATING);

	/* 等待充电器识别 */
	for (uint32_t tick; tick < 65535; tick++);
}

/**
 * @brief 请求 12V 输出
 *        协议状态：D+ = 0.6V, D- = 0V，短暂维持后切换至 D+ = 0.6V, D- = 3.3V
 */
void bsp_Qc_Inveigle_12V(void)
{
	/* 第一阶段：D+ = 0.6V, D- = 0V */
	Qc_Gpio_Init(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN, GPIO_MODE_IN_FLOATING);
	GPIO_ResetBit(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN);
	Qc_Gpio_Init(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN, GPIO_MODE_OUT_PP);
	Qc_Gpio_Init(AFC_CTR_GPIO, AFC_CTR_PIN, GPIO_MODE_IN_FLOATING);
	qc_delay(5000); 

	/* 第二阶段：D+ = 0.6V, D- = 3.3V */
	GPIO_ResetBit(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN);
	Qc_Gpio_Init(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN, GPIO_MODE_OUT_PP);

	GPIO_SetBit(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN);
	Qc_Gpio_Init(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN, GPIO_MODE_OUT_PP);

	Qc_Gpio_Init(AFC_CTR_GPIO, AFC_CTR_PIN, GPIO_MODE_IN_FLOATING);

	qc_delay(5000);
}

/**
 * @brief 启用 AFC（三星自适应快充）模式
 *        将 AFC_CTR 引脚拉低，D+/D- 浮空
 */
void bsp_Qc_Enable_Afc(void)
{
	/* D+/D- 浮空 */
	Qc_Gpio_Init(DRI_D_NEGATIVE_GPIO, DRI_D_NEGATIVE_PIN, GPIO_MODE_IN_FLOATING);
	Qc_Gpio_Init(DRI_D_POSITIVE_GPIO, DRI_D_POSITIVE_PIN, GPIO_MODE_IN_FLOATING);

	/* AFC 控制引脚输出 0 */
	GPIO_ResetBit(AFC_CTR_GPIO, AFC_CTR_PIN);
	Qc_Gpio_Init(AFC_CTR_GPIO, AFC_CTR_PIN, GPIO_MODE_OUT_PP);
}