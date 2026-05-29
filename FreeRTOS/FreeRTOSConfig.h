#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Cortex-M3 FreeRTOS 配置文件
 *----------------------------------------------------------*/

/* 系统时钟与 Tick 配置 */
#define configCPU_CLOCK_HZ                           (72000000UL)        // CPU 主频 72MHz
#define configTICK_RATE_HZ                           ((TickType_t)1000)  // Tick 频率 1kHz，即 1ms

/* 任务与优先级配置 */
#define configMAX_PRIORITIES                         8                   // 最大任务优先级数量
#define configMINIMAL_STACK_SIZE                     ((unsigned short)128) // 空闲任务最小栈，单位 word
#define configMAX_TASK_NAME_LEN                      16                  // 任务名最大长度

/* 内存配置 */
#define configTOTAL_HEAP_SIZE                        ((size_t)12000)     // FreeRTOS 堆大小，单位字节
#define configSUPPORT_STATIC_ALLOCATION              0                   // 不使用静态内存分配
#define configSUPPORT_DYNAMIC_ALLOCATION             1                   // 使用动态内存分配
#define configAPPLICATION_ALLOCATED_HEAP             0                   // 堆由 FreeRTOS 管理

/* 内核功能配置 */
#define configUSE_PREEMPTION                         1                   // 启用抢占式调度
#define configUSE_IDLE_HOOK                          0                   // 不使用空闲钩子
#define configUSE_TICK_HOOK                          0                   // 不使用 Tick 钩子
#define configUSE_MALLOC_FAILED_HOOK                 0                   // 不使用内存申请失败钩子
#define configUSE_16_BIT_TICKS                       0                   // 使用 32 位 Tick

/* Cortex-M3 相关配置 */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION      1                   // 启用优化任务选择
#define configUSE_TIME_SLICING                       1                   // 启用时间片调度
#define configIDLE_SHOULD_YIELD                      1                   // 空闲任务让出 CPU

/* Cortex-M 内核扩展配置 */
#define configENABLE_FPU                             0                   // Cortex-M3 无 FPU
#define configENABLE_MPU                             0                   // 不启用 MPU
#define configENABLE_TRUSTZONE                       0                   // 不启用 TrustZone
#define configRUN_FREERTOS_SECURE_ONLY               0                   // 不使用安全模式

/* 软件定时器配置 */
#define configUSE_TIMERS                             1                   // 启用软件定时器
#define configTIMER_TASK_PRIORITY                    (configMAX_PRIORITIES - 1) // 定时器任务优先级
#define configTIMER_QUEUE_LENGTH                     10                  // 定时器命令队列长度
#define configTIMER_TASK_STACK_DEPTH                 256                 // 定时器任务栈大小

/* 同步与通信配置 */
#define configUSE_MUTEXES                            1                   // 启用互斥量
#define configUSE_RECURSIVE_MUTEXES                  1                   // 启用递归互斥量
#define configUSE_COUNTING_SEMAPHORES                1                   // 启用计数信号量
#define configUSE_QUEUE_SETS                         0                   // 不使用队列集
#define configQUEUE_REGISTRY_SIZE                    0                   // 不使用队列注册表

/* 任务控制配置 */
#define configUSE_TASK_NOTIFICATIONS                 1                   // 启用任务通知
#define configUSE_TRACE_FACILITY                     0                   // 不启用跟踪功能
#define configUSE_STATS_FORMATTING_FUNCTIONS         0                   // 不启用状态格式化函数

/* 协程配置 */
#define configUSE_CO_ROUTINES                        0                   // 不使用协程
#define configMAX_CO_ROUTINE_PRIORITIES              1                   // 协程优先级数量

/* 可选 API 配置 */
#define INCLUDE_vTaskPrioritySet                     1                   // 启用任务优先级设置
#define INCLUDE_uxTaskPriorityGet                    1                   // 启用获取任务优先级
#define INCLUDE_vTaskDelete                          1                   // 启用删除任务
#define INCLUDE_vTaskSuspend                         1                   // 启用挂起/恢复任务
#define INCLUDE_vTaskDelayUntil                      1                   // 启用绝对延时
#define INCLUDE_vTaskDelay                           1                   // 启用相对延时
#define INCLUDE_xTaskGetSchedulerState               0                   // 不获取调度器状态
#define INCLUDE_xTaskGetCurrentTaskHandle            1                   // 启用获取当前任务句柄
#define INCLUDE_uxTaskGetStackHighWaterMark          0                   // 不获取栈剩余空间
#define INCLUDE_xTaskGetIdleTaskHandle               0                   // 不获取空闲任务句柄
#define INCLUDE_eTaskGetState                        0                   // 不获取任务状态
#define INCLUDE_xEventGroupSetBitFromISR             1                   // 启用中断中设置事件位
#define INCLUDE_xTimerPendFunctionCall               1                   // 软件定时器需要
#define INCLUDE_xTaskAbortDelay                      0                   // 不启用中止延时
#define INCLUDE_xTaskGetHandle                       0                   // 不通过名称获取任务句柄
#define INCLUDE_xTaskResumeFromISR                   1                   // 启用中断中恢复任务

/* Cortex-M3 中断优先级配置 */
#define configPRIO_BITS                              4                   // Cortex-M3 通常为 4 位优先级
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15                  // 最低中断优先级
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1                   // 可调用 FreeRTOS API 的最高优先级

#define configKERNEL_INTERRUPT_PRIORITY              (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS)) // 内核中断优先级
#define configMAX_SYSCALL_INTERRUPT_PRIORITY         (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS)) // API 中断优先级上限

/* 中断服务函数映射 */
#define vPortSVCHandler                              SVC_Handler         // SVC 中断映射
#define xPortPendSVHandler                           PendSV_Handler      // PendSV 中断映射
#define xPortSysTickHandler                          SysTick_Handler     // SysTick 中断映射

/* 其他配置 */
#define configTICK_TYPE_WIDTH_IN_BITS                TICK_TYPE_WIDTH_32_BITS // Tick 类型宽度
#define configENABLE_BACKWARD_COMPATIBILITY          1                   // 启用向后兼容
#define configUSE_NEWLIB_REENTRANT                   0                   // 不启用 newlib 重入
#define configUSE_POSIX_ERRNO                        0                   // 不使用 POSIX errno

#endif