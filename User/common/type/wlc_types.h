#ifndef _WLC_TYPES_H_
#define _WLC_TYPES_H_

/*
 * File: wlc_types.h 无线充电发射端固件
 * Description: Qi 协议相关类型定义、常量及数据结构
 *              基于 Qi Specification v1.3
 */

#include "wlc_platform_types.h"

#define WLC_LIB_DEVICE_CONT 1

/*
 * Qi v1.3 标准时序参数，单位：ms
 * Reference: Qi Specification v1.3 - Tables 11, 12, 14, 16
 *
 * 说明：
 * - 以下时间参数主要用于协议状态机超时判断、窗口控制和数据包交互控制。
 * - 实际工程中应结合定时器精度、任务调度周期和中断延迟预留裕量。
 */

/* Ping 阶段时序参数，Table 11 */
#define QI_T_DETECT_MAX          5000    /* 检测时间窗口 */
#define QI_T_WAKE_MIN            19      /* 唤醒窗口最小值 */
#define QI_T_WAKE_TYP            40      /* 唤醒窗口典型值 */
#define QI_T_WAKE_MAX            64      /* 唤醒窗口最大值 */
#define QI_T_PING_MIN            65      /* 数字 Ping 超时时间最小值 */
#define QI_T_PING_TYP            65      /* 数字 Ping 超时时间典型值 */
#define QI_T_PING_MAX            70      /* 数字 Ping 超时时间最大值 */
#define QI_T_TERMINATE_MAX       28      /* 功率终止窗口 */
#define QI_T_RESET_TYP           25      /* 复位窗口典型值 */
#define QI_T_RESET_MAX           28      /* 复位窗口最大值 */
#define QI_T_NOPOWER_MIN         32      /* 无功率信号窗口最小值 */

/* 配置阶段时序参数，Table 12 */
#define QI_T_SILENT_MIN          6       /* 静默窗口最小值 */
#define QI_T_SILENT_TYP          7       /* 静默窗口典型值 */
#define QI_T_START_MIN           11      /* 下一个数据包开始窗口最小值 */
#define QI_T_START_MAX           19      /* 下一个数据包开始窗口最大值 */
#define QI_T_NEXT_MIN            22      /* 下一个数据包超时时间最小值 */
#define QI_T_NEXT_MAX            25      /* 下一个数据包超时时间最大值 */
#define QI_T_RESPONSE_MIN        3       /* 响应开始窗口最小值 */
#define QI_T_RESPONSE_MAX        10      /* 响应开始窗口最大值 */
#define QI_T_RESPONSE_TIMEOUT    5       /* 响应超时时间 */

/* 协商阶段时序参数，Table 14 */
#define QI_T_NEGOTIATE_MIN       300     /* 协商超时时间最小值 */
#define QI_T_NEGOTIATE_MAX       500     /* 协商超时时间最大值 */
#define QI_T_FOD_GRACE_MAX       5000    /* FOD 宽限窗口最大值 */

/* 功率传输阶段时序参数，Table 16 */
#define QI_T_CE_INTERVAL_TYP     250     /* Control Error 数据包发送间隔典型值 */
#define QI_T_CE_INTERVAL_MAX_BPP 350     /* BPP 模式 Control Error 最大间隔 */
#define QI_T_CE_INTERVAL_MAX_EPP 700     /* EPP 模式 Control Error 最大间隔，v1.3+ */
#define QI_T_CE_TIMEOUT_MIN      800     /* Control Error 超时时间最小值 */
#define QI_T_CE_TIMEOUT_TYP      1500    /* Control Error 超时时间典型值 */
#define QI_T_CE_TIMEOUT_MAX      1828    /* Control Error 超时时间最大值 */
#define QI_T_CONTROL_MIN         24      /* 功率控制窗口最小值 */
#define QI_T_ACTIVE_TYP          20      /* PTx 功率控制窗口典型值 */
#define QI_T_ACTIVE_MAX          21      /* PTx 功率控制窗口最大值 */
#define QI_T_RP_INTERVAL_TYP     1500    /* Received Power 数据包发送间隔典型值 */
#define QI_T_RP_INTERVAL_MAX_RP8 4050    /* RP8 模式最大间隔 */
#define QI_T_RP_INTERVAL_MAX_RP  2050    /* RP/0 和 RP/4 模式最大间隔 */
#define QI_T_RP_TIMEOUT_MIN      8000    /* Received Power 超时时间最小值 */
#define QI_T_RP_TIMEOUT_TYP      23000   /* Received Power 超时时间典型值 */
#define QI_T_RP_TIMEOUT_MAX      24028   /* Received Power 超时时间最大值 */
#define QI_T_RP_CAIL_TIMEOUT     10000   /* Received Power 校准超时时间 */

