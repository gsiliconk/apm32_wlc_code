#include "wlc_timer.h"
/*
 * 文件: wlc_timer.c
 * 描述: WLC 定时器实现 - 软件定时器池管理
 */
 
/* ========================= 定时器结构体定义 ========================= */
// 硬件定时器结构体 - 跟踪定时器状态
typedef struct {
    bool isUsed;           /* 定时器使用标志 */
    bool isExpired;        /* 定时器到期标志 */
    uint32_t timeout;      /* 超时值，单位毫秒 */
    uint32_t startTime;    /* 定时器启动时间，单位毫秒 */
} hardwareTimer_t;
 
/* ============================= 静态变量 ============================= */
static volatile uint32_t timer_tickcount = 0; // 全局定时器滴答计数器，1ms精度
static hardwareTimer_t hardwareTimers[HARDWARE_TIMER_MAX]; // 硬件定时器池 - 定时器结构体数组

/* ========================== 定时器函数实现 ========================== */
/**
 * @brief 定时器中断服务例程回调
 * 由1ms定时中断调用此函数
 */
void wlc_timer_isr_callback(void)
{
    /* 自增定时器滴答计数器 */
    timer_tickcount++;
}

/**
 * @brief 获取当前系统时间戳（毫秒）
 * @return 当前时间戳值
 */
uint32_t wlc_timer_get_timestamp(void)
{
    return timer_tickcount;
}

/**
 * @brief 创建一个具有指定超时时间的新定时器
 * @param time 超时值，单位毫秒
 * @return 定时器 ID（从1开始的索引），无可用定时器时返回0
 */
uint32_t wlc_timer_create(uint32_t time)
{
	uint32_t timer = UINT32_MAX;
	
	/* 查找未使用的定时器槽位 */
	for(uint32_t i = 0; i < HARDWARE_TIMER_MAX; i++)
	{
		if(!hardwareTimers[i].isUsed)
		{
			timer = i;
			break;
		}
	}

	/* 若无可用定时器则返回错误 */
	if (timer >= HARDWARE_TIMER_MAX)
	{
		return 0;
	}

	/* 初始化定时器结构体 */
	hardwareTimers[timer].isUsed = true;
	hardwareTimers[timer].isExpired = false;
	hardwareTimers[timer].timeout = time;
	hardwareTimers[timer].startTime = timer_tickcount;
    
	/* 返回从1开始的索引 */
	return timer + 1U;
}

/**
 * @brief 检查定时器是否已到期
 * @param timer 定时器 ID（从1开始的索引）
 * @return 定时器到期返回 true，否则返回 false
 */
bool wlc_timer_is_expired(uint32_t timer)
{
	/* 无效的定时器 ID */
	if(timer == 0U)
	{
		return true;
	}
    
	/* 转换为从0开始的索引 */
	timer--;
    
	/* 超出范围 */
	if(timer >= HARDWARE_TIMER_MAX)
	{
		return true;
	}
    
	/* 定时器未使用 */
	if(!hardwareTimers[timer].isUsed)
	{
		return true;
	}
    
	/* 只检查一次到期条件 */
	if(!hardwareTimers[timer].isExpired)
	{
		uint32_t elapsed = timer_tickcount - hardwareTimers[timer].startTime;
		if(elapsed >= hardwareTimers[timer].timeout)
		{
			hardwareTimers[timer].isExpired = true;
		}
	}
    
	return hardwareTimers[timer].isExpired;
}

/**
 * @brief 检查是否有任何定时器正在运行
 * @return 只要有任何定时器活跃，返回 true，否则返回 false
 */
bool wlc_timer_is_wait(void)
{
	for(uint32_t i = 0; i < HARDWARE_TIMER_MAX; i++)
	{
		if(hardwareTimers[i].isUsed && !hardwareTimers[i].isExpired)
		{
			uint32_t elapsed = timer_tickcount - hardwareTimers[i].startTime;
			if(elapsed < hardwareTimers[i].timeout)
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief 销毁定时器并释放其资源
 * @param timer 定时器 ID（从1开始的索引）
 */
void wlc_timer_destroy(uint32_t timer)
{
	/* 无效的定时器 ID */
	if(timer == 0U)
	{
		return;
	}

	/* 转换为从0开始的索引 */
	timer--;
    
	/* 超出范围 */
	if (timer >= HARDWARE_TIMER_MAX)
	{
		return;
	}
    
	/* 重置定时器状态 */
	hardwareTimers[timer].isUsed = false;
	hardwareTimers[timer].isExpired = false;
	hardwareTimers[timer].timeout = 0;
	hardwareTimers[timer].startTime = 0;
}