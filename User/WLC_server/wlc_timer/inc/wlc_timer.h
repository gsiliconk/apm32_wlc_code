#ifndef _WLC_TIMER_H_
#define _WLC_TIMER_H_
/*
 * 文件: wlc_timer.h
 * 描述: WLC 定时器模块接口头文件 - 软件定时器管理
 */
 
#include "wlc_lib_interal_functions.h"

/* 最大并发定时器数量 */
#define HARDWARE_TIMER_MAX 10U

/* ============================= 定时器管理函数 ============================= */
/**
 * @brief 创建一个具有指定超时时间的新定时器
 * @param time 超时值，毫秒（裸机）或系统节拍（FreeRTOS）
 * @return 定时器 ID（从1开始的索引），创建失败返回0
 */
uint32_t wlc_timer_create(uint32_t time);

/**
 * @brief 检查定时器是否已到期
 * @param timer 定时器 ID（从1开始的索引）
 * @return 定时器已到期或无效返回 true，否则返回 false
 */
bool wlc_timer_is_expired(uint32_t timer);

/**
 * @brief 销毁定时器并释放其资源
 * @param timer 定时器 ID（从1开始的索引）
 */
void wlc_timer_destroy(uint32_t timer);

/**
 * @brief 检查是否有任何定时器正在运行
 * @return 只要有至少一个定时器活跃，返回 true，否则返回 false
 */
bool wlc_timer_is_wait(void);

/**
 * @brief 定时器 ISR 回调 - 由1ms定时中断调用
 */
void wlc_timer_isr_callback(void);

/**
 * @brief 获取当前系统时间戳（毫秒）
 * @return 当前时间戳（毫秒）
 */
uint32_t wlc_timer_get_timestamp(void);

#endif