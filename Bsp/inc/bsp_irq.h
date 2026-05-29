#ifndef __BSP_IRQ_H_
#define __BSP_IRQ_H_

#include "bsp_platform.h"

#ifdef WLC_LIB_DEVICE_CONT
#define WLC_LIB_DEVICE_DAT WLC_LIB_DEVICE_CONT
#else
#define WLC_LIB_DEVICE_DAT 1
#endif

void bsp_irq_init(void);
void bsp_irq_deinit(void);
void bsp_irq_enable(uint8_t model, uint8_t endis);
void bsp_irq_set_callback(void (*cb)(uint8_t device, uint8_t ch, uint32_t pwidth));

#endif