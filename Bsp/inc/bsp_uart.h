#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "bsp_platform.h"
#include "ring_buffer.h"

void bsp_uart_init(void);
// 批量发送数据函数
void uart_send_byte(uint8_t *data, uint16_t len);
// 单字节发送函数
void uart_send_single_byte(uint8_t data);
// 格式化输出函数
int bsp_uart_printf(char *fmt, va_list args);

#endif // __UART_H