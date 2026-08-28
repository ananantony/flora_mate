《"花伴"(FloraMate) 智能家庭灌溉系统》STM32CubeMX 工程配置表 V2.0（Top 板 / MCU 最小系统板）

────────────────────────────────────────

## 1. 文档信息

| 项目 | 内容 |
|---|---|
| 文档名 | 花伴 FloraMate · CubeMX 工程配置表（Top 板） |
| 版本 | **V2.0**（对齐双板架构 · Top 板原理图 `SCH_Schematic1_2026-08-25.pdf`） |
| 修订记录 | **V2.0**：由 V1.4（WeAct 核心板 / 单板 V3.0）全面改写，适配 **FloraMate Top 板（MCU 最小系统板）**。**按键**改至 **PC0~PC3**；**心跳 LED** 改至 **PC12**；**I2C2（PB10/PB12）** 至 2×10Pin 驱动板互联；控制信号 **PA0~PA5** 经 510 Ω 限流后接 U1 排针；调试串口经 **CH340C**；**不启用** MCU 片上 USB FS。 |
| 芯片 | **STM32F401RCT6**（Cortex-M4F，84 MHz，64 KB SRAM，256 KB Flash，LQFP64） |
| 目标硬件 | **FloraMate Top 板 V1.0**（自研 MCU 最小系统板，图纸名 `top`） |
| 依据文档 | `top板硬件设计方案.md` / `总体设计书_V1.2.md` / 驱动板方案（控制逻辑仍为 GPIO High = 负载 ON） |
| 用途 | 在 STM32CubeMX 中**按本表勾选/填参**，生成与 Top 板硬件 1:1 对应的初始工程 |
| 库选择 | **HAL** |
| Toolchain | MDK-ARM V5 / STM32CubeIDE / Makefile，任选 |

> **与 V1.4 的关键差异速查**
>
> | 功能 | V1.4（WeAct 单板） | V2.0（Top 板） |
> |---|---|---|
> | 按键 K1~K4 | PA6/PA7/PB0/PB1 | **PC0/PC1/PC2/PC3** |
> | 心跳 LED | PC13 | **PC12** |
> | I2C2（驱动板） | 无 | **PB10=SCL，PB12=SDA** |
> | 调试串口 | 排针 USART1 | **CH340C USB 虚拟串口** |
> | 泵/阀控制 | 板载 TLP281 | **PA0~PA5 → U1 → 驱动板** |
> | PC13 | LED | **悬空预留** |

────────────────────────────────────────

## 2. 新建工程

1. **File → New Project**（或启动页 **ACCESS TO MCU SELECTOR**）。
2. **Part Number** 搜 `STM32F401RC` → 选 **STM32F401RCTx**（LQFP64）。
3. 双击芯片型号 → 进入引脚/外设视图。

> 本板为 **LQFP64 全引脚布局** 的专用 PCB，工程必须按 **STM32F401RCT6** 芯片全脚位建模，不可选 LQFP48。

────────────────────────────────────────

## 3. SYS（系统）配置

| 项 | CubeMX 操作 | 目标 |
|---|---|---|
| Debug | **SYS → Debug: Serial Wire** | 保留 **PA13=SWDIO、PA14=SWCLK**；**严禁**选 `No Debug` |
| Timebase Source | **SYS → Timebase Source: SysTick** | 与 HAL 默认一致；**勿**选 TIM2（TIM2 已被 PWM 占用） |

**约束**：PA13/PA14/PA9/PA10/PA11/PA12 这些有特殊功能的引脚，未经评审**不得**挪作业务 GPIO。

### 3.1 Top 板已占用 / 默认连接的引脚

