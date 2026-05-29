#ifndef _WLC_PLATFORM_TYPES_H_
#define _WLC_PLATFORM_TYPES_H_

/*
 * 文件: wlc_platform_types.h
 * 描述: 平台特定类型定义及标准库头文件包含
 */
/* 标准库包含，提供基本的 C 类型和实用工具 */
#include <stdint.h>    /* 整数类型（uint8_t、uint16_t 等） */
#include <stdio.h>     /* 标准输入/输出函数 */
#include <stdbool.h>   /* 布尔类型支持 */
#include <stddef.h>    /* 标准定义（size_t、NULL 等） */
#include <string.h>    /* 字符串操作函数 */
#include <stdlib.h>    /* 通用实用工具（malloc、free 等） */
#include "wlc_lib_interal_functions.h"

/* 将地址转换为 uint8_t 指针的宏，常用于规避 MISRA/QAC 类型转换警告 */
#define QAC_CAST_ADDR_TO_U8_PTR(addr)   ((uint8_t*)(void*)(addr))

static inline void Wlc_Assert_Fail(const uint8_t * file, uint32_t line)
{
    (void)wlc_lib_uart_printf("assert_failed, file: %s, line: %d\r\n", (char*)file, line);
    while(1);
}

#if (QAC_USE_ASSERT == TRUE)
#define Wlc_Assert_Param(expr) \
do {    \
    if (!(expr)) {  \
        Wlc_Assert_Fail(QAC_CAST_ADDR_TO_U8_PTR(__FILE__), __LINE__); \
    }   \
} while(0)
#else
#define Wlc_Assert_Param(expr) ((void)0)
#endif


#endif