/* 动作超时时间，Section 7.3.7 */
#define QI_T_NAK_MAX             1000    /* NAK 窗口最大值 */
#define QI_T_ATN_MAX             500     /* ATN 窗口最大值 */
#define QI_T_DSR_MAX             1000    /* DSR 窗口最大值 */
#define QI_T_DTS_MAX             1000    /* 数据传输流窗口最大值 */
#define QI_T_DTS_TIMEOUT_MIN     2000    /* 数据传输流超时时间最小值 */
#define QI_T_RENEGOTIATE_MAX     5000    /* 重新协商窗口最大值 */

/* ============================================================================
 * Qi 数据包类型，Section 8 in Qi v1.3
 *
 * 说明：
 * - PRx -> PTx：接收端发送给发射端的数据包。
 * - PTx -> PRx：发射端发送给接收端的数据包。
 * ========================================================================== */

/* 电源接收器数据包，PRx -> PTx */
#define QI_PKT_SIG               0x01    /* 信号强度数据包 */
#define QI_PKT_EPT               0x02    /* 结束功率传输数据包 */
#define QI_PKT_CE                0x03    /* 控制误差数据包 */
#define QI_PKT_RP8               0x04    /* 8-bit 接收功率数据包 */
#define QI_PKT_CHS               0x05    /* 充电状态数据包 */
#define QI_PKT_PCH               0x06    /* 功率控制保持时间数据包 */
#define QI_PKT_GRQ               0x07    /* 通用请求数据包 */
#define QI_PKT_NEGO              0x09    /* 重新协商数据包 */
#define QI_PKT_DSR               0x15    /* 数据流响应数据包 */
#define QI_PKT_ADT_1E            0x16    /* 辅助数据传输数据包，偶数帧 */
#define QI_PKT_ADT_1O            0x17    /* 辅助数据传输数据包，奇数帧 */
#define QI_PKT_SRQ               0x20    /* 特定请求数据包 */
#define QI_PKT_FOD               0x22    /* FOD 状态数据包 */
#define QI_PKT_ADC               0x25    /* 辅助数据控制数据包 */
#define QI_PKT_RP                0x31    /* 16-bit 接收功率数据包 */
#define QI_PKT_CFG               0x51    /* 配置数据包 */
#define QI_PKT_WPID_HI           0x54    /* 无线功率 ID，高字节 */
#define QI_PKT_WPID_LO           0x55    /* 无线功率 ID，低字节 */
#define QI_PKT_ID                0x71    /* 识别数据包 */
#define QI_PKT_XID               0x81    /* 扩展识别数据包 */

/* 功率发射器数据包，PTx -> PRx */
#define QI_PKT_NULL              0x00    /* 无可用数据 */
#define QI_PKT_PTX_ID            0x30    /* PTx 识别数据包 */
#define QI_PKT_CAP               0x31    /* PTx 能力数据包 */

/* ============================================================================
 * EPT，结束电源传输，结束功率传输原因码，Section 8.7 in Qi v1.3
 * ========================================================================== */

#define QI_EPT_NULL              0x00    /* 未知原因 */
#define QI_EPT_CC                0x01    /* 充电完成 */
#define QI_EPT_IF                0x02    /* 内部故障 */
#define QI_EPT_OT                0x03    /* 过温 */
#define QI_EPT_OV                0x04    /* 过压 */
#define QI_EPT_OC                0x05    /* 过流 */
#define QI_EPT_BF                0x06    /* 电池故障 */
#define QI_EPT_NR                0x08    /* 无响应 */
#define QI_EPT_AN                0x0A    /* 协商中止 */
#define QI_EPT_RST               0x0B    /* 重启 */
#define QI_EPT_REP               0x0C    /* 重新 Ping */
#define QI_EPT_RFID              0x0D    /* 检测到 RFID/NFC 卡 */

