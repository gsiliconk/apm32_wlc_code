/*
 * 文件: Tomato_wlc.h
 * 描述: WLC无线充电主接口头文件
 *
 * 功能:
 *   1. 提供FSK调制相关接口，用于Qi协议中的发射端通信。
 *   2. 提供Qi功率校准接口，用于功率损耗估算和FOD检测。
 *   3. 声明应用层必须实现的系统测量与时间接口。
 *
 * 注意:
 *   - 本文件仅声明接口，不包含具体硬件实现。
 *   - 应用层需实现本文件中声明的外部回调函数。
 *   - 电压、电流、功率等物理量必须按接口规定单位返回。
 */

#ifndef _TOMATO_WLC_H_
#define _TOMATO_WLC_H_

/* ============================================================================
 * 头文件包含
 * ============================================================================
 */
#include "bsp_pwm.h"                    /* PWM驱动接口 */
#include "wlc_power_bridge.h"           /* 功率桥控制 */
#include "wlc_types.h"                  /* WLC通用类型定义 */
#include "Qi_Power_Delivery.h"          /* Qi功率传输协议 */
#include "wlc_timer.h"                  /* 定时器工具 */
#include "wlc_app.h"                    /* 应用层接口 */
#include "wlc_physical.h"               /* 物理层抽象 */
#include "wlc_lib_interal_functions.h"  /* WLC库内部函数 */
#include "wlc_platform_types.h"         /* 平台相关类型定义 */

/* ============================================================================
 * 通用宏定义
 * ============================================================================
 */

/**
 * @brief WLC库调试输出接口
 *
 * @note 统一映射到底层UART打印函数，便于后续替换日志实现。
 */
#define tomato_lib_printf   wlc_lib_uart_printf

/* ============================================================================
 * FSK配置宏
 * ============================================================================
 */

/**
 * @brief FSK单次最大传输字节数
 *
 * @note Qi协议中单次FSK消息长度通常不超过27字节。
 *       该宏用于发送长度检查和内部缓冲区分配。
 */
#define WLC_PWM_FSK_MAX_BYTES       (27)

/**
 * @brief 每个FSK比特对应的PWM周期数
 *
 * @note 该值决定FSK比特调制的时间分辨率。
 *       数值越大，单个比特持续时间越长，调制节奏越慢。
 */
#define WLC_PWM_FSK_COUNT           (256)

/**
 * @brief FSK计数掩码
 *
 * @note 当WLC_PWM_FSK_COUNT为2的幂时，可用该掩码替代取模运算，
 *       用于快速判断当前PWM计数在FSK周期内的位置。
 */
#define WLC_PWM_FSK_MASK            (WLC_PWM_FSK_COUNT - 1)

/* ============================================================================
 * 应用层必须实现的接口
 * ============================================================================
 * 以下接口由WLC库调用，但具体实现依赖应用平台。
 *
 * 要求:
 *   - 时间戳必须单调递增。
 *   - 电压单位为mV。
 *   - 电流单位为mA。
 *   - 功率单位为mW。
 *   - 多设备场景下，device为设备索引，从0开始。
 * ============================================================================
 */

/**
 * @brief 获取系统时间戳
 *
 * @return 当前系统时间，单位ms。
 *
 * @note 该函数用于超时判断、功率采样时间对齐以及FSK相关时序处理。
 *       返回值应单调递增，建议直接使用系统tick或等效毫秒计数器。
 */
extern uint32_t wlc_timer_get_timestamp(void);

/**
 * @brief 获取发射端输入电压
 *
 * @param device 设备ID，从0开始。
 *
 * @return 输入电压，单位mV。
 *
 * @note 该值通常由ADC采样后换算得到。
 *       例如返回5000表示5.0V，返回12000表示12.0V。
 */
extern uint16_t wlc_app_get_vin(uint8_t device);

/**
 * @brief 获取发射端输入电流
 *
 * @param device 设备ID，从0开始。
 *
 * @return 输入电流，单位mA。
 *
 * @note 该值用于输入功率计算、过流保护以及FOD相关判断。
 *       例如返回1000表示1.0A。
 */
extern uint16_t wlc_app_get_current(uint8_t device);

/**
 * @brief 获取校准参考功率
 *
 * @param device 设备ID，从0开始。
 *
 * @return 参考功率，单位mW。
 *
 * @note 该接口通常返回发射端输入功率或经过补偿后的参考功率。
 *       常见计算方式为:
 *       P = Vin(mV) * Iin(mA) / 1000，结果单位为mW。
 */
extern uint32_t wlc_app_get_reference_power(uint8_t device);

/**
 * @brief 获取当前PWM ARR寄存器值
 *
 * @param device 设备ID，从0开始。
 *
 * @return PWM自动重载寄存器ARR值。
 *
 * @note ARR用于计算或同步当前PWM频率。
 *       一般情况下，ARR值越大，PWM频率越低。
 */
extern uint16_t wlc_pwm_server_get_arr(uint8_t device);

/* ============================================================================
 * FSK调制接口
 * ============================================================================
 */

