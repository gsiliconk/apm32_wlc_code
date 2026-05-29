#ifndef __FREERTOS_TASK_H_
#define __FREERTOS_TASK_H_

#include "FreeRTOS.h"
#include "task.h"

#include "bsp_uart.h"
#include "bsp_timer.h"
#include "bsp_irq.h"
#include "bsp_pwm.h"
#include "bsp_adc.h"
#include "bsp_qc.h"

#include "wlc_lib.h"
#include "Tomato_wlc.h"

void App_Task_Create(void);

#endif