/* ============================================================================
 * 制造商代码，WPC 注册
 * ========================================================================== */

#define QI_MANUF_APPLE           0x005A  /* 苹果. */
#define QI_MANUF_SAMSUNG         0x0042  /* 三星 */
#define QI_MANUF_HUAWEI          0x0060  /* 华为 */
#define QI_MANUF_OPPO            0x00D7  /* OPPO */
#define QI_MANUF_VIVO            0x004A  /* Vivo */
#define QI_MANUF_XIAOMI          0x009F  /* Xiaomi */
#define QI_MANUF_QI_GENERIC      0x0024  /* 通用 Qi 设备 */

/* ============================================================================
 * 功率控制参数
 * ========================================================================== */

#define QI_FREQ_APPLE            128000  /* Hz，Apple 快充频率 */
#define QI_FREQ_MIN              110000  /* Hz，最小工作频率 */
#define QI_FREQ_MAX              205000  /* Hz，最大工作频率 */
#define QI_FREQ_PING             175000  /* Hz，数字 Ping 频率 */
#define QI_DUTY_MIN              25      /* %，最小占空比 */
#define QI_DUTY_MAX              50      /* %，最大占空比 */
#define QI_PHASE_SHIFT_MIN       40      /* %，最小相移 */

/* 功率控制保持时间范围，Section 8.12 */
#define QI_PCH_MIN               10      /* ms，最小 hold-off 时间 */
#define QI_PCH_MAX               100     /* ms，最大 hold-off 时间 */
#define QI_PCH_DEFAULT           10      /* ms，默认 hold-off 时间 */
#define QI_ACTIVE_TIME           20      /* ms，默认有效控制时间 */

/* 参考功率范围 */
#define QI_POWER_MIN             0       /* W，最小功率 */
#define QI_POWER_BPP_MAX         5       /* W，BPP 基本功率配置最大功率 */
#define QI_POWER_EPP_MAX         15      /* W，EPP 扩展功率配置最大功率 */

/*
 * PWM 单位 = 1 / 24 MHz = 41.7 ns
 * PWM 频率 = 24 MHz / (ARR + 1)
 *
 * FSK 调制通过改变 PWM 周期实现：
 *
 * positive 3  -282.00  -249.00 ns
 * positive 2  -157.00  -124.00 ns
 * positive 1  -94.50   -61.50 ns
 * positive 0  -63.25   -30.25 ns
 * negative 0   30.25    63.25 ns
 * negative 1   61.50    94.50 ns
 * negative 2   124.00   157.00 ns
 * negative 3   249.00   282.00 ns
 *
 * 说明：
 * - 这里的正/负深度本质上对应 PWM 周期的微调方向。
 * - 具体符号含义需要与底层 PWM 更新逻辑保持一致。
 */

// FSK 调制深度 +3，约 -250 ns
#define QI_FSK_DEPTH_P3    (-6)

// FSK 调制深度 +2，约 -125 ns
#define QI_FSK_DEPTH_P2    (-3)

// FSK 调制深度 +1，约 -81.3 ns
#define QI_FSK_DEPTH_P1    (-2)

// FSK 调制深度 +0，约 -41.7 ns
#define QI_FSK_DEPTH_P0    (-1)

// FSK 调制深度 -3，约 +250 ns
#define QI_FSK_DEPTH_N3    (6)

// FSK 调制深度 -2，约 +125 ns
#define QI_FSK_DEPTH_N2    (3)

// FSK 调制深度 -1，约 +81.3 ns
#define QI_FSK_DEPTH_N1    (2)

// FSK 调制深度 -0，约 +41.7 ns
#define QI_FSK_DEPTH_N0    (1)

/* ============================================================================
 * 枚举类型定义
 * ========================================================================== */

