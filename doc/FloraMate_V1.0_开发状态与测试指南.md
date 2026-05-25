# FloraMate V1.0 — 开发状态与测试实践指南

> 文档版本：1.0.0 · 日期：2026-05-15 · 作者：tonymeng

---

## 一、整体架构速览

```
┌─────────────────────────────────────────────────────────────┐
│ App 层（业务逻辑）                                            │
│  app_init  app_main_fsm  app_pump_fsm  app_menu             │
│  app_display  app_config  app_event  app_log                │
├─────────────────────────────────────────────────────────────┤
│ BSP 层（板级抽象）                                            │
│  bsp_tick  bsp_usart_log  bsp_relay  bsp_pump_pwm           │
│  bsp_key   bsp_oled       bsp_eeprom                        │
├─────────────────────────────────────────────────────────────┤
│ Common（公共库）                                              │
│  ring_buffer   crc16   floramate_types                      │
├─────────────────────────────────────────────────────────────┤
│ HAL / CMSIS（STM32CubeMX 生成，不手动修改）                   │
└─────────────────────────────────────────────────────────────┘
```

MCU：STM32F401RCT6 @ 84 MHz，256 KB Flash，64 KB SRAM，LQFP64

---

## 二、已开发完成功能清单

### 2.1 BSP 层

| 模块 | 文件 | 实现内容 | 状态 |
|------|------|----------|------|
| **时基** | `bsp_tick.c/h` | SysTick 1 ms 基准；`GetMs` / `ElapsedMs` / `DelayMs` | ✅ 完成 |
| **USART 日志** | `bsp_usart_log.c/h` | USART1 115200 8N1；非阻塞环形缓冲 TX；`TxFlush` 中断推进 | ✅ 完成 |
| **继电器** | `bsp_relay.c/h` | CH1 水泵总电源；CH2~CH5 四路阀；CH6 备用；互锁与自检 | ✅ 完成 |
| **水泵 PWM** | `bsp_pump_pwm.c/h` | TIM2-CH1（PA0）100 Hz，1000 级分辨率；`SetDutyPercent` / `Stop` | ✅ 完成 |
| **按键** | `bsp_key.c/h` | 4 键（PA1–PA4）消抖 20 ms；短按 / 长按 / 超长按阈值可配；事件回调注入 | ✅ 完成 |
| **OLED** | `bsp_oled.c/h` + `bsp_oled_font.h` | 1.3" 128×64 SSD1306/1315 I2C；6×8 + 16×16 字模 | ✅ 完成 |
| **EEPROM** | `bsp_eeprom.c/h` | AT24C64 I2C；字节 / 页随机读写；在线探测 `IsOnline` | ✅ 完成 |

### 2.2 Common 层

| 模块 | 文件 | 实现内容 | 状态 |
|------|------|----------|------|
| **环形缓冲** | `ring_buffer.c/h` | 通用 FIFO，字节级读写，非线程安全（供 UART TX 使用） | ✅ 完成 |
| **CRC-16** | `crc16.c/h` | CRC-16-CCITT（多项式 0x1021，初值 0xFFFF）；`Compute` + `Update` | ✅ 完成 |
| **类型定义** | `floramate_types.h` | `Fm_ErrorCode` 错误码枚举；`Fm_ValveIndex`；版本宏；硬限常量 | ✅ 完成 |

### 2.3 App 层

| 模块 | 文件 | 实现内容 | 状态 |
|------|------|----------|------|
| **日志** | `app_log.c/h` + `app_log_ring_buffer.c/h` + `app_log_types.h` | MPSC 环形缓冲；6 级过滤（FATAL–VERBOSE）；紧急模式直传；限速宏；异步 UART 消费 | ✅ 完成 |
| **事件队列** | `app_event.c/h` | 8 槽 FIFO；`PostKey` 回调（供 BSP 按键调用）；`Pop`；`APP_EVENT_KEY_*` 枚举（SHORT / LONG / HOLD） | ✅ 完成 |
| **配置存储** | `app_config.c/h` | AT24C64 双 Bank 轮换；Bank header + payload 各一 CRC-16；版本管理（`APP_CONFIG_VERSION=2`）；`SetField` / `GetField` / `Dump` / `FactoryReset` | ✅ 完成 |
| **显示调度** | `app_display.c/h` | 100 ms 节流；按主 FSM 状态切换页面（BOOT / SELFTEST / IDLE 倒计时 / AUTO_RUN 进度 / MENU 委托 / DONE / ERROR / SLEEP） | ✅ 完成 |
| **调试菜单** | `app_menu.c/h` | 主菜单 / 参数列表 / 单参数编辑 / 手动测试（继电器 + PWM）/ 系统信息 / 出厂复位；K1–K4 完整映射 | ✅ 完成 |
| **主状态机** | `app_main_fsm.c/h` | 7 态 FSM：BOOT → SELFTEST → IDLE_3S → AUTO_RUN → DONE → SLEEP / ERROR；心跳 LED；总任务超时保护 | ✅ 完成 |
| **水泵子 FSM** | `app_pump_fsm.c/h` | 单路 9 态 FSM：IDLE → INIT → OPEN_MAIN → STEP（8 档）→ RAMP_DOWN → CLOSE_MAIN → CLOSE_VALVE → GAP → DONE / ERROR；PWM 阶梯执行 | ✅ 完成 |
| **应用装配** | `app_init.c/h` | `App_Init`（6 步顺序初始化）+ `App_Loop`（永不返回主循环） | ✅ 完成 |

