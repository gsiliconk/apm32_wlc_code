#ifndef _BSP_PWM_H_
#define _BSP_PWM_H_

#include "bsp_platform.h"

void     bsp_pwm_init(void);
void     bsp_pwm_enable(uint8_t device, uint8_t en_dis);
void     bsp_pwm_set_arr(uint8_t device, uint16_t arr);
uint16_t bsp_pwm_get_arr(uint8_t device);
uint32_t bsp_pwm_get_clock(void);
void     bsp_pwm_set_half_bridge1_pulse(uint8_t device, uint16_t pulse);
void     bsp_pwm_set_half_bridge2_pulse(uint8_t device, uint16_t pulse);
void     bsp_pwm_set_phase(uint8_t device, uint16_t phase);
void     bsp_pwm_set_callback(uint8_t device, void (*cb)(void));

#endif