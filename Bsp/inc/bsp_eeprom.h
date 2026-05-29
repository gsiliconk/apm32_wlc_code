#ifndef _BSP_EEPROM_H_
#define _BSP_EEPROM_H_

#include "bsp_platform.h"

// AT24C02设备地址
#define AT24C02_ADDR_WRITE    0xA0
#define AT24C02_ADDR_READ     0xA1

// EEPROM操作返回值枚举
typedef enum {
    EEPROM_SUCCESS = 0,     // 操作成功
    EEPROM_ERROR,           // 一般错误
    EEPROM_TIMEOUT,         // 超时错误
    EEPROM_ADDR_OVERFLOW    // 地址溢出错误
} eeprom_status_t;


void							bsp_eeprom_init(void); // 初始化EEPROM I2C接口
uint32_t					bsp_eeprom_get_start_addr(void); // 获取eeprom的起始地址
eeprom_status_t		bsp_eeprom_write_data(uint32_t addr, uint8_t* data, uint32_t len); // 向EEPROM写入数据，自动处理页边界
eeprom_status_t		bsp_eeprom_read_data(uint32_t addr, uint8_t* data, uint32_t len); // 从 EEPROM 读取数据，自动处理页边界

#endif