《"花伴"(FloraMate) 智能家庭灌溉系统》STM32CubeMX 工程配置表 V1.0

────────────────────────────────────────

1. 文档信息


| 项目        | 内容                                                                                                                                                                                                                                                                                                               |
| --------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 文档名       | 花伴 FloraMate · CubeMX 工程配置表                                                                                                                                                                                                                                                                                      |
| 版本        | **V1.4**（对齐硬件方案 V3.0，AO4407A 高边开关版）                                                                                                                                                                                                                                                                              |
| 修订记录      | V1.0 首版；**V1.1**：水泵 PWM 频率由 25 kHz 调整为 **1 kHz**（TIM2 PSC=83 / ARR=999）；取消"未用引脚设为 Analog 省电"配置；删除 W25Q 升级路径。**V1.2**：以 CubeMX 6.17 实际生成代码为准回写 PLL 参数（**PLLN 336→168 / PLLP DIV4→DIV2 / PLLQ 7→4**，SYSCLK 仍为 84 MHz 不变；CubeMX 选了等效但 VCO 频率更低的路径，EMC 略优）。**V1.3**：水泵 PWM 频率调整为 **100 Hz**（TIM2 PSC=839 / ARR=999）。**V1.4**：对齐硬件方案 V3.0——以 AO4407A P 沟道高边开关替代 AO3400+S8050；去除独立 PUMP_EN；**引脚重新分组**：阀 Z1~Z5 + 泵 PWM 统一集中到右侧 PA0~PA5 连续区域，按键移至 PA6/PA7/PB0/PB1，PB12~PB15 / PA8 / PB0~PB1 旧角色全部释放；Label 去掉引脚编号后缀。 |
| 芯片        | **STM32F401RCT6**（Cortex-M4F，84 MHz，64 KB SRAM，256 KB Flash，LQFP64）                                                                                                                                                                                                                                              |
| 开发板       | WeAct MiniSTM32F401RCT6 V3.x（**HSE = 25 MHz**，LSE = 32.768 kHz）                                                                                                                                                                                                                                                  |
| 依据文档      | `硬件设计方案_V3.0.md`（§ 5 引脚分配表 / § 6 子模块接线 / § 7 PDN） / `总体设计书_V1.2.md`（§ 3 ~ § 5）                                                                                                                                                                                                                                   |
| 用途        | 在 STM32CubeMX 中**按本表勾选/填参**生成与硬件 1:1 对应的初始工程；菜单名以你安装的 MX 版本为准（6.x / 7.x / 12.x 类同）                                                                                                                                                                                                                               |
| 库选择       | **HAL**（与老 F103 版一致；不强制 LL）                                                                                                                                                                                                                                                                                      |
| Toolchain | MDK-ARM V5（Keil5）/ STM32CubeIDE / Makefile，任选                                                                                                                                                                                                                                                                    |


────────────────────────────────────────

1. 新建工程
2. **File → New Project**（或启动页 **ACCESS TO MCU SELECTOR**）。
3. **Part Number** 搜 `STM32F401RC` → 选 **STM32F401RCTx**（LQFP64）。
4. 双击芯片型号 → 进入引脚/外设视图。

> 选 LQFP64 即可，**不要**选 LQFP48；虽然本开发板 PC0~PC12/PD2 未引出，但芯片本体仍是 LQFP64，工程要按芯片实物来。

────────────────────────────────────────

1. SYS（系统）配置


| 项               | CubeMX 操作                          | 目标                                                                     |
| --------------- | ---------------------------------- | ---------------------------------------------------------------------- |
| Debug           | **SYS → Debug: Serial Wire**       | 保留 **PA13=SWDIO、PA14=SWCLK**；**严禁**选 `No Debug`（否则下次只能 BOOT0 进 ISP 救板） |
| Timebase Source | **SYS → Timebase Source: SysTick** | 与 HAL 默认一致；**勿**选 TIM2（TIM2 已被 PWM 占用）                                 |


**约束**：PA13/PA14/PA9/PA10/PA11/PA12 这些"有特殊功能的"引脚，未经评审**不得**挪作业务 GPIO。

### 2.1 核心板已占用 / 默认连接的引脚


| 引脚                        | 板上连接                                    | CubeMX 处理                                                    |
| ------------------------- | --------------------------------------- | ------------------------------------------------------------ |
| **PH0 / PH1**             | HSE 25 MHz 晶振 Y2 + 8pF×2                | RCC 选 HSE = Crystal/Ceramic Resonator                        |
| **PC14 / PC15**           | LSE 32.768 kHz 晶振 Y1 + 1.5pF×2          | 本版**不启用** RTC → RCC LSE = Disable；PC14/PC15 **不要**作 GPIO     |
| **PA11 / PA12**           | USB Type-C 接口（D-/D+）                    | 本版**不启用** USB → **保持空闲**；**不要**作业务 GPIO                      |
| **PA13 / PA14**           | SWD 接口                                  | SYS Debug: SW（如上）                                            |
| **PC13**                  | 板载红色 LED D3（VCC→330Ω→D3→PC13）           | GPIO Output，**低电平点亮**（心跳灯）                                   |
| **PA0**                   | 板载 SB1 用户键（**本项目不焊 SB1**）               | TIM2_CH1 PWM 输出（水泵调速 `PUMP_PWM`）                              |
| **PA1 ~ PA5**             | 引出（W25Q 焊盘仅 PA4/PA5，**本项目不焊 W25Q**）    | **V1.4 新**：PA1~PA5 = `VALVE_Z1`~`VALVE_Z5`（GPIO Output，高边开关驱动）  |
| **PA6 / PA7**             | 引出（W25Q MISO/MOSI 焊盘，**不焊**）            | **V1.4 新**：PA6 = `KEY_K1`，PA7 = `KEY_K2`（GPIO Input Pull-up）    |