| 引脚 | 板上连接 | CubeMX 处理 |
|---|---|---|
| **PH0 / PH1** | HSE 25 MHz 晶振 X2 + **15 pF×2**（C12/C13） | RCC 选 HSE = Crystal/Ceramic Resonator |
| **PC14 / PC15** | 未贴 LSE 晶振 | RCC LSE = **Disable**；**不要**作 GPIO |
| **PA11 / PA12** | 经 R14/R16 **10 kΩ 下拉**至 GND（禁用 MCU USB 物理层） | **不启用** USB OTG FS；保持空闲 |
| **PA13 / PA14** | SWD1 排针 | SYS Debug: SW |
| **BOOT0** | R24 **10 kΩ 下拉** | 默认从 Flash 启动，无需 MX 配置 |
| **NRST** | R25 10 kΩ 上拉 + C14 100 nF | 默认，无需 MX 配置 |
| **PA0 ~ PA5** | 510 Ω 串阻 → U1 → 驱动板 PUMP/VALVE CMD | PA0=TIM2 PWM；PA1~PA5=GPIO 阀控 |
| **PC0 ~ PC3** | KEY1 端子 K1~K4（RC 滤波 + 10 kΩ 外部上拉） | GPIO Input，**No pull** |
| **PC12** | 心跳灯 LED2（原理图位号；本文档称 LED1），阳极经 **R20 (470Ω)** 接 3.3V，低电平点亮 | GPIO Output，初始 **High（灭）** |
| **PB6 / PB7** | I2C1 → AT24C08C + OLED1（R6/R7 4.7 kΩ 上拉） | I2C1 Fast 400 kHz |
| **PB10 / PB12** | I2C2 → U1 Pin15/16 → 驱动板（R8/R9 4.7 kΩ 上拉 + R13/R17 100 Ω 串阻） | **PB10=I2C2_SCL**；**PB12 接 SDA 网络**（见 § 6.6 复用说明） |
| **PA9 / PA10** | 经 R12/R15 1 kΩ → CH340C → Type-C USB | USART1 115200 8N1 |

────────────────────────────────────────

## 4. RCC 时钟配置

### 4.1 RCC 外设页

| 项 | 选择 |
|---|---|
| High Speed Clock (HSE) | **Crystal/Ceramic Resonator**（板载 **25 MHz**） |
| Low Speed Clock (LSE) | **Disable**（本版不启用 RTC） |
| Master Clock Output (MCO) | Disable |
| Audio Clock Input (I2S) | Disable |

### 4.2 Clock Configuration 页（**关键，必须精确**）

```
Input frequency        : 25 MHz   (HSE)
PLL Source Mux         : HSE
/M  (PLLM)            : 25        → 1 MHz
×N  (PLLN)            : 168       → 168 MHz (VCO 输出)
/P  (PLLP)            : 2         → SYSCLK = 84 MHz   ★
/Q  (PLLQ)            : 4         → 42 MHz (USB 不启用，PLLQ 数值无关紧要)
System Clock Mux       : PLLCLK
AHB Prescaler          : /1        → HCLK   = 84 MHz
APB1 Prescaler         : /2        → PCLK1  = 42 MHz, APB1 Timer Clock = 84 MHz
APB2 Prescaler         : /1        → PCLK2  = 84 MHz, APB2 Timer Clock = 84 MHz
Voltage Scale          : Scale 2   (F401 ≤ 84 MHz 用 Scale 2)
```

设置完毕后 MX 顶部应显示 **84 MHz** 绿色（不要黄/红）。

### 4.3 Flash 等待周期（自动）

84 MHz / 3.3 V → **Latency = 2 WS**，MX 会自动配置。

────────────────────────────────────────

## 5. 外设清单（必配）