---

## 三、暂未开发 / 预留功能清单

### 3.1 V1.0 代码中已有占位但逻辑未实现

| 功能 | 预留位置 | 说明 |
|------|----------|------|
| **AUTO_RUN 中跳过当前路（K1）** | `app_main_fsm.c:343` | `LOG_INFO("auto run: SKIP zone (K1) -- not implemented in V1.0")` 仅记录日志 |
| **AUTO_RUN 中暂停（K2）** | `app_main_fsm.c:347` | 同上；`App_Pump_Fsm_Pause/Resume` 函数已声明但未实现实际状态挂起 |
| **单路超时软检测** | `app_main_fsm.c:244-245` | 注释说明已预留注入点，目前靠总任务超时（600 s）兜底 |
| **SLEEP 状态低功耗** | `app_main_fsm.c:287-289` | 状态机进入后永久驻留，未调用 `__WFI` / Stop 模式 |
| **菜单周期逻辑** | `app_menu.c:App_Menu_Tick` | 函数体中只有预留注释，无超时返回等实际逻辑 |
| **CH6 继电器** | `bsp_relay.h: BSP_RELAY_RSV_CH6` | 枚举值已定义，实际驱动管脚已配置（PB0），业务层未使用 |

### 3.2 V2.x 规划中的扩展功能

| 功能 | 优先级 | 说明 |
|------|--------|------|
| **看门狗（IWDG）** | 高 | 错误码 `FM_ERR_020_WATCHDOG` 已占位；需在 `App_Loop` 内定期喂狗 |
| **串口命令行（CLI）** | 已完成 | `app_serial_debug`：3s 等待窗内 `debug` 进调试，否则正常 BOOT 流程；`AUTO_ENTER` 宏可上电直进调试 |
| **定时自动浇灌** | 中 | 需外挂 RTC（MCU 内置 RTC 或外置 DS3231）+ 配置时间表；EEPROM 预留空间足够 |
| **单路超时精细控制** | 低 | 目前靠总超时兜底；可在子 FSM Tick 内加 `per_channel_timeout_s` 判断 |
| **水流传感器** | 低 | 架构支持，硬件 V1.0 未焊接；可接 PA5/PA6 外部中断计脉冲 |
| **蓝牙 / WiFi 远程** | 低 | 预留 USART2 / SPI2 外扩模块；需增加通信适配层 |
| **日志持久化（EEPROM 日志区）** | 低 | AT24C64 剩余 ~7.5 KB（0x0200–0x1FFF）可用于滚动日志；EEPROM 地址空间已预留 |

---

## 四、测试流程指南

### 4.1 硬件首次上电检查清单

```
□ 确认 3.3 V / 5 V 双电源正常，无异常发热
□ ST-Link 连接成功，STM32CubeIDE Debug 模式下单步走 App_Init 前半段
□ OLED 显示 "Serial Wait" 与 `debug in Ns` 倒计时
□ USART1（PA9 TX = 115200 8N1）收到日志：
    [xxx][I][FloraMate] [App_Init,73] ==== FloraMate V1.0 ====
    [xxx][I][FloraMate] [App_Init,74] Build: May 15 2026 ...
    [xxx][I][FloraMate] [App_Init,95] init done. eeprom=1 oled=1
□ 等待窗内串口每秒输出 `[I] hb ... wait_remain_ms=...`；进入调试后无心跳
□ 发送 `debug` 后 OLED 显示当前命令与 OK/ERR，串口打印 help
□ 不发送 debug、等待 3s 后应进入 BOOT/自检/倒计时/自动浇灌流程
□ 心跳 LED（PC13）以 ~500 ms 闪烁
```