────────────────────────────────────────

1. RCC 时钟配置

### 3.1 RCC 外设页


| 项                         | 选择                                       |
| ------------------------- | ---------------------------------------- |
| High Speed Clock (HSE)    | **Crystal/Ceramic Resonator**（板载 25 MHz） |
| Low Speed Clock (LSE)     | **Disable**（本版不启用 RTC，省一对脚的功耗）           |
| Master Clock Output (MCO) | Disable                                  |
| Audio Clock Input (I2S)   | Disable                                  |


### 3.2 Clock Configuration 页（**关键，必须精确**）

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

> **说明**：CubeMX 6.17 自动选择了 PLLN=168/PLLP=DIV2 这条路径，VCO 输出 168 MHz（F401 VCO 合法范围 100~432 MHz），EMC 比 VCO=336 MHz 略优。两种路径 SYSCLK 都是 84 MHz，所有外设时钟完全一致；这里以工具实际生成为准。如手工新建工程让 CubeMX 自动算最优解，得到的也是这套参数。

### 3.3 Flash 等待周期（自动）

84 MHz / 3.3 V → **Latency = 2 WS**，MX 会自动配置；可在生成代码里搜 `FLASH_LATENCY_2` 确认。

────────────────────────────────────────

1. 外设清单（必配）


| 外设                      | 引脚 / 模式                                        | CubeMX 主要选择                                                                   |
| ----------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------- |
| **TIM2** PWM（水泵调速）      | PA0 = `TIM2_CH1`（AF1）                          | Label: `PUMP_PWM`；模式 **PWM Generation CH1**；初始 CCR=0（停泵）                       |
| **GPIO** 阀 MOSFET ×5   | PA1/PA2/PA3/PA4/PA5 = `GPIO_Output`            | Label: `VALVE_Z1`~`VALVE_Z5`；初始 **Low**（P-MOS 栅极上拉=12V=截止，失效安全）              |
| **GPIO** 按键 ×4          | PA6/PA7/PB0/PB1 = `GPIO_Input` + **Pull-up**   | Label: `KEY_K1`~`KEY_K4`；按下接 GND                                               |
| **GPIO** 心跳灯             | PC13 = `GPIO_Output`                           | Label: `LED_HEARTBEAT`；初始 **High（灭）**                                          |
| **I2C1**                | PB6 = `I2C1_SCL`（AF4），PB7 = `I2C1_SDA`（AF4）    | Label: `I2C1_SCL` / `I2C1_SDA`；**Fast Mode 400 kHz**                           |
| **USART1**              | PA9 = `USART1_TX`（AF7），PA10 = `USART1_RX`（AF7） | Label: `USART1_TX` / `USART1_RX`；**Asynchronous 115200 8N1**                   |

> **V1.4 引脚分组原则**：PA0~PA5（右侧连续 6 个引脚）= 全部高边开关控制（泵 PWM + 阀 Z1~Z5），接 TLP281 驱动 AO4407A；PA6/PA7/PB0/PB1（右侧紧邻 4 个）= 4 路按键；PB12~PB15 / PA8 / PB2~PB3 等释放为备用。


**不启用的外设**：USB OTG FS、SPI1（W25Q 不焊）、ADC1（Phase 2）、IWDG（本版同老 F103，开发期勿喂狗触发误复位）、RTC、CAN、SDIO、I2S。

────────────────────────────────────────

1. 各外设参数细节（在 Parameter Settings）

### 5.1 GPIO 输出 — 阀 MOSFET（P 沟道高边开关）+ 心跳灯（V1.4）


| 引脚       | Label            | Output level | Mode      | Speed | Pull    | 说明                                  |
| -------- | ---------------- | ------------ | --------- | ----- | ------- | ------------------------------------- |
| **PA1**  | `VALVE_Z1`       | **Low**      | Push-Pull | Low   | No pull | 阀 Z1；Low → TLP281 截止 → Gate 上拉 12V → P-MOS 截止（失效安全）|
| **PA2**  | `VALVE_Z2`       | **Low**      | Push-Pull | Low   | No pull | 阀 Z2                                 |
| **PA3**  | `VALVE_Z3`       | **Low**      | Push-Pull | Low   | No pull | 阀 Z3                                 |
| **PA4**  | `VALVE_Z4`       | **Low**      | Push-Pull | Low   | No pull | 阀 Z4                                 |
| **PA5**  | `VALVE_Z5`       | **Low**      | Push-Pull | Low   | No pull | 阀 Z5（备用）；与 Z1~Z4 完全等价               |
| **PC13** | `LED_HEARTBEAT`  | **High**     | Push-Pull | Low   | No pull | 板载红色 LED；低电平点亮；上电先灭，固件 500ms 翻转     |