| 外设 | 引脚 / 模式 | CubeMX 主要选择 |
|---|---|---|
| **TIM2** PWM（水泵） | PA0 = `TIM2_CH1`（AF1） | Label: `PUMP_PWM`；**PWM Generation CH1**；初始 CCR=0 |
| **GPIO** 阀 ×5 | PA1~PA5 = `GPIO_Output` | Label: `VALVE_Z1`~`VALVE_Z5`；初始 **Low**（失效安全） |
| **GPIO** 按键 ×4 | PC0~PC3 = `GPIO_Input` | Label: `KEY_K1`~`KEY_K4`；**No pull**（板载 10 kΩ 外部上拉） |
| **GPIO** 心跳灯 | PC12 = `GPIO_Output` | Label: `LED_HEARTBEAT`；初始 **High（灭）** |
| **I2C1** | PB6=SCL，PB7=SDA（AF4） | Fast Mode **400 kHz**；OLED 0x3C + AT24C08C 0x50 |
| **I2C2** | PB10=SCL（AF4）；PB12=SDA 网络 | 见 § 6.6；经 U1 至驱动板（Phase 2 功率计） |
| **USART1** | PA9=TX，PA10=RX（AF7） | **115200 8N1**；经 CH340C 输出 USB 虚拟串口 |

> **Top 板控制信号分组**：PA0~PA5 连续 6 脚 = 泵 PWM + 阀 Z1~Z5，经 **510 Ω** 限流后接 **U1（2×10Pin）** 下发至驱动板光耦输入；逻辑仍为 **GPIO/PWM High = 光耦 ON = 负载通电**。

**不启用的外设**：USB OTG FS、SPI、ADC1（Phase 2）、IWDG（开发期）、RTC、CAN、SDIO、I2S。

────────────────────────────────────────

## 6. 各外设参数细节（Parameter Settings）

### 6.1 GPIO 输出 — 阀控制 + 心跳灯

| 引脚 | Label | Output level | Mode | Speed | Pull | 说明 |
|---|---|---|---|---|---|---|
| **PA1** | `VALVE_Z1` | **Low** | Push-Pull | Low | No pull | 经 R2(510Ω)→U1→驱动板 VALVE1_CMD |
| **PA2** | `VALVE_Z2` | **Low** | Push-Pull | Low | No pull | 经 R4(510Ω)→VALVE2_CMD |
| **PA3** | `VALVE_Z3` | **Low** | Push-Pull | Low | No pull | 经 R5(510Ω)→VALVE3_CMD |
| **PA4** | `VALVE_Z4` | **Low** | Push-Pull | Low | No pull | 经 R10(510Ω)→VALVE4_CMD |
| **PA5** | `VALVE_Z5` | **Low** | Push-Pull | Low | No pull | 经 R11(510Ω)→VALVE5_CMD |
| **PC12** | `LED_HEARTBEAT` | **High** | Push-Pull | Low | No pull | LED2 阴极接 PC12（原理图位号；本文档称 LED1），阳极经 **R20 (470Ω)** 接 3.3V；**低电平点亮** |

> **P 沟道高边极性（驱动板侧，与 V3.0 一致）**
>
> | Top 板输出 | 驱动板 TLP281 | 负载 |
> |---|---|---|
> | **Low（复位默认）** | 截止 | 断电 ✅ 失效安全 |
> | **High（开阀/起泵）** | 导通 | 通电 ✅ |

### 6.2 GPIO 输入 — 4 路按键（PC0~PC3）

| 引脚 | Label | Mode | Pull | 触发方式 | 说明 |
|---|---|---|---|---|---|
| **PC0** | `KEY_K1` | Input | **No pull** | 软件 20 ms 轮询 | K1；板载 R29(10k)+C21(100n) RC 滤波 |
| **PC1** | `KEY_K2` | Input | **No pull** | 同上 | K2；R30+C22 |
| **PC2** | `KEY_K3` | Input | **No pull** | 同上 | K3；R28+C20 |
| **PC3** | `KEY_K4` | Input | **No pull** | 同上 | K4；R31+C23 |

> 每路按键已有 **10 kΩ 外部上拉 + ~1 ms RC 硬件消抖**，CubeMX **勿开内部 Pull-up**（避免与外部上拉并联造成不确定电平）。固件仍建议保留 **20 ms 软件去抖**。
>
> 按键功能映射（与总体设计书一致）：K1=启动/跳过自检，K2=跳过当前路，K3=暂停/恢复，K4=菜单/确认。