/* Qi 协议阶段枚举 */
typedef enum {
    QI_PHASE_SELECTION = 0,           /* 选择/检测阶段 */
    QI_PHASE_PING,                    /* Ping 阶段，用于设备检测 */
    QI_PHASE_ID_CONFIG,               /* 识别与配置阶段 */
    QI_PHASE_NEGOTIATION,             /* 协商阶段，仅 EPP 使用 */
    QI_PHASE_POWER_TRANSFER,          /* 功率传输阶段 */
    QI_PHASE_RENEGOTIATION,           /* 重新协商阶段 */
    QI_PHASE_CALIBRATION              /* 校准阶段 */
} qi_phase_t;

/* Qi 协议模式枚举 */
typedef enum {
    QI_MODE_BPP = 0,                  /* 基准功耗配置文件，≤5 W */
    QI_MODE_EPP                       /* 扩展电源配置文件，>5 W */
} qi_mode_t;

/* PTx 对 PRx 请求的响应类型 */
typedef enum {
    QI_RESPONSE_ACK,                  /* 确认 */
    QI_RESPONSE_NAK,                  /* 拒绝 */
    QI_RESPONSE_ND,                   /* 无定义/无数据 */
    QI_RESPONSE_ID,                   /* 返回识别信息 */
    QI_RESPONSE_CAP,                  /* 返回能力信息 */
    QI_RESPONSE_NOT_AVAILABLE         /* 数据不可用 */
} qi_response_t;

/* 设备类型枚举，基于制造商代码识别 */
typedef enum {
    QI_DEVICE_UNKNOWN = 0,            /* 未知设备 */
    QI_DEVICE_APPLE,                  /* Apple 设备 */
    QI_DEVICE_SAMSUNG,                /* Samsung 设备 */
    QI_DEVICE_HUAWEI,                 /* Huawei 设备 */
    QI_DEVICE_OPPO,                   /* OPPO 设备 */
    QI_DEVICE_VIVO,                   /* Vivo 设备 */
    QI_DEVICE_XIAOMI,                 /* Xiaomi 设备 */
    QI_DEVICE_QI_GENERIC,             /* 通用 Qi 设备 */
    QI_DEVICE_COMMON                  /* 普通兼容设备 */
} qi_device_type_t;

/* 功率控制配置枚举 */
typedef enum {
    QI_PROFILE_QI_STANDARD = 0,       /* 标准 Qi 频率/相位控制策略 */
    QI_PROFILE_APPLE_FAST             /* Apple 快充控制策略 */
} qi_power_profile_t;

/* 输入/母线功率状态枚举 */
typedef enum {
    QI_POWER_5V = 0,                  /* 5V 输入 */
    QI_POWER_9V,                      /* 9V 输入 */
    QI_POWER_12V                      /* 12V 输入 */
} qi_power_state_t;

/* 接收功率数据位宽 */
typedef enum {
    QI_RX_POWER_BITS_8 = 8,           /* 8-bit 接收功率 */
    QI_RX_POWER_BITS_24 = 24          /* 24-bit 接收功率 */
} qi_rx_power_bits_t;

/* 接收功率类型 */
typedef enum {
    QI_RX_POWER_0 = 0x0,              /* RP/0 类型 */
    QI_RX_POWER_1 = 0x1,              /* RP/1 类型 */
    QI_RX_POWER_2 = 0x2,              /* RP/2 类型 */
    QI_RX_POWER_4 = 0x4               /* RP/4 类型 */
} qi_rx_power_type_t;

/* ============================================================================
 * 数据结构定义
 * ========================================================================== */

/* 
 * 接收到的数据包结构体
 * 用于保存从无线通信解调得到的 Qi 数据包。
 */
typedef struct {
    uint8_t device;                   /* 设备 ID，通常为 0-based 索引 */
    uint8_t channel;                  /* 通道 ID */
    uint8_t header;                   /* 数据包头/数据包类型标识 */
    uint8_t data[27];                 /* 数据包负载，最大长度 */
    uint8_t length;                   /* 实际数据长度，单位：字节 */
    bool valid;                       /* 数据包有效标志 */
} qi_packet_t;

/* 
 * Qi ID 数据结构
 * 由 ID 数据包解析得到，Section 8.11。
 */
