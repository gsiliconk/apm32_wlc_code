# APM32 WLC Code

这是一个基于 `APM32F103CBT6 + FreeRTOS` 的 Qi v1.3 无线充电发射端固件工程，同时包含一套围绕当前源码整理完成的中文统一项目教程。

这个仓库的目标不是只放一份单纯的 MCU 工程，而是把下面两部分一起保留下来：

- 无线充电发射端固件源码与 Keil 工程
- 面向新手的统一项目教程站

## 项目内容

当前仓库主要包含这些部分：

- `Bsp/`
  - 板级驱动
  - 包括 ADC、PWM、IRQ、EEPROM、QC、UART 等底层模块
- `Core/`
  - 主入口、中断、FreeRTOS 任务入口
- `Drivers/`
  - APM32 标准外设库与 CMSIS 相关文件
- `FreeRTOS/`
  - FreeRTOS 内核与移植相关代码
- `Lib/`
  - 协议和公共库相关头文件
- `User/`
  - 无线充电协议驱动、服务层、应用层代码
- `MDK-ARM/`
  - Keil 工程文件
- `HW_Doc/`
  - 项目硬件资料与统一教程 Markdown 正文
- `tutorial-site/`
  - 教程展示站点前端工程

## 教程说明

仓库中已经整理出一份统一教程，正文来源是：

- [HW_Doc/WLC_Complete_Guide.md](D:/apm32_wlc_code/wlc_code/HW_Doc/WLC_Complete_Guide.md)

教程站前端位于：

- [tutorial-site](D:/apm32_wlc_code/wlc_code/tutorial-site)

这个教程站当前只保留“完整小白教程”入口，不再混入面试速查内容。

## 如何启动教程站

进入目录：

```powershell
cd tutorial-site
```

首次使用先安装依赖：

```powershell
npm install
```

开发模式启动：

```powershell
.\启动教程站.cmd
```

或者直接运行：

```powershell
npm run dev
```

默认访问地址：

- [http://127.0.0.1:4175/](http://127.0.0.1:4175/)

构建命令：

```powershell
npm run build
```

## 开发说明

这个仓库当前更适合作为“项目源码 + 项目教程”的组合仓库使用。

如果你主要想看项目整体结构，建议先按这个顺序：

1. 阅读 `HW_Doc/WLC_Complete_Guide.md`
2. 再看 `Core/Src/freertos_task.c`
3. 再看 `User/WLC_driver/`、`User/WLC_server/`、`User/WLC_app/`
4. 最后结合 `Bsp/` 看底层硬件驱动实现

## 说明补充

- 仓库已上传统一教程站相关代码
- 面试精简版教程文件未上传到 GitHub
- `tutorial-site/node_modules`、`tutorial-site/dist` 等本地产物已忽略