### 4.2 BSP 单模块验证

#### 4.2.1 继电器

```
测试方法：进入串口调试态（上电后发送 `debug`）
操作：`valve open 2`~`5` 测四路阀；`valve open 1` 测水泵总电源（建议先开一路阀）
验证：
  □ 对应 CH 继电器可听见"咔哒"声
  □ OLED 显示阀号和 ON/OFF 状态
  □ 串口无 I2C 错误日志
退出时验证继电器全部 OFF（发送 `stop` 或 `valve close`）
```

#### 4.2.2 水泵 PWM

```
测试方法：进入串口调试态后用阀和泵命令联调
推荐步骤：
  1. `valve open 2`（任一路阀）
  2. `pump 30`，再按需 `pump 50` / `pump 80`
  3. 用示波器或逻辑分析仪测量 PA0 波形：
     □ 频率 100 Hz（±1%）
     □ 占空比与 OLED 显示一致
  4. `pump off`，验证 PWM 归零且 CH1 释放
```

#### 4.2.3 OLED

```
在 IDLE 倒计时页观察：
  □ 倒计时数字每秒递减（使用 16×16 大字模）
  □ 屏幕无撕裂 / 残影
  □ 调整菜单中的 oled_contrast 参数，保存后对比度变化
```

#### 4.2.4 按键

```
  □ SYS_SERIAL_WAIT / SYS_SERIAL_DEBUG 下按键扫描暂停，按 K1~K4 不应改变状态
  □ `valve open 1` + `valve open 2` 可同时 on；`valve close` 全部释放
恢复菜单/自动流程后再按原 IDLE / DONE / ERROR 映射验证按键
```

#### 4.2.5 EEPROM

```
验证步骤：
  1. 进入菜单 → 参数列表，将某个参数改为非默认值（如 step_count=3）
  2. 长按 K3 保存 → 日志出现 "config saved to bank X"
  3. 断电 → 重新上电
  4. 进入菜单验证修改已持久化
  5. 工厂复位页长按 K4（3 s）确认 → 验证参数恢复默认值
  6. 串口确认 "factory reset done"
```

### 4.3 系统级端到端测试

#### 4.3.1 标准浇灌流程（当前阶段暂停）

```
前提：至少 channel_enable bit0 = 1（Z1 开启），step_count ≥ 1，step_duty[0] > 0
步骤（恢复自动流程后执行）：
  1. 上电 → 等待 IDLE 倒计时结束（或 K1 长按跳过）
  2. 观察 AUTO_RUN 进度：
     □ OLED 显示 "Zone 1 / Step X / Duty Y%"
     □ 当前路阀 CH2~CH5 先吸合，约 200 ms 后 CH1 水泵电源吸合
     □ PWM 按配置阶梯输出
  3. 单路完成后观察：
     □ PWM 先 RAMP_DOWN → CH1 断开 → 阀断开 → 路间静默
     □ 继续下一路（若已使能）
  4. 全部完成后：
     □ 进入 DONE 页（"All Done!"），2 s 后转 SLEEP
     □ 所有继电器确认为 OFF 状态（万用表/目视指示灯）
```

#### 4.3.2 停止按钮测试

```
步骤：AUTO_RUN 进行中 → 短按 K3
验证：
  □ 日志 "auto run: user STOP (K3)"
  □ 所有继电器立即 OFF（App_Pump_Fsm_Abort 调用 MainOffForce）
  □ 系统进入 DONE 后转 SLEEP
  □ 水流在 < 500 ms 内停止（阀门响应时间）
```

#### 4.3.3 超时保护测试

```
方法：将 total_timeout_s 设置为极小值（如 5 s）
步骤：开始自动浇灌 → 不干预 → 等待 5 s
验证：
  □ 日志出现 "auto run: TOTAL timeout (5s)"
  □ 系统进入 ERROR 状态
  □ 所有继电器 OFF，心跳 LED 快速闪烁（100 ms）
  □ OLED 显示错误码
```

#### 4.3.4 EEPROM 掉电安全测试

```
步骤：
  1. 开始保存配置时（长按 K3），立即断电
  2. 重新上电，观察配置是否完好
原理：双 Bank 写入先 payload 后 header；
     断在 payload 写入中途 → 该 Bank header 未更新，回退到旧 Bank；
     断在 header 写入中途 → CRC 校验失败，回退到旧 Bank。
预期：任何断电时机均不会丢失上一次成功保存的配置。
```