### 6.3 未使用引脚处理

本项目**不考虑低功耗**，未使用引脚保持 **Reset 默认态（灰色）**：

- CubeMX **不勾选** "Set all free pins as analog"（见 § 9.2）；
- **保持 Reset 的引脚**：PC13、PC14/PC15、PA6/PA7、PC4~PC11、PB0~PB9、PB11(无)、PB13~PB15、PA8/PA15、PD2 等；
- Phase 2 扩展传感器优先从 **PA6/PA7、PB8/PB9、PB13~PB15、PA8** 取用。

### 6.4 TIM2 — Channel 1 PWM（PA0，水泵调速）

页面：**Timers → TIM2 → Mode**

| 项 | 设置 | 说明 |
|---|---|---|
| Clock Source | **Internal Clock** | APB1 Timer = 84 MHz |
| Channel1 | **PWM Generation CH1** | PA0 → R1(510Ω) → U1 PUMP_CMD |
| Channel2/3/4 | Disable | — |

页面：**TIM2 → Parameter Settings**

| 项 | 数值 | 计算 / 说明 |
|---|---|---|
| **Prescaler (PSC)** | **839** | 84 MHz / 840 = 100 kHz |
| **Counter Period (ARR)** | **999** | 100 kHz / 1000 = **100 Hz** PWM ★ |
| Counter Mode | Up | — |
| auto-reload preload | **Enable** | 占空比无毛刺更新 |
| Master/Slave Mode | **Disable** | — |

**PWM Generation Channel 1** 子参数：

| 项 | 数值 | 说明 |
|---|---|---|
| Mode | PWM mode 1 | CNT < CCR → 高电平 |
| **Pulse (CCR)** | **0** | 上电停泵 ★ |
| Output compare preload | **Enable** | — |
| CH Polarity | **High** | PA0 高 → 光耦 ON → 水泵加电 |

> 占空比公式：`CCR = 999 × duty%`。固件：`__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr)`。
>
> **干转保护**仍为软件互锁：占空比 > 0 前须 `Bsp_Valve_AnyOn()==true`（驱动板侧逻辑不变）。

### 6.5 I2C1 — OLED + AT24C08C

页面：**Connectivity → I2C1 → Mode**

| 项 | 选择 |
|---|---|
| **I2C** | **I2C** |
| I2C Speed Mode | **Fast Mode** |
| I2C Speed Frequency (Hz) | **400000** |
| Fast Mode Duty Cycle | **Mode_2** |

页面：**I2C1 → GPIO Settings**

| 引脚 | Signal | Mode | Pull | Speed | 说明 |
|---|---|---|---|---|---|
| **PB6** | I2C1_SCL | AF Open Drain | **No pull-up** | High | 板载 R6=4.7kΩ 外部上拉 |
| **PB7** | I2C1_SDA | AF Open Drain | **No pull-up** | High | 板载 R7=4.7kΩ 外部上拉 |

> 从机地址：OLED **0x3C**（SH1106/SSD1306）；AT24C08C **0x50**（E0/E1/E2/WP 均接 GND）。

### 6.6 I2C2 — 驱动板互联（功率计预留）

> **原理图走线（已确认）**
>
> | 网络 | MCU 引脚 | 硬件 |
> |---|---|---|
> | I2C2_SCL | **PB10**（Pin 29） | R8=4.7kΩ 上拉 + R13=100Ω 串阻 → U1 Pin15 |
> | I2C2_SDA | **PB12**（Pin 33） | R9=4.7kΩ 上拉 + R17=100Ω 串阻 → U1 Pin16 |
>
> PB8/PB9 **未** 接入 I2C2 总线（悬空预留）。

**STM32F401 复用功能重要说明**