> **P 沟道极性铁律（V1.4）**
>
> | GPIO | TLP281 | AO4407A Gate | Vgs | MOSFET | 负载 |
> |------|--------|-------------|-----|--------|------|
> | Low（复位默认）| OFF | 经 10kΩ 上拉到 12V | 0V | **截止** | 断电 ✅ 失效安全 |
> | High（开阀指令）| ON（拉 Gate→GND）| ~0.3V | −11.7V | **导通** | 通电 ✅ |
>
> **无需 S8050 反相级**：P 沟道直接由 TLP281 下拉栅极，逻辑与 N 沟道版完全相同（GPIO High = 负载 ON），电路更简洁。

### 5.2 GPIO 输入 — 4 路按键（V1.4：移至 PA6/PA7/PB0/PB1）


| 引脚      | Label    | Mode  | Pull        | 触发方式         | 说明        |
| ------- | -------- | ----- | ----------- | ------------ | --------- |
| **PA6** | `KEY_K1` | Input | **Pull-up** | 软件 20ms 轮询去抖 | 启动 / 跳过自检 |
| **PA7** | `KEY_K2` | Input | **Pull-up** | 同上           | 跳过当前路     |
| **PB0** | `KEY_K3` | Input | **Pull-up** | 同上           | 暂停 / 恢复   |
| **PB1** | `KEY_K4` | Input | **Pull-up** | 同上           | 菜单 / 确认   |


> 按键模块板内已有 10 kΩ 上拉，与 MCU 内部 ~40 kΩ 并联等效 ~8 kΩ，对按键无影响。**不**启用外部中断，用轮询足够。
> PA6/PA7 原为 W25Q MISO/MOSI 焊盘，**W25Q 永久不焊**，可直接复用为按键输入。

### 5.3 未使用引脚处理

本项目**不考虑低功耗**，对未使用引脚采取**"放任 Reset 默认态"**策略：

- CubeMX **不勾选**"Set all free pins as analog"（见 § 7.2）；
- **V1.4 版已释放的引脚（全部保持灰色 Reset 状态）**：PB12/PB13/PB14/PB15、PA8、PB2(BOOT1)、PB3/PB4/PB5/PB8/PB9/PB10/PA15；
- 这些引脚上电后是高阻态，既不输出也不形成确定输入，**对本项目无干扰**。Phase 2 扩展浮球/水浸等传感器时优先从 PB12~PB15 / PA8 中取用。

> 优点：`MX_GPIO_Init()` 函数只包含实际用到的引脚；后期接预留传感器无需先删除配置。

### 5.4 TIM2 — Channel 1 PWM（PA0，水泵调速）

页面：**Timers → TIM2 → Mode**


| 项            | 设置                     | 说明                    |
| ------------ | ---------------------- | --------------------- |
| Clock Source | **Internal Clock**     | 用 PLL→APB1×2 = 84 MHz |
| Channel1     | **PWM Generation CH1** | PA0 自动复用为 AF1         |
| Channel2/3/4 | Disable                | —                     |


页面：**TIM2 → Parameter Settings**


| 项                                                    | 数值          | 计算 / 说明                                 |
| ---------------------------------------------------- | ----------- | --------------------------------------- |
| **Prescaler (PSC)**                                  | **839**     | 84 MHz / (839+1) = 100 kHz 计数频率         |
| **Counter Mode**                                     | Up          | —                                       |
| **Counter Period (ARR)**                             | **999**     | 100 kHz / (999+1) = **100 Hz** PWM 频率 ★ |
| Internal Clock Division (CKD)                        | No Division | —                                       |
| **auto-reload preload**                              | **Enable**  | 占空比修改与下一周期同步生效，无毛刺                      |
| Trigger Output (TRGO) Parameters → Master/Slave Mode | **Disable** | 本设计不做主从触发                               |
| TRGO → Trigger Event Selection                       | **Reset**   | 与上一项配套                                  |


**PWM Generation Channel 1** 子参数：


| 项                      | 数值         | 说明                            |
| ---------------------- | ---------- | ----------------------------- |
| Mode                   | PWM mode 1 | CNT < CCR → 高电平有效             |
| **Pulse (CCR)**        | **0**      | 初始占空比 = 0%，**上电不起转** ★        |
| Output compare preload | **Enable** | 占空比修改顺滑                       |
| Fast Mode              | Disable    | —                             |
| CH Polarity            | **High**   | 与 TLP281 输入"高电平有效"匹配；PA0=高 → TLP281 ON → AO4407A Gate 拉低 → 水泵转 |


