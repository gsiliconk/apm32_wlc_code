#ifndef _QI_IDCONFIG_H_
#define _QI_IDCONFIG_H_
/* 文件: Qi_IdConfig.h
 * 描述: Qi协议ID/Config阶段模块头文件
 */

#include "wlc_types.h"
#include "wlc_platform_types.h"

/* ID/Config阶段状态枚举 - 导出用于诊断 */
typedef enum {
    IDCFG_STATE_WAIT_ID = 0,           /* 等待ID数据包（状态2） */
    IDCFG_STATE_WAIT_XID,              /* 等待扩展ID数据包（状态3） */
    IDCFG_STATE_WAIT_OPTIONAL,         /* 等待可选数据包（状态4） */
    IDCFG_STATE_WAIT_CFG,              /* 等待配置数据包（状态4->5） */
    IDCFG_STATE_COMPLETE,              /* 阶段完成（状态5） */
    IDCFG_STATE_ERROR                  /* 错误状态 */
} idcfg_state_t;



/* 初始化指定设备的ID/Config模块 */
void qi_idconfig_init(uint8_t device);
/* 启动ID/Config阶段 - 准备接收数据包 */
void qi_idconfig_start(uint8_t device);
/* 处理ID/Config数据包 - 解析和验证接收到的数据包 */
bool qi_idconfig_process(uint8_t device, const qi_packet_t *packet, 
                         qi_id_data_t *id, qi_cfg_data_t *cfg);
/* 获取当前Qi模式（BPP或EPP）*/
qi_mode_t qi_idconfig_get_mode(uint8_t device);
/* 获取当前ID/Config阶段状态 */
idcfg_state_t qi_idconfig_get_state(uint8_t device);
/* 检查ID/Config阶段是否完成 */
bool qi_idconfig_is_complete(uint8_t device);
/* 检查ID/Config阶段是否发生错误 */
bool qi_idconfig_has_error(uint8_t device);

#endif