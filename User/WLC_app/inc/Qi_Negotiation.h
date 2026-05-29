#ifndef _QI_NEGOTIATION_H_
#define _QI_NEGOTIATION_H_

#include "wlc_types.h"
#include "wlc_platform_types.h"
#include "wlc_app.h"
#include "Qi_Response.h"

/** @brief Qi协商状态枚举类型 */
typedef enum {
    QI_STATE_NEGGOTIATION,     /* 初始协商状态 */
    QI_STATE_RENEGGOTIATION    /* 重新协商状态 */
} qi_negotiation_state_t;

/** @brief 协商模块结构体 - 管理协商过程中的状态和参数 */
typedef struct {
    uint32_t timerId;          /* 协商超时定时器ID */
    bool next_phase;           /* 下一阶段标志 - 协商完成时置位 */
    uint32_t change_count;     /* 变更计数 - 用于验证协商完整性 */
    qi_negotiation_state_t phase;  /* 当前协商状态 */
    qi_response_t response;    /* 待发送的响应类型 */
} negotiation_modult_t;

/* 初始化协商模块 */
void qi_negotiation_init(uint8_t device);
/* 协商阶段主处理函数 */
bool qi_negtiation_process(uint8_t device, wlc_context_t *ctx, const qi_packet_t *packet);


#endif