typedef struct {
    uint8_t major_version;            /* Qi 规范主版本号 */
    uint8_t minor_version;            /* Qi 规范次版本号 */
    uint16_t manufacturer_code;       /* PRMC，功率接收端制造商代码 */
    uint32_t basic_id;                /* 20-bit 基本设备标识 */
    bool ext_bit;                     /* 扩展 ID 存在标志 */
    qi_device_type_t device_type;     /* 解析得到的设备类型 */
} qi_id_data_t;

/* 
 * Qi Configuration 数据结构
 * 由 CFG 数据包解析得到，Section 8.4。
 */
typedef struct {
    uint8_t reference_power;          /* 参考功率，单位为 0.5 W */
    uint8_t window_size;              /* 窗口大小，单位为 4 ms */
    uint8_t window_offset;            /* 窗口偏移，单位为 4 ms */
    uint8_t count;                    /* 可选数据包数量 */
    bool neg_bit;                     /* 是否支持协商，EPP 使用 */
    uint8_t max_power_w;              /* 最大功率，单位：W */
    uint8_t depth;                    /* FSK 调制深度或协议相关深度参数 */
} qi_cfg_data_t;

/* 协商阶段相关数据 */
typedef struct {
    uint32_t rx_guaranteed_power;     /* 接收端保证功率 */
    qi_rx_power_bits_t rx_power_bits; /* 接收功率数据位宽 */
    uint32_t rx_wpid;                 /* 接收端无线功率 ID */
    bool end_negotiation_flag;        /* 结束协商标志 */
} qi_neg_data_t;

/* 
 * 功率传输合约结构体，Section 2.2
 * 描述 PTx 与 PRx 在功率传输阶段采用的主要控制参数。
 */
typedef struct {
    uint16_t reference_power_w;       /* 参考功率，单位：W */
    uint8_t window_size_ms;           /* 窗口大小，单位：ms */
    uint8_t window_offset_ms;         /* 窗口偏移，单位：ms */
    uint8_t control_holdoff_ms;       /* 控制保持时间，单位：ms */
    qi_mode_t mode;                   /* BPP 或 EPP 模式 */
} qi_power_contract_t;

/* ============================================================================
 * 旧版本兼容宏定义
 *
 * 说明：
 * - 以下宏用于兼容旧代码中的命名方式。
 * - 新代码建议直接使用 QI_PKT_xxx 和 QI_MANUF_xxx 命名。
 * ========================================================================== */

/* 旧版本数据包类型名称兼容 */
#define WLC_Rx_Signal_Strenth    QI_PKT_SIG
#define WLC_Rx_End_Transfer      QI_PKT_EPT
#define WLC_Rx_Control_Error     QI_PKT_CE
#define WLC_Rx_Receive_PW_8b     QI_PKT_RP8
#define WLC_Rx_Charge_Status     QI_PKT_CHS
#define WLC_Rx_Hold_Off          QI_PKT_PCH
#define WLC_Rx_General_RQ        QI_PKT_GRQ
#define WLC_Rx_Renegotiate       QI_PKT_NEGO
#define WLC_Rx_Special_RQ        QI_PKT_SRQ
#define WLC_Rx_FOD_Status        QI_PKT_FOD
#define WLC_Rx_Receive_PW_24b    QI_PKT_RP
#define WLC_Rx_Cfg               QI_PKT_CFG
#define WLC_Rx_WPID_MSB          QI_PKT_WPID_HI
#define WLC_Rx_WPID_LSB          QI_PKT_WPID_LO
#define WLC_Rx_ID                QI_PKT_ID
#define WLC_Rx_EXT_ID            QI_PKT_XID

/* 旧版本制造商代码兼容 */
#define WLC_RX_APPLE_ManufacturerCode    QI_MANUF_APPLE
#define WLC_RX_Sumsung_ManufacturerCode  QI_MANUF_SAMSUNG
#define WLC_RX_HUAWEI_ManufacturerCode   QI_MANUF_HUAWEI
#define WLC_RX_OPPO_ManufacturerCode     QI_MANUF_OPPO
#define WLC_RX_QI_Device_ID              QI_MANUF_QI_GENERIC

#endif /* _WLC_TYPES_H_ */