> **占空比公式**：`CCR = ARR × duty% = 999 × duty%`，**千分之一占空比分辨率**。例：
> · 35% → CCR = 350
> · 55% → CCR = 550
> · 75% → CCR = 750
> · 95% → CCR = 950
> 固件中用 `__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr)` 动态调整。
>
> **若后期想切回 25 kHz**：把 PSC 改为 3、ARR 改为 839 即可（千分位占空比可读性会略差，但仍能保留 4 档阶梯精度）。

### 5.5 I2C1 — OLED + AT24C64 共用

页面：**Connectivity → I2C1 → Mode**


| 项                                              | 选择                                  |
| ---------------------------------------------- | ----------------------------------- |
| **I2C**                                        | **I2C**                             |
| **Master Features → I2C Speed Mode**           | **Fast Mode**                       |
| **Master Features → I2C Speed Frequency (Hz)** | **400000**                          |
| Master Features → Fast Mode Duty Cycle         | **Mode_2**（Tlow/Thigh = 2/1，节省总线时间） |
| Slave Features                                 | 全部 Disable（Master only）             |


页面：**I2C1 → Parameter Settings**（多数自动算）


| 项                              | 值       | 说明                      |
| ------------------------------ | ------- | ----------------------- |
| Clock No Stretch Mode          | Disable | 允许从机拉长 SCL（AT24C 写周期会用） |
| General Call Address Detection | Disable | —                       |
| Primary Address Length         | 7-bit   | —                       |
| Dual Address Acknowledged      | Disable | —                       |
| Primary slave address          | 0       | Master 模式不关             |


页面：**I2C1 → GPIO Settings**


| 引脚      | Signal   | Mode                          | Pull           | Speed | 说明                                            |
| ------- | -------- | ----------------------------- | -------------- | ----- | --------------------------------------------- |
| **PB6** | I2C1_SCL | Alternate Function Open Drain | **No pull-up** | High  | 模块板已带 OLED 4.7kΩ + AT24 10kΩ 外部上拉，**不需要**再开内部 |
| **PB7** | I2C1_SDA | Alternate Function Open Drain | **No pull-up** | High  | 同上                                            |


> ⚠️ **不要**在这里勾"Internal Pull-up"。开了之后内部 ~40 kΩ 弱上拉 + 外部 3.2 kΩ 并联仍是 3.2 kΩ 不变，但会增加上拉路径阻抗的不确定性，**保持 No pull-up 让外部外置上拉**唯一驱动总线，是行业惯例。

### 5.6 USART1 — 调试日志

页面：**Connectivity → USART1 → Mode**


| 项                     | 选择                      |
| --------------------- | ----------------------- |
| **Mode**              | **Asynchronous**（全双工异步） |
| Hardware Flow Control | **Disable**（不接 RTS/CTS） |


页面：**USART1 → Parameter Settings**


| 项              | 值                         |
| -------------- | ------------------------- |
| Baud Rate      | **115200**                |
| Word Length    | 8 Bits (including Parity) |
| Parity         | **None**                  |
| Stop Bits      | **1**                     |
| Data Direction | Receive and Transmit      |
| Over Sampling  | 16 Samples                |


页面：**USART1 → GPIO Settings**


| 引脚       | Signal    | Mode                             | Pull          | Speed | 说明                                            |
| -------- | --------- | -------------------------------- | ------------- | ----- | --------------------------------------------- |
| **PA9**  | USART1_TX | Alternate Function **Push-Pull** | No pull-up    | High  | TX 推挽，发日志                                     |
| **PA10** | USART1_RX | Alternate Function Push-Pull     | **Pull-up** ★ | High  | **必须开内部上拉**：避免 USB-TTL 未连时 RX 浮空触发 ORE / 随机字节 |


页面：**USART1 → NVIC Settings**


| 项                           | 选择          | 说明                               |
| --------------------------- | ----------- | -------------------------------- |
| **USART1 global interrupt** | **Enabled** | RX 中断 + 环形缓冲；**勿**在 ISR 里 printf |


────────────────────────────────────────

1. NVIC（中断优先级）

### 6.1 NVIC 优先级分组

页面：**System Core → NVIC → Code generation**


| 项                  | 选择                                                          |
| ------------------ | ----------------------------------------------------------- |
| **Priority Group** | **4 bits for pre-emption priority, 0 bits for subpriority** |


> F4 是 16 个优先级位（M4 标准），全部用作 Preempt，与软件文档配套；不用 SubPriority 防止思路混乱。

### 6.2 各中断优先级