| 引脚 | 原理图网络 | 芯片 AF4 功能 | CubeMX 能否配为标准 I2C2 |
|---|---|---|---|
| **PB10** | I2C2_SCL | **I2C2_SCL** | ✅ 可以 |
| **PB12** | I2C2_SDA | **I2C2_SMBA**（SMBus 告警，**非 SDA**） | ❌ 不能作为硬件 I2C2_SDA |
| PB9（未用） | — | **I2C2_SDA** | ✅ 芯片原生 SDA 脚，但本板未走线 |

LQFP64 封装**无 PB11**（另一 I2C2_SDA 复用脚），因此本板若要用 **硬件 I2C2 外设**，理想 SDA 应为 **PB9**，而原理图实际接到 **PB12**。

**推荐 CubeMX / 固件策略（二选一）**

| 方案 | CubeMX 配置 | 适用场景 |
|---|---|---|
| **A（首版推荐）** | 仅启用 **I2C2_SCL=PB10**；**PB12 保持 GPIO 开漏输出/输入**，固件对 SDA 做 **软件 bit-bang** | 不改 PCB，与原理图 PB10/PB12 网络一致 |
| **B（改板后）** | **I2C2：PB10(SCL) + PB9(SDA)**，400 kHz | 需把 SDA 走线从 PB12 改到 PB9 |

若选方案 A，CubeMX 中 **不要** 把 PB12 设为 I2C2_SDA（MX 不会提供该选项）；I2C2 外设只绑定 SCL，SDA 由 BSP 软件模拟。

页面：**Connectivity → I2C2 → Mode**（方案 A：仅作参考初始化，或暂不启用直至驱动板就绪）

| 项 | 选择 |
|---|---|
| **I2C** | **I2C** |
| I2C Speed Mode | **Fast Mode** |
| I2C Speed Frequency (Hz) | **400000** |

页面：**I2C2 → GPIO Settings**

| 引脚 | Signal | Mode | Pull | Speed | 说明 |
|---|---|---|---|---|---|
| **PB10** | I2C2_SCL | AF Open Drain | **No pull-up** | High | R8=4.7kΩ 外部上拉 + R13 串阻 |
| **PB12** | （GPIO 标签 `I2C2_SDA_BB`） | **GPIO Output Open Drain** 或 Input | No pull | High | 软件模拟 SDA；**勿**选 I2C2_SDA AF |

> I2C2 event/error 中断：**Disable**（首版可暂不启用整个 I2C2 外设，待驱动板功率计就绪后再集成）。

### 6.7 USART1 — CH340C USB 调试串口

页面：**Connectivity → USART1 → Mode**

| 项 | 选择 |
|---|---|
| Mode | **Asynchronous** |
| Hardware Flow Control | **Disable** |

页面：**USART1 → Parameter Settings**

| 项 | 值 |
|---|---|
| Baud Rate | **115200** |
| Word Length | 8 Bits |
| Parity | **None** |
| Stop Bits | **1** |
| Data Direction | Receive and Transmit |

页面：**USART1 → GPIO Settings**

| 引脚 | Signal | Mode | Pull | Speed | 说明 |
|---|---|---|---|---|---|
| **PA9** | USART1_TX | AF Push-Pull | No pull | High | 经 R12(1kΩ)→CH340C RXD |
| **PA10** | USART1_RX | AF Push-Pull | **Pull-up** ★ | High | 经 R15(1kΩ)→CH340C TXD；防 USB 未插时 RX 浮空 ORE |

页面：**USART1 → NVIC Settings**

| 项 | 选择 |
|---|---|
| USART1 global interrupt | **Enabled**，Preemption Priority = **6** |

────────────────────────────────────────

## 7. NVIC（中断优先级）

### 7.1 优先级分组

| 项 | 选择 |
|---|---|
| Priority Group | **4 bits for pre-emption priority, 0 bits for subpriority** |

### 7.2 各中断优先级

