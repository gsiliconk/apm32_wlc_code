#include "freertos_task.h"

static TaskHandle_t app_1ms_Task_Handle = NULL;
static TaskHandle_t app_10ms_Task_Handle = NULL;

wlc_lib_api_t wlc_lib_datas = 
{
	.wlc_lib_adc_get_clock							=	bsp_adc_get_clock,
	.wlc_lib_adc_switch_group						=	bsp_adc_switch_group,
	.wlc_lib_adc_get_value							=	bsp_adc_get_value,
	.wlc_lib_adc_get_continue_data			=	bsp_adc_get_continue_data,
	
	.wlc_lib_pwm_get_clock							=	bsp_pwm_get_clock,
	.wlc_lib_pwm_enable									=	bsp_pwm_enable,
	.wlc_lib_pwm_set_callback						=	bsp_pwm_set_callback,
	.wlc_lib_pwm_set_arr								=	bsp_pwm_set_arr,
	.wlc_lib_pwm_get_arr								=	bsp_pwm_get_arr,
	.wlc_lib_pwm_set_half_bridge1_pulse	=	bsp_pwm_set_half_bridge1_pulse,
	.wlc_lib_pwm_set_half_bridge2_pulse	=	bsp_pwm_set_half_bridge2_pulse,
	.wlc_lib_pwm_set_phase							=	bsp_pwm_set_phase,
	
	.wlc_lib_irq_enable									=	bsp_irq_enable,
	
	.wlc_lib_uart_printf								=	bsp_uart_printf,
};

void app_1ms_task(void)
{
	static TickType_t xLastWakeTime = 0;
	xLastWakeTime = xTaskGetTickCount();
	
	bsp_timer_init();
	bsp_uart_init();
	bsp_irq_init();
	bsp_pwm_init();
	bsp_adc_init();
	bsp_Qc_Init();
	bsp_Qc_Inveigle_9V();
	bsp_Qc_Inveigle_12V();
	
	bsp_timer_set_callback(wlc_lib_timer_handle_int);
	bsp_irq_set_callback(wlc_lib_ask_handle_int);
	
	wlc_lib_init(&wlc_lib_datas);
	while(1)
	{
		wlc_lib_handler();
		
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
	}
}

void app_10ms_task(void)
{
	static TickType_t xLastWakeTime = 0;
	xLastWakeTime = xTaskGetTickCount();
	
	
	while(1)
	{
		tomato_calibration_mainfunction();
		
		vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
	}
}

void App_Task_Create(void)
{
	BaseType_t ret = pdPASS;
	/* app_1ms_task */
	ret = xTaskCreate((TaskFunction_t)app_1ms_task,
										(char *)				"app_1ms_task",
										(uint16_t)			255,
										(void *)				NULL,
										(UBaseType_t)		6,
										(TaskHandle_t *)&app_1ms_Task_Handle);
	if(ret != pdPASS)
	{
		// 添加错误处理代码
	}
	
	/* app_10ms_task */
	ret = xTaskCreate((TaskFunction_t)app_10ms_task,
										(char *)				"app_10ms_task",
										(uint16_t)			255,
										(void *)				NULL,
										(UBaseType_t)		7,
										(TaskHandle_t *)&app_10ms_Task_Handle);
	if(ret != pdPASS)
	{
		// 添加错误处理代码
	}
}