| 中断                                                      | Enable      | Pre-emption Priority | 说明                                    |
| ------------------------------------------------------- | ----------- | -------------------- | ------------------------------------- |
| **NMI / HardFault / MemManage / BusFault / UsageFault** | 默认          | 0                    | 系统级，必须最高                              |
| **SVC**                                                 | 默认          | 0                    | —                                     |
| **PendSV**                                              | 默认          | 15                   | HAL 用，**勿改**                          |
| **SysTick**                                             | 默认          | **15**（最低）           | HAL 1ms tick，**不要**在 ISR 里 printf     |
| **USART1 global interrupt**                             | **Enable**  | **6**                | RX 收字节 → 环形缓冲 / 置标志                   |
| **TIM2 global interrupt**                               | **Disable** | —                    | PWM 输出不需要中断                           |
| **I2C1 event / I2C1 error**                             | **Disable** | —                    | OLED + EEPROM 都用阻塞/查询模式即可；后续加 DMA 再启用 |
| **EXTI**（任何）                                            | **Disable** | —                    | 4 按键走轮询，不用外部中断                        |
| **IWDG**（看门狗）                                           | **Disable** | —                    | **本版不启用 IWDG**（与 V1.2 总设有微调，见 § 8.3）  |


### 6.3 NVIC 约束（与软件文档同步）

- **不要**在任何中断里调用 `printf` / `HAL_Delay` / `osDelay`；
- USART1 RX ISR 仅"收字节 → 环形缓冲 / 置标志"；
- 如未来加"紧急停机外部中断"，可把其优先级设为 **3 ~ 4**，USART1 仍保 **6**。

────────────────────────────────────────

1. Project Manager（工程生成选项）

### 7.1 Project 页


| 项                                    | 推荐                                                            |
| ------------------------------------ | ------------------------------------------------------------- |
| Project Name                         | `FloraMate_F401`（或自定义）                                        |
| Project Location                     | 任意，**避免中文路径与空格**                                              |
| Toolchain / IDE                      | **MDK-ARM V5.32**（Keil5）/ **STM32CubeIDE** / **Makefile**，三选一 |
| Linker Settings → Minimum Heap Size  | **0x200**（512 B；F401 SRAM 64 KB 富余）                           |
| Linker Settings → Minimum Stack Size | **0x800**（2 KB；与软件文档建议一致）                                     |


### 7.2 Code Generator 页


| 项                                                                            | 推荐         | 说明                                                                        |
| ---------------------------------------------------------------------------- | ---------- | ------------------------------------------------------------------------- |
| Copy only the necessary library files                                        | ☑ 勾选       | 工程瘦身                                                                      |
| Generate peripheral initialization as a pair of '.c/.h' files per peripheral | ☑ 勾选 ★     | 每个外设独立 `gpio.c/gpio.h`、`tim.c/tim.h`、`i2c.c/i2c.h`、`usart.c/usart.h`，便于维护 |
| Backup previously generated files when re-generating                         | ☑ 勾选       | 重生成前自动备份                                                                  |
| Keep User Code when re-generating                                            | ☑ 勾选 ★     | **必勾**，保留 `/* USER CODE BEGIN */`~`/* USER CODE END */` 之间手写代码            |
| Delete previously generated files when not re-generated                      | ☐ 不勾       | 防止误删手写文件                                                                  |
| Set all free pins as analog (to optimize the power consumption)              | **☐ 不勾** ★ | 本项目不考虑低功耗（§ 5.3），未用引脚保持 Reset 默认态即可                                       |


### 7.3 Advanced Settings 页


| 项                           | 推荐           | 说明                                   |
| --------------------------- | ------------ | ------------------------------------ |
| Driver Selector（每个外设）       | 全部保持 **HAL** | 不混 LL                                |
| Generated Function Calls 顺序 | 默认（按字母序）     | 通常 RCC → GPIO → I2C1 → TIM2 → USART1 |
| Register Callback           | Disable      | 用静态回调注册（默认）即可                        |


────────────────────────────────────────

1. 全引脚分配总表（按 LQFP64 + WeAct 板原理图）


> **V1.4 引脚分组总览**（右侧排针从上到下）
> - 右-26 PA0：泵 PWM（TIM2）
> - 右-27~31 PA1~PA5：阀 Z1~Z5（连续，接 TLP281 → AO4407A）
> - 右-32~33 PA6~PA7：按键 K1~K2
> - 右-34~35 PB0~PB1：按键 K3~K4
>
> 左侧 PA9/PA10 = UART；PB6/PB7 = I2C（AF 固定，无法移动）。PB12~PB15 / PA8 全部释放为 Phase 2 备用。