| 中断 | Enable | Pre-emption Priority | 说明 |
|---|---|---|---|
| NMI / HardFault / MemManage / BusFault / UsageFault | 默认 | 0 | 系统级 |
| PendSV | 默认 | 15 | HAL 用，勿改 |
| SysTick | 默认 | **15** | HAL 1 ms tick |
| **USART1 global interrupt** | **Enable** | **6** | RX 环形缓冲 |
| TIM2 global interrupt | **Disable** | — | PWM 不需中断 |
| I2C1 / I2C2 event & error | **Disable** | — | 阻塞模式即可 |
| EXTI（任何） | **Disable** | — | 按键走轮询 |
| IWDG | **Disable** | — | 开发期不启用 |

────────────────────────────────────────

## 8. Project Manager（工程生成选项）

### 8.1 Project 页

| 项 | 推荐 |
|---|---|
| Project Name | `FloraMate_F401`（或自定义） |
| Project Location | 避免中文路径与空格 |
| Toolchain / IDE | MDK-ARM V5 / STM32CubeIDE / Makefile |
| Minimum Heap Size | **0x200** |
| Minimum Stack Size | **0x800** |

### 8.2 Code Generator 页

| 项 | 推荐 | 说明 |
|---|---|---|
| Copy only the necessary library files | ☑ | 工程瘦身 |
| Generate peripheral initialization as a pair of '.c/.h' files | ☑ ★ | 每外设独立 gpio/tim/i2c/usart |
| Backup previously generated files when re-generating | ☑ | — |
| Keep User Code when re-generating | ☑ ★ | 保留 USER CODE 区 |
| Set all free pins as analog | **☐ 不勾** ★ | 未用引脚保持 Reset 默认态 |

────────────────────────────────────────

## 9. 全引脚分配总表（Top 板 · LQFP64）

| 引脚 | 原理图网络 | 本项目 CubeMX 配置 | 备注 |
|---|---|---|---|
| **PA0** | PUMP_CMD | **TIM2_CH1 PWM** `PUMP_PWM` | R1=510Ω→U1；100 Hz；CCR=0 |
| **PA1** | VALVE1_CMD | **GPIO Out Low** `VALVE_Z1` | R2=510Ω |
| **PA2** | VALVE2_CMD | **GPIO Out Low** `VALVE_Z2` | R4=510Ω |
| **PA3** | VALVE3_CMD | **GPIO Out Low** `VALVE_Z3` | R5=510Ω |
| **PA4** | VALVE4_CMD | **GPIO Out Low** `VALVE_Z4` | R10=510Ω |
| **PA5** | VALVE5_CMD | **GPIO Out Low** `VALVE_Z5` | R11=510Ω |
| **PA6** | — | 未配置（Reset） | Phase 2 备用 |
| **PA7** | — | 未配置（Reset） | Phase 2 备用 |
| **PA8** | — | 未配置（Reset） | Phase 2 备用 |
| **PA9** | USART1_TX | **USART1_TX** AF7 `USART1_TX` | → CH340C |
| **PA10** | USART1_RX | **USART1_RX** AF7 Pull-up `USART1_RX` | → CH340C |
| **PA11** | PA11（R14 下拉） | 保留，USB 不启用 | — |
| **PA12** | PA12（R16 下拉） | 保留，USB 不启用 | — |
| **PA13** | SWDIO | **SYS Debug: SW** | SWD1 |
| **PA14** | SWCLK | **SYS Debug: SW** | SWD1 |
| **PA15** | — | 未配置（Reset） | 备用 |
| **PB0~PB5** | — | 未配置（Reset） | 备用 |
| **PB6** | I2C1_SCL | **I2C1_SCL** AF4 OD `I2C1_SCL` | EEPROM + OLED |
| **PB7** | I2C1_SDA | **I2C1_SDA** AF4 OD `I2C1_SDA` | EEPROM + OLED |
| **PB8~PB9** | — | 未配置（Reset） | 备用；PB9 为芯片原生 I2C2_SDA |
| **PB10** | I2C2_SCL | **I2C2_SCL** AF4 OD `I2C2_SCL` | R8+R13 → U1 Pin15 |
| **PB11** | — | （LQFP64 无此脚） | — |
| **PB12** | I2C2_SDA | **GPIO OD** `I2C2_SDA_BB` | R9+R17 → U1 Pin16；软件模拟 SDA |
| **PB13~PB15** | — | 未配置（Reset） | 备用 |
| **PC0** | K1 | **GPIO In No-Pull** `KEY_K1` | RC 滤波按键 |
| **PC1** | K2 | **GPIO In No-Pull** `KEY_K2` | 同上 |
| **PC2** | K3 | **GPIO In No-Pull** `KEY_K3` | 同上 |
| **PC3** | K4 | **GPIO In No-Pull** `KEY_K4` | 同上 |
| **PC4~PC11** | — | 未配置（Reset） | 备用 |
| **PC12** | LED2（原理图位号；本文档称 LED1） | **GPIO Out High** `LED_HEARTBEAT` | 低电平点亮；R20=470Ω |
| **PC13** | PC13 | 未配置（Reset） | 悬空预留 |
| **PC14/PC15** | LSE 焊盘（未贴） | 保留，勿改 GPIO | — |
| **PD2** | — | 未配置（Reset） | 备用 |
| **PH0/PH1** | HSE 25 MHz | **RCC: HSE Crystal** | C12/C13=15pF |
| **NRST** | NRST | 默认 | R25 上拉 |
| **BOOT0** | BOOT0 | 默认 | R24 下拉 → Flash 启动 |

