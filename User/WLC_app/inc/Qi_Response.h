#ifndef _QI_RESPONSE_H_
#define _QI_RESPONSE_H_

#include "wlc_types.h"
#include "wlc_platform_types.h"

/* Qi响应发送完成回调函数类型 */
typedef void (*Response_Callback_t)(void);

/* Qi响应模块结构体 */
typedef struct
{
    qi_response_t response;       /**< 当前响应的类型 */
    Response_Callback_t callback; /**< 传输完成回调函数指针 */
    uint32_t timerId;             /**< 定时器ID（用于超时重发） */
    uint8_t resp_data[8];         /**< 响应数据缓冲区 */
    uint8_t resp_len;             /**< 响应数据长度 */
    bool active;                  /**< 模块激活标志 */
    bool start;                   /**< 开始发送标志 */
} response_modult_t;

/* 初始化Qi响应模块 */
void qi_response_init(uint8_t device, qi_response_t response, uint32_t timeout, Response_Callback_t callback);
/* 轮询检查响应状态并处理完成事件 */
bool qi_response_active(void);
/* 发送指定类型的Qi响应 */
void qi_response(uint8_t device, qi_response_t response);
/* 配置FSK调制深度参数 */
void qi_response_set_fsk_params(uint8_t device, uint8_t value);
/* 停止当前响应过程 */
void qi_response_stop(void);

#endif