| 引脚                          | 核心板连接              | 本项目 CubeMX 配置                                   | 备注                                       |
| --------------------------- | ------------------ | ------------------------------------------------ | ---------------------------------------- |
| **PA0**                     | 引出 + SB1 焊盘（不焊）   | **TIM2_CH1 PWM**（`PUMP_PWM`）                     | 泵调速 100Hz；**SB1 不焊**                     |
| **PA1**                     | 引出                 | **GPIO Output Low PP** `VALVE_Z1`                | 阀 Z1；TLP281 → AO4407A 高边开关               |
| **PA2**                     | 引出                 | **GPIO Output Low PP** `VALVE_Z2`                | 阀 Z2                                    |
| **PA3**                     | 引出                 | **GPIO Output Low PP** `VALVE_Z3`                | 阀 Z3                                    |
| **PA4**                     | 引出 + W25Q F_CS 焊盘  | **GPIO Output Low PP** `VALVE_Z4`                | 阀 Z4；**W25Q 不焊**，焊盘悬空无影响                 |
| **PA5**                     | 引出 + W25Q SCK 焊盘   | **GPIO Output Low PP** `VALVE_Z5`                | 阀 Z5（备用）；**W25Q 不焊**                     |
| **PA6**                     | 引出 + W25Q MISO 焊盘  | **GPIO Input Pull-up** `KEY_K1`                  | 按键 K1；**W25Q 不焊**，可复用                    |
| **PA7**                     | 引出 + W25Q MOSI 焊盘  | **GPIO Input Pull-up** `KEY_K2`                  | 按键 K2；同上                                |
| **PA8**                     | 引出                 | 未配置（保持 Reset 默认）                                 | **V1.4 释放**（原阀 Z5）；Phase 2 备用            |
| **PA9**                     | 引出                 | **USART1_TX** AF7 PP `USART1_TX`                 | 调试日志 TX                                  |
| **PA10**                    | 引出                 | **USART1_RX** AF7 PP Pull-up `USART1_RX`         | 调试日志 RX；**必须开 Pull-up**                  |
| **PA11**                    | USB D-             | 保留（USB 不启用）                                      | —                                        |
| **PA12**                    | USB D+             | 保留                                               | —                                        |
| **PA13**                    | SWDIO              | **SYS Debug: SW**                                | 必保留                                      |
| **PA14**                    | SWCLK              | **SYS Debug: SW**                                | 必保留                                      |
| **PA15**                    | 引出                 | 未配置（保持 Reset 默认）                                 | 备用                                       |
| **PB0**                     | 引出                 | **GPIO Input Pull-up** `KEY_K3`                  | 按键 K3；**V1.4 改**（原备用 MOSFET）              |
| **PB1**                     | 引出                 | **GPIO Input Pull-up** `KEY_K4`                  | 按键 K4；**V1.4 改**（原 PUMP_EN）               |
| **PB2**                     | BOOT1 / 引出         | 未配置（保持 Reset 默认）                                 | 不作业务                                     |
| **PB3**                     | 引出                 | 未配置（保持 Reset 默认）                                 | 备用（Phase 2 浮球信号候选）                       |
| **PB4**                     | 引出                 | 未配置（保持 Reset 默认）                                 | 备用                                       |
| **PB5**                     | 引出                 | 未配置（保持 Reset 默认）                                 | 备用                                       |
| **PB6**                     | 引出                 | **I2C1_SCL** AF4 OD No-Pull `I2C1_SCL`           | OLED + AT24C64 共用                        |
| **PB7**                     | 引出                 | **I2C1_SDA** AF4 OD No-Pull `I2C1_SDA`           | OLED + AT24C64 共用                        |
| **PB8**                     | 引出                 | 未配置（保持 Reset 默认）                                 | 备用                                       |
| **PB9**                     | 引出                 | 未配置（保持 Reset 默认）                                 | 备用                                       |
| **PB10**                    | 引出                 | 未配置（保持 Reset 默认）                                 | 备用（Phase 2 水浸传感器候选）                      |
| **PB12**                    | 引出                 | 未配置（保持 Reset 默认）                                 | **V1.4 释放**（原阀 Z1）；Phase 2 备用            |
| **PB13**                    | 引出                 | 未配置（保持 Reset 默认）                                 | **V1.4 释放**（原阀 Z2）；Phase 2 备用            |
| **PB14**                    | 引出                 | 未配置（保持 Reset 默认）                                 | **V1.4 释放**（原阀 Z3）；Phase 2 备用            |
| **PB15**                    | 引出                 | 未配置（保持 Reset 默认）                                 | **V1.4 释放**（原阀 Z4）；Phase 2 备用            |
| **PC13**                    | 板载 LED D3          | **GPIO Output High PP** `LED_HEARTBEAT`          | 心跳灯，低电平点亮                                |
| **PC14**                    | LSE Y1             | 保留（不启用 RTC，**勿**改 GPIO）                          | —                                        |
| **PC15**                    | LSE Y1             | 保留                                               | —                                        |
| **PH0 / PH1**               | HSE Y2             | **RCC: HSE Crystal**                             | —                                        |
| **NRST**                    | SW1 复位键            | 默认（无需配置）                                         | —                                        |
| **BOOT0**                   | 板载下拉               | 默认                                               | 跑 Flash，烧写时短接 +3.3V 进 ISP               |
| **VBAT**                    | 板载 R3=10K NC       | 不接电池                                             | Phase 2 加 CR2032 时单独评估                   |
| **VDD / VSS / VDDA / VSSA** | 供电与模拟地             | —                                                | 板载已处理                                    |


────────────────────────────────────────

1. 生成代码后必做的几件事

### 9.1 USART1 RX 中断回调（环形缓冲）

CubeMX 只生成 `MX_USART1_UART_Init()`，不生成回调框架。在 `main.c` 的 `USER CODE BEGIN PV` 区添加环形缓冲 + 在 `MX_USART1_UART_Init()` 后调 `HAL_UART_Receive_IT(&huart1, &rx_byte, 1)` 启动接收（**不**在 ISR 里 printf）。