**Cube Label 汇总**：`PUMP_PWM`(PA0)、`VALVE_Z1`~`VALVE_Z5`(PA1~PA5)、`KEY_K1`~`KEY_K4`(PC0~PC3)、`LED_HEARTBEAT`(PC12)、`I2C1_SCL/SDA`(PB6/PB7)、`I2C2_SCL`(PB10)、`I2C2_SDA_BB`(PB12)、`USART1_TX/RX`(PA9/PA10)。

**GPIO 统计**：PWM 1 + 阀 5 + 按键 4 + I2C 4 + 串口 2 + LED 1 = **17 个**业务脚。

────────────────────────────────────────

## 10. 生成代码后必做事项

### 10.1 USART1 RX 中断 + 环形缓冲

CubeMX 只生成 `MX_USART1_UART_Init()`。在 `main.c` 中启动 `HAL_UART_Receive_IT()`，RX ISR 仅入环形缓冲，**禁止在 ISR 里 printf**。

### 10.2 TIM2 PWM 启动

```c
/* USER CODE BEGIN 2 */
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);  /* 占空比 0%，水泵停 */
/* USER CODE END 2 */
```

### 10.3 I2C1 上电扫描

调试期确认 **0x3C**（OLED）与 **0x50**（AT24C08C）同时在线。

### 10.4 从 V1.4 迁移固件时需改的 BSP 引脚

| 模块 | V1.4 引脚 | V2.0 引脚 |
|---|---|---|
| `bsp_key` | PA6/PA7/PB0/PB1 | **PC0/PC1/PC2/PC3** |
| `bsp_led` / 心跳 | PC13 | **PC12** |
| I2C2（新增） | — | PB10=SCL；PB12=SDA（软件 bit-bang） |

泵/阀/串口/I2C1 引脚与 V1.4 **相同**，BSP 可直接复用（除按键与 LED）。

### 10.5 GPIO 上电默认态

确认 `MX_GPIO_Init()` 在业务代码之前执行；PA1~PA5 初始 **Low** → 驱动板光耦截止 → 全部负载断电（失效安全）。

────────────────────────────────────────

## 11. 工程构建期自检表