### 4.4 串口调试方法

连接 USART1（PA9，115200 8N1），使用 SecureCRT / MobaXterm / VSCode Serial Monitor：

```
观察启动序列（关键日志示例）：
[0][I][FloraMate] [App_Init,73] ==== FloraMate V1.0 ====
[1][I][FloraMate] [App_Init,74] Build: May 15 2026 16:00:00
[2][I][FloraMate] [App_Config_Init,...] config loaded from bank A (seq=3)
[3][I][FloraMate] [App_Init,95] init done. eeprom=1 oled=1
[I] boot: send debug within 3s for serial debug
[I] hb uptime_ms=1000 wait_remain_ms=2000
debug
[I] serial debug active (no hb)
```

调整日志等级（菜单 log_level 参数）：
- `0` = ERROR 只看报错
- `1` = WARN  推荐日常调试
- `2` = INFO  查看状态转移（默认）
- `3` = DEBUG 查看 FSM 每步细节

常用命令见 `串口调试指令说明_V1.0.md`。最小联调序列：

```text
debug
status
valve open 0
pump 30
pump off
valve close
```

---

## 五、后续开发优先级建议

### 阶段 1（硬件验证期，本周）

```
1. 完成 § 4.1 ~ § 4.3 所有硬件验证项
2. 记录并修复实测中发现的参数不合理（阀门开合时序、PWM 阶梯时间）
3. 确认 EEPROM 双 Bank 切换正常（§ 4.2.5 + § 4.3.4）
```

### 阶段 2（功能完善，2~3 周）

```
4. 实现看门狗（IWDG）
   - App_Loop 每轮喂狗
   - 配置超时约 2 s（主循环空载 < 200 μs，有充足余量）

5. 恢复/完善自动运行入口
   - 需要长期停在调试态时，将 `APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT` 置 1 重新编译

6. 实现单路超时精细检测
   - 在 App_Main_Fsm_Tick 的 AUTO_RUN case 内，对当前运行路
     增加 per_channel_timeout_s 判断并记录错误码
```

### 阶段 3（可选增强）

```
7. 日志持久化
   - 在 EEPROM 0x0200 起划出滚动日志区（每条 32 B，可存约 230 条）
   - App_Log 增加 RegisterPersistCallback，将 FATAL/ERROR 级别落盘

8. RTC + 定时浇灌
   - 接入 MCU 内置 RTC（无需外接晶振，F401 内置 RC 精度够用）
   - 在 EEPROM 中增加 schedule[7] 数组（每天时刻 + 使能位）
   - App_Main_Fsm 在 SLEEP 态周期唤醒比对 RTC

9. 暂停 / 继续功能
   - App_Pump_Fsm_Pause：保存 step_idx，PWM 降为 0 并断 CH1
   - App_Pump_Fsm_Resume：恢复 CH1 + 剩余时间继续
```

---

## 六、已知约束与注意事项

| 约束 | 说明 |
|------|------|
| **浇灌期间不可断电** | V1.0 无断电续浇逻辑；若中途断电，重启后会重新从第一路开始 |
| **EEPROM 寿命** | AT24C64 典型写寿命 1,000,000 次；双 Bank + `update_count` 统计；建议累计写入 > 500,000 次时更换芯片 |
| **看门狗未启用** | V1.0 无 IWDG；软件死锁时需手动断电复位，**待阶段 2 修复** |
| **上电 3s 调试窗** | 默认 3s 内可发 `debug` 联调，超时走正常自动浇灌；`AUTO_ENTER=1` 可常开调试 |
| **CH6 未使用** | 枚举已定义，管脚已配置（PB0），暂无业务连接 |
| **K1 / K2 运行中无效** | AUTO_RUN 期间 K1（跳路）、K2（暂停）仅记录日志，**V1.0 预留** |
| **SLEEP 不省电** | 进入 SLEEP 后 CPU 仍全速运行；**阶段 2 加 `__WFI`** |
| **单路最大占空比 95%** | `FM_PUMP_DUTY_MAX_PERCENT = 95`，硬限，不允许 100%（过热保护） |
| **总任务硬限 600 s** | `FM_TOTAL_TASK_HARD_LIMIT_S = 600`，代码层不允许 EEPROM 配置覆盖 |

---

*文档由 AI 辅助生成，基于代码实际实现状态；如需更新请同步修改本文件。*
