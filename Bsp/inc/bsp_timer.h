#ifndef __BSP_TIMER_H
#define __BSP_TIMER_H

#include "bsp_platform.h"

void bsp_timer_set_callback(void (*cb)(void));
void bsp_timer_init(void);

#endif