详见配套软件设计文档的 `bsp_uart.c`。

### 9.2 TIM2 PWM 启动

CubeMX 只生成 `MX_TIM2_Init()`，**不会自动启动 PWM 输出**。需在 `main()` 进入主循环前手动启动：

```c
/* USER CODE BEGIN 2 */
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);  /* 占空比 0%，水泵停 */
/* USER CODE END 2 */
```

> ⚠️ 若忘记调用 `HAL_TIM_PWM_Start`，PA0 不输出 PWM，水泵不会转，但也不报错——排错时优先检查此处。

### 9.3 I2C1 上电后扫描

调试期建议在 `main()` 中加一段 I²C 扫描，确认 OLED 0x3C 与 AT24C64 0x50 同时在线（详见硬件方案 § 13 验收测试 #5）：

```c
for (uint8_t addr = 1; addr < 128; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("I2C device found at 0x%02X\r\n", addr);
    }
}
```

应同时出现：`0x3C`（OLED）与 `0x50`（AT24C64）。

### 9.4 GPIO 上电默认态确认

`MX_GPIO_Init()` 生成后会先把阀控引脚（PA1~PA5）写成 **Low**，与 CubeMX 中 `Output level=Low` 一致。**确认 `MX_GPIO_Init()` 在任何业务代码之前调用**，避免上电瞬间 MOSFET 误导通。V1.4 P 沟道架构中，GPIO Low → TLP281 截止 → AO4407A Gate 上拉 12V → MOSFET 截止 = 全部负载断电（失效安全），无需额外看护。

────────────────────────────────────────

1. 已知的"自动生成无害冗余"（与老 F103 版同样的提示）


| 现象                                                             | 是否问题 | 处理                                                  |
| -------------------------------------------------------------- | ---- | --------------------------------------------------- |
| `HAL_RCC_GPIOH_CLK_ENABLE()` 被生成                               | 否    | F4 上 HSE 引脚 PH0/PH1 物理位于 GPIOH，时钟使能是正常行为            |
| `TIM2 → TRGO` 字段被生成（默认 Reset）                                  | 否    | 已在 § 5.4 显式设 `Trigger Event Selection = Reset`，不会影响 |
| 未使用 GPIO 出现在 `MX_GPIO_Init()` 里                                | 否    | 勾了"Set all free pins as analog" 后会生成 Analog 配置，正常   |
| Cube 自动添加 `HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4)` | 否    | 与 § 6.1 一致                                          |


────────────────────────────────────────

1. 与硬件方案 V3.0 的对照检查


| 硬件方案章节                             | CubeMX 对应                       | 检查点                                                  |
| ---------------------------------- | -------------------------------- | ---------------------------------------------------- |
| § 5 全引脚分配表 V3.0                    | § 8 全引脚分配总表                      | 逐行核对：PA0=泵 PWM；PA1~PA5=阀 Z1~Z5；PA6/PA7/PB0/PB1=按键   |
| § 6 固态驱动（TLP281 + AO4407A P-MOS）  | § 5.1 阀 GPIO（PA1~PA5）初始 Low      | GPIO Low = P-MOS 截止 = 失效安全 ✓；**无 S8050**             |
| § 6 泵 PWM（PA0 → TLP281 → AO4407A） | § 5.4 TIM2 CH Polarity = High    | PA0 High → TLP281 ON → Gate 拉低 → 水泵转；CCR=0 = 停泵 ✓    |
| § 6 无独立 PUMP_EN                    | —（无该引脚）                          | L1 层纯软件（占空比=0 + 阀全关联锁）；看门狗复位保底 ✓                    |
| § 6 OLED I²C 0x3C                  | § 5.5 I2C1 Fast Mode 400kHz      | I²C1 配置 OK                                           |
| § 6 AT24C64 I²C 0x50 + WP 跳线       | § 5.5 I2C1 共用 + § 9.3 扫描验证       | 一致                                                   |
| § 6 心跳灯 PC13 低电平点亮                | § 5.1 PC13 初始 High（灭）            | 一致                                                   |
| § 7 PDN（无 LM2596，无 LR7843 模块）     | —                                | CubeMX 不涉及 PDN，硬件验收即可                                |


────────────────────────────────────────

1. 可选 / 后续（**非首版必开**）


| 功能                  | CubeMX 操作                                                                  | 启用时机                   |
| ------------------- | -------------------------------------------------------------------------- | ---------------------- |
| **IWDG 独立看门狗**      | RCC → LSI Enable；Watchdog → IWDG → Activated；Prescaler=64，Reload=625 (≈1s) | Phase 2（出厂量产前评估，开发期不开） |
| **RTC 实时时钟**        | RCC → LSE Crystal；Timers → RTC → Activated                                 | Phase 3（若取消米家托管）       |
| **ADC1 4 通道**       | Analog → ADC1 → IN5/IN6/IN7/IN8（按需）+ DMA                                   | Phase 2 加土壤湿度时         |
| **TIM1_CH1 备用 PWM** | Timers → TIM1 → CH1 PWM @ PA8                                              | 若 TIM2 让位给输入捕获         |
| **I2C1 DMA**        | I2C1 → DMA Settings → Add TX/RX DMA                                        | OLED 全屏刷新性能不够时         |
| **USB CDC**（虚拟串口）   | Middleware → USB Device → CDC                                              | Phase 3，取代 USART1 调试链路 |