/**
 * @brief 设置FSK调制深度
 *
 * @param device 设备ID，从0开始。
 * @param depth  FSK频偏值。
 *               正值表示提高频率，负值表示降低频率。
 *
 * @note 该接口应在FSK发送前调用。
 *       调制深度会影响接收端对FSK信号的解调可靠性，
 *       过小可能导致识别困难，过大可能影响功率传输稳定性。
 */
extern void tomato_wlc_fsk_set_depth(uint8_t device, int32_t depth);

/**
 * @brief 获取FSK发送完成标志
 *
 * @param device 设备ID，从0开始。
 *
 * @return true  FSK发送已完成。
 * @return false FSK发送仍在进行。
 *
 * @note FSK发送为异步过程，发送完成通常由PWM中断或底层状态机置位。
 *       上层可通过轮询该接口判断当前传输是否结束。
 */
extern bool tomato_wlc_get_fsk_flag(uint8_t device);

/**
 * @brief 清除FSK发送完成标志
 *
 * @param device 设备ID，从0开始。
 *
 * @note 发起新的FSK发送前应先调用该接口，
 *       避免读取到上一次发送遗留的完成状态。
 */
extern void tomato_wlc_clear_fsk_flag(uint8_t device);

/**
 * @brief 发送FSK消息
 *
 * @param device 设备ID，从0开始。
 * @param msg    待发送数据缓冲区。
 * @param count  待发送字节数，不能超过WLC_PWM_FSK_MAX_BYTES。
 *
 * @note 该接口用于发送完整FSK消息，发送过程为异步执行。
 *       调用后可通过tomato_wlc_get_fsk_flag()查询完成状态。
 *
 * @warning 调用前需确保:
 *          1. 上一次FSK传输已经完成。
 *          2. 已调用tomato_wlc_clear_fsk_flag()清除完成标志。
 *          3. count未超过WLC_PWM_FSK_MAX_BYTES。
 */
extern void tomato_wlc_fsk_send_msg(uint8_t device, uint8_t *msg, uint32_t count);

/**
 * @brief 发送FSK标准响应字节
 *
 * @param device   设备ID，从0开始。
 * @param response 响应类型，通常为ACK、NAK或ND。
 *
 * @note 该接口用于发送Qi协议中的标准单字节响应，
 *       相比tomato_wlc_fsk_send_msg()更适合快速响应场景。
 */
extern void tomato_wlc_fsk_byte_response(uint8_t device,
                                         wlc_pwm_fsk_response_t response);

/* ============================================================================
 * Qi校准接口
 * ============================================================================
 */

/**
 * @brief 初始化Qi功率校准模块
 *
 * @param device 设备ID，从0开始。
 *
 * @note 系统初始化阶段调用，用于清空校准状态和准备校准数据结构。
 *       建议在进入功率传输流程前完成初始化。
 */
extern void tomato_calibration_init(uint8_t device);

/**
 * @brief Qi校准主处理函数
 *
 * @note 应在主循环或周期任务中调用。
 *       该函数用于推进校准状态机、处理采样数据并更新内部校准结果。
 *
 * @warning 如果系统正在进行正常功率传输，需要确认调用该函数不会与
 *          功率控制流程或采样更新流程产生冲突。
 */
extern void tomato_calibration_mainfunction(void);

/**
 * @brief 计算发射端与接收端之间的功率损耗
 *
 * @param rx_power 接收端报告或估算的功率，单位mW。
 *
 * @return 功率损耗，单位mW。
 *         正值表示存在传输损耗；
 *         负值通常表示测量误差、采样不同步或校准偏差。
 *
 * @note 该结果常用于FOD检测、安全保护和效率估算。
 *       计算精度依赖输入功率测量精度以及校准数据质量。
 */
extern int16_t tomato_calibration_calculate_ploss(uint32_t rx_power);

/**
 * @brief 保存Qi校准数据点
 *
 * @param rp_type     接收端功率类型。
 * @param tx_estimate 发射端估计功率，单位mW。
 * @param rx_estimate 接收端估计功率，单位mW。
 *
 * @return true  数据点保存成功。
 * @return false 数据点保存失败。
 *
 * @note 校准阶段可在不同功率等级下采集多个数据点，
 *       后续用于功率损耗估算或插值计算。
 *
 * @warning 该接口建议仅在校准流程中调用，
 *          正常功率传输期间不建议随意更新校准数据。
 */
extern bool tomato_calibration_points_data(qi_rx_power_type_t rp_type,
                                           uint32_t tx_estimate,
                                           uint32_t rx_estimate);

/**
 * @brief 获取指定时间窗口内对齐后的功率值
 *
 * @param packet_rx_time 接收到功率数据包的时间戳，单位ms。
 * @param window_size    平均窗口大小，单位ms。
 * @param offset         相对packet_rx_time的窗口偏移，单位ms。
 *
 * @return 对齐后的功率值，单位mW；
 *         若窗口无有效数据，则返回错误码。
 *
 * @note 接收端上报的功率与发射端本地采样通常存在时间延迟。
 *       该接口用于按时间窗口查找或平均本地功率采样值，
 *       从而提高功率损耗计算的准确性。
 */
extern int16_t tomato_calibration_get_aligned_power(uint32_t packet_rx_time,
                                                    uint16_t window_size,
                                                    uint16_t offset);

#endif /* _TOMATO_WLC_H_ */