#ifndef __BSP_ADC_H
#define __BSP_ADC_H
#include "bsp_platform.h"

void     bsp_adc_init(void);
void		 bsp_adc_deinit(void);
uint32_t bsp_adc_get_clock(void);
uint8_t  bsp_adc_enable(uint8_t model);
uint8_t  bsp_adc_disable(uint8_t model);
void     bsp_adc_switch_group(uint8_t device, uint8_t mode);
uint16_t bsp_adc_get_value(uint8_t device, uint8_t model, uint8_t channel);
uint8_t  bsp_adc_get_continue_data(uint8_t model, uint16_t *buf, uint32_t len);

#endif