────────────────────────────────────────

1. 工程构建期自检表（生成后第一次编译前）


| 序   | 检查项                                        | 通过判据                                                     |
| --- | ------------------------------------------ | -------------------------------------------------------- |
| 1   | Clock Configuration 顶部                     | 显示 **84 MHz**，绿色（不黄/红）                                   |
| 2   | Pinout 视图 PA0                              | 显示 `TIM2_CH1`（绿色），Label = `PUMP_PWM`                     |
| 3   | Pinout 视图 PA1~PA5                          | 全部 `GPIO_Output`，Label = `VALVE_Z1`~`VALVE_Z5`           |
| 4   | Pinout 视图 PA6/PA7/PB0/PB1                  | 全部 `GPIO_Input`，Label = `KEY_K1`~`KEY_K4`               |
| 5   | Pinout 视图 PB6/PB7                          | 显示 `I2C1_SCL` / `I2C1_SDA`（绿色）                           |
| 6   | Pinout 视图 PA13/PA14                        | 显示 `SYS_JTMS-SWDIO` / `SYS_JTCK-SWCLK`（绿色）               |
| 7   | Pinout 视图 PA9/PA10                         | 显示 `USART1_TX` / `USART1_RX`（绿色）                         |
| 8   | 阀控引脚 Output level（PA1~PA5）                 | 全部 **Low**（P-MOS 截止，失效安全）                                 |
| 9   | 按键引脚 Pull（PA6/PA7/PB0/PB1）                 | 全部 **GPIO_Input + Pull-up**                              |
| 10  | PB12~PB15 / PA8                            | **均保持 Reset 默认（灰色）**，未配置任何外设                             |
| 11  | TIM2 ARR/PSC                               | **PSC=839, ARR=999**（100 Hz PWM）                         |
| 12  | TIM2 CCR1                                  | **0**（上电不起转）                                             |
| 13  | I2C1 Speed                                 | **Fast Mode 400000 Hz**                                  |
| 14  | USART1 Baud                                | **115200 8N1**                                           |
| 15  | NVIC USART1 优先级                            | **6**，Enabled                                            |
| 16  | NVIC TIM2 / I2C1 / EXTI 中断                 | **全部 Disabled**                                          |
| 17  | Code Generator                             | **Generate per peripheral pair** + **Keep User Code** 已勾 |


14 项全过 → Generate Code → 进 Keil/CubeIDE，初次编译应**零错误零警告**（除了 HAL 库自带的几个无害警告）。

────────────────────────────────────────

1. CubeMX `.ioc` 文件关键字段速查（手工核对用）

如果你想跳过 GUI 直接看 `.ioc` 文本是否正确，关键字段如下（值仅供核对）：

```
Mcu.Family=STM32F4
Mcu.IPNb=14
Mcu.Name=STM32F401R(B-C-D-E)Tx
Mcu.Package=LQFP64
Mcu.UserName=STM32F401RCTx

RCC.HSE_VALUE=25000000
RCC.PLLM=25
RCC.PLLN=168
RCC.PLLP=RCC_PLLP_DIV2
RCC.PLLQ=4
RCC.SYSCLKFreq_VALUE=84000000
RCC.HCLKFreq_Value=84000000
RCC.APB1Freq_Value=42000000
RCC.APB2Freq_Value=84000000
RCC.APB1TimFreq_Value=84000000

TIM2.Prescaler=839
TIM2.Period=999
TIM2.Channel-PWM Generation1 CH1=TIM_CHANNEL_1
TIM2.Pulse-PWM Generation1 CH1=0

I2C1.IPParameters=Timing,I2C_Speed_Mode,I2C_Speed_Frequency,...
I2C1.I2C_Speed_Mode=I2C_Fast
I2C1.I2C_Speed_Frequency=400000

USART1.BaudRate=115200
USART1.WordLength=UART_WORDLENGTH_8B
USART1.Parity=UART_PARITY_NONE
USART1.StopBits=UART_STOPBITS_1

NVIC.PriorityGroupConfig=NVIC_PRIORITYGROUP_4
NVIC.USART1_IRQn=true\:6\:0\:false\:false\:true\:false\:true\:true
```

────────────────────────────────────────

**文档结束。**

> 配套阅读：
> · `硬件设计方案_V3.0.md`（本表的依据；V3.0 AO4407A P-MOS 高边开关架构）
> · `总体设计书_V1.2.md`（架构与状态机）
> · 软件设计文档（BSP 层：`bsp_valve.c` / `bsp_pump_pwm.c` / `bsp_oled.c` / `bsp_eeprom.c` / `bsp_key.c` / `bsp_uart.c`）