| 序 | 检查项 | 通过判据 |
|---|---|---|
| 1 | Clock Configuration | **84 MHz** 绿色 |
| 2 | PA0 | `TIM2_CH1`，Label=`PUMP_PWM` |
| 3 | PA1~PA5 | 全部 `GPIO_Output Low`，Label=`VALVE_Z1`~`VALVE_Z5` |
| 4 | PC0~PC3 | 全部 `GPIO_Input No-Pull`，Label=`KEY_K1`~`KEY_K4` |
| 5 | PC12 | `GPIO_Output High`，Label=`LED_HEARTBEAT` |
| 6 | PB6/PB7 | `I2C1_SCL/SDA` |
| 7 | PB10 | `I2C2_SCL` |
| 8 | PB12 | `I2C2_SDA_BB`（GPIO OD，非 AF） |
| 8 | PA9/PA10 | `USART1_TX/RX` |
| 9 | PA13/PA14 | `SYS_JTMS-SWDIO/JTCK-SWCLK` |
| 10 | TIM2 PSC/ARR | **839 / 999**（100 Hz） |
| 11 | TIM2 CCR1 | **0** |
| 12 | I2C1/I2C2 Speed | **400000 Hz** |
| 13 | USART1 | **115200 8N1**，RX **Pull-up** |
| 14 | PC13 | **未配置**（灰色 Reset） |
| 15 | USB OTG FS | **未启用** |
| 16 | NVIC | 仅 USART1=6 Enabled；TIM2/I2C/EXTI Disabled |

14 项全过 → Generate Code → 初次编译应零错误。

────────────────────────────────────────

## 12. 与 Top 板硬件方案对照检查

| 硬件方案章节 | CubeMX 对应 | 检查点 |
|---|---|---|
| § 四 PA0~PA5 控制输出 | § 6.1 + § 6.4 | PA0=PWM；PA1~PA5=阀 GPIO Low ✓ |
| § 四 PC0~PC3 按键 | § 6.2 | No pull（外部 10k 上拉）✓ |
| § 四 PC12 LED | § 6.1 | 初始 High，低电平点亮 ✓ |
| § 四 PB6/PB7 I2C1 | § 6.5 | Fast 400 kHz ✓ |
| § 四 PB10/PB12 I2C2 | § 6.6 | PB10=SCL ✓；PB12 需软件 SDA ⚠ |
| § 四 PA9/PA10 USART1 | § 6.7 | 115200 → CH340C ✓ |
| § 四 PA11/PA12 下拉 | § 3.1 | USB FS 不启用 ✓ |
| § 五 510Ω 限流 / 失效安全 | § 6.1/6.4 | GPIO Low = 负载 OFF ✓ |

────────────────────────────────────────

## 13. `.ioc` 关键字段速查

```
Mcu.Name=STM32F401RCTx
Mcu.Package=LQFP64

RCC.HSE_VALUE=25000000
RCC.PLLM=25
RCC.PLLN=168
RCC.PLLP=RCC_PLLP_DIV2
RCC.PLLQ=4
RCC.SYSCLKFreq_VALUE=84000000

TIM2.Prescaler=839
TIM2.Period=999
TIM2.Pulse-PWM Generation1 CH1=0

I2C1.I2C_Speed_Mode=I2C_Fast
I2C1.I2C_Speed_Frequency=400000
I2C2.I2C_Speed_Mode=I2C_Fast
I2C2.I2C_Speed_Frequency=400000

USART1.BaudRate=115200

NVIC.PriorityGroupConfig=NVIC_PRIORITYGROUP_4
NVIC.USART1_IRQn=true\:6\:0\:false\:false\:true\:false\:true\:true
```

────────────────────────────────────────

**文档结束。**

> 配套阅读：
> · `top板硬件设计方案.md`（本表硬件依据）
> · `硬件设计方案_V3.0.md`（驱动板 AO4407A 高边开关架构）
> · `总体设计书_V1.2.md`（软件状态机与互